#include "../lsr_config.h"
#include "lsr_nt.h"
#include "lsr_tc.h"
//#include "lsr_node.h"
#include "../periodic/lsr_periodic.h"

#include <utlist.h>
#include <assert.h>

/** level 2 neighbor, it is always part of the neighbor table */
typedef struct neighbor {
	struct timeval timeout;
	dessert_meshif_t *iface;
	mac_addr addr; //key
	mac_addr node_addr;
	uint16_t weight;
} neighbor_t;

// neighbor table; list of all neighboring l2 interfaces
static neighbor_t *nt = NULL;
static int nt_capacity = 0;
static int nt_used = 0;

dessert_result_t lsr_nt_dump_neighbor_table(neighbor_info_t ** const result, int * const neighbor_count) {
	lsr_nt_age_all();
	int out_size = nt_used;
	neighbor_info_t *out = calloc(out_size, sizeof(neighbor_info_t));
	int out_used = 0;
	
	
	for(int i = 0; i < nt_used; ++i) {
		int j;
		for(j = 0; j < out_used; ++j) {
			if(mac_equal(nt[i].node_addr, out[j].addr)) {
				if(nt[i].weight >= out[j].weight)
					goto next_neighbor;
				break;
			}
		}
		assert(out_used <= out_size);
		if(j == out_used)
			out_used++;
		
		mac_copy(out[j].addr, nt[i].node_addr);
		out[j].weight = nt[i].weight;
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
	
	neighbor_t *neighbor = NULL;
	for(int i = 0; i < nt_used; ++i)
		if(iface == nt[i].iface && mac_equal(neighbor_l2, nt[i].addr))
			neighbor = nt + i;
	if(!neighbor) {
		if(!nt_capacity) {
			nt_capacity = 2;
			nt = calloc(nt_capacity, sizeof(neighbor_t));
		}
		if(nt_used == nt_capacity) {
			nt_capacity *= 2;
			nt = realloc(nt, nt_capacity * sizeof(neighbor_t));
		}
		assert(nt_used < nt_capacity);
		neighbor = nt + nt_used++;
		mac_copy(neighbor->addr, neighbor_l2);
		mac_copy(neighbor->node_addr, neighbor_l25);
		neighbor->iface = iface;
	}
	neighbor->timeout = timeout;
	neighbor->weight = weight;
	
	return DESSERT_OK;
}

dessert_result_t lsr_nt_age_all(void) {
	struct timeval now;
	gettimeofday(&now, NULL);
	for(int i = 0; i < nt_used; ++i)
		if(dessert_timevalcmp(&now, &nt[i].timeout) >= 0)
			nt[i] = nt[--nt_used]; //overwrite deleted ngbr with last one
	return DESSERT_OK;
}

uint8_t *lsr_nt_node_addr(mac_addr l2_addr, dessert_meshif_t *iface) {
	for(int i = 0; i < nt_used; ++i)
		if(iface == nt[i].iface && mac_equal(l2_addr, nt[i].addr))
			return nt[i].node_addr;
	return NULL;
}

/**
 * pre-condition: all nodes that are referenced by neighbor interfaces have infinite weight
 */
dessert_result_t lsr_nt_set_neighbor_weights(void) {
	for(int i = 0; i < nt_used; ++i) {
		lsr_tc_set_next_hop(nt[i].node_addr, nt[i].addr, nt[i].iface, nt[i].weight);
	}
	return DESSERT_OK;
}

