#include "../config.h"
#include "../json.h"
#include "interface.h"
#include "vwire.h"

static int
vwire_json_load(interface_config_t *interface_config)
{
    json_object *jr = NULL, *ja;
    int i, vwire_pairs_num;
    vwire_config_t *vwire_pairs_mem = NULL;
    int ret = 0;

    if (!interface_config) {
        ret = -1;
        goto done;
    }

    jr = JR(CONFIG_PATH, "vwire.json");
    if (!jr) {
        ret = -1;
        goto done;
    }

    vwire_pairs_num = JA(jr, "vwire_pairs", &ja);
    if (vwire_pairs_num == -1) {
        ret = -1;
        goto done;
    }

    vwire_pairs_mem = (vwire_config_t *)malloc(sizeof(vwire_config_t) * vwire_pairs_num);

    #define VWIRE_JV(item) \
        jv = JV(jo, item); \
        if (!jv) { \
            printf("parse %s failed\n", item); \
            ret = -1; \
            goto done; \
        }

    for (i = 0; i < vwire_pairs_num; i++) {
        json_object *jo, *jv;
        vwire_config_t *vwire_config = vwire_pairs_mem + i;
        jo = JO(ja, i);

        VWIRE_JV("id");
        vwire_config->id = JV_I(jv);

        VWIRE_JV("port1");
        vwire_config->port1 = JV_I(jv);

        if (!VWIRE_VALID_PORTID(vwire_config->port1)) {
            ret = -1;
            goto done;
        }

        VWIRE_JV("port2");
        vwire_config->port2 = JV_I(jv);

        if (!VWIRE_VALID_PORTID(vwire_config->port2)) {
            ret = -1;
            goto done;
        }

        printf("vwire pair %u port1 %u port2 %u\n",
            vwire_config->id, vwire_config->port1, vwire_config->port2);

        interface_config->vwire_pair_num ++;
    }

    #undef VWIRE_JV

done:
    if (jr) JR_FREE(jr);

    if (ret) {
        if (vwire_pairs_mem) {
            free(vwire_pairs_mem);
            interface_config->vwire_pair_num = 0;
        }
    } else {
        interface_config->vwire_pairs = vwire_pairs_mem;
    }

    return ret;
}

int vwire_init(void *config)
{
    config_t *c = config;
    interface_config_t *interface_config = (interface_config_t *)c->interface_config;
    
    int ret = vwire_json_load(interface_config);
    if (ret) {
        rte_exit(EXIT_FAILURE, "vwire json load error");
        return ret; 
    }

    return 0;
}

int vwire_pair(void *config, uint16_t port_id)
{
    config_t *c = config;
    interface_config_t *interface_config = c->interface_config;
    vwire_config_t *vwire_config;
    int i;

    for (i = 0; i < interface_config->vwire_pair_num; i++) {
        vwire_config = (vwire_config_t *)interface_config->vwire_pairs + i;
        if (vwire_config) {
            if (port_id == vwire_config->port1) {
                return vwire_config->port2;
            }

            if (port_id == vwire_config->port2) {
                return vwire_config->port1;
            }
        }
    }

    return -1;
}

// file format utf-8
// ident using space