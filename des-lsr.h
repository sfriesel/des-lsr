#include <dessert.h>
#include <libcli.h>

//////////////// CONSTANTS
#define LSR_INFINITY 				1000
#define LSR_EXT_HELLO 				DESSERT_EXT_USER
#define LSR_EXT_TC 					DESSERT_EXT_USER + 1

//////////////// DAEMON CONFIG
#define HELLO_INTERVAL				1500 	// milliseconds
#define TC_INTERVAL					4000 	// milliseconds
#define NH_REFRESH_INTERVAL			4000 	// milliseconds
#define RT_REFRESH_INTERVAL			5000 	// milliseconds
#define NH_ENTRY_AGE				32
#define RT_ENTRY_AGE				32
#define TTL_MAX 					3

//////////////// PERIODICS
extern u_int16_t					hello_interval;
extern u_int16_t 					tc_interval;
extern u_int16_t 					nh_refresh_interval;
extern u_int16_t 					rt_refresh_interval;
extern u_int16_t 					nh_entry_age;
extern u_int16_t 					rt_entry_age;
extern dessert_periodic_t *			periodic_send_hello;
extern dessert_periodic_t *			periodic_send_tc;
extern dessert_periodic_t *			periodic_refresh_nh;
extern dessert_periodic_t *			periodic_refresh_rt;

//////////////// FUNCTIONS FROM des-lsr_routingLogic.c
void init_logic();
dessert_per_result_t send_hello(void *data, struct timeval *scheduled, struct timeval *interval);
dessert_per_result_t send_tc(void *data, struct timeval *scheduled, struct timeval *interval);
dessert_per_result_t refresh_list();

void init_rt();
dessert_per_result_t refresh_rt();

//////////////// FUNCTIONS FROM des-lsr_packetHandler.c
dessert_cb_result sys_to_mesh(dessert_msg_t *msg, uint32_t len, dessert_msg_proc_t *proc, dessert_sysif_t *sysif, dessert_frameid_t id);

dessert_cb_result drop_errors(dessert_msg_t* msg, uint32_t len, dessert_msg_proc_t *proc, dessert_meshif_t *iface, dessert_frameid_t id);
dessert_cb_result process_hello(dessert_msg_t* msg, uint32_t len, dessert_msg_proc_t *proc, dessert_meshif_t *iface, dessert_frameid_t id);
dessert_cb_result process_tc(dessert_msg_t* msg, uint32_t len, dessert_msg_proc_t *proc, dessert_meshif_t *iface, dessert_frameid_t id);
dessert_cb_result forward_packet(dessert_msg_t* msg, uint32_t len, dessert_msg_proc_t *proc, dessert_meshif_t *iface, dessert_frameid_t id);
dessert_cb_result mesh_to_sys(dessert_msg_t* msg, uint32_t len, dessert_msg_proc_t *proc, dessert_meshif_t *iface, dessert_frameid_t id);

//////////////// FUNCTIONS FROM des-lsr_cli.c
int cli_set_hello_interval(struct cli_def* cli, char* command, char* argv[], int argc);
int cli_show_hello_interval(struct cli_def* cli, char* command, char* argv[], int argc);
int cli_set_tc_interval(struct cli_def* cli, char* command, char* argv[], int argc);
int cli_show_tc_interval(struct cli_def* cli, char* command, char* argv[], int argc);
int cli_set_refresh_list(struct cli_def* cli, char* command, char* argv[], int argc);
int cli_show_refresh_list(struct cli_def* cli, char* command, char* argv[], int argc);
int cli_set_refresh_rt(struct cli_def* cli, char* command, char* argv[], int argc);
int cli_show_refresh_rt(struct cli_def* cli, char* command, char* argv[], int argc);
