#include "lsr_tc.h"
#include "lsr_nt.h"
#include "../lsr_config.h"
#include <utlist.h>

#define INFINITY UINT8_MAX

//edges pointing to all reachable nodes, weight an d lifetime refer to the overall route
edge_t *topology = NULL;
//the set of all currently known nodes
node_t *node_set = NULL;

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
	edge_t *edge = NULL;
	CDL_FOREACH(topology, edge) {
		if(mac_equal(edge->node->addr, node_addr)) {
			break;
		}
	}
	if(!edge) {
		edge = lsr_create_edge(node, NODE_LIFETIME, DEFAULT_WEIGHT);
		CDL_PREPEND(topology, edge);
	}
	else {
		edge->lifetime = NEIGHBOR_LIFETIME;
		edge->weight = DEFAULT_WEIGHT;
	}
	return DESSERT_OK;
}

int lsr_tc_node_cmp(node_t *left, node_t *right) {
	return memcmp(left->addr, right->addr, ETH_ALEN);
}

#if 0
void lsr_tc_node_ref(node_t *node) {
	node->refcount++;
}

void lsr_tc_node_unref(node_t *node) {
	assert(node->refcount != 0);
	node->refcount--;
	if(!node->ref_count) {
		edge_t *tmp1, *tmp2;
		CDL_FOREACH_SAFE(node->neighbors, node->neighbors, tmp1, tmp2) {
			CDL_DELETE(node->neighbors, node->neighbors);
			lsr_tc_node_unref(node->neighbors->node);
			free(node->neighbors);
		}
		
		free(node);
	}
}
#endif

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
	//FIXME!!
	int result = seq_nr;
	result -= node->seq_nr;
	result = result & UINT8_MAX;
	return (result < UINT8_MAX / 2);
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

dessert_result_t lsr_tc_age(edge_t *edge) {
	edge->lifetime--;
	if(!edge->lifetime) {
		CDL_DELETE(edge, edge);
	}
	return DESSERT_OK;
}

dessert_result_t lsr_tc_age_all(void) {
	edge_t *route_edge = NULL;
	CDL_FOREACH(topology, route_edge) {
		edge_t *node_neighbor_edge;
		CDL_FOREACH(route_edge->node->neighbors, node_neighbor_edge) {
			lsr_tc_age(node_neighbor_edge);
		}
		lsr_tc_age(route_edge);
	}
	return DESSERT_OK;
}

#if 0
dessert_result_t lsr_tc_dijkstra() {
	node_t *node = NULL;
	HASH_ITER(topology, node) {
		node->weight = INFINITY;
	}
	
	
}
#endif

