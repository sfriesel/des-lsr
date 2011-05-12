#include <dessert.h>
#include <dessert-extra.h>
#include <uthash.h>
#include "des-lsr.h"

//////////////// DATABASE
// a hashmap for all neighbours of a node
typedef struct node_neighbours {
	u_int8_t addr[ETH_ALEN];
	u_int8_t rssi;
	u_int8_t entry_age;
	UT_hash_handle hh;
} node_neighbors;
extern node_neighbors *dir_neighbors_head;

// a hashmap for all nodes
typedef struct all_nodes {
	u_int8_t addr[ETH_ALEN];
	u_int8_t next_hop[ETH_ALEN];
	u_int8_t entry_age;
	u_int8_t seq_nr;
	node_neighbors* neighbors;
	UT_hash_handle hh;
} all_nodes_t;
extern all_nodes_t *all_nodes_head;

//////////////// EXTENSIONS
typedef struct  hello_ext {
	u_int16_t seq_nr;
} __attribute__((__packed__)) hello_ext_t;

typedef struct  tc_ext {
	u_int16_t seq_nr;
	node_neighbors* neighbors;
} __attribute__((__packed__)) tc_ext_t;
