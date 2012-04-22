#include "lsr_tc.h"
#include "lsr_nt.h"
#include "lsr_node.h"
#include "../lsr_config.h"
#include <utlist.h>
#include <stdio.h>
#include <assert.h>

//the set of all currently known nodes
static node_t *node_set = NULL;
static node_t *this_node = NULL;

static node_t *lsr_tc_create_node(mac_addr addr, struct timeval timeout) {
	node_t *node = lsr_node_new(addr, timeout);
	HASH_ADD_KEYPTR(hh, node_set, node->addr, ETH_ALEN, node);
	return node;
}

static node_t *lsr_tc_get_node(mac_addr addr) {
	node_t *node = NULL;
	HASH_FIND(hh, node_set, addr, ETH_ALEN, node);
	return node;
}

static node_t *lsr_tc_get_or_create_node(mac_addr addr, struct timeval timeout) {
	node_t *node = lsr_tc_get_node(addr);
	if(!node)
		node = lsr_tc_create_node(addr, timeout);
	return node;
}

static struct timeval lsr_tc_calc_timeout(struct timeval now, uint8_t lifetime) {
	struct timeval timeout;
	dessert_ms2timeval(lifetime * tc_interval, &timeout);
	dessert_timevaladd2(&timeout, &timeout, &now);
	return timeout;
}

static dessert_result_t lsr_tc_update_edge(node_t *node, mac_addr neighbor_addr, uint16_t weight, struct timeval now) {
	struct timeval ngbr_timeout = lsr_tc_calc_timeout(now, node_lifetime);
	node_t *neighbor = lsr_tc_get_or_create_node(neighbor_addr, ngbr_timeout);
	lsr_node_update_edge(node, neighbor, weight, now);
	return DESSERT_OK;
}

void lsr_tc_update(mac_addr node_addr, uint16_t seq_nr, neighbor_info_t neighbor_infos[], int count, struct timeval now) {
	struct timeval timeout = lsr_tc_calc_timeout(now, node_lifetime);
	node_t *node = lsr_tc_get_or_create_node(node_addr, timeout);
	lsr_node_set_timeout(node, timeout);
	for(int i = 0; i < count; ++i) {
		lsr_tc_update_edge(node, neighbor_infos[i].addr, neighbor_infos[i].weight, now);
	}
	lsr_node_remove_old_edges(node, now);
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
	if(!dest || !dest->next_hop_iface) {
		return DESSERT_ERR;
	}
	
	mac_copy(*next_hop, dest->next_hop_addr);
	*iface = dest->next_hop_iface;
	return DESSERT_OK;
}

void lsr_tc_set_next_hop(mac_addr l25, mac_addr l2, dessert_meshif_t *iface, uint32_t weight) {
	node_t *ngbr = lsr_tc_get_node(l25);
	if(!ngbr) {
		dessert_warn("ignoring next hop " MAC " because node " MAC " is not in node_set", EXPLODE_ARRAY6(l2), EXPLODE_ARRAY6(l25));
		return;
	}
	lsr_node_set_nexthop(ngbr, l2, iface, weight);
}

dessert_result_t lsr_tc_age_all(void) {
	struct timeval now;
	gettimeofday(&now, NULL);
	node_t *node, *tmp;
	HASH_ITER(hh, node_set, node, tmp) {
		bool dead = lsr_node_is_dead(node, &now);
		if(dead) {
			HASH_DEL(node_set, node);
			//TODO this is inefficient; use better method to ensure no ngbr references exist
			for(node_t *possible_ngbr = node_set; possible_ngbr; possible_ngbr = possible_ngbr->hh.next)
				for(int j = 0; j < possible_ngbr->neighbor_count; ++j)
					if(node == possible_ngbr->neighbors[j].node)
						possible_ngbr->neighbors[j] = possible_ngbr->neighbors[--possible_ngbr->neighbor_count];
			lsr_node_delete(node);
		}
	}
	return DESSERT_OK;
}

dessert_result_t lsr_tc_dijkstra(void) {
	if(!this_node) //initialize node_set with this node
		this_node = lsr_tc_create_node(dessert_l25_defsrc, (struct timeval){INT32_MAX, 999999});
	
	for(node_t *node = node_set; node; node = node->hh.next) {
		node->weight = INFINITE_WEIGHT;
		node->next_hop_iface = NULL;
	}
	this_node->weight = 0;
	
	//initialize direct neighbors weights
	lsr_nt_set_neighbor_weights();
	
	int node_count = HASH_COUNT(node_set);
	assert(node_count);
	node_t *node_list[node_count];
	int i = 0;
	for(node_t *node = node_set; node; node = node->hh.next)
		node_list[i++] = node;
	assert(i == node_count);
	
	for(i = 0; i < node_count; ++i) {
		for(int j = i + 1; j < node_count; ++j) {
			if(node_list[j]->weight < node_list[i]->weight) {
				node_t *tmp = node_list[i];
				node_list[i] = node_list[j];
				node_list[j] = tmp;
			}
		}
		node_t *current_node = node_list[i]; //unvisited node with smallest weight
		if(current_node->weight == INFINITE_WEIGHT)
			break; //this and all other unvisited nodes are unreachable
		dessert_trace("processing node with %d ngbrs and weight %d", current_node->neighbor_count, current_node->weight);
		for(int j = 0; j < current_node->neighbor_count; ++j) {
			edge_t *neighbor_edge = current_node->neighbors + j;
			node_t * neighbor_node = neighbor_edge->node;
			uint32_t dist = current_node->weight + neighbor_edge->weight;
			if(neighbor_node->weight > dist) {
				neighbor_node->weight = dist;
				mac_copy(neighbor_node->next_hop_addr, current_node->next_hop_addr);
				neighbor_node->next_hop_iface = current_node->next_hop_iface;
			}
		}
	}
	{
		char *buf;
		size_t bufSize;
		FILE *f = open_memstream(&buf, &bufSize);
		lsr_tc_nodeset_print_routes(f, ";");
		fclose(f);
		dessert_info("%s", buf);
		free(buf);
	}
	return DESSERT_OK;
}

void lsr_tc_nodeset_print(FILE *f, const char *delim) {
	for(node_t *node = node_set; node; node = node->hh.next) {
		lsr_node_print(node, f);
		fputs(delim, f);
	}
}

void lsr_tc_nodeset_print_routes(FILE *f, const char *delim) {
	for(node_t *node = node_set; node; node = node->hh.next) {
		lsr_node_print_route(node, f);
		fputs(delim, f);
	}
}

