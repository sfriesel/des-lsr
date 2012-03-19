#include "lsr_tc.h"
#include "lsr_nt.h"
#include "../lsr_config.h"
#include <utlist.h>

//the set of all currently known nodes
static node_t *node_set = NULL;
static node_t *this_node = NULL;

#if 0
edge_t *lsr_create_edge(node_t *target, uint32_t lifetime, uint32_t weight) {
	edge_t *edge = malloc(sizeof(edge_t));
	edge->node = target;
	edge->lifetime = lifetime;
	edge->weight = weight;
	return edge;
}
#endif

node_t *lsr_tc_create_node(mac_addr addr) {
	node_t *node = lsr_node_new(addr);
	HASH_ADD_KEYPTR(hh, node_set, node->addr, ETH_ALEN, node);
	return node;
}

node_t *lsr_tc_get_node(mac_addr addr) {
	node_t *node = NULL;
	HASH_FIND(hh, node_set, addr, ETH_ALEN, node);
	return node;
}

node_t *lsr_tc_get_or_create_node(mac_addr addr) {
	node_t *node = lsr_tc_get_node(addr);
	if(!node) {
		node = lsr_tc_create_node(addr);
	}
	return node;
}

struct timeval lsr_tc_calc_timeout(uint8_t lifetime) {
	uint32_t lifetime_ms = lifetime * tc_interval;
	struct timeval timeout;
	gettimeofday(&timeout, NULL);
	dessert_timevaladd(&timeout, lifetime_ms / 1000, (lifetime_ms % 1000) * 1000);
	return timeout;
}

dessert_result_t lsr_tc_update_node(mac_addr node_addr, uint16_t seq_nr) {
	node_t *node = lsr_tc_get_or_create_node(node_addr);
	lsr_node_set_timeout(node, lsr_tc_calc_timeout(node_lifetime));
	
	return DESSERT_OK;
}

dessert_result_t lsr_tc_update_node_neighbor(mac_addr node_addr, mac_addr neighbor_addr, uint8_t lifetime, uint8_t weight) {
	node_t *node = lsr_tc_get_or_create_node(node_addr);
	node_t * neighbor = lsr_tc_get_or_create_node(neighbor_addr);
	
	lsr_node_update_neighbor(node, neighbor, lsr_tc_calc_timeout(lifetime), weight);
	
	return DESSERT_OK;
}

bool lsr_tc_check_unicast_seq_nr(mac_addr node_addr, uint16_t seq_nr) {
	node_t *node = lsr_tc_get_node(node_addr);
	if (!node) {
		return true;
	}
	return lsr_node_check_unicast_seq_nr(node, seq_nr);
}

bool lsr_tc_check_broadcast_seq_nr(mac_addr node_addr, uint16_t seq_nr) {
	node_t *node = lsr_tc_get_node(node_addr);
	if (!node) {
		return true;
	}
	return lsr_node_check_broadcast_seq_nr(node, seq_nr);
}

dessert_result_t lsr_tc_get_next_hop(mac_addr dest_addr, mac_addr *next_hop, dessert_meshif_t **iface) {
	node_t *dest = NULL;
	HASH_FIND(hh, node_set, dest_addr, ETH_ALEN, dest);
	if(!dest || !dest->next_hop) {
		return DESSERT_ERR;
	}
	
	memcpy(*next_hop, dest->next_hop->addr, ETH_ALEN);
	*iface = dest->next_hop->iface;
	return DESSERT_OK;
}

dessert_result_t lsr_tc_age_all(void) {
	struct timeval now;
	gettimeofday(&now, NULL);
	node_t *node, *tmp;
	HASH_ITER(hh, node_set, node, tmp) {
		bool dead = lsr_node_age(node, &now);
		if(dead) {
			HASH_DEL(node_set, node);
			lsr_node_delete(node);
		}
	}
	return DESSERT_OK;
}

typedef struct priority_queue {
	struct priority_queue *prev, *next;
	node_t *node;
} priority_queue_t;

node_t *priority_queue_poll(priority_queue_t **queue) {
	if(!*queue) {
		return NULL;
	}
	priority_queue_t *min_item = *queue;
	priority_queue_t *item;
	CDL_FOREACH(*queue, item) {
		if(item->node->weight < min_item->node->weight) {
			min_item = item;
		}
	}
	CDL_DELETE(*queue, min_item);
	node_t *result = min_item->node;
	free(min_item);
	return result;
}

void priority_queue_add(priority_queue_t **queue, node_t *node) {
	priority_queue_t *el = malloc(sizeof(priority_queue_t));
	el->node = node;
	CDL_PREPEND(*queue, el);
}

dessert_result_t lsr_tc_dijkstra() {
	//don't calculate routes if own address is unknown
	if(mac_equal(dessert_l25_defsrc, ether_null)) {
		return DESSERT_ERR;
	}
	node_t *node = NULL;
	for(node = node_set; node; node = node->hh.next) {
		node->weight = INFINITE_WEIGHT;
		node->next_hop = NULL;
	}
	
	//create this node as soon as defsrc is available
	if(!this_node) {
		this_node = lsr_tc_create_node(dessert_l25_defsrc);
	}
	//initialize this node to have distance 0
	this_node->weight = 0;
	
	//initialize direct neighbors weights
	lsr_nt_set_neighbor_weights();
	
	//queue of all unvisited nodes
	priority_queue_t *queue = NULL;
	for(node = node_set; node; node = node->hh.next) {
		priority_queue_add(&queue, node);
	}
	node_t *current_node;
	while((current_node = priority_queue_poll(&queue))) {
		for(int i = 0; i < current_node->neighbor_count; ++i) {
			edge_t *neighbor_edge = current_node->neighbors + i;
			node_t * neighbor_node = neighbor_edge->node;
			uint32_t dist = current_node->weight + neighbor_edge->weight;
			if(neighbor_node->weight > dist) {
				neighbor_node->weight = dist;
				neighbor_node->next_hop = current_node->next_hop;
			}
		}
	}
	char *out = lsr_tc_nodeset_to_route_string(";");
	dessert_info("%s", out);
	free(out);
	return DESSERT_OK;
}

char *lsr_tc_nodeset_to_string(const char *delim) {
	int size = 4096;
	char *buf = malloc(size);
	buf[0] = '\0';
	int used = 0;
	node_t *node, *tmp;
	HASH_ITER(hh, node_set, node, tmp) {
		char *line = lsr_node_to_string(node);
		if((int)strlen(line) + (int)strlen(delim) >= size - used) {
			buf = realloc(buf, size *= 2);
		}
		used += snprintf(buf + used, size - used, "%s%s", line, delim);
		free(line);
	}
	return buf;
}

char *lsr_tc_nodeset_to_route_string(const char *delim) {
	int size = 4096;
	char *buf = malloc(size);
	buf[0] = '\0';
	int used = 0;
	node_t *node, *tmp;
	HASH_ITER(hh, node_set, node, tmp) {
		char *line = lsr_node_to_route_string(node);
		if((int)strlen(line) + (int)strlen(delim) >= size -used) {
			buf = realloc(buf, size *= 2);
		}
		used += snprintf(buf + used, size - used, "%s%s", line, delim);
		free(line);
	}
	return buf;
}

