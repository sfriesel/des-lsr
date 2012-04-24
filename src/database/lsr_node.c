#include "lsr_node.h"
#include "lsr_tc.h"
#include "lsr_database.h"
#include "lsr_nt.h"

#include <assert.h>

static const uint64_t SEQ_NR_THRESHOLD = 500; //consider sequence numbers of at most threshold smaller then the current as older

node_t *lsr_node_new(mac_addr addr, struct timeval timeout) {
	node_t *this = calloc(1, sizeof(*this));
	this->multicast_seq_nr = 0;
	this->unicast_seq_nr = 0;
	this->timeout = timeout;
	this->neighbors = NULL;
	this->next_hop_iface = NULL;
	this->weight = INFINITE_WEIGHT;
	mac_copy(this->addr, addr);
	this->neighbor_size = 0;
	this->neighbor_count = 0;
	return this;
}

bool lsr_node_is_dead(node_t *this, const struct timeval *now) {
	return dessert_timevalcmp(now, &this->timeout) >= 0 && !this->next_hop_iface;
}

void lsr_node_delete(node_t *this) {
	free(this->neighbors);
	free(this);
}

void lsr_node_set_timeout(node_t *this, struct timeval timeout) {
	if(dessert_timevalcmp(&timeout, &this->timeout) > 0)
		this->timeout = timeout;
}

void lsr_node_set_nexthop(node_t *this, mac_addr addr, dessert_meshif_t *iface, uint32_t nexthop_weight) {
	if(this->weight > nexthop_weight) {
		mac_copy(this->next_hop_addr, addr);
		this->next_hop_iface = iface;
		this->weight = nexthop_weight;
	}
}

static inline int get_neighbor_index(node_t *this, const mac_addr addr) {
	int i;
	for(i = 0; i < this->neighbor_count; ++i)
		if(mac_equal(addr, this->neighbors[i].node->addr))
			break;
	return i;
}

static inline int get_unused_index(node_t *this) {
	if(this->neighbor_count >= this->neighbor_size) {
		assert(this->neighbor_size < UINT8_MAX);
		this->neighbor_size = this->neighbor_size < UINT8_MAX/2 ? this->neighbor_size * 2: UINT8_MAX;
		this->neighbors = realloc(this->neighbors, sizeof(struct edge) * this->neighbor_size);
	}
	return this->neighbor_count;
}

void lsr_node_update_edge(node_t *this, node_t *neighbor, uint16_t weight, struct timeval now) {
	if(!this->neighbors) {
		this->neighbor_size = 2;
		this->neighbors = calloc(this->neighbor_size, sizeof(struct edge));
	}
	
	int i = get_neighbor_index(this, neighbor->addr);
	
	if(i >= this->neighbor_count) { //addr not found, new neighbor
		dessert_trace("new neighbor " MAC, EXPLODE_ARRAY6(neighbor->addr));
		i = get_unused_index(this);
		this->neighbor_count++;
		this->neighbors[i].node = neighbor;
	}
	else
		dessert_trace("refreshed neighbor " MAC, EXPLODE_ARRAY6(neighbor->addr));
	
	this->neighbors[i].last_update = now;
	this->neighbors[i].weight = weight;
}


void lsr_node_remove_old_edges(node_t *this, struct timeval cutoff) {
	if(!this->neighbors || !this->neighbor_count)
		return;
	for(int i = 0; i < this->neighbor_count; ++i) {
		if(dessert_timevalcmp(&this->neighbors[i].last_update, &cutoff) < 0) {
			dessert_trace("delete neighbor " MAC, EXPLODE_ARRAY6(this->neighbors[i].node->addr));
			//delete this neighbor by moving the last neighbor in its place
			this->neighbor_count--;
			this->neighbors[i] = this->neighbors[this->neighbor_count];
		}
	}
}

static uint64_t guess_seq_nr(uint64_t old, uint16_t new) {
	uint64_t guess = new;
	if(guess >= old)
		return guess;
	
	guess += ((old - guess) & ~(uint64_t)UINT16_MAX);
	while(guess < old - min(old, SEQ_NR_THRESHOLD))
		guess += UINT16_MAX + 1;
	return guess;
}

static inline bool check_seq_nr(uint64_t *old, uint16_t seq_nr) {
	uint64_t guess = guess_seq_nr(*old, seq_nr);
	if(guess > *old) {
		*old = guess;
		return true;
	}
	else
		return false;
}

bool lsr_node_check_broadcast_seq_nr(node_t *node, uint16_t seq_nr) {
	return check_seq_nr(&(node->multicast_seq_nr), seq_nr);
}

bool lsr_node_check_unicast_seq_nr(node_t *node, uint16_t seq_nr) {
	return check_seq_nr(&(node->unicast_seq_nr), seq_nr);
}

const char * const lsr_node_table_header = "node l25 addr     | timeout           | multicast seq nr | unicast seq nr   | ngbr count ";

void lsr_node_print(node_t *this, FILE *f) {
	fprintf(f, MAC " | %10jd.%06jd | %16jd | %16jd | %11jd", EXPLODE_ARRAY6(this->addr), (intmax_t)this->timeout.tv_sec, (intmax_t)this->timeout.tv_usec, (intmax_t)this->multicast_seq_nr, (intmax_t)this->unicast_seq_nr, (intmax_t)this->neighbor_count);
}

void lsr_node_print_route(node_t *this, FILE *f) {
	struct timeval now;
	gettimeofday(&now);
	fprintf(f, "%02hhx%02hhx%02hhx\t", this->addr[3], this->addr[4], this->addr[5]);
	uint8_t *next_hop_l25 = lsr_nt_node_addr(this->next_hop_addr, this->next_hop_iface);
	if(next_hop_l25)
		fprintf(f, "%02hhx%02hhx%02hhx\t", next_hop_l25[3], next_hop_l25[4], next_hop_l25[5]);
	else
		fputs("<null>\t", f);
	if(this->weight <= 99)
		fprintf(f, "%3jd\t", (uintmax_t)this->weight);
	else
		fputs(">99\t", f);
	intmax_t age_ms = (now.tv_sec - this->last_update.tv_sec) * 1000;
	age_ms += (now.tv_usec - this->last_update.tv_usec) / 1000;
	intmax_t age_intervals = age_ms / tc_interval;
	fprintf(f, "%jd", age_intervals);
}

