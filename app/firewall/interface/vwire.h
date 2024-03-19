#ifndef _M_VWIRE_H_
#define _M_VWIRE_H_

#include "interface.h"

typedef struct {
    uint16_t id;
    uint16_t port1;
    uint16_t port2;
} vwire_config_t;

#define VWIRE_VALID_PORTID(i) ((i < interface_config->port_num) && (interface_config->ports[i].type == PORT_TYPE_VWIRE))

int vwire_init(void *config);
int vwire_pair(void *config, uint16_t port_id);

#endif

// file format utf-8
// ident using space