#include <arpa/inet.h>
#include <rte_acl.h>
#include <json-c/json.h>
#include <rte_ip.h>

#include "../config.h"
#include "../module.h"
#include "../packet.h"

#include "acl.h"

struct rte_acl_ctx *m_acl_ctx;

struct rte_acl_field_def acl_field_def[5] = {
    {
        .type = RTE_ACL_FIELD_TYPE_BITMASK,
        .size = sizeof(uint8_t),
        .field_index = 0,
        .input_index = 0,
        .offset = offsetof(ip4_tuple_t, proto),
    },
    {
        .type = RTE_ACL_FIELD_TYPE_MASK,
        .size = sizeof(uint32_t),
        .field_index = 1,
        .input_index = 1,
        .offset = offsetof(ip4_tuple_t, sip),
    },
    {
        .type = RTE_ACL_FIELD_TYPE_MASK,
        .size = sizeof (uint32_t),
        .field_index = 2,
        .input_index = 2,
        .offset = offsetof (ip4_tuple_t, dip),
    },
    /*
     * Next 2 fields (src & dst ports) form 4 consecutive bytes.
     * They share the same input index.
     */
    {
        .type = RTE_ACL_FIELD_TYPE_RANGE,
        .size = sizeof(uint16_t),
        .field_index = 3,
        .input_index = 3,
        .offset = offsetof(ip4_tuple_t, sp),
    },
    {
        .type = RTE_ACL_FIELD_TYPE_RANGE,
        .size = sizeof(uint16_t),
        .field_index = 4,
        .input_index = 3,
        .offset = offsetof(ip4_tuple_t, dp),
    },
};

struct rte_acl_config acl_cfg = {
    .num_categories = 1,
    .num_fields = RTE_DIM(acl_field_def),
    .max_size = 100000000,
};

RTE_ACL_RULE_DEF(acl_rule, RTE_DIM(acl_field_def));

struct rte_acl_param acl_param = {
    .name = "acl param",
    .socket_id = SOCKET_ID_ANY,
    .rule_size = RTE_ACL_RULE_SZ(RTE_DIM(acl_field_def)),
    .max_rule_num = MAX_ACL_RULE_NUM,
};

MODULE_DECLARE(acl) = {
    .name = "acl",
    .id = MOD_ID_ACL,
    .enabled = true,
    .log = true,
    .logf = "/opt/firewall/log/acl.log",
    .init = acl_init,
    .proc = acl_proc,
    .priv = NULL
};

static int
acl_rule_load(config_t *config)
{
    const char *file = "acl.json";
    char f[MAX_FILE_PATH] = {0};
    long fsize;
    char *js = NULL;
    char *p = NULL;
    json_object *jr = NULL, *ja;
    int i, rule_num;
    int ret = 0;

    sprintf(f, "%s/%s", CONFIG_PATH, file);

    FILE *fd = fopen(f, "r");
    if (!fd) {
        printf("open file %s failed\n", f);
        return -1;
    }

    fseek(fd, 0, SEEK_END);
    fsize = ftell(fd);
    fseek(fd, 0, SEEK_SET);
    
    js = (char *)malloc(fsize + 1);
    fread(js, 1, fsize, fd);
    js[fsize] = '\0';
    fclose(fd);

    jr = json_tokener_parse(js);
    if (!jr) {
        printf("can't find json root in file %s\n", f);
        free(js);
        return -1;
    }

    if (json_object_object_get_ex(jr, "rules", &ja) == 0) {
        printf("can't find rules array in file %s\n", f);
        free(js);
        json_object_put(jr);
        return -1;
    }

    rule_num = json_object_array_length(ja);
    struct acl_rule r[rule_num];

    for (i = 0; i < rule_num; i++) {
        json_object *jo, *jv;
        char ip[16], mask[4];

        jo = json_object_array_get_idx(ja, i);

        // id
        if (!json_object_object_get_ex(jo, "id", &jv)) {
            printf("acl rule id not found\n");
            ret = -1;
            goto done;
        }
        r[i].data.priority = atoi(json_object_get_string(jv));
        r[i].data.userdata = r[i].data.priority;

        // src ip and mask
        memset(ip, 0, 16);
        memset(mask, 0, 4);
        if (!json_object_object_get_ex(jo, "sip", &jv)) {
            printf("src address not found\n");
            ret = -1;
            goto done;
        }
        if ((p = strstr(json_object_get_string(jv), "/")) != NULL) {
            *p = ' ';
            sscanf(json_object_get_string(jv), "%s %s", ip, mask);
            r[i].field[1].value.u32 = ntohl(inet_addr(ip));
            r[i].field[1].mask_range.u32 = atoi(mask);
        } else {
            r[i].field[1].value.u32 = ntohl(inet_addr(json_object_get_string(jv)));
            r[i].field[1].mask_range.u32 = 32;
        }

        // destination ip and mask
        memset(ip, 0, 16);
        memset(mask, 0, 4);
        if (!json_object_object_get_ex(jo, "dip", &jv)) {
            printf("dst address not found\n");
            ret = -1;
            goto done;
        }

        if ((p = strstr(json_object_get_string(jv), "/")) != NULL) {
            *p = ' ';
            sscanf(json_object_get_string(jv), "%s %s", ip, mask);
            r[i].field[2].value.u32 = ntohl(inet_addr(ip));
            r[i].field[2].mask_range.u32 = atoi(mask);
        } else {
            r[i].field[2].value.u32 = ntohl(inet_addr(json_object_get_string(jv)));
            r[i].field[2].mask_range.u32 = 32;
        }

        // src port
        if (!json_object_object_get_ex(jo, "sp", &jv)) {
            printf("src port not found\n");
            ret = -1;
            goto done;
        }
        r[i].field[3].value.u16 = atoi(json_object_get_string(jv));
        r[i].field[3].mask_range.u16 = 0xffff;

        // dst port
        if (!json_object_object_get_ex(jo, "dp", &jv)) {
            printf("dst port not found\n");
            ret = -1;
            goto done;
        }
        r[i].field[4].value.u16 = atoi(json_object_get_string(jv));
        r[i].field[4].mask_range.u16 = 0xffff;

        if (!json_object_object_get_ex(jo, "proto", &jv)) {
            printf("proto not found\n");
            ret = -1;
            goto done;
        }
        r[i].field[0].value.u8 = atoi(json_object_get_string(jv));
        r[i].field[0].mask_range.u8 = 0xff;

        if (!json_object_object_get_ex(jo, "action", &jv)) {
            printf("action not found\n");
            ret = -1;
            goto done;
        }
        r[i].data.category_mask = 1;
        r[i].data.action = atoi(json_object_get_string(jv));

        printf("acl add rule id %u action %u proto %u sip %u smask %u dip %u dmask %u sp %u dp %u\n",
            r[i].data.priority,
            r[i].data.userdata,
            r[i].field[0].value.u8,
            r[i].field[1].value.u32,
            r[i].field[1].mask_range.u32,
            r[i].field[2].value.u32,
            r[i].field[2].mask_range.u32,
            r[i].field[3].value.u16,
            r[i].field[4].value.u16
        );
    }

    if (!m_acl_ctx) {
        m_acl_ctx = rte_acl_create(&acl_param);
        if (!m_acl_ctx) {
            printf("create acl ctx failed\n");
            ret = -1;
            goto done;
        }
    }

    if (rule_num) {
        if (rte_acl_add_rules(m_acl_ctx, (const struct rte_acl_rule *)r, rule_num)) {
            printf("add acl rules failed\n");
            ret = -1;
            goto done;
        }

        memcpy(acl_cfg.defs, acl_field_def, sizeof(struct rte_acl_field_def) * RTE_DIM(acl_field_def));
        if (rte_acl_build(m_acl_ctx, &acl_cfg)) {
            printf("build acl rules failed\n");
            ret = -1;
            goto done;
        }
    }

done:
    if (js) free(js);
    if (jr) json_object_put(jr);

    if (ret) {
        config->acl_ctx = NULL;
    } else {
        config->acl_ctx = m_acl_ctx;
    }

    return ret;
}

