#include <rte_ethdev.h>

#include "../module.h"
#include "interface.h"

MODULE_DECLARE(interface) = {
    .name = "interface",
    .id = 1,
    .enabled = true,
    .log = false,
    .logf = NULL,
    .init = interface_init,
    .proc = interface_proc,
    .priv = NULL
};

int interface_init(__rte_unused void* cfg)
{
    struct rte_eth_dev_info dev_info;
    uint16_t portid;
    int ret;

    RTE_ETH_FOREACH_DEV(portid) {
        ret = rte_eth_dev_info_get(portid, &dev_info);
        if (ret) {
            rte_exit(EXIT_FAILURE, "get dev info failed");
        }

        printf("device name %s driver name %s max_rx_queues %u max_tx_queues %u\n",
            dev_info.device->name, dev_info.driver_name, dev_info.max_rx_queues, dev_info.max_tx_queues);
    }

    return 0;
}


mod_ret_t interface_proc(__rte_unused struct rte_mbuf *mbuf, mod_hook_t hook)
{
    printf("interface proc\n");

    if (hook == MOD_HOOK_RECV) {
        printf("recv\n");
    }

    if (hook == MOD_HOOK_SEND) {
        printf("send\n");
    }

    return MOD_RET_ACCEPT;
}

void interface_list(void)
{
    printf("interface list\n");
    return;
}

// file-format: utf-8
// ident using spaces