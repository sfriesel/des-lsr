// --- LIBRARIES --- //
#include "des-lsr.h"
#include "des-lsr_items.h"

// --- DAEMON INITIALIZATION --- //
int main (int argc, char** argv) {
	FILE *cfg;

	if ((argc == 3) && (strcmp(argv[2], "-nondaemonize") == 0)) {
		dessert_init("lsr", 0x01, DESSERT_OPT_NODAEMONIZE);
		char* cfg_file_name = argv[1];
		cfg = fopen(cfg_file_name, "r");
		if (!cfg) {
			printf("Config file '%s' not found. Exit ...\n", cfg_file_name);
			return EXIT_FAILURE;
		}
	} else {
		cfg = dessert_cli_get_cfg(argc, argv);
		dessert_init("lsr", 0x01, DESSERT_OPT_DAEMONIZE);
	}
	dessert_info("logging set");

	init_logic();
	dessert_info("routing initialized");

	dessert_sysrxcb_add(sys_to_mesh, 10);
	dessert_meshrxcb_add(dessert_msg_ifaceflags_cb, 20);
	dessert_meshrxcb_add(drop_errors, 40);
	dessert_meshrxcb_add(process_hello, 50);
	dessert_meshrxcb_add(process_tc, 60);
	dessert_meshrxcb_add(forward_packet, 70);
	dessert_meshrxcb_add(mesh_to_sys, 80);
	dessert_info("callbacks registered");

	cli_register_command(dessert_cli, dessert_cli_cfg_iface, "sys", dessert_cli_cmd_addsysif, PRIVILEGE_PRIVILEGED, MODE_CONFIG, "initialize sys interface");
	cli_register_command(dessert_cli, dessert_cli_cfg_iface, "mesh", dessert_cli_cmd_addmeshif, PRIVILEGE_PRIVILEGED, MODE_CONFIG, "initialize mesh interface");
	cli_file(dessert_cli, cfg, PRIVILEGE_PRIVILEGED, MODE_CONFIG);
	dessert_info("cli initialized");

	dessert_cli_run();
	dessert_run();
	return (0);
}
