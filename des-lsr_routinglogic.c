#include "des-lsr.h"
#include "des-lsr_items.h"

node_neighbors* dir_neighbors_head 	= NULL;
all_nodes_t* all_nodes_head 			= NULL;

u_int16_t hello_interval 				= HELLO_INTERVAL;
u_int16_t tc_interval 					= TC_INTERVAL;
u_int16_t nh_refresh_interval 			= NH_REFRESH_INTERVAL;
u_int16_t rt_refresh_interval 			= RT_REFRESH_INTERVAL;
u_int16_t nh_entry_age 					= NH_ENTRY_AGE;
u_int16_t rt_entry_age 					= RT_ENTRY_AGE;

dessert_periodic_t* periodic_send_hello;
dessert_periodic_t* periodic_send_tc;
dessert_periodic_t* periodic_refresh_nh;
dessert_periodic_t* periodic_refresh_rt;

void init_logic() {
	// registering periodic for HELLO packets
	struct timeval hello_interval_t;
	hello_interval_t.tv_sec = hello_interval / 1000;
	hello_interval_t.tv_usec = (hello_interval % 1000) * 1000;
	periodic_send_hello = dessert_periodic_add(send_hello, NULL, NULL, &hello_interval_t);

	// registering periodic for TC packets
	struct timeval tc_interval_t;
	tc_interval_t.tv_sec = tc_interval / 1000;
	tc_interval_t.tv_usec = (tc_interval % 1000) * 1000;
	periodic_send_tc = dessert_periodic_add(send_tc, NULL, NULL, &tc_interval_t);

	// registering periodic for refreshing neighboring list
	struct timeval refresh_neighbor_t;
	refresh_neighbor_t.tv_sec = nh_refresh_interval / 1000;
	refresh_neighbor_t.tv_usec = (nh_refresh_interval % 1000) * 1000;
	periodic_refresh_nh = dessert_periodic_add(refresh_list, NULL, NULL, &refresh_neighbor_t);

	// registering periodic for refreshing routing table
	struct timeval refresh_rt_t;
	refresh_rt_t.tv_sec = rt_refresh_interval / 1000;
	refresh_rt_t.tv_usec = (rt_refresh_interval % 1000) * 1000;
	periodic_refresh_rt = dessert_periodic_add(refresh_rt, NULL, NULL, &refresh_rt_t);
}

// --- PERIODIC PIPELINE --- //
u_int16_t hello_seq_nr = 0;
u_int16_t tc_seq_nr = 0;

int send_hello(void *data, struct timeval *scheduled, struct timeval *interval) {
	dessert_msg_t *hello;
	dessert_msg_new(&hello);
	hello->ttl = 1;							// hello only for direct neighbors

	dessert_ext_t *ext = NULL;
	dessert_msg_addext(hello, &ext, LSR_EXT_HELLO, sizeof(hello_ext_t));
	hello_ext_t* hello_ext_data = (hello_ext_t*) ext->data;
	hello_ext_data->seq_nr = hello_seq_nr++;

	dessert_meshsend_fast(hello, NULL);		// send without creating copy
	dessert_msg_destroy(hello);				// we have created msg, we have to destroy it
	dessert_info("HELLO packet sent");
	return 0;
}

int send_tc(void *data, struct timeval *scheduled, struct timeval *interval) {
	node_neighbors *neighbor = dir_neighbors_head;
	if (!neighbor) {
		return 0;
	}

	dessert_msg_t *tc;
	dessert_msg_new(&tc);
	tc->ttl = TTL_MAX;

	// add TC extension
	dessert_ext_t *ext;
	dessert_msg_addext(tc, &ext, LSR_EXT_TC, sizeof(tc_ext_t));
	tc_ext_t* tc_ext_data = (tc_ext_t*) ext->data;
	tc_ext_data->seq_nr = tc_seq_nr++;
	tc_ext_data->neighbors = malloc(sizeof(dir_neighbors_head));
	memcpy(tc_ext_data->neighbors, dir_neighbors_head, sizeof(dir_neighbors_head));

	// add l2.5 header
	dessert_msg_addext(tc, &ext, DESSERT_EXT_ETH, ETHER_HDR_LEN);
	struct ether_header* l25h = (struct ether_header*) ext->data;
	memcpy(l25h->ether_shost, dessert_l25_defsrc, ETH_ALEN);
	memcpy(l25h->ether_dhost, ether_broadcast, ETH_ALEN);

	dessert_meshsend_fast(tc, NULL);
	dessert_msg_destroy(tc);
	dessert_info("TC packet sent");
	return 0;
}

