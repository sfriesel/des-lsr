#include "des-lsr.h"
#include "des-lsr_items.h"

dir_neighbours_t* dir_neighbours_head = NULL;
all_nodes_t* all_nodes_head = NULL;

// adds hello sending, tc sending and refreshing topology information to the periodics
void init_logic() {
	struct timeval interval;
	interval.tv_sec = 1;
	interval.tv_usec = 500000;
	dessert_periodic_add(send_hello, NULL, NULL, &interval);

	interval.tv_sec = 2;
	interval.tv_usec = 0;
	dessert_periodic_add(send_tc, NULL, NULL, &interval);

	interval.tv_sec = 3;
	interval.tv_usec = 0;
	dessert_periodic_add(refresh_list, NULL, NULL, &interval);

	interval.tv_sec = 4;
	interval.tv_usec = 0;
	dessert_periodic_add(refresh_rt, NULL, NULL, &interval);
}

// --- PERIODIC PIPELINE --- //
u_int16_t hello_seq_num = 0;

int send_hello(void *data, struct timeval *scheduled, struct timeval *interval){
	dessert_msg_t *hello;
	dessert_msg_new(&hello);
	hello->ttl = 1;							// hello only for direct neighbours

	dessert_ext_t *ext = NULL;
	dessert_msg_addext(hello, &ext, LSR_EXT_HELLO, sizeof(hello_ext_t));
	hello_ext_t* hello_ext_data =  (hello_ext_t*)ext->data;
	hello_ext_data->seq_num = hello_seq_num++;

	dessert_meshsend_fast(hello, NULL);		// send without creating copy
	dessert_msg_destroy(hello);				// we have created msg, we have to destroy it
	dessert_info("HELLO packet sent");
	return 0;
}

int send_tc(void *data, struct timeval *scheduled, struct timeval *interval) {
	dir_neighbours_t *neighbour = dir_neighbours_head;
	if(!neighbour) return 0;		// return if no neighbours detected

	dessert_msg_t *tc;
	dessert_msg_new(&tc);
	tc->ttl = LSR_TTL_DEFAULT;		// tc packet should go to all nodes

	dessert_ext_t *ext;
	dessert_msg_addext(tc, &ext, LSR_EXT_TC, sizeof(HASH_COUNT(neighbour)*ETH_ALEN));
	void *current_addr_ptr = ext->data;

	// copying hardware addresses into extension
	while(neighbour) {
		memcpy(current_addr_ptr, neighbour->addr, ETH_ALEN);
		neighbour = neighbour->hh.next;
	}

	// add l2.5 header
	dessert_msg_addext(tc, &ext, DESSERT_EXT_ETH, ETHER_HDR_LEN);
	struct ether_header* l25h = (struct ether_header*) ext->data;
	memcpy(l25h->ether_shost, dessert_l25_defsrc, ETH_ALEN);
	memcpy(l25h->ether_dhost, ether_broadcast, ETH_ALEN);	// tc := broadcast packet

	dessert_meshsend_fast(tc, NULL);
	dessert_msg_destroy(tc);
	dessert_info("TC packet sent");
	return 0;
}

int refresh_list(void *data, struct timeval *scheduled, struct timeval *interval){
	dir_neighbours_t *dir_neigh = dir_neighbours_head;

	while (dir_neigh) {
		if (dir_neigh->counter-- == 0) {
			dir_neighbours_t* el_to_delete = dir_neigh;
			dir_neigh = dir_neigh->hh.next;
			HASH_DEL(dir_neighbours_head, el_to_delete);
			dessert_info("deleting neighbour (counter timeout)");
		} else {
			dessert_info("neighbour found: %02x:%02x:%02x:%02x:%02x:%02x | %u",
				dir_neigh->addr[0], dir_neigh->addr[1], dir_neigh->addr[2],
				dir_neigh->addr[3], dir_neigh->addr[4], dir_neigh->addr[5],
				dir_neigh->counter);
			dir_neigh = dir_neigh->hh.next;
		}
	}

	return 0;
}

int refresh_rt(void *data, struct timeval *scheduled, struct timeval *interval){
	all_nodes_t *ptr = all_nodes_head;
	all_nodes_t *node;
	node_neighbours *neighbour_ptr;

	while (ptr) {
		// if node is direct neighbour, they are next hops for themselves
		dir_neighbours_t *dir_neighbour;
		HASH_FIND(hh, dir_neighbours_head, ptr->addr, ETH_ALEN, dir_neighbour);
		if (dir_neighbour) memcpy(ptr->next_hop, ptr->addr, ETH_ALEN);

		// add next hop information for neighbours of current node
		neighbour_ptr = ptr->neighbours;
		while(neighbour_ptr){
			HASH_FIND(hh, all_nodes_head, neighbour_ptr->addr, ETH_ALEN, node);
			if (node) memcpy(node->next_hop, ptr->addr, ETH_ALEN);
			else {
				node = malloc(sizeof(all_nodes_t));
				memcpy(node->addr, neighbour_ptr->addr, ETH_ALEN);
				memcpy(node->next_hop, ptr->addr, ETH_ALEN);
				node->counter = LSR_DEFAULT_COUNTER;
				node->neighbours = NULL;
				HASH_ADD_KEYPTR(hh, all_nodes_head, node->addr, ETH_ALEN, node);
			}

			dessert_info("adding node to routing table: %02x:%02x:%02x:%02x:%02x:%02x",
				neighbour_ptr->addr[0], neighbour_ptr->addr[1], neighbour_ptr->addr[2],
				neighbour_ptr->addr[3], neighbour_ptr->addr[4], neighbour_ptr->addr[5]);

			neighbour_ptr = neighbour_ptr->hh.next;
		}

		ptr = ptr->hh.next;
	}

	return 0;
}

