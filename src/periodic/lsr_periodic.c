#include "lsr_periodic.h"
#include "../database/lsr_database.h"

dessert_per_result_t send_tc(void *data, struct timeval *scheduled, struct timeval *interval) {
	dessert_msg_t *tc;
	dessert_msg_new(&tc);
	tc->ttl = TTL_MAX;
	tc->u8 = lsr_db_tc_get_seq_nr();

	dessert_ext_t *ext;
	//FIXME may overflow for >31 neighbors
	uint32_t ext_size = sizeof(tc_ext_t) * HASH_COUNT(node_neighbors_head);
	if(dessert_msg_addext(tc, &ext, LSR_EXT_TC, ext_size) != DESSERT_OK) {
		dessert_notice("TC extension too big! This is an implementation bug");
	}

	// copy NH list into extension
	node_neighbors_t *neighbor;
	tc_ext_t *iter = (tc_ext_t*) ext->data;
	for(neighbor = node_neighbors_head; neighbor != NULL; neighbor = neighbor->hh.next) {
		memcpy(iter->addr, neighbor->addr, ETH_ALEN);
		iter->entry_age = neighbor->entry_age;
		iter->weight = neighbor->weight;
	}

	// add l2.5 header
	dessert_msg_addext(tc, &ext, DESSERT_EXT_ETH, ETHER_HDR_LEN);
	struct ether_header* l25h = (struct ether_header*) ext->data;
	memcpy(l25h->ether_shost, dessert_l25_defsrc, ETH_ALEN);
	memcpy(l25h->ether_dhost, ether_broadcast, ETH_ALEN);

	dessert_meshsend_fast(tc, NULL);
	dessert_msg_destroy(tc);
	pthread_rwlock_unlock(&pp_rwlock);
	return 0;
}
#endif
dessert_per_result_t age_neighbors(void *data, struct timeval *scheduled, struct timeval *interval) {
	lsr_db_nt_age_all();
	return DESSERT_PER_KEEP;
}

dessert_per_result_t age_nodes(void *data, struct timeval *scheduled, struct timeval *interval) {
	lsr_db_tc_age_all();
	return DESSERT_PER_KEEP;
}
