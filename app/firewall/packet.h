#ifndef _M_PACKET_H_
#define _M_PACKET_H_

#include <rte_mbuf_core.h>

/** 
 * assure packet_t 8 bytes aligned
 * */
typedef struct {
	uint16_t in_port;
	uint16_t out_port;
	uint8_t reserved[124];
} packet_t;

#endif