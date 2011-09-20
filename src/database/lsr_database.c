#include "lsr_database.h"
#include "lsr_nt.h"
#include "lsr_tc.h"
#include "../lsr_config.h"
#include <pthread.h>

pthread_rwlock_t db_lock = PTHREAD_RWLOCK_INITIALIZER;

uint64_t tc_seq_nr = 0;
uint64_t data_seq_nr = 0;

dessert_result_t lsr_db_dump_neighbor_table(neighbor_info_t **result_out, int *neighbor_count) {
	pthread_rwlock_rdlock(&db_lock);
	dessert_result_t result = lsr_nt_dump_neighbor_table(result_out, neighbor_count);
	pthread_rwlock_unlock(&db_lock);
	return result;
}

dessert_result_t lsr_db_nt_update(mac_addr neighbor_l2, mac_addr neighbor_l25, dessert_meshif_t *iface, uint8_t seq_nr) {
	pthread_rwlock_wrlock(&db_lock);
	dessert_result_t result = lsr_nt_update(neighbor_l2, neighbor_l25, iface, seq_nr, DEFAULT_WEIGHT);
	pthread_rwlock_unlock(&db_lock);
	return result;
}

dessert_result_t lsr_db_tc_update(mac_addr node_addr, uint8_t seq_nr) {
	pthread_rwlock_wrlock(&db_lock);
	dessert_result_t result = lsr_tc_update_node(node_addr, seq_nr);
	pthread_rwlock_unlock(&db_lock);
	return result;
}

dessert_result_t lsr_db_tc_neighbor_update(mac_addr node_addr, mac_addr neighbor_addr, uint8_t age, uint8_t weight) {
	pthread_rwlock_wrlock(&db_lock);
	dessert_result_t result = lsr_tc_update_node_neighbor(node_addr, neighbor_addr, age, weight);
	pthread_rwlock_unlock(&db_lock);
	return result;
}

bool lsr_db_tc_check_seq_nr(mac_addr node_addr, uint8_t seq_nr) {
	pthread_rwlock_rdlock(&db_lock);
	bool result = lsr_tc_node_check_seq_nr(node_addr, seq_nr);
	pthread_rwlock_unlock(&db_lock);
	return result;
}

uint64_t lsr_db_data_get_seq_nr(void) {
	pthread_rwlock_wrlock(&db_lock);
	uint64_t result = data_seq_nr++;
	pthread_rwlock_unlock(&db_lock);
	return result;
}

uint64_t lsr_db_tc_get_seq_nr(void) {
	pthread_rwlock_wrlock(&db_lock);
	uint64_t result = tc_seq_nr++;
	pthread_rwlock_unlock(&db_lock);
	return result;
}

bool lsr_db_data_check_seq_nr(mac_addr node_addr, uint16_t seq_nr) {
	pthread_rwlock_rdlock(&db_lock);
	bool result = true;
	pthread_rwlock_unlock(&db_lock);
	return result;
}

dessert_result_t lsr_db_get_next_hop(mac_addr dest_addr, mac_addr *next_hop, dessert_meshif_t **iface) {
	pthread_rwlock_rdlock(&db_lock);
	dessert_result_t result = lsr_tc_get_next_hop(dest_addr, next_hop, iface);
	pthread_rwlock_unlock(&db_lock);
	return result;
}

dessert_result_t lsr_db_nt_age_all(void) {
	pthread_rwlock_rdlock(&db_lock);
	dessert_result_t result = lsr_nt_age_all();
	pthread_rwlock_unlock(&db_lock);
	return result;
}
dessert_result_t lsr_db_tc_age_all(void) {
	pthread_rwlock_rdlock(&db_lock);
	dessert_result_t result = lsr_tc_age_all();
	pthread_rwlock_unlock(&db_lock);
	return result;
}