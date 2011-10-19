#include "../lsr_config.h"
#include "lsr_nt.h"
#include "lsr_tc.h"

#include <utlist.h>

// neighbor table; hash map of all neighboring l2 interfaces
static neighbor_t *nt = NULL;

dessert_result_t lsr_nt_dump_neighbor_table(neighbor_info_t ** const result, int * const neighbor_count) {
	// circular list of edges pointing to l25 neighbor nodes
	edge_t *neighbors = NULL;
	
	*neighbor_count = 0;
	neighbor_t *neighbor, *tmp;
	HASH_ITER(hh, nt, neighbor, tmp) {
		edge_t *neighbor_edge = NULL;
		CDL_FOREACH(neighbors, neighbor_edge) {
			if(neighbor->node == neighbor_edge->node) {
				if(neighbor->weight < neighbor_edge->weight) {
					neighbor_edge->weight = neighbor->weight;
					neighbor_edge->lifetime = neighbor->lifetime;
				}
				break;
			}
		}
		if(!neighbor_edge) {
			neighbor_edge = malloc(sizeof(edge_t));
			neighbor_edge->weight = neighbor->weight;
			neighbor_edge->lifetime = neighbor->lifetime;
			neighbor_edge->node = neighbor->node;
			CDL_PREPEND(neighbors, neighbor_edge);
			++(*neighbor_count);
		}
	}
	
	edge_t *neighbor_edge, *tmp2, *tmp3;
	neighbor_info_t *iter = *result = malloc(*neighbor_count * sizeof(neighbor_info_t));
	
	CDL_FOREACH_SAFE(neighbors, neighbor_edge, tmp2, tmp3) {
		memcpy(iter->addr, neighbor_edge->node->addr, ETH_ALEN);
		iter->lifetime = neighbor_edge->lifetime;
		iter->weight = neighbor_edge->weight;
		iter++;
		CDL_DELETE(neighbors, neighbor_edge);
		free(neighbor_edge);
	}
	return DESSERT_OK;
}

dessert_result_t lsr_nt_update(mac_addr neighbor_l2, mac_addr neighbor_l25, dessert_meshif_t *iface, uint16_t seq_nr, uint8_t weight) {
	node_t *node = lsr_tc_get_or_create_node(neighbor_l25);
	
	neighbor_t *neighbor = NULL;
	HASH_FIND(hh, nt, neighbor_l2, ETH_ALEN, neighbor);
	if(!neighbor) {
		neighbor = malloc(sizeof(neighbor_t));
		memcpy(neighbor->addr, neighbor_l2, ETH_ALEN);
		neighbor->node = node;
		neighbor->iface = iface;
		HASH_ADD_KEYPTR(hh, nt, neighbor->addr, ETH_ALEN, neighbor);
	}
	neighbor->lifetime = NEIGHBOR_LIFETIME;
	neighbor->weight = weight;
	
	return DESSERT_OK;
}

dessert_result_t lsr_nt_age(neighbor_t *neighbor) {
	neighbor->lifetime--;
	if(!neighbor->lifetime) {
		HASH_DEL(nt, neighbor);
		CDL_DELETE(neighbor, neighbor);
		free(neighbor);
	}
	return DESSERT_OK;
}

dessert_result_t lsr_nt_age_all(void) {
	for(neighbor_t *neighbor = nt; neighbor; neighbor = neighbor->hh.next) {
		lsr_nt_age(neighbor);
	}
	return DESSERT_OK;
}

/**
 * pre-condition: all nodes that are referenced by neighbor interfaces have infinite weight
 */
dessert_result_t lsr_nt_set_neighbor_weights(void) {
	for(neighbor_t *neighbor = nt; neighbor; neighbor = neighbor->hh.next) {
		if(neighbor->weight < neighbor->node->weight) {
			neighbor->node->next_hop = neighbor;
			neighbor->node->weight = neighbor->weight;
		}
	}
	return DESSERT_OK;
}

