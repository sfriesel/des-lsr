#include "lsr_node.h"
#include "lsr_tc.h"
#include "lsr_database.h"

static const uint64_t SEQ_NR_THRESHOLD = 500; //consider sequence numbers of at most threshold smaller then the current as older

node_t *lsr_node_new(mac_addr addr) {
	node_t *this = calloc(1, sizeof(*this));
	this->multicast_seq_nr = 0;
	this->unicast_seq_nr = 0;
	this->timeout.tv_sec = UINT32_MAX;
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

void lsr_node_delete(node_t *this) {
	free(this->multicast_gaps);
	free(this->unicast_gaps);
	free(this->neighbors);
	free(this);
}

int lsr_node_cmp(node_t *left, node_t *right) {
	return memcmp(left->addr, right->addr, ETH_ALEN);
}

void lsr_node_set_timeout(node_t *this, struct timeval timeout) {
	this->timeout = timeout;
	//TODO seq_nr
}

int _get_first_index(node_t *this, const mac_addr addr) {
	int i;
	for(i = 0; i < this->neighbor_count; ++i) {
		if(mac_equal(addr, this->neighbors[i].node->addr)) {
			break;
		}
	}
	return i;
}

void lsr_node_update_neighbor(node_t *this, mac_addr neighbor_addr, struct timeval timeout, uint8_t weight) {
	int i = _get_first_index(this, neighbor_addr);
	
	if(i >= this->neighbor_count) {
		i = _get_first_index(this, ether_null);
		if(i >= this->neighbor_count) {
			this->neighbor_count++;
			if(this->neighbor_count >= this->neighbor_size) {
				this->neighbors = realloc(this->neighbors, this->neighbor_size *= 2);
			}
		}
	}
	
	this->neighbors[i].timeout = timeout;
	this->neighbors[i].weight = weight;
}

void _split_gap(struct seq_interval *gaps, uint64_t x, int index) {
	if(x == gaps[index].start) {
		gaps[index].start = x + 1;
		return;
	}
	if(x == gaps[index].end - 1) {
		gaps[index].end = x;
		return;
	}
	
	int insert_index = 0;
	for(int i = 0; i < GAP_COUNT; ++i) {
		if(gaps[i].start == gaps[i].end) {
			insert_index = i;
			break;
		}
		if(gaps[i].start < gaps[insert_index].start) {
			insert_index = i;
		}
	}
	
	gaps[insert_index].start = gaps[index].start;
	gaps[insert_index].end = x;
	gaps[index].start = x + 1;
}

uint64_t _guess_seq_nr(uint64_t old, uint16_t new) {
	uint64_t guess = ((old >> 16 - 1) << 16) + new;
	while(guess < old - min(old, SEQ_NR_THRESHOLD)) {
		guess += UINT16_MAX + 1;
	}
	return guess;
}

bool _lsr_node_check_seq_nr(uint64_t *old, struct seq_interval *gaps, uint16_t seq_nr) {
	uint64_t guess = _guess_seq_nr(*old, seq_nr);
	if(guess > *old) {
		*old = guess;
		return true;
	}
	for(int i = 0; i < GAP_COUNT; ++i) {
		if(gaps[i].start <= guess && gaps[i].end > guess) {
			_split_gap(gaps, guess, i);
			return true;
		}
	}
	return false;
}

bool lsr_node_check_broadcast_seq_nr(node_t *node, uint16_t seq_nr) {
	return _lsr_node_check_seq_nr(&(node->multicast_seq_nr), node->multicast_gaps, seq_nr);
}

bool lsr_node_check_unicast_seq_nr(node_t *node, uint16_t seq_nr) {
	return _lsr_node_check_seq_nr(&(node->unicast_seq_nr), node->unicast_gaps, seq_nr);
}


