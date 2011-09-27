#include <string.h>

#include "lsr_config.h"
#include "periodic/lsr_periodic.h"
#include "pipeline/lsr_pipeline.h"
#include "cli/lsr_cli.h"

uint16_t hello_interval = HELLO_INTERVAL;
uint16_t tc_interval = TC_INTERVAL;
uint16_t neighbor_aging_interval = NEIGHBOR_AGING_INTERVAL;
uint16_t node_aging_interval = NODE_AGING_INTERVAL;
uint8_t  neighbor_lifetime = NEIGHBOR_LIFETIME;
uint8_t  node_lifetime = NODE_LIFETIME;

static void init_periodics() {
	//TODO
	#if 0
	// registering periodic for HELLO packets
	struct timeval hello_interval_t;
	hello_interval_t.tv_sec = hello_interval / 1000;
	hello_interval_t.tv_usec = (hello_interval % 1000) * 1000;
	periodic_send_hello = dessert_periodic_add(send_hello, NULL, NULL, &hello_interval_t);
	#endif
	
	// registering periodic for TC packets
	struct timeval tc_interval_timeval = { tc_interval / 1000, (tc_interval % 1000) * 1000};
	periodic_send_tc = dessert_periodic_add(lsr_periodic_send_tc, NULL, NULL, &tc_interval_timeval);

	//TODO
	#if 0
	// registering periodic for refreshing neighboring list
	struct timeval refresh_neighbor_t;
	refresh_neighbor_t.tv_sec = neighbor_aging_interval / 1000;
	refresh_neighbor_t.tv_usec = (neighbor_aging_interval % 1000) * 1000;
	periodic_refresh_nh = dessert_periodic_add(refresh_list, NULL, NULL, &refresh_neighbor_t);

	//TODO
	// registering periodic for refreshing routing table
	struct timeval refresh_rt_t;
	refresh_rt_t.tv_sec = node_aging_interval / 1000;
	refresh_rt_t.tv_usec = (node_aging_interval % 1000) * 1000;
	periodic_refresh_rt = dessert_periodic_add(refresh_rt, NULL, NULL, &refresh_rt_t);
	#endif
}

// --- DAEMON INITIALIZATION --- //
int main (int argc, char *argv[]) {
	FILE *cfg;
	/* initialize daemon with correct parameters */
	if(argc > 1 && (strcmp(argv[1], "-d") == 0)) {
		dessert_info("starting LSR in non daemonize mode");
		dessert_init("LSR", 0x03, DESSERT_OPT_NODAEMONIZE);
		dessert_logcfg(DESSERT_LOG_STDERR);
		char cfg_file_name[] = "des-lsr.cli";
		cfg = fopen(cfg_file_name, "r");
		if (cfg == NULL) {
			printf("Config file '%s' not found. Exit... \n", cfg_file_name);
			return EXIT_FAILURE;
		}
	} else {
		//FIXME dessert should parse it's options independent of position
		cfg = dessert_cli_get_cfg(argc, argv);
		dessert_info("starting LSR in daemonize mode");
		dessert_init("LSR", 0x03, DESSERT_OPT_DAEMONIZE);
	}
	

	/* cli initialization */
	cli_register_command(dessert_cli, dessert_cli_cfg_iface, "sys", dessert_cli_cmd_addsysif, PRIVILEGE_PRIVILEGED, MODE_CONFIG, "initialize sys interface");
	cli_register_command(dessert_cli, dessert_cli_cfg_iface, "mesh", dessert_cli_cmd_addmeshif, PRIVILEGE_PRIVILEGED, MODE_CONFIG, "initialize mesh interface");

	struct cli_command* cli_cfg_set = cli_register_command(dessert_cli, NULL, "set", NULL, PRIVILEGE_PRIVILEGED, MODE_CONFIG, "set variable");
	cli_register_command(dessert_cli, cli_cfg_set, "hello_interval", cli_set_hello_interval, PRIVILEGE_PRIVILEGED, MODE_CONFIG, "set HELLO packet interval");
	cli_register_command(dessert_cli, dessert_cli_show, "hello_interval", cli_show_hello_interval, PRIVILEGE_UNPRIVILEGED, MODE_EXEC, "show HELLO packet size");
	cli_register_command(dessert_cli, cli_cfg_set, "tc_interval", cli_set_tc_interval, PRIVILEGE_PRIVILEGED, MODE_CONFIG, "set TC packet interval");
	cli_register_command(dessert_cli, dessert_cli_show, "tc_interval", cli_show_tc_interval, PRIVILEGE_UNPRIVILEGED, MODE_EXEC, "show TC packet size");
	cli_register_command(dessert_cli, cli_cfg_set, "refresh_list", cli_set_refresh_list, PRIVILEGE_PRIVILEGED, MODE_CONFIG, "set refresh NH interval");
	cli_register_command(dessert_cli, dessert_cli_show, "refresh_list", cli_show_refresh_list, PRIVILEGE_UNPRIVILEGED, MODE_EXEC, "show refresh NH interval");
	cli_register_command(dessert_cli, cli_cfg_set, "refresh_rt", cli_set_refresh_rt, PRIVILEGE_PRIVILEGED, MODE_CONFIG, "set refresh RT interval");
	cli_register_command(dessert_cli, dessert_cli_show, "refresh_rt", cli_show_refresh_rt, PRIVILEGE_UNPRIVILEGED, MODE_EXEC, "show refresh RT interval");
	cli_register_command(dessert_cli, dessert_cli_show, "rt", cli_show_rt, PRIVILEGE_UNPRIVILEGED, MODE_EXEC, "show RT");
	
	cli_file(dessert_cli, cfg, PRIVILEGE_PRIVILEGED, MODE_CONFIG);
	
	/* callback registration */
	//dessert_sysrxcb_add(dessert_msg_ifaceflags_cb_sys, 5);
	dessert_sysrxcb_add(lsr_sys2mesh, 10);
	
	dessert_meshrxcb_add(dessert_msg_ifaceflags_cb, 10);
	dessert_meshrxcb_add(lsr_process_ttl, 20);
	dessert_meshrxcb_add(lsr_drop_errors, 30);
	//dessert_meshrxcb_add(lsr_process_hello, 40);
	dessert_meshrxcb_add(lsr_process_tc, 50);
	dessert_meshrxcb_add(lsr_forward_multicast, 55);
	dessert_meshrxcb_add(lsr_forward_unicast, 60);
	dessert_meshrxcb_add(lsr_mesh2sys, 70);
	dessert_meshrxcb_add(lsr_unhandled, 200);

	/* periodic function initialization */
	init_periodics();

	/* running cli & daemon */
	dessert_cli_run();
	dessert_run();
	return (0);
}

