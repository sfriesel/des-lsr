#include <string.h>
#include <unistd.h>
#include <stdbool.h>

#include "lsr_config.h"
#include "periodic/lsr_periodic.h"
#include "pipeline/lsr_pipeline.h"
#include "cli/lsr_cli.h"

uintmax_t tc_ratio = TC_RATIO;
uintmax_t tc_interval = HELLO_INTERVAL * TC_RATIO;
uintmax_t neighbor_lifetime = NEIGHBOR_LIFETIME;
uintmax_t node_lifetime = NODE_LIFETIME;

static void init_periodics(void) {
	struct timeval hello_tc_interval_tv;
	dessert_ms2timeval(HELLO_INTERVAL, &hello_tc_interval_tv);
	periodic_send_hello_tc = dessert_periodic_add(lsr_periodic_send_hello_tc, NULL, NULL, &hello_tc_interval_tv);
	
	struct timeval rt_rebuild_tv;
	dessert_ms2timeval(RT_REBUILD_INTERVAL, &rt_rebuild_tv);
	periodic_rebuild_rt = dessert_periodic_add(lsr_periodic_rebuild_rt, NULL, NULL, &rt_rebuild_tv);
}

static void init_cli(void) {
	struct cli_command *_lsr_cli_set =
	    cli_register_command(dessert_cli, NULL, "set", NULL, PRIVILEGE_PRIVILEGED  , MODE_CONFIG, "set variable");
	struct args {
		struct cli_command *parent;
		char *name;
		int (*cb)(struct cli_def *, char *, char **, int);
		char *help;
	} cfg_args[] = {
		{ dessert_cli_cfg_iface, "sys"           , dessert_cli_cmd_addsysif   , "initialize sys interface"  },
		{ dessert_cli_cfg_iface, "mesh"          , dessert_cli_cmd_addmeshif  , "initialize mesh interface" },
		{ _lsr_cli_set         , "hello_interval", cli_set_hello_interval     , "set HELLO packet interval" },
		{ _lsr_cli_set         , "tc_ratio"      , cli_set_tc_ratio           , "set TC packet ratio"       },
		{ _lsr_cli_set         , "rebuild_rt"    , cli_set_rt_rebuild_interval, "set rebuild RT interval"   },
		{ NULL                 , NULL            , NULL                       , NULL                        }
	};
	struct args unpriv_args[] = {
		{ dessert_cli_show     , "hello_interval", cli_show_hello_interval     , "show HELLO interval"        },
		{ dessert_cli_show     ,       "tc_ratio", cli_show_tc_ratio           , "show TC packet ratio"       },
		{ dessert_cli_show     ,     "rt_rebuild", cli_show_rt_rebuild_interval, "show refresh RT interval"   },
		{ dessert_cli_show     ,             "rt", cli_show_rt                 , "show RT"                    },
		{ dessert_cli_show     ,             "nt", cli_show_nt                 , "show NT"                    },
		{ NULL                 , NULL            , NULL                        , NULL                         }
	};
	
	for(struct args *a =    cfg_args; a->name; ++a)
		cli_register_command(dessert_cli, a->parent, a->name, a->cb, PRIVILEGE_PRIVILEGED  , MODE_CONFIG, a->help);
	for(struct args *a = unpriv_args; a->name; ++a)
		cli_register_command(dessert_cli, a->parent, a->name, a->cb, PRIVILEGE_UNPRIVILEGED, MODE_EXEC  , a->help);
}

static void init_pipeline(void) {
	dessert_sysrxcb_add(dessert_msg_ifaceflags_cb_sys, 5);
	dessert_sysrxcb_add(lsr_loopback, 10);
	dessert_sysrxcb_add(lsr_sys2mesh, 15);
	
	dessert_meshrxcb_add(dessert_msg_ifaceflags_cb, 10);
	dessert_meshrxcb_add(lsr_process_ttl, 20);
	dessert_meshrxcb_add(lsr_drop_errors, 30);
	//dessert_meshrxcb_add(lsr_process_hello, 40);
	dessert_meshrxcb_add(lsr_process_tc, 50);
	dessert_meshrxcb_add(lsr_forward_multicast, 55);
	dessert_meshrxcb_add(lsr_forward_unicast, 60);
	dessert_meshrxcb_add(lsr_mesh2sys, 70);
	dessert_meshrxcb_add(lsr_unhandled, 200);
}

int main(int argc, char *argv[]) {
	int used = 0;
	int size = 2;
	const char **config_files = malloc(sizeof(FILE *) * size);
	
	dessert_status_flags_t init_flags = dessert_status_flags_init;
	init_flags.daemonize = 0;
	uint16_t logcfg_flags = DESSERT_LOG_SYSLOG | DESSERT_LOG_STDERR;
	
	int c;
	while((c = getopt (argc, argv, "dc:")) != -1) {
		switch(c) {
			case 'd':
				init_flags.daemonize = 1;
				logcfg_flags &= ~DESSERT_LOG_STDERR;
				break;
			case 'c':
				if(used == size) {
					config_files = realloc(config_files, size *= 2);
				}
				config_files[used++] = optarg;
				break;
			default:
				exit(EXIT_FAILURE);
				break;
		}
	}
	
	dessert_init("LSR", 0x03, init_flags);
	dessert_logcfg(logcfg_flags);

	init_cli();
	init_pipeline();
	init_periodics();

	for(int i = 0; i < used; ++i) {
		FILE *cfg = fopen(config_files[i], "r");
		if(!cfg) {
			dessert_err("could not open config file %s\n", config_files[i]);
			exit(EXIT_FAILURE);
		}
		cli_file(dessert_cli, cfg, PRIVILEGE_PRIVILEGED, MODE_CONFIG);
	}

	dessert_cli_run();
	return dessert_run();
}

