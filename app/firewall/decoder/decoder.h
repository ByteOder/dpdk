#ifndef _M_DECODER_H_
#define _M_DECODER_H_

#include "../module.h"

int decoder_init(__rte_unused void* cfg);
mod_ret_t decoder_proc(struct rte_mbuf *mbuf, mod_hook_t hook);

#endif

// file format utf-8
// ident using space