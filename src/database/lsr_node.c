#include "lsr_node.h"
#include "lsr_tc.h"
#include "lsr_database.h"
#include "lsr_nt.h"

#include <assert.h>

static const uint64_t SEQ_NR_THRESHOLD = 500; //consider sequence numbers of at most threshold smaller then the current as older

node_t *lsr_node_new(mac_addr addr) {
	node_t *this = calloc(1, sizeof(*this));
	this->multicast_seq_nr = 0;
	this->unicast_seq_nr = 0;
	this->timeout.tv_sec = INT32_MAX;
	this->timeout.tv_usec = 999999;
	this->multicast_gaps = NULL;
	this->unicast_gaps = NULL;
	this->neighbors = NULL;
	this->next_hop = NULL;
	this->weight = INFINITE_WEIGHT;
	mac_copy(this->addr, addr);
	this->neighbor_size = 0;
	this->neighbor_count = 0;
	return this;
}

//ages the node and returns whether to delete it
bool lsr_node_age(node_t *this, const struct timeval *now) {
	//remove all outdated neighbor edges
	for(int i = 0; i < this->neighbor_count; ++i) {
		if(dessert_timevalcmp(now, &this->neighbors[i].timeout) < 0) {
			this->neighbor_count--;
			//copy the last array element into the gap
			this->neighbors[i] = this->neighbors[this->neighbor_count];
		}
	}
	// if node has no next_hop, it's not referenced
	// as neighbor of any reachable node
	if(dessert_timevalcmp(now, &this->timeout) < 0 && !this->next_hop) {
		return true;
	}
	return false;
}

void lsr_node_delete(node_t *this) {
	free(this->multicast_gaps);
	free(this->unicast_gaps);
	free(this->neighbors);
	free(this);
}

void lsr_node_set_timeout(node_t *this, struct timeval timeout) {
	this->timeout = timeout;
}

static inline int get_neighbor_index(node_t *this, const mac_addr addr) {
	int i;
	for(i = 0; i < this->neighbor_count; ++i) {
		if(mac_equal(addr, this->neighbors[i].node->addr)) {
			break;
		}
	}
	return i;
}

static inline int get_unused_index(node_t *this) {
	if(this->neighbor_count >= this->neighbor_size) {
		uint8_t old_size = this->neighbor_size;
		this->neighbor_size *= 2;
		assert(this->neighbor_size > old_size);
		this->neighbors = realloc(this->neighbors, sizeof(struct edge) * this->neighbor_size);
	}
	return this->neighbor_count;
}

void lsr_node_update_neighbor(node_t *this, node_t *neighbor, struct timeval timeout, uint8_t weight) {
	if(!this->neighbors) {
		this->neighbor_size = 2;
		this->neighbors = calloc(this->neighbor_size, sizeof(struct edge));
	}
	
	int i = get_neighbor_index(this, neighbor->addr);
	
	if(i >= this->neighbor_count) { //addr not found, new neighbor
		i = get_unused_index(this);
		this->neighbor_count++;
		this->neighbors[i].node = neighbor;
	}
	
	this->neighbors[i].timeout = timeout;
	this->neighbors[i].weight = weight;
}

static int gap_insert_index(struct seq_interval *gaps, uint16_t seq_nr){
	int insert_index = 0;
	for(int i = 0; i < GAP_COUNT; ++i) {
		if(gaps[i].start == gaps[i].end) {
			insert_index = i;
			break;
		}
		if(seq_nr - gaps[i].start < seq_nr - gaps[insert_index].start) {
			insert_index = i;
		}
	}
	return insert_index;
}

static void split_gap(struct seq_interval *gaps, uint64_t x, int index, uint16_t seq_nr) {
	if(x == gaps[index].start) {
		gaps[index].start = x + 1;
		return;
	}
	if(x == gaps[index].end - 1U) {
		gaps[index].end = x;
		return;
	}
	
	int insert_index = gap_insert_index(gaps, seq_nr);
	
	gaps[insert_index].start = gaps[index].start;
	gaps[insert_index].end = x;
	gaps[index].start = x + 1;
}

