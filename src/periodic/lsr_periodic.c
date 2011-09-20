#include "lsr_periodic.h"
#include "../lsr_config.h"
#include "../database/lsr_database.h"
#include "../pipeline/lsr_pipeline.h"

#include <uthash.h>

typedef struct tc_ext {
	mac_addr addr;
	uint8_t lifetime;
	uint8_t weight;
} __attribute__((__packed__)) tc_ext_t;

dessert_per_result_t send_tc(void *data, struct timeval *scheduled, struct timeval *interval) {
	dessert_msg_t *tc;
	dessert_msg_new(&tc);
	tc->ttl = TTL_MAX;
	tc->u8 = lsr_db_tc_get_seq_nr();
	
	dessert_ext_t *ext;
	
	// add l2.5 header
	dessert_msg_addext(tc, &ext, DESSERT_EXT_ETH, ETHER_HDR_LEN);
	struct ether_header* l25h = (struct ether_header *) ext->data;
	memcpy(l25h->ether_dhost, ether_broadcast, ETH_ALEN);
	
	neighbor_info_t *neighbor_list;
	int neighbor_count;
	lsr_db_dump_neighbor_table(&neighbor_list, &neighbor_count);
	uint32_t ext_size = sizeof(tc_ext_t) * neighbor_count;
	if(dessert_msg_addext(tc, &ext, LSR_EXT_TC, ext_size) != DESSERT_OK) {
		dessert_notice("TC extension too big! This is an implementation bug");
	}

	// copy NH list into extension
	tc_ext_t *tc_element = (tc_ext_t*) ext->data;
	for(int i = 0; i < neighbor_count; ++i) {
		memcpy(tc_element[i].addr, neighbor_list[i].addr, ETH_ALEN);
		tc_element[i].lifetime = neighbor_list[i].lifetime;
		tc_element[i].weight = neighbor_list[i].weight;
	}

	lsr_send_randomized(tc);
	dessert_msg_destroy(tc);
	return 0;
}

dessert_per_result_t age_neighbors(void *data, struct timeval *scheduled, struct timeval *interval) {
	lsr_db_nt_age_all();
	return DESSERT_PER_KEEP;
}

dessert_per_result_t age_nodes(void *data, struct timeval *scheduled, struct timeval *interval) {
	lsr_db_tc_age_all();
	return DESSERT_PER_KEEP;
}
