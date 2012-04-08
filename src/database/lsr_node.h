#ifndef LSR_NODE
#define LSR_NODE

#include <uthash.h>
#include <dessert.h>

typedef struct edge {
	struct timeval last_update;
	struct node   *node;
	uint32_t       weight;
} edge_t;

typedef struct node {
	uint64_t multicast_seq_nr;           //the highest seen multicast sequence number of this node
	uint64_t unicast_seq_nr;             //the highest seen unicast sequence number of this node
	struct timeval timeout;
	struct edge *neighbors;              //list of edges pointing to neighboring nodes (accordings to the node's TC)
	mac_addr next_hop_addr;                   //the level 2 hop which should be used for forwarding to this node
	dessert_meshif_t *next_hop_iface;
	uint32_t weight;                     //total weight of the route to this node
	UT_hash_handle hh;                   //hash to group all nodes in a node set. hash key is addr
	mac_addr addr;                       //l2.5 address of the node
	uint8_t neighbor_size;               //size of the array pointed to by neighbors
	uint8_t neighbor_count;              //number of entries in neighbors
} node_t;

node_t *lsr_node_new(mac_addr addr, struct timeval timeout);
bool lsr_node_is_dead(node_t *this, const struct timeval *now);
void lsr_node_delete(node_t *this);
void lsr_node_set_timeout(node_t *this, struct timeval timeout);
struct neighbor;
void lsr_node_set_nexthop(node_t *this, mac_addr addr, dessert_meshif_t *iface, uint32_t nexthop_weight);
void lsr_node_update_edge(node_t *this, node_t *neighbor, uint16_t weight, struct timeval now);
void lsr_node_remove_old_edges(node_t *node, struct timeval cutoff);
bool lsr_node_check_broadcast_seq_nr(node_t *node, uint16_t seq_nr);
bool lsr_node_check_unicast_seq_nr(node_t *node, uint16_t seq_nr);

void lsr_node_print(node_t *this, FILE *f);
extern const char * const lsr_node_table_header;
void lsr_node_print_route(node_t *this, FILE *f);

#endif

