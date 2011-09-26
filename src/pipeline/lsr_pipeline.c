#include "lsr_pipeline.h"
#include "../lsr_config.h"
#include "../database/lsr_database.h"

#include <pthread.h>
#include <string.h>

typedef struct tc_ext {
	uint8_t addr[ETH_ALEN];
	uint8_t age;
	uint8_t weight;
} __attribute__((__packed__)) tc_ext_t;

int lsr_send_randomized(dessert_msg_t *msg) {
	if(msg->ttl) {
		return dessert_meshsend_randomized(msg);
	}
	else {
		return DESSERT_ERR;
	}
}

int lsr_send(dessert_msg_t *msg, dessert_meshif_t *iface) {
	if(msg->ttl) {
		return dessert_meshsend(msg, iface);
	}
	else {
		return DESSERT_ERR;
	}
}

dessert_cb_result_t lsr_process_ttl(dessert_msg_t* msg, uint32_t len, dessert_msg_proc_t *proc, dessert_meshif_t *iface, dessert_frameid_t id) {
	if(msg->ttl)
		msg->ttl--;
	return DESSERT_MSG_KEEP;
}

dessert_cb_result_t lsr_process_tc(dessert_msg_t* msg, uint32_t len, dessert_msg_proc_t *proc, dessert_meshif_t *iface, dessert_frameid_t id) {
	dessert_ext_t *ext;

	if(!dessert_msg_getext(msg, &ext, LSR_EXT_TC, 0)) {
		return DESSERT_MSG_KEEP;
	}

	struct ether_header* l25h = dessert_msg_getl25ether(msg);
	
	if(!lsr_db_tc_check_seq_nr(l25h->ether_shost, msg->u8)) {
		return DESSERT_MSG_DROP;
	}

	//if tc travelled exactly one hop, handle as hello packet
	if(msg->ttl == TTL_MAX-1) {
		lsr_db_nt_update(msg->l2h.ether_shost, l25h->ether_shost, iface, msg->u8);
	}
	
	lsr_db_tc_update(l25h->ether_shost, msg->u8);
	
	int neigh_count = (ext->len - DESSERT_EXTLEN)/sizeof(tc_ext_t);
	tc_ext_t *tc_data = (tc_ext_t*) ext->data;
	// add NH to RT
	for(int i = 0; i < neigh_count; ++i) {
		lsr_db_tc_neighbor_update(l25h->ether_shost, tc_data[i].addr, tc_data[i].age, tc_data[i].weight);
	}
	lsr_send_randomized(msg);	// resend TC packet
	
	return DESSERT_MSG_DROP;
}

// --- CALLBACK PIPELINE --- //
dessert_cb_result_t lsr_drop_errors(dessert_msg_t* msg, uint32_t len,	dessert_msg_proc_t *proc, dessert_meshif_t *iface, dessert_frameid_t id) {
	if(proc->lflags & DESSERT_RX_FLAG_L2_SRC || proc->lflags & DESSERT_RX_FLAG_L25_SRC) {
		dessert_info("dropping packets sent to myself");
		return DESSERT_MSG_DROP;
	}

	return DESSERT_MSG_KEEP;
}

//also broadcast
dessert_cb_result_t lsr_forward_multicast(dessert_msg_t* msg, uint32_t len, dessert_msg_proc_t *proc, dessert_meshif_t *iface, dessert_frameid_t id) {
	if(!(proc->lflags & DESSERT_RX_FLAG_L25_MULTICAST)) {
		return DESSERT_MSG_KEEP;
	}
	
	struct ether_header* l25h = dessert_msg_getl25ether(msg);
	if(!lsr_db_data_check_seq_nr(l25h->ether_shost, msg->u16)) {
		return DESSERT_MSG_DROP;
	}
	//de-duplicated
	if(proc->lflags & DESSERT_RX_FLAG_L25_MULTICAST) {
		lsr_send_randomized(msg);
	}
	return DESSERT_MSG_KEEP;
}

dessert_cb_result_t lsr_forward_unicast(dessert_msg_t* msg, uint32_t len, dessert_msg_proc_t *proc, dessert_meshif_t *iface, dessert_frameid_t id) {
	// if current node is the l2 destination of the message but message is not for the current node
	if(proc->lflags & DESSERT_RX_FLAG_L25_DST || !(proc->lflags & DESSERT_RX_FLAG_L2_DST)) {
		return DESSERT_MSG_KEEP;
	}
	struct ether_header* l25h = dessert_msg_getl25ether(msg);
	
	dessert_meshif_t *out_iface;
	mac_addr next_hop;
	dessert_result_t result = lsr_db_get_next_hop(l25h->ether_shost, &next_hop, &out_iface);

	if(result == DESSERT_OK) {
		memcpy(msg->l2h.ether_dhost, next_hop, ETH_ALEN);
		lsr_send(msg, out_iface);
	}
	else {
		dessert_trace("no route to dest " MAC, EXPLODE_ARRAY6(l25h->ether_dhost));
	}

	return DESSERT_MSG_DROP;
}

dessert_cb_result_t lsr_mesh2sys(dessert_msg_t* msg, uint32_t len, dessert_msg_proc_t *proc, dessert_meshif_t *iface, dessert_frameid_t id) {
	if((proc->lflags & DESSERT_RX_FLAG_L25_DST && !(proc->lflags & DESSERT_RX_FLAG_L25_OVERHEARD))
	      || proc->lflags & DESSERT_RX_FLAG_L25_BROADCAST
	      || proc->lflags & DESSERT_RX_FLAG_L25_MULTICAST ) {
		dessert_syssend_msg(msg);
		return DESSERT_MSG_DROP;
	}

	return DESSERT_MSG_KEEP;
}

dessert_cb_result_t lsr_unhandled(dessert_msg_t* msg, uint32_t len, dessert_msg_proc_t *proc, dessert_meshif_t *iface, dessert_frameid_t id) {
	dessert_crit("unhandled packet");
	return DESSERT_MSG_DROP;
}

dessert_cb_result_t lsr_sys2mesh(dessert_msg_t *msg, uint32_t len, dessert_msg_proc_t *proc, dessert_sysif_t *sysif, dessert_frameid_t id) {
	struct ether_header* l25h = dessert_msg_getl25ether(msg);
	msg->u16 = lsr_db_data_get_seq_nr();
	
	if(proc->lflags & (DESSERT_RX_FLAG_L25_BROADCAST | DESSERT_RX_FLAG_L25_MULTICAST)) {
		memcpy(msg->l2h.ether_dhost, ether_broadcast, ETH_ALEN);
		lsr_send_randomized(msg);
	}
	else {
		dessert_meshif_t *out_iface;
		mac_addr next_hop;
		dessert_result_t result = lsr_db_get_next_hop(l25h->ether_shost, &next_hop, &out_iface);
		
		if(result == DESSERT_OK) {
			memcpy(msg->l2h.ether_dhost, next_hop, ETH_ALEN);
			lsr_send(msg, out_iface);
		}
		else {
			dessert_trace("no route to dest " MAC, EXPLODE_ARRAY6(l25h->ether_dhost));
		}
	}
	return DESSERT_MSG_DROP;
}


