#include <dessert.h>
#include <dessert-extra.h>
#include <uthash.h>
#define LSR_DEFAULT_COUNTER 32

typedef struct  hello_ext {
	u_int16_t seq_num;
} __attribute__((__packed__)) hello_ext_t;

// a hashmap for all current neighbours
typedef struct dir_neighbours{
	u_int8_t addr[ETH_ALEN];
	u_int8_t counter;
	UT_hash_handle hh;
} dir_neighbours_t;
extern dir_neighbours_t *dir_neighbours_head;

// a hashmap for all neighbours of a node
typedef struct node_neighbours{
	u_int8_t addr[ETH_ALEN];
	UT_hash_handle hh;
} node_neighbours;

// a hashmap for all nodes
typedef struct all_nodes {
	u_int8_t addr[ETH_ALEN];
	u_int8_t next_hop[ETH_ALEN];
	u_int8_t counter;
	node_neighbours* neighbours;
	UT_hash_handle hh;
} all_nodes_t;
extern all_nodes_t *all_nodes_head;
