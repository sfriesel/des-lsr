#include <dessert.h>

extern dessert_periodic_t *periodic_send_hello_tc;
extern dessert_periodic_t *periodic_rebuild_rt;

dessert_per_result_t lsr_periodic_send_hello_tc(void *data, struct timeval *scheduled, struct timeval *interval);
dessert_per_result_t lsr_periodic_rebuild_rt(void *data, struct timeval *scheduled, struct timeval *interval);