int refresh_list(void *data, struct timeval *scheduled, struct timeval *interval) {
	dessert_meshif_t *iface;
	struct d_int avg_val;
	node_neighbors *dir_neigh = dir_neighbors_head;

	while (dir_neigh) {
		if (dir_neigh->entry_age-- == 0) {
			node_neighbors* el_to_delete = dir_neigh;
			dir_neigh = dir_neigh->hh.next;
			HASH_DEL(dir_neighbors_head, el_to_delete);
		} else {
			// MESHIFLIST_ITERATOR_START(iface)
				if (!dessert_search_con(dir_neigh->addr, "wlan0", &avg_val)) {
					dir_neigh->rssi = avg_val.value;
				} else {
					node_neighbors* el_to_delete = dir_neigh;
					dir_neigh = dir_neigh->hh.next;
					HASH_DEL(dir_neighbors_head, el_to_delete);
				}
			// MESHIFLIST_ITERATOR_STOP;

			dessert_info("neighbor found: %02x:%02x:%02x:%02x:%02x:%02x | %u",
				dir_neigh->addr[0], dir_neigh->addr[1], dir_neigh->addr[2],
				dir_neigh->addr[3], dir_neigh->addr[4], dir_neigh->addr[5],
				dir_neigh->entry_age);
			dir_neigh = dir_neigh->hh.next;
		}
	}

	return 0;
}

int refresh_rt(void *data, struct timeval *scheduled, struct timeval *interval) {
	all_nodes_t *ptr = all_nodes_head;
	all_nodes_t *node;
	node_neighbors *neighbor_ptr;

	while (ptr) {
		// if entry is deprecated
		if (ptr->entry_age-- == 0) {
			all_nodes_t* el_to_delete = ptr;
			ptr = ptr->hh.next;
			free(el_to_delete->neighbors);
			HASH_DEL(all_nodes_head, el_to_delete);
			continue;
		}

		// if node is direct neighbor, they are next hops for themselves
		node_neighbors *dir_neighbor;
		HASH_FIND(hh, dir_neighbors_head, ptr->addr, ETH_ALEN, dir_neighbor);
		if (dir_neighbor) {
			memcpy(ptr->next_hop, ptr->addr, ETH_ALEN);
		}

		// add next hop information for neighbors of current node
		neighbor_ptr = ptr->neighbors;
		while (neighbor_ptr) {
			HASH_FIND(hh, all_nodes_head, neighbor_ptr->addr, ETH_ALEN, node);
			if (node) {
				// if neighbor is known, its next hop is ptr
				memcpy(node->next_hop, ptr->addr, ETH_ALEN);
			}
			else {
				// if neighbor is unknown, add it to known nodes and its next hop is ptr
				node = malloc(sizeof(all_nodes_t));
				memcpy(node->addr, neighbor_ptr->addr, ETH_ALEN);
				memcpy(node->next_hop, ptr->addr, ETH_ALEN);
				node->entry_age = RT_ENTRY_AGE;
				node->neighbors = NULL;
				HASH_ADD_KEYPTR(hh, all_nodes_head, node->addr, ETH_ALEN, node);
			}

			dessert_info("adding node to routing table: %02x:%02x:%02x:%02x:%02x:%02x",
				neighbor_ptr->addr[0], neighbor_ptr->addr[1], neighbor_ptr->addr[2],
				neighbor_ptr->addr[3], neighbor_ptr->addr[4], neighbor_ptr->addr[5]);

			neighbor_ptr = neighbor_ptr->hh.next;
		}

		ptr = ptr->hh.next;
	}

	return 0;
}

// --- CALLBACK PIPELINE --- //
int drop_errors(dessert_msg_t* msg, size_t len,	dessert_msg_proc_t *proc, const dessert_meshif_t *iface, dessert_frameid_t id) {
	if (proc->lflags & DESSERT_LFLAG_PREVHOP_SELF || proc->lflags & DESSERT_LFLAG_SRC_SELF) {
		return DESSERT_MSG_DROP;
	}

	dessert_info("dropping packets sent to myself");
	return DESSERT_MSG_KEEP;
}

