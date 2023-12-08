#ifndef _M_CONFIG_H_
#define _M_CONFIG_H_

#include <stdbool.h>

#define MAX_FILE_PATH 256

#define CONFIG_PATH "/opt/firewall/config"
#define BINARY_PATH "/opt/firewall/bin"

typedef struct {
    struct rte_mempool *pktmbuf_pool;
    int promiscuous;
    int worker_num;

    void *interface_config;
} config_t;

extern config_t config;

#endif

// file format utf-8
// ident using space