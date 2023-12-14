/**
 * An implementation for modular design. Each specific module can be 
 * implemented easily just follow the convention in this part.
 * !!! Be careful when include header file in *.c that module.h must 
 * be in front of any specific module's header file. 
 */

#ifndef __M_MODULE_H__
#define __M_MODULE_H__

#include <rte_mbuf.h>
#include <rte_log.h>

/**
 * module section.
 * */
#define __module__ __attribute((section(".module_section")))


/**
 * module hook. 
 */
typedef enum {
    MOD_HOOK_RECV,
    MOD_HOOK_INGRESS,
    MOD_HOOK_PREROUTING,
    MOD_HOOK_FORWARD,
    MOD_HOOK_POSTROUTING,
    MOD_HOOK_LOCALIN,
    MOD_HOOK_LOCALOUT,
    MOD_HOOK_EGRESS,
    MOD_HOOK_SEND,
} mod_hook_t;


/**
 * module return value. 
 */
typedef enum {
    MOD_RET_ACCEPT,
    MOD_RET_DROP,
    MOD_RET_STOLEN,
} mod_ret_t;


/**
 * module function.
 * */
typedef mod_ret_t (*mod_func_t)(struct rte_mbuf *mbuf, mod_hook_t hook);
typedef int (*mod_init_t)(__rte_unused void* cfg);


/**
 * module struct.
 */
typedef struct {
    const char *name;           /** module name */
    uint16_t id;                /** module id */
    bool enabled;               /** module switch */
    bool log;                   /** log switch */
    const char *logf;           /** log file path */
    mod_init_t init;            /** init function */
    mod_func_t proc;            /** process function */
    void *priv;                 /** private use */
} module_t;


/**
 * module array. 
 */
#define MAX_MODULE_NUM 128
extern int max_module_id;
extern module_t* modules[MAX_MODULE_NUM];


/**
 * module macro.
 * */
#define MODULE_DECLARE(m) module_t m __module__

#define MODULE_REGISTER(m) \
do { \
    if (((m)->id > 0) && ((m)->id < 128) && (!modules[(m)->id])) { \
        modules[(m)->id] = m; \
        if ((m)->id > max_module_id) { \
            max_module_id = (m)->id; \
        } \
    } \
} while(0)

#define MODULE_FOREACH(m, id) \
    for (id = 0, m = modules[id]; id <= max_module_id; m = modules[++id])


/**
 * module public interface.
 * */
int modules_load(void);
int modules_init(void *config);
int modules_proc(struct rte_mbuf *pkt, mod_hook_t hook);

#endif

// file-format: utf-8
// ident using spaces