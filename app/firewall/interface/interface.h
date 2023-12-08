#ifndef _M_INTERFACE_H_
#define _M_INTERFACE_H_

#include "../module.h"

#define MAX_PORT_NUM   32
#define MAX_VWIRE_NUM  16

typedef enum {
    PORT_TYPE_NONE,
    PORT_TYPE_VWIRE,
} port_type_t;

typedef struct {
    uint16_t id;
    port_type_t type;
    char bus[16];
    char mac[32];
} port_config_t;

typedef struct {
    port_config_t ports[MAX_PORT_NUM];
    void *vwire_pairs;
    uint16_t port_num;
    uint16_t vwire_pair_num;
} interface_config_t;

int interface_init(__rte_unused void* cfg);
mod_ret_t interface_proc(__rte_unused struct rte_mbuf *mbuf, mod_hook_t hook);

void interface_list(void);

#endif

// file-format: utf-8
// ident using spaces