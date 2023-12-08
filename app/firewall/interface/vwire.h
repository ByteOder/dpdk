#ifndef _M_VWIRE_H_
#define _M_VWIRE_H_

typedef struct {
    uint16_t id;
    uint16_t port1;
    uint16_t port2;
} vwire_config_t;

int vwire_init(__rte_unused void* cfg);

#endif

// file format utf-8
// ident using space