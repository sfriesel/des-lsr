#include "des-lsr.h"
#include "des-lsr_items.h"
#include <pthread.h>
#include <string.h>

node_neighbors_t* node_neighbors_head	= NULL;
all_nodes_t* all_nodes_head			= NULL;

u_int16_t hello_interval 		= HELLO_INTERVAL;
u_int16_t tc_interval 			= TC_INTERVAL;
u_int16_t nh_refresh_interval 	= NH_REFRESH_INTERVAL;
u_int16_t rt_refresh_interval 	= RT_REFRESH_INTERVAL;
u_int16_t nh_entry_age 			= NH_ENTRY_AGE;
u_int16_t rt_entry_age 			= RT_ENTRY_AGE;

dessert_periodic_t* periodic_send_hello;
dessert_periodic_t* periodic_send_tc;
dessert_periodic_t* periodic_refresh_nh;
dessert_periodic_t* periodic_refresh_rt;

u_int16_t tc_seq_nr = 0;

pthread_rwlock_t pp_rwlock = PTHREAD_RWLOCK_INITIALIZER;

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
int send_hello(void *data, struct timeval *scheduled, struct timeval *interval) {
	dessert_msg_t *hello;
	dessert_msg_new(&hello);
	hello->ttl = 1;
	dessert_ext_t *ext = NULL;
	dessert_msg_addext(hello, &ext, LSR_EXT_HELLO, sizeof(hello_ext_t));
	dessert_msg_addext(hello, &ext, DESSERT_EXT_ETH, ETHER_HDR_LEN);
	struct ether_header* l25h = (struct ether_header*) ext->data;
	memcpy(l25h->ether_shost, dessert_l25_defsrc, ETH_ALEN);
	memcpy(l25h->ether_dhost, ether_broadcast, ETH_ALEN);
	dessert_meshsend_fast(hello, NULL);	 // send without creating copy
	dessert_msg_destroy(hello);          // we have created msg, we have to destroy it
	return 0;
}

int process_hello(dessert_msg_t* msg, size_t len, dessert_msg_proc_t *proc, const dessert_meshif_t *iface, dessert_frameid_t id) {
	dessert_ext_t *ext;

	if(dessert_msg_getext(msg, &ext, LSR_EXT_HELLO, 0)) {
		pthread_rwlock_wrlock(&pp_rwlock);
		struct ether_header* l25h = dessert_msg_getl25ether(msg);
		node_neighbors_t *neighbor = malloc(sizeof(node_neighbors_t));
		HASH_FIND(hh, node_neighbors_head, l25h->ether_shost, ETH_ALEN, neighbor);
		if (neighbor) {
			neighbor->entry_age = NH_ENTRY_AGE;
		} else {
			neighbor = malloc(sizeof(node_neighbors_t));
			memcpy(neighbor->addr, l25h->ether_shost, ETH_ALEN);
			neighbor->entry_age = NH_ENTRY_AGE;
			neighbor->weight = 1;
			HASH_ADD_KEYPTR(hh, node_neighbors_head, neighbor->addr, ETH_ALEN, neighbor);
		}
		pthread_rwlock_unlock(&pp_rwlock);
		return DESSERT_MSG_DROP;
	}

	return DESSERT_MSG_KEEP;
}

int send_tc(void *data, struct timeval *scheduled, struct timeval *interval) {
	pthread_rwlock_wrlock(&pp_rwlock);
	if (HASH_COUNT(node_neighbors_head) == 0) {
		return 0;
	}

	dessert_msg_t *tc;
	dessert_msg_new(&tc);
	tc->ttl = TTL_MAX;
	tc->u8 = ++tc_seq_nr;

	// delete old entries from NH list
	node_neighbors_t *dir_neigh = node_neighbors_head;
	while (dir_neigh) {
		if (dir_neigh->entry_age-- == 0) {
			node_neighbors_t* el_to_delete = dir_neigh;
			HASH_DEL(node_neighbors_head, el_to_delete);
			free(el_to_delete);
		}
		dir_neigh = dir_neigh->hh.next;
	}

	// add TC extension
	dessert_ext_t *ext;
	u_int8_t ext_size = 1 + ((sizeof(node_neighbors_t)- sizeof(node_neighbors_head->hh)) * HASH_COUNT(node_neighbors_head));
	dessert_msg_addext(tc, &ext, LSR_EXT_TC, ext_size);
	void* tc_ext = ext->data;
	memcpy(tc_ext, &(ext_size), 1);
	tc_ext++;

	// copy NH list into extension
	dir_neigh = node_neighbors_head;
	while (dir_neigh) {
		memcpy(tc_ext, dir_neigh->addr, ETH_ALEN);
		tc_ext += ETH_ALEN;
		memcpy(tc_ext, &(dir_neigh->entry_age), 1);
		tc_ext++;
		memcpy(tc_ext, &(dir_neigh->weight), 1);
		tc_ext++;
		dir_neigh = dir_neigh->hh.next;
	}

	// add l2.5 header
	dessert_msg_addext(tc, &ext, DESSERT_EXT_ETH, ETHER_HDR_LEN);
	struct ether_header* l25h = (struct ether_header*) ext->data;
	memcpy(l25h->ether_shost, dessert_l25_defsrc, ETH_ALEN);
	memcpy(l25h->ether_dhost, ether_broadcast, ETH_ALEN);

	dessert_meshsend_fast(tc, NULL);
	dessert_msg_destroy(tc);
	pthread_rwlock_unlock(&pp_rwlock);
	return 0;
}