// --- CALLBACK PIPELINE --- //
int drop_errors(dessert_msg_t* msg, size_t len,	dessert_msg_proc_t *proc, const dessert_meshif_t *iface, dessert_frameid_t id){
	if (proc->lflags & DESSERT_LFLAG_PREVHOP_SELF) return DESSERT_MSG_DROP;
	if (proc->lflags & DESSERT_LFLAG_SRC_SELF) return DESSERT_MSG_DROP;
	dessert_info("dropping packets sent to myself");
	return DESSERT_MSG_KEEP;
}

int process_hello(dessert_msg_t* msg, size_t len, dessert_msg_proc_t *proc, const dessert_meshif_t *iface, dessert_frameid_t id){
	dessert_ext_t *ext;

	if(dessert_msg_getext(msg, &ext, LSR_EXT_HELLO, 0)){
		dir_neighbours_t *neighbour;
		HASH_FIND(hh, dir_neighbours_head, msg->l2h.ether_shost, ETH_ALEN, neighbour);

		if (neighbour) neighbour->counter = LSR_DEFAULT_COUNTER;
		else {
			neighbour = malloc(sizeof(dir_neighbours_t));
			if (neighbour) {
				memcpy(neighbour->addr, msg->l2h.ether_shost, ETH_ALEN);
				neighbour->counter = LSR_DEFAULT_COUNTER;

				// do no forward msg->l2h.ether_shost
				// better: forward keypointer to struct element
				// because msg can be deleted
 				HASH_ADD_KEYPTR(hh, dir_neighbours_head, neighbour->addr, ETH_ALEN, neighbour);
			}
		}

		dessert_info("receiving HELLO packet from %02x:%02x:%02x:%02x:%02x:%02x",
			msg->l2h.ether_shost[0], msg->l2h.ether_shost[1], msg->l2h.ether_shost[2],
			msg->l2h.ether_shost[3], msg->l2h.ether_shost[4], msg->l2h.ether_shost[5]);

		neighbour = dir_neighbours_head;
		while(neighbour){
			dessert_info("Neighbour %02x:%02x:%02x:%02x:%02x:%02x",
				neighbour->addr[0], neighbour->addr[1], neighbour->addr[2],
				neighbour->addr[3], neighbour->addr[4], neighbour->addr[5]);
			neighbour = neighbour->hh.next;
		}
		return DESSERT_MSG_DROP;
	}

	return DESSERT_MSG_KEEP;
}

int process_tc(dessert_msg_t* msg, size_t len, dessert_msg_proc_t *proc, const dessert_meshif_t *iface, dessert_frameid_t id) {
	dessert_ext_t *ext;

	if(dessert_msg_getext(msg, &ext, LSR_EXT_TC, 0)){
		struct ether_header *l25h = dessert_msg_getl25ether(msg);
		size_t neighbours_count = ext->len / ETH_ALEN;
		void *neighbour_ptr = ext->data;
		all_nodes_t *node;
		HASH_FIND(hh, all_nodes_head, l25h->ether_shost, ETH_ALEN, node);

		if (node) {
			// if found in struct for all nodes, delete his neighbours
			node_neighbours* neighbour_el = node->neighbours;
			while (neighbour_el) {
				HASH_DEL(node->neighbours, neighbour_el);
				neighbour_el = node->neighbours;
			}
		} else {
			// if not found in struct for all nodes, add him
			node = malloc(sizeof(all_nodes_t));

			if (node) {
				memcpy(node->addr, l25h->ether_shost, ETH_ALEN);
				memcpy(node->next_hop, ether_broadcast, ETH_ALEN);
				node->counter = LSR_DEFAULT_COUNTER;
				node->neighbours = NULL;
				HASH_ADD_KEYPTR(hh, all_nodes_head, node->addr, ETH_ALEN, node);
			} else return DESSERT_MSG_DROP;
		}

		// add all neighbours of the node
		while (neighbours_count-- > 0) {
			node_neighbours *node_neighbour = malloc(sizeof(node_neighbours));
			if (node_neighbour == NULL) return DESSERT_MSG_DROP;
			memcpy(node_neighbour->addr, neighbour_ptr, ETH_ALEN);
			HASH_ADD_KEYPTR(hh, node->neighbours, node_neighbour->addr, ETH_ALEN, node_neighbour);
			neighbour_ptr += ETH_ALEN;
		}

		dessert_info("receiving TC packet from %02x:%02x:%02x:%02x:%02x:%02x",
			msg->l2h.ether_shost[0], msg->l2h.ether_shost[1], msg->l2h.ether_shost[2],
			msg->l2h.ether_shost[3], msg->l2h.ether_shost[4], msg->l2h.ether_shost[5]);
		return DESSERT_MSG_DROP;
	}

	return DESSERT_MSG_KEEP;
}

int forward_packet(dessert_msg_t* msg, size_t len, dessert_msg_proc_t *proc, const dessert_meshif_t *iface, dessert_frameid_t id) {
	// if current node is the destination of the message but message isnt for the current node
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
