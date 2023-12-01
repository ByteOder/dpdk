#include "module.h"

// module secetion start and end point, see module_section.lds
module_t __module_start__;
module_t __module_end__;


module_t* modules[MAX_MODULE_NUM] = {0};
int max_module_id = -1;


int modules_load(void)
{
    printf("modules load\n");

    module_t *m;

    for (m = &__module_start__; m < &__module_end__; m ++) {
        MODULE_REGISTER(m);
    }

    return 0;
}

int modules_init(void *config)
{
    printf("modules init ...\n");

    __rte_unused module_t *m;
    __rte_unused int id;

    MODULE_FOREACH(m, id) {
        if (m && m->init && m->enabled) {
            printf("[%d] %s init\n", id, m->name);
            if (m->init(config)) {
                return -1;
            }
        }
    }

    return 0;
}

int modules_proc(struct rte_mbuf *pkt, mod_hook_t hook)
{
    printf("modules proc\n");

    module_t *m;
    int id;
    mod_ret_t ret;

    MODULE_FOREACH(m, id) {
        if (m && m->proc && m->enabled) {
            ret = m->proc(pkt, hook);

            if (ret == MOD_RET_STOLEN) {
                break;
            }

            if (ret == MOD_RET_DROP) {
                continue;
            }

            if (ret == MOD_RET_ACCEPT) {
                continue;
            }
        }
    }

    return 0;
}

// file-format: utf-8
// ident using spaces