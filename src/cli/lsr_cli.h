#ifndef LSR_CLI
#define LSR_CLI
#include <dessert.h>

int cli_set_hello_interval(struct cli_def* cli, char* command, char* argv[], int argc);
int cli_show_hello_interval(struct cli_def* cli, char* command, char* argv[], int argc);
int cli_set_tc_ratio(struct cli_def* cli, char* command, char* argv[], int argc);
int cli_show_tc_ratio(struct cli_def* cli, char* command, char* argv[], int argc);
int cli_set_rt_rebuild_interval(struct cli_def* cli, char* command, char* argv[], int argc);
int cli_show_rt_rebuild_interval(struct cli_def* cli, char* command, char* argv[], int argc);
int cli_show_rt(struct cli_def* cli, char* command, char* argv[], int argc);
int cli_show_nt(struct cli_def* cli, char* command, char* argv[], int argc);

#endif
