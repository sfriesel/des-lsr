#ifndef LSR_CONFIG
#define LSR_CONFIG

#include <stdint.h>

#define LSR_EXT_TC DESSERT_EXT_USER

// milliseconds
#define HELLO_INTERVAL           4000
#define RT_REBUILD_INTERVAL      1000

//every n-th HELLO is turned into a TC (1 = always TC; 2=TC HELLO TC HELLO... etc.)
#define TC_RATIO                 2

//decrements once per aging interval
//in multiples of HELLO_INTERVAL
#define NEIGHBOR_LIFETIME   5
//in multiples of TC_RATIO * HELLO_INTERVAL = TC-interval
#define NODE_LIFETIME       32
#define DEFAULT_WEIGHT      1

#define LSR_TTL_MAX             UINT8_MAX

extern uintmax_t tc_ratio;
extern uintmax_t tc_interval;
extern uintmax_t neighbor_lifetime;
extern uintmax_t node_lifetime;

#endif
