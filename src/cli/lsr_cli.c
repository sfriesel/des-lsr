#include "lsr_cli.h"
#include "../lsr_config.h"
#include "../periodic/lsr_periodic.h"
#include "../database/lsr_database.h"

int cli_set_hello_interval(struct cli_def *cli, char *command, char *argv[], int argc) {
	if(argc != 1) {
		cli_print(cli, "usage %s [interval in ms]\n", command);
		return CLI_ERROR;
	}

	dessert_periodic_del(periodic_send_hello_tc);
	uintmax_t hello_interval = strtoul(argv[0], NULL, 0);
	tc_interval = hello_interval * tc_ratio;
	struct timeval hello_interval_tv;
	dessert_ms2timeval(hello_interval, &hello_interval_tv);
	periodic_send_hello_tc = dessert_periodic_add(lsr_periodic_send_hello_tc, NULL, NULL, &hello_interval_tv);
	dessert_notice("setting HELLO interval to %d ms\n", hello_interval);
	return CLI_OK;
}

int cli_show_hello_interval(struct cli_def *cli, char *command, char *argv[], int argc) {
	cli_print(cli, "HELLO interval = %jd ms\n", dessert_timeval2ms(&periodic_send_hello_tc->interval));
	return CLI_OK;
}

int cli_set_tc_ratio(struct cli_def *cli, char *command, char *argv[], int argc) {
	if(argc != 1) {
		cli_print(cli, "usage %s [interval in ms]\n", command);
		return CLI_ERROR;
	}

	uintmax_t new_tc_ratio = (uint16_t) strtoul(argv[0], NULL, 10);
	tc_interval = tc_interval / tc_ratio * new_tc_ratio;
	tc_ratio = new_tc_ratio;
	dessert_notice("setting tc_ratio to %jd ms\n", (intmax_t)tc_ratio);
	return CLI_OK;
}

int cli_show_tc_ratio(struct cli_def *cli, char *command, char *argv[], int argc) {
	cli_print(cli, "TC ratio = %jd ms\n", (uintmax_t)tc_ratio);
	return CLI_OK;
}

int cli_set_rt_rebuild_interval(struct cli_def *cli, char *command, char *argv[], int argc) {
	if(argc != 1) {
		cli_print(cli, "usage %s [interval in ms]\n", command);
		return CLI_ERROR;
	}

	dessert_periodic_del(periodic_rebuild_rt);
	uintmax_t rt_rebuild_interval = strtoul(argv[0], NULL, 0);
	struct timeval rt_rebuild_tv;
	dessert_ms2timeval(rt_rebuild_interval, &rt_rebuild_tv);
	periodic_rebuild_rt = dessert_periodic_add(lsr_periodic_rebuild_rt, NULL, NULL, &rt_rebuild_tv);
	dessert_notice("setting rt_rebuild_interval to %jd ms\n", (intmax_t)rt_rebuild_interval);
	return CLI_OK;
}

int cli_show_rt_rebuild_interval(struct cli_def *cli, char *command, char *argv[], int argc) {
	cli_print(cli, "rt_rebuild_interval = %jd ms\n", dessert_timeval2ms(&periodic_rebuild_rt->interval));
	return CLI_OK;
}

int cli_show_rt(struct cli_def *cli, char *command, char *argv[], int argc) {
	char *output = lsr_db_rt_to_string();
	cli_print(cli, "%s", output);
	free(output);
	return CLI_OK;
}

int cli_show_nt(struct cli_def *cli, char *command, char *argv[], int argc) {
	neighbor_info_t *neighbor_list = NULL;
	int neighbor_count = 0;
	lsr_db_dump_neighbor_table(&neighbor_list, &neighbor_count);

	cli_print(cli,  "###############################################");
	cli_print(cli,  "## NEIGHBOR L25         # WEIGHT  # LIFETIME ##");
	cli_print(cli,  "###############################################");

	for(int i = 0; i < neighbor_count; ++i) {
		cli_print(cli, "## " MAC "\t# %d\t# %d",
				EXPLODE_ARRAY6(neighbor_list[i].addr), neighbor_list[i].weight, neighbor_list[i].lifetime);
	}

	return CLI_OK;
}
