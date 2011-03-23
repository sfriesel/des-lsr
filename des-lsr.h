#include <dessert.h>
#include <dessert-extra.h>
#include <libcli.h>
#include <linux/if_ether.h>

//////////////// CONSTANTS
#define LSR_EXT_HELLO DESSERT_EXT_USER
#define LSR_EXT_TC DESSERT_EXT_USER + 1
#define LSR_TTL_DEFAULT 255

//////////////// FUNCTIONS FROM des-lsr_routingLogic.c
void init_logic();
int send_hello(void *data, struct timeval *scheduled, struct timeval *interval);
int send_tc(void *data, struct timeval *scheduled, struct timeval *interval);
int refresh_list();

void init_rt();
int refresh_rt();

//////////////// FUNCTIONS FROM des-lsr_packetHandler.c
int sys_to_mesh(dessert_msg_t *msg, size_t len, dessert_msg_proc_t *proc, dessert_sysif_t *sysif, dessert_frameid_t id);

int drop_errors(dessert_msg_t* msg, size_t len, dessert_msg_proc_t *proc, const dessert_meshif_t *iface, dessert_frameid_t id);
int process_hello(dessert_msg_t* msg, size_t len, dessert_msg_proc_t *proc, const dessert_meshif_t *iface, dessert_frameid_t id);
int process_tc(dessert_msg_t* msg, size_t len, dessert_msg_proc_t *proc, const dessert_meshif_t *iface, dessert_frameid_t id);
int forward_packet(dessert_msg_t* msg, size_t len, dessert_msg_proc_t *proc, const dessert_meshif_t *iface, dessert_frameid_t id);
int mesh_to_sys(dessert_msg_t* msg, size_t len, dessert_msg_proc_t *proc, const dessert_meshif_t *iface, dessert_frameid_t id);
