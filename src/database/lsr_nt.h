#ifndef LSR_NT
#define LSR_NT

#include "lsr_database.h"

#include <uthash.h>
#include <dessert.h>

struct edge;

/** level 2 neighbor with reference to l25 node */
typedef struct neighbor_iface {
	dessert_meshif_t *iface;
	mac_addr addr;
	struct edge *edge;
	uint32_t lifetime;
	uint32_t weight;
	UT_hash_handle hh;
	struct neighbor_iface *prev, *next; //circular list for all l2 interfaces belonging to one l25 neighbor
} neighbor_iface_t;

dessert_result_t lsr_nt_update(mac_addr neighbor_l2, mac_addr neighbor_l25, dessert_meshif_t *iface, uint8_t seq_nr, uint8_t weight);
dessert_result_t lsr_nt_age_all(void);

dessert_result_t lsr_nt_dump_neighbor_table(neighbor_info_t **result, int *neighbor_count);

#endif
