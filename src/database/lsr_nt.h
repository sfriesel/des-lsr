#ifndef LSR_NT
#define LSR_NT

#include <uthash.h>
#include <dessert.h>

struct node; //forward declare struct node

typedef struct edge {
	struct node *node;
	uint32_t lifetime;
	uint32_t weight;
	struct edge *prev, *next;
} edge_t;

/** level 2 neighbor with reference to l25 node */
typedef struct neighbor_iface {
	dessert_meshif_t *iface;
	mac_addr addr;
	struct node *node;
	uint32_t lifetime;
	uint32_t weight;
	UT_hash_handle hh;
	struct neighbor_iface *prev, *next; //circular list for all l2 interfaces belonging to one l25 neighbor
} neighbor_iface_t;

dessert_result_t lsr_nt_update(mac_addr neighbor_l2, mac_addr neighbor_l25, dessert_meshif_t *iface, uint8_t seq_nr, uint8_t weight);
dessert_result_t lsr_nt_age_all(void);

#endif