int acl_init(__rte_unused void* cfg)
{
    if (acl.log) {
        if (rte_log_init(acl.logf, MOD_ID_ACL, RTE_LOG_DEBUG)) {
            rte_exit(EXIT_FAILURE, "acl init rte log error");
        }
    }

    if (acl_rule_load(cfg)) {
        rte_exit(EXIT_FAILURE, "acl rule load error");
    }

    acl_list();

    return 0;
}

static mod_ret_t 
acl_proc_ingress(struct rte_mbuf *mbuf)
{
    rte_log_f(RTE_LOG_DEBUG, MOD_ID_ACL, "== acl proc ingress\n");

    struct rte_acl_rule_data *data;
    packet_t *p;
    ip4_tuple_t *k;
    uint32_t r;
    int ret;

    p = rte_mbuf_to_priv(mbuf);
    if (!p) {
        goto done;
    }

    k = &p->tuple.v4;

    ret = rte_acl_classify(m_acl_ctx, (const unsigned char **)&k, &r, 1, 1);
    if (ret) {
        goto done;
    }

    rte_log_f(RTE_LOG_DEBUG, MOD_ID_ACL, "packet proto %u sip %u dip %u sp %u dp %u\n",
        k->proto, k->sip, k->dip, k->sp, k->dp);

    if (!r){
        rte_log_f(RTE_LOG_DEBUG, MOD_ID_ACL, "no acl rule match\n");
        goto done;
    }

    data = rte_acl_rule_data(m_acl_ctx, r);
    if (!data) {
        rte_log_f(RTE_LOG_DEBUG, MOD_ID_ACL, "illegal acl rule id\n");
        goto done;
    }

    rte_log_f(RTE_LOG_DEBUG, MOD_ID_ACL, "match acl id %u action %u\n", r, data->action);

    if (data->action == ACL_ACTION_DENY) {
        rte_pktmbuf_free(mbuf);
        rte_log_f(RTE_LOG_DEBUG, MOD_ID_ACL, "acl deny drop packet\n");
        return MOD_RET_STOLEN;
    }

done:
    return MOD_RET_ACCEPT;
}

mod_ret_t acl_proc(struct rte_mbuf *mbuf, mod_hook_t hook)
{
    if (hook == MOD_HOOK_INGRESS) {
        return acl_proc_ingress(mbuf);
    }

    return MOD_RET_ACCEPT;
}

void acl_list(void)
{
    rte_acl_dump(m_acl_ctx);
}

// file format utf-8
// ident using space