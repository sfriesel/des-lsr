#ifndef LSR_TC
#define LSR_TC
#include <uthash.h>
#include <dessert.h>
#include "lsr_node.h"
#include "lsr_database.h"

dessert_result_t lsr_tc_get_next_hop(mac_addr dest_addr, mac_addr *next_hop, dessert_meshif_t **iface);
void lsr_tc_update(mac_addr node_addr, uint16_t seq_nr, neighbor_info_t neighbor_infos[], int count, struct timeval now);
dessert_result_t lsr_tc_update_edge(node_t *node, mac_addr neighbor_addr, uint16_t weight, struct timeval now);
node_t *lsr_tc_get_node(mac_addr node_addr);
node_t *lsr_tc_get_or_create_node(mac_addr addr, struct timeval timeout);
bool lsr_tc_check_broadcast_seq_nr(mac_addr node_addr, uint16_t seq_nr, struct timeval now);
bool lsr_tc_check_unicast_seq_nr(mac_addr node_addr, uint16_t seq_nr, struct timeval now);
void lsr_tc_delete_node(node_t *node);
dessert_result_t lsr_tc_age_all(void);
dessert_result_t lsr_tc_dijkstra(void);
void lsr_tc_nodeset_print(FILE *f, const char *delim);
void lsr_tc_nodeset_print_routes(FILE *f, const char *delim);

#endif
