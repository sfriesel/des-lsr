#include "lsr_tc.h"
#include "lsr_nt.h"
#include "../lsr_config.h"
#include <utlist.h>

#define INFINITE_WEIGHT UINT32_MAX

//the set of all currently known nodes
static node_t *node_set = NULL;

edge_t *lsr_create_edge(node_t *target, uint32_t lifetime, uint32_t weight) {
	edge_t *edge = malloc(sizeof(edge_t));
	edge->node = target;
	edge->lifetime = lifetime;
	edge->weight = weight;
	return edge;
}

node_t *lsr_tc_create_node(mac_addr addr) {
	node_t *node = malloc(sizeof(node_t));
	memset(node, 0, sizeof(node_t));
	node->next_hop = NULL;
	memcpy(node->addr, addr, ETH_ALEN);
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

dessert_result_t lsr_tc_update_node(mac_addr node_addr, uint8_t seq_nr) {
	node_t *node = lsr_tc_get_or_create_node(node_addr);
	node->lifetime = NEIGHBOR_LIFETIME;
	node->weight = DEFAULT_WEIGHT;
	
	return DESSERT_OK;
}

int lsr_tc_node_cmp(node_t *left, node_t *right) {
	return memcmp(left->addr, right->addr, ETH_ALEN);
}

dessert_result_t lsr_tc_update_node_neighbor(mac_addr node_addr, mac_addr neighbor_addr, uint8_t lifetime, uint8_t weight) {
	node_t *node = lsr_tc_get_or_create_node(node_addr);
	
	edge_t *edge = NULL;
	CDL_FOREACH(node->neighbors, edge) {
		if(mac_equal(edge->node->addr, neighbor_addr)) {
			break;
		}
	}
	if(!edge) {
		node_t *neighbor = lsr_tc_get_or_create_node(neighbor_addr);
		edge = lsr_create_edge(neighbor, NEIGHBOR_LIFETIME, DEFAULT_WEIGHT);
		CDL_PREPEND(node->neighbors, edge);
	}
	else {
		edge->lifetime = NEIGHBOR_LIFETIME;
		edge->weight = DEFAULT_WEIGHT;
	}
	return DESSERT_OK;
}

bool lsr_tc_node_check_seq_nr(mac_addr node_addr, uint8_t seq_nr) {
	node_t *node = NULL;
	HASH_FIND(hh, node_set, node_addr, ETH_ALEN, node);
	if (!node) {
		return false;
	}
	int64_t base = node->seq_nr >> 8;
	
	for(int64_t guess = ((base - 1) << 8) + seq_nr; ; guess += UINT8_MAX + 1) {
		if(abs(guess - node->seq_nr) < UINT8_MAX/2) {
			if(guess <= node->seq_nr) {
				return false;
			} else {
				node->seq_nr = guess;
				return true;
			}
		}
	}
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

dessert_result_t lsr_tc_age(node_t *node) {
	if(node->lifetime)
		node->lifetime--;
	// if node has no next_hop, it's not referenced
	// as neighbor of any reachable node
	if(!node->lifetime && !node->next_hop) {
		if(node->neighbors) {
			edge_t *tmp1, *tmp2, *edge;
			CDL_FOREACH_SAFE(node->neighbors, edge, tmp1, tmp2) {
				CDL_DELETE(node->neighbors, edge);
				free(edge);
			}
		}
		HASH_DEL(node_set, node);
		free(node);
	}
	return DESSERT_OK;
}

dessert_result_t lsr_tc_age_all(void) {
	for(node_t *node = node_set; node; node = node->hh.next)
		lsr_tc_age(node);
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
	node_t *node = NULL;
	for(node = node_set; node; node = node->hh.next) {
		node->weight = INFINITE_WEIGHT;
		node->next_hop = NULL;
	}
	//initialize direct neighbors weights
	lsr_nt_set_neighbor_weights();
	
	//queue of all unvisited nodes
	priority_queue_t *queue = NULL;
	for(node = node_set; node; node = node->hh.next) {
		priority_queue_add(&queue, node);
	}
	node_t *current_node;
	while((current_node = priority_queue_poll(&queue))) {
		edge_t *neighbor_edge;
		CDL_FOREACH(current_node->neighbors, neighbor_edge) {
			node_t * neighbor_node = neighbor_edge->node;
			uint32_t dist = current_node->weight + neighbor_edge->weight;
			if(neighbor_node->weight > dist) {
				neighbor_node->weight = dist;
				neighbor_node->next_hop = current_node->next_hop;
			}
		}
	}
	return DESSERT_OK;
}

