#include "../lsr_config.h"
#include "lsr_nt.h"
#include "lsr_tc.h"
#include "../periodic/lsr_periodic.h"

#include <utlist.h>

// neighbor table; hash map of all neighboring l2 interfaces
static neighbor_t *nt = NULL;

dessert_result_t lsr_nt_dump_neighbor_table(neighbor_info_t ** const result, int * const neighbor_count) {
	lsr_nt_age_all();
	int out_size = HASH_COUNT(nt);
	neighbor_info_t *out = calloc(out_size, sizeof(neighbor_info_t));
	int out_used = 0;
	
	for(neighbor_t *neighbor = nt; neighbor; neighbor = neighbor->hh.next) {
		int j;
		for(j = 0; j < out_used; ++j) {
			if(mac_equal(neighbor->node->addr, out[j].addr)) {
				if(neighbor->weight >= out[j].weight)
					goto next_neighbor;
				break;
			}
		}
		
		if(j == out_used)
			out_used++;
		
		mac_copy(out[j].addr, neighbor->node->addr);
		out[j].weight = neighbor->weight;
		next_neighbor:;
	}
	*neighbor_count = out_used;
	*result = out;
	return DESSERT_OK;
}

struct timeval lsr_nt_calc_timeout(struct timeval now) {
	uint32_t lifetime_ms = neighbor_lifetime * hello_interval;
	struct timeval timeout;
	dessert_ms2timeval(lifetime_ms, &timeout);
	dessert_timevaladd2(&timeout, &timeout, &now);
	return timeout;
}

dessert_result_t lsr_nt_update(mac_addr neighbor_l2, mac_addr neighbor_l25, dessert_meshif_t *iface, uint16_t seq_nr, uint16_t weight, struct timeval now) {
	struct timeval timeout = lsr_nt_calc_timeout(now);
	node_t *node = lsr_tc_get_or_create_node(neighbor_l25, timeout);
	
	neighbor_t *neighbor = NULL;
	HASH_FIND(hh, nt, neighbor_l2, ETH_ALEN, neighbor);
	if(!neighbor) {
		neighbor = malloc(sizeof(neighbor_t));
		memcpy(neighbor->addr, neighbor_l2, ETH_ALEN);
		neighbor->node = node;
		neighbor->iface = iface;
		HASH_ADD_KEYPTR(hh, nt, neighbor->addr, ETH_ALEN, neighbor);
	}
	neighbor->timeout = timeout;
	neighbor->weight = weight;
	
	return DESSERT_OK;
}

dessert_result_t lsr_nt_age(neighbor_t *neighbor, const struct timeval *now) {
	if(dessert_timevalcmp(now, &neighbor->timeout) >= 0) {
		HASH_DEL(nt, neighbor);
		free(neighbor);
	}
	return DESSERT_OK;
}

dessert_result_t lsr_nt_age_all(void) {
	struct timeval now;
	gettimeofday(&now, NULL);
	for(neighbor_t *neighbor = nt; neighbor; neighbor = neighbor->hh.next) {
		lsr_nt_age(neighbor, &now);
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

