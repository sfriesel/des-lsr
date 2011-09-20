#include "../lsr_config.h"
#include "lsr_nt.h"
#include "lsr_tc.h"

#include <utlist.h>

// neighbor table; hash map of all neighboring l2 interfaces
neighbor_iface_t *nt = NULL;

// circular list of edge point to l25 neighbor nodes
edge_t *neighbors = NULL;

dessert_result_t lsr_nt_dump_neighbor_table(neighbor_info_t **result, int *neighbor_count) {
	*neighbor_count = 0;
	edge_t *neighbor;
	CDL_FOREACH(neighbors, neighbor) {
		(*neighbor_count)++;
	}
	
	neighbor_info_t *iter = *result = malloc(*neighbor_count * sizeof(neighbor_info_t));
	
	CDL_FOREACH(neighbors, neighbor) {
		memcpy(iter->addr, neighbor->node->addr, ETH_ALEN);
		iter->lifetime = neighbor->lifetime;
		iter->weight = neighbor->weight;
		iter++;
	}
	
	return DESSERT_OK;
}

dessert_result_t lsr_nt_update(mac_addr neighbor_l2, mac_addr neighbor_l25, dessert_meshif_t *iface, uint8_t seq_nr, uint8_t weight) {
	node_t *node = lsr_tc_get_or_create_node(neighbor_l25);
	neighbor_iface_t *ngbr_ifaces = node->next_hop; //circular list for all l2 interfaces belonging to neighbor_l25
	
	neighbor_iface_t *neighbor = NULL;
	HASH_FIND(hh, nt, neighbor_l2, ETH_ALEN, neighbor);
	if(!neighbor) {
		neighbor = malloc(sizeof(neighbor_iface_t));
		memcpy(neighbor->addr, neighbor_l2, ETH_ALEN);
		neighbor->edge->node = node;
		neighbor->iface = iface;
		CDL_PREPEND(ngbr_ifaces, neighbor);
		HASH_ADD_KEYPTR(hh, nt, neighbor->addr, ETH_ALEN, neighbor);
	}
	neighbor->lifetime = NEIGHBOR_LIFETIME;
	neighbor->weight = weight;
	
	return DESSERT_OK;
}

dessert_result_t lsr_nt_age(neighbor_iface_t *neighbor) {
	neighbor->lifetime--;
	if(!neighbor->lifetime) {
		HASH_DEL(nt, neighbor);
		CDL_DELETE(neighbor, neighbor);
		free(neighbor);
	}
	return DESSERT_OK;
}

dessert_result_t lsr_nt_age_all(void) {
	neighbor_iface_t *neighbor = NULL;
	for(neighbor = nt; neighbor != NULL; neighbor = neighbor->hh.next) {
		lsr_nt_age(neighbor);
	}
	return DESSERT_OK;
}

