#ifndef LSR_NT
#define LSR_NT

#include "lsr_database.h"

#include <uthash.h>
#include <dessert.h>

dessert_result_t lsr_nt_update(mac_addr neighbor_l2, mac_addr neighbor_l25, dessert_meshif_t *iface, uint16_t seq_nr, uint16_t weight, struct timeval now);
dessert_result_t lsr_nt_age_all(void);
uint8_t *lsr_nt_node_addr(mac_addr l2_addr, dessert_meshif_t *iface);

dessert_result_t lsr_nt_dump_neighbor_table(neighbor_info_t **result, int *neighbor_count);

dessert_result_t lsr_nt_set_neighbor_weights(void);

#endif
