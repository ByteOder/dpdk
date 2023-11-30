#ifndef _M_INTERFACE_H_
#define _M_INTERFACE_H_

#include "../module.h"

int interface_init(__rte_unused void* cfg);
mod_ret_t interface_proc(__rte_unused struct rte_mbuf *mbuf, mod_hook_t hook);

void interface_list(void);

#endif

// file-format: utf-8
// ident using spaces