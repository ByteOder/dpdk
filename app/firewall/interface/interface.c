#include "../module.h"
#include "interface.h"

MODULE_DECLARE(interface) = {
    .name = "interface",
    .id = 1,
    .enabled = true,
    .init = interface_init,
    .proc = interface_proc,
    .priv = NULL
};

int interface_init(__rte_unused void* cfg)
{
    printf("interface init\n");
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