static uint64_t guess_seq_nr(uint64_t old, uint16_t new) {
	uint64_t guess = new;
	if(guess >= old) {
		return guess;
	}
	guess += ((old - guess) & ~(uint64_t)UINT16_MAX);
	while(guess < old - min(old, SEQ_NR_THRESHOLD)) {
		guess += UINT16_MAX + 1;
	}
	return guess;
}

static bool check_seq_nr(uint64_t *old, struct seq_interval *gaps, uint16_t seq_nr) {
	uint64_t guess = guess_seq_nr(*old, seq_nr);
	if(guess > *old) {
		if(guess > *old + 1) { //need to create a new gap
			if(guess > *old + SEQ_NR_THRESHOLD) { //seq nr made a big jump -- invalidate old gaps
				memset(gaps, 0, GAP_COUNT * sizeof(struct seq_interval));
			}
			int insert_index = gap_insert_index(gaps, seq_nr);
			gaps[insert_index].start = *old + 1;
			gaps[insert_index].end = guess;
		}
		*old = guess;
		return true;
	}
	for(int i = 0; i < GAP_COUNT; ++i) {
		if(gaps[i].start <= guess && gaps[i].end > guess) {
			split_gap(gaps, guess, i, seq_nr);
			return true;
		}
	}
	return false;
}

bool lsr_node_check_broadcast_seq_nr(node_t *node, uint16_t seq_nr) {
	if(!node->multicast_gaps) {
		node->multicast_gaps = calloc(GAP_COUNT, sizeof(struct seq_interval));
	}
	return check_seq_nr(&(node->multicast_seq_nr), node->multicast_gaps, seq_nr);
}

bool lsr_node_check_unicast_seq_nr(node_t *node, uint16_t seq_nr) {
	if(!node->unicast_gaps) {
		node->unicast_gaps = calloc(GAP_COUNT, sizeof(struct seq_interval));
	}
	return check_seq_nr(&(node->unicast_seq_nr), node->unicast_gaps, seq_nr);
}

char *lsr_node_to_string(node_t *this) {
	int bufSize = 90;
	char *buf = malloc(bufSize);
	int l = snprintf(buf, bufSize, MAC " | %10jd.%06jd | %16jd | %16jd | %11jd",
	                 EXPLODE_ARRAY6(this->addr),
	                 (intmax_t)this->timeout.tv_sec,
	                 (intmax_t)this->timeout.tv_usec,
	                 (intmax_t)this->multicast_seq_nr,
	                 (intmax_t)this->unicast_seq_nr,
	                 (intmax_t)this->neighbor_count);
	assert(l < bufSize);
	return buf;
}

const char *lsr_node_to_string_header(void) {
	static const char *header = "node l25 addr     | timeout           | multicast seq nr | unicast seq nr   | ngbr count ";
	return header;
}

char *lsr_node_to_route_string(node_t *this) {
	int bufSize = 22;
	char *buf = malloc(bufSize);
	char *iter = buf;

	iter += snprintf(buf, 10, "%02hhx%02hhx%02hhx | ", this->addr[3], this->addr[4], this->addr[5]);
	assert(iter = buf + 9);
	if(this->next_hop)
		iter += snprintf(iter, 10, "%02hhx%02hhx%02hhx | ", this->next_hop->node->addr[3], this->next_hop->node->addr[4], this->next_hop->node->addr[5]);
	else
		iter += snprintf(iter, 10, "<null> | ");
	assert(iter == buf + 18);
	if(this->next_hop) {
		if(this-> weight <= 99)
			iter += snprintf(iter, 4, "%3jd", (uintmax_t)this->weight);
		else
			iter += snprintf(iter, 4, ">99");
	}
	else
		iter += snprintf(iter, 4, "inf");
	assert(iter < buf + bufSize);
	
	return buf;
}

const char *lsr_node_to_route_string_header(void) {
	static const char *header = "dest   | nexthop| wgt";
	return header;
}

