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
		{ dessert_cli_show     ,     "rebuild_rt", cli_show_rt_rebuild_interval, "show refresh RT interval"   },
		{ dessert_cli_show     ,             "rt", cli_show_rt                 , "show RT"                    },
		{ dessert_cli_show     ,             "nt", cli_show_nt                 , "show NT"                    },
		{ NULL                 , NULL            , NULL                        , NULL                         }
	};
	
	for(struct args *a =    cfg_args; a->name; ++a)
		cli_register_command(dessert_cli, a->parent, a->name, a->cb, PRIVILEGE_PRIVILEGED  , MODE_CONFIG, a->help);
	for(struct args *a = unpriv_args; a->name; ++a)
		cli_register_command(dessert_cli, a->parent, a->name, a->cb, PRIVILEGE_UNPRIVILEGED, MODE_EXEC  , a->help);
}

dessert_cb_result_t lsr_msg_flags_sys_cb(dessert_msg_t *msg, uint32_t len, dessert_msg_proc_t *proc, dessert_sysif_t *iface, dessert_frameid_t id) {
	return dessert_msg_ifaceflags_cb(msg, len, proc, (dessert_meshif_t *)iface, id);
}

static void init_pipeline(void) {
	dessert_sysrxcb_add(lsr_msg_flags_sys_cb, 5);
	dessert_sysrxcb_add(lsr_loopback, 10);
	dessert_sysrxcb_add(lsr_sys2mesh, 15);
	
	dessert_meshrxcb_add(dessert_msg_ifaceflags_cb, 10);
	dessert_meshrxcb_add(dessert_msg_dump_cb, 15);
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
	const char **config_names = malloc(sizeof(*config_names) * size);
	FILE **config_files = NULL;
	
	uint16_t init_flags = DESSERT_OPT_DAEMONIZE;
	uint16_t logcfg_flags = 0;
	
	int c;
	while((c = getopt (argc, argv, "nc:")) != -1) {
		switch(c) {
			case 'n':
				init_flags &= ~DESSERT_OPT_DAEMONIZE;
				logcfg_flags |= DESSERT_LOG_STDERR;
				break;
			case 'c':
				if(used == size) {
					config_names = realloc(config_names, sizeof(*config_files) * (size *= 2));
				}
				config_names[used++] = optarg;
				break;
			default:
				exit(EXIT_FAILURE);
				break;
		}
	}
	
	//open config files before (possibly) daemonizing
	config_files = malloc(sizeof(FILE *) * used);
	int i;
	for(i = 0; i < used; ++i) {
		config_files[i] = fopen(config_names[i], "r");
		if(!config_files[i]) {
			dessert_err("could not open config file %s\n", config_names[i]);
			exit(EXIT_FAILURE);
		}
	}
	free(config_names);
	
	dessert_init("LSR", 0x03, init_flags);
	dessert_logcfg(logcfg_flags);
	init_cli();
	//dessert_cli_run();

	for(i = 0; i < used; ++i) {
		cli_file(dessert_cli, config_files[i], PRIVILEGE_PRIVILEGED, MODE_CONFIG);
		fclose(config_files[i]);
	}
	free(config_files);

	init_pipeline();
	init_periodics();

	return dessert_run();
}

