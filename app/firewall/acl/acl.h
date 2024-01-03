#ifndef _M_ACL_H_
#define _M_ACL_H_

#include "../module.h"
#include "../packet.h"

#define MAX_ACL_RULE_NUM (1U << 16)

#define ACL_ACTION_ALLOW 0
#define ACL_ACTION_DENY 1

int acl_init(__rte_unused void* cfg);
mod_ret_t acl_proc(struct rte_mbuf *mbuf, mod_hook_t hook);
void acl_list(void);

#endif

// file format utf-8
// ident using space