#include "lsr_tc.h"
#include "lsr_nt.h"
#include "../lsr_config.h"
#include <utlist.h>

//the set of all currently known nodes
static node_t *node_set = NULL;
static node_t *this_node = NULL;

node_t *lsr_tc_create_node(mac_addr addr, struct timeval timeout) {
	node_t *node = lsr_node_new(addr, timeout);
	HASH_ADD_KEYPTR(hh, node_set, node->addr, ETH_ALEN, node);
	return node;
}

node_t *lsr_tc_get_node(mac_addr addr) {
	node_t *node = NULL;
	HASH_FIND(hh, node_set, addr, ETH_ALEN, node);
	return node;
}

node_t *lsr_tc_get_or_create_node(mac_addr addr, struct timeval timeout) {
	node_t *node = lsr_tc_get_node(addr);
	if(!node) {
		node = lsr_tc_create_node(addr, timeout);
	}
	return node;
}

struct timeval lsr_tc_calc_timeout(struct timeval now, uint8_t lifetime) {
	struct timeval timeout;
	dessert_ms2timeval(lifetime * tc_interval, &timeout);
	dessert_timevaladd2(&timeout, &timeout, &now);
	return timeout;
}

node_t *lsr_tc_update_node(mac_addr node_addr, uint16_t seq_nr, struct timeval now) {
	struct timeval timeout = lsr_tc_calc_timeout(now, node_lifetime);
	node_t *node = lsr_tc_get_or_create_node(node_addr, timeout);
	lsr_node_set_timeout(node, timeout);
	return node;
}

dessert_result_t lsr_tc_update_edge(node_t *node, mac_addr neighbor_addr, uint16_t weight, struct timeval now) {
	struct timeval ngbr_timeout = lsr_tc_calc_timeout(now, node_lifetime);
	node_t *neighbor = lsr_tc_get_or_create_node(neighbor_addr, ngbr_timeout);
	lsr_node_update_edge(node, neighbor, weight, now);
	return DESSERT_OK;
}

bool lsr_tc_check_unicast_seq_nr(mac_addr node_addr, uint16_t seq_nr, struct timeval now) {
	node_t *node = lsr_tc_get_or_create_node(node_addr, lsr_tc_calc_timeout(now, node_lifetime));
	
	return lsr_node_check_unicast_seq_nr(node, seq_nr);
}

bool lsr_tc_check_broadcast_seq_nr(mac_addr node_addr, uint16_t seq_nr, struct timeval now) {
	node_t *node = lsr_tc_get_or_create_node(node_addr, lsr_tc_calc_timeout(now, node_lifetime));
	
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

dessert_result_t lsr_tc_dijkstra() {
	if(!this_node) //initialize node_set with this node
		this_node = lsr_tc_create_node(dessert_l25_defsrc, (struct timeval){UINT32_MAX, 999999});
	
	for(node_t *node = node_set; node; node = node->hh.next) {
		node->weight = INFINITE_WEIGHT;
		node->next_hop = NULL;
	}
	this_node->weight = 0;
	
	//initialize direct neighbors weights
	lsr_nt_set_neighbor_weights();
	
	int node_count = HASH_COUNT(node_set);
	node_t *node_list[node_count];
	int i = 0;
	for(node_t *node = node_set; node; node = node->hh.next)
		node_list[i++] = node;
	
	for(i = 0; i < node_count; ++i) {
		for(int j = i + 1; j < node_count; ++j) {
			if(node_list[j]->weight < node_list[i]->weight) {
				node_t *tmp = node_list[i];
				node_list[i] = node_list[j];
				node_list[j] = tmp;
			}
		}
		node_t *current_node = node_list[i]; //unvisited node with smallest weight
		for(int j = 0; j < current_node->neighbor_count; ++j) {
			edge_t *neighbor_edge = current_node->neighbors + j;
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