int process_tc(dessert_msg_t* msg, size_t len, dessert_msg_proc_t *proc, const dessert_meshif_t *iface, dessert_frameid_t id) {
	dessert_ext_t *ext;

	if(dessert_msg_getext(msg, &ext, LSR_EXT_TC, 0)){
		pthread_rwlock_wrlock(&pp_rwlock);
		all_nodes_t *node = malloc(sizeof(all_nodes_t));
		node_neighbors_t *neighbor = malloc(sizeof(node_neighbors_t));
		struct ether_header* l25h = dessert_msg_getl25ether(msg);
		void* tc_ext = (void*) ext->data;
		u_int8_t ext_size;
		u_int8_t addr[ETH_ALEN];
		u_int8_t entry_age;
		u_int8_t weight;

		// if node is not in RT, add the node
		HASH_FIND(hh, all_nodes_head, l25h->ether_shost, ETH_ALEN, node);
		if (!node) {
			dessert_info("NODE NOT IN RT");
			node = malloc(sizeof(all_nodes_t));
			memcpy(node->addr, l25h->ether_shost, ETH_ALEN);
			node->entry_age = RT_ENTRY_AGE;
			node->seq_nr = msg->u8;
			HASH_ADD_KEYPTR(hh, all_nodes_head, node->addr, ETH_ALEN, node);
		}

		// add NH to RT
		memcpy(&ext_size, tc_ext, 1);
		tc_ext++;
		while (ext_size-1 > 0) {
			memcpy(addr, tc_ext, ETH_ALEN);
			ext_size -= ETH_ALEN;
			tc_ext += ETH_ALEN;
			memcpy(&entry_age, tc_ext, 1);
			ext_size--;
			tc_ext++;
			memcpy(&weight, tc_ext, 1);
			ext_size--;
			tc_ext++;
			HASH_FIND(hh, all_nodes_head, l25h->ether_shost, ETH_ALEN, node);
			if (node->seq_nr >= msg->u8) {
				return DESSERT_MSG_DROP;
			}
			node->entry_age = RT_ENTRY_AGE;
			node->seq_nr = msg->u8;
			HASH_FIND(hh, node->neighbors, addr, ETH_ALEN, neighbor);
			if (neighbor) {
				neighbor->entry_age = entry_age;
				neighbor->weight = weight;
			} else {
				neighbor = malloc(sizeof(node_neighbors_t));
				memcpy(neighbor->addr, addr, ETH_ALEN);
				neighbor->entry_age = entry_age;
				neighbor->weight = weight;
				HASH_ADD_KEYPTR(hh, node->neighbors, neighbor->addr, ETH_ALEN, neighbor);
			}
		}

		dessert_meshsend_fast_randomized(msg);	// resend TC packet
		pthread_rwlock_unlock(&pp_rwlock);
		return DESSERT_MSG_DROP;
	}

	return DESSERT_MSG_KEEP;
}

int refresh_list(void *data, struct timeval *scheduled, struct timeval *interval) {
	pthread_rwlock_wrlock(&pp_rwlock);
	node_neighbors_t *neighbor = node_neighbors_head;
	while (neighbor) {
		if (neighbor->entry_age-- == 0) {
			node_neighbors_t* el_to_delete = neighbor;
			HASH_DEL(node_neighbors_head, el_to_delete);
			free(el_to_delete);
		} else {
			neighbor->weight = 1;
		}
		neighbor = neighbor->hh.next;
	}
	pthread_rwlock_unlock(&pp_rwlock);
	return 0;
}

int refresh_rt(void *data, struct timeval *scheduled, struct timeval *interval) {
	pthread_rwlock_wrlock(&pp_rwlock);
	all_nodes_t *node = all_nodes_head;
	node_neighbors_t *neighbor;

	while (node) {
		if (node->entry_age-- == 0) {
			all_nodes_t* el_to_delete = node;
			HASH_DEL(all_nodes_head, el_to_delete);
			free(el_to_delete);
		}

		dessert_info("RT ENTRY %02x:%02x:%02x:%02x:%02x:%02x | Seqnr = %d | EntryAge = %d",
			node->addr[0], node->addr[1], node->addr[2], node->addr[3],
			node->addr[4], node->addr[5], node->seq_nr, node->entry_age);

		neighbor = node->neighbors;
		while (neighbor) {
			dessert_info("\t\tRT ENTRY NH %02x:%02x:%02x:%02x:%02x:%02x | Weight = %d | EntryAge = %d",
				neighbor->addr[0], neighbor->addr[1], neighbor->addr[2], neighbor->addr[3],
				neighbor->addr[4], neighbor->addr[5], neighbor->weight, neighbor->entry_age);
			neighbor = neighbor->hh.next;
		}
		node = node->hh.next;
	}
	pthread_rwlock_unlock(&pp_rwlock);
	return 0;
}

// --- CALLBACK PIPELINE --- //
int drop_errors(dessert_msg_t* msg, size_t len,	dessert_msg_proc_t *proc, const dessert_meshif_t *iface, dessert_frameid_t id) {
	if (proc->lflags & DESSERT_LFLAG_PREVHOP_SELF || proc->lflags & DESSERT_LFLAG_SRC_SELF) {
		dessert_info("dropping packets sent to myself");
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
