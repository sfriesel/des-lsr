#ifndef LSR_TC
#define LSR_TC
#include <uthash.h>
#include <dessert.h>
#include "lsr_database.h"
struct edge; //forward declare
struct neighbor;

#define GAP_COUNT 10

typedef struct node {
	mac_addr addr; //l2.5
	struct neighbor *next_hop;
	uint64_t broadcast_seq_nr;
	uint64_t unicast_seq_nr;
	struct seq_interval broadcast_gaps[GAP_COUNT];
	struct seq_interval unicast_gaps[GAP_COUNT];
	struct edge *neighbors;
	uint32_t lifetime;
	uint32_t weight;
	UT_hash_handle hh;
} node_t;

typedef struct edge {
	node_t *node;
	uint32_t lifetime;
	uint32_t weight;
	struct edge *prev, *next;
} edge_t;

dessert_result_t lsr_tc_get_next_hop(mac_addr dest_addr, mac_addr *next_hop, dessert_meshif_t **iface);
dessert_result_t lsr_tc_update_node(mac_addr node_addr, uint16_t seq_nr);
dessert_result_t lsr_tc_update_node_neighbor(mac_addr node_addr, mac_addr neighbor_addr, uint8_t lifetime, uint8_t weight);
node_t *lsr_tc_get_node(mac_addr node_addr);
node_t *lsr_tc_get_or_create_node(mac_addr addr);
bool lsr_tc_node_check_broadcast_seq_nr(mac_addr node_addr, uint16_t seq_nr);
bool lsr_tc_node_check_unicast_seq_nr(mac_addr node_addr, uint16_t seq_nr);
dessert_result_t lsr_tc_age_all(void);
dessert_result_t lsr_tc_dijkstra(void);
edge_t *lsr_create_edge(node_t *target, uint32_t lifetime, uint32_t weight);

#endif
