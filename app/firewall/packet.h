#ifndef _M_PACKET_H_
#define _M_PACKET_H_

#include <rte_mbuf_core.h>

/** 
 * assure packet_t 8 bytes aligned
 * */
#pragma pack(1)

typedef struct {
    uint16_t iport;
    uint16_t oport;
    uint32_t ptype;
    uint32_t flags;

    struct {
        uint16_t l2_len;
        uint16_t l3_len;
        uint16_t l4_len;
        uint16_t tunnel_len;
        uint16_t inner_l2_len;
        uint16_t inner_l3_len;
        uint16_t inner_l4_len;
    };

    uint8_t reserved[102];
} packet_t;

#pragma pack()

#endif