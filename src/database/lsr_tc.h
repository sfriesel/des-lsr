#ifndef LSR_TC
#define LSR_TC
#include <uthash.h>
#include <dessert.h>
struct edge; //forward declare
struct neighbor_iface;

typedef struct node {
	mac_addr addr; //l2.5
	struct neighbor_iface *next_hop;
	uint64_t seq_nr;
	struct edge *neighbors;
	UT_hash_handle hh;
} node_t;

dessert_result_t lsr_tc_get_next_hop(mac_addr dest_addr, mac_addr *next_hop, dessert_meshif_t **iface);
dessert_result_t lsr_tc_update_node(mac_addr node_addr, uint8_t seq_nr);
dessert_result_t lsr_tc_update_node_neighbor(mac_addr node_addr, mac_addr neighbor_addr, uint8_t lifetime, uint8_t weight);
node_t *lsr_tc_get_node(mac_addr node_addr);
node_t *lsr_tc_get_or_create_node(mac_addr addr);
bool lsr_tc_node_check_seq_nr(mac_addr node_addr, uint8_t seq_nr);
dessert_result_t lsr_tc_age_all(void);

#endif