int process_hello(dessert_msg_t* msg, size_t len, dessert_msg_proc_t *proc, const dessert_meshif_t *iface, dessert_frameid_t id) {
	dessert_ext_t *ext;

	if(dessert_msg_getext(msg, &ext, LSR_EXT_HELLO, 0)) {
		node_neighbors *neighbor;
		HASH_FIND(hh, dir_neighbors_head, msg->l2h.ether_shost, ETH_ALEN, neighbor);

		if (neighbor) {
			neighbor->entry_age = NH_ENTRY_AGE;
		}
		else {
			neighbor = malloc(sizeof(node_neighbors));
			if (neighbor) {
				memcpy(neighbor->addr, msg->l2h.ether_shost, ETH_ALEN);
				neighbor->entry_age = NH_ENTRY_AGE;

				// do no forward msg->l2h.ether_shost
				// better: forward keypointer to struct element
				// because msg can be deleted
 				HASH_ADD_KEYPTR(hh, dir_neighbors_head, neighbor->addr, ETH_ALEN, neighbor);
			}
		}

		dessert_info("receiving HELLO packet from %02x:%02x:%02x:%02x:%02x:%02x",
			msg->l2h.ether_shost[0], msg->l2h.ether_shost[1], msg->l2h.ether_shost[2],
			msg->l2h.ether_shost[3], msg->l2h.ether_shost[4], msg->l2h.ether_shost[5]);

		neighbor = dir_neighbors_head;
		while (neighbor) {
			dessert_info("Neighbor %02x:%02x:%02x:%02x:%02x:%02x",
				neighbor->addr[0], neighbor->addr[1], neighbor->addr[2],
				neighbor->addr[3], neighbor->addr[4], neighbor->addr[5]);
			neighbor = neighbor->hh.next;
		}
		return DESSERT_MSG_DROP;
	}

	return DESSERT_MSG_KEEP;
}

int process_tc(dessert_msg_t* msg, size_t len, dessert_msg_proc_t *proc, const dessert_meshif_t *iface, dessert_frameid_t id) {
	dessert_ext_t *ext;
	all_nodes_t *node;

	if(dessert_msg_getext(msg, &ext, LSR_EXT_TC, 0)){
		struct tc_ext* tc_ext = (struct tc_ext*) ext->data;
		struct ether_header *l25h = dessert_msg_getl25ether(msg);
		HASH_FIND(hh, all_nodes_head, l25h->ether_shost, ETH_ALEN, node);

		if (node) {	// if found in struct for all nodes, delete its neighbors
			if (node->seq_nr >= tc_ext->seq_nr){
				return DESSERT_MSG_DROP;
			}

			node_neighbors* neighbor_el = node->neighbors;
			while (neighbor_el) {
				HASH_DEL(node->neighbors, neighbor_el);
				neighbor_el = node->neighbors;
			}
		} else { 	// if not found in struct for all nodes, add it
			node = malloc(sizeof(all_nodes_t));

			if (node) {
				memcpy(node->addr, l25h->ether_shost, ETH_ALEN);
				memcpy(node->next_hop, ether_broadcast, ETH_ALEN);
				node->neighbors = NULL;
				HASH_ADD_KEYPTR(hh, all_nodes_head, node->addr, ETH_ALEN, node);
			} else {
				return DESSERT_MSG_DROP;
			}
		}

		// add all neighbors of the node
		node->neighbors = malloc(sizeof(tc_ext->neighbors));
		node_neighbors *neighbor = tc_ext->neighbors;
		while (neighbor) {
			memcpy(node->neighbors->addr, neighbor->addr, ETH_ALEN);
			node->neighbors->entry_age = neighbor->entry_age;
			node->neighbors->rssi = neighbor->rssi;
			HASH_ADD_KEYPTR(hh, node->neighbors, neighbor->addr, ETH_ALEN, neighbor);
			neighbor = neighbor->hh.next;
		}

		node->entry_age = RT_ENTRY_AGE;

		dessert_info("receiving TC packet from %02x:%02x:%02x:%02x:%02x:%02x",
			msg->l2h.ether_shost[0], msg->l2h.ether_shost[1], msg->l2h.ether_shost[2],
			msg->l2h.ether_shost[3], msg->l2h.ether_shost[4], msg->l2h.ether_shost[5]);
		return DESSERT_MSG_DROP;
	}

	return DESSERT_MSG_KEEP;
}

int forward_packet(dessert_msg_t* msg, size_t len, dessert_msg_proc_t *proc, const dessert_meshif_t *iface, dessert_frameid_t id) {
	// if current node is the destination of the message but message is not for the current node
	if (memcmp(dessert_l25_defsrc, msg->l2h.ether_dhost, ETH_ALEN) == 0 && !(proc->lflags & DESSERT_LFLAG_DST_SELF)) {
		all_nodes_t* node;
		HASH_FIND(hh, all_nodes_head, msg->l2h.ether_dhost, ETH_ALEN, node);

		if (node && memcmp(node->next_hop, ether_broadcast, ETH_ALEN) != 0) {
			memcpy(msg->l2h.ether_dhost, node->next_hop, ETH_ALEN);
			dessert_meshsend_fast(msg, NULL);
		}

		return DESSERT_MSG_DROP;
	}

	return DESSERT_MSG_KEEP;
}
