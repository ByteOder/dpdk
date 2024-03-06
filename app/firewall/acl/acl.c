#include <arpa/inet.h>
#include <rte_acl.h>
#include <rte_ip.h>

#include "../config.h"
#include "../module.h"
#include "../packet.h"
#include "../json.h"
#include "../cli.h"

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
    char *js = NULL;
    char *p = NULL;
    json_object *jr = NULL, *ja;
    int i, rule_num;
    int ret = 0;

    js = JS(CONFIG_PATH, "acl.json");
    if (!js) {
        return -1;
    }

    jr = JR(js);
    if (!jr) {
        JS_FREE(js);
        return -1;
    }

    rule_num = JA(jr, "rules", &ja);
    if (rule_num == -1) {
        JS_FREE(js);
        JR_FREE(jr);
        return -1;
    }

    struct acl_rule r[rule_num];

    #define ACL_JV(item) \
        jv = JV(jo, item); \
        if (!jv) { \
            ret = -1; \
            goto done; \
        }

    for (i = 0; i < rule_num; i++) {
        json_object *jo, *jv;
        char ip[16], mask[4];

        jo = JO(ja, i);
        
        ACL_JV("enabled");
        if (!JV_I(jv)) {
            continue;
        }

        ACL_JV("id");
        r[i].data.priority = JV_I(jv);
        r[i].data.userdata = r[i].data.priority;

        memset(ip, 0, 16);
        memset(mask, 0, 4);
        
        ACL_JV("sip");
        if ((p = strstr(JV_S(jv), "/")) != NULL) {
            *p = ' ';
            sscanf(JV_S(jv), "%s %s", ip, mask);
            r[i].field[1].value.u32 = ntohl(inet_addr(ip));
            r[i].field[1].mask_range.u32 = atoi(mask);
        } else {
            r[i].field[1].value.u32 = ntohl(inet_addr(JV_S(jv)));
            r[i].field[1].mask_range.u32 = 32;
        }

        memset(ip, 0, 16);
        memset(mask, 0, 4);

        ACL_JV("dip");
        if ((p = strstr(JV_S(jv), "/")) != NULL) {
            *p = ' ';
            sscanf(JV_S(jv), "%s %s", ip, mask);
            r[i].field[2].value.u32 = ntohl(inet_addr(ip));
            r[i].field[2].mask_range.u32 = atoi(mask);
        } else {
            r[i].field[2].value.u32 = ntohl(inet_addr(JV_S(jv)));
            r[i].field[2].mask_range.u32 = 32;
        }

        ACL_JV("sp");
        r[i].field[3].value.u16 = JV_I(jv);
        r[i].field[3].mask_range.u16 = 0xffff;

        ACL_JV("dp");
        r[i].field[4].value.u16 = JV_I(jv);
        r[i].field[4].mask_range.u16 = 0xffff;

        ACL_JV("proto");
        r[i].field[0].value.u8 = JV_I(jv);
        r[i].field[0].mask_range.u8 = 0xff;

        ACL_JV("action");
        r[i].data.category_mask = 1;
        r[i].data.action = JV_I(jv);

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

    #undef ACL_JV

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
    if (js) JS_FREE(js);
    if (jr) JR_FREE(jr);

    if (ret) {
        config->acl_ctx = NULL;
    } else {
        config->acl_ctx = m_acl_ctx;
    }

    return ret;
}

static int 
acl_show(struct cli_def *cli, const char *command, char *argv[], int argc) 
{
    cli_print(cli, "command %s argv %s argc %d", command, argv[0], argc);

    char *js = NULL;
    json_object *jr = NULL, *ja;
    int i, rule_num;
    int ret = 0;
    
    js = JS(CONFIG_PATH, "acl.json");
    if (!js) {
        return -1;
    }

    jr = JR(js);
    if (!jr) {
        JS_FREE(js);
        return -1;
    }

    rule_num = JA(jr, "rules", &ja);
    if (rule_num == -1) {
        JS_FREE(js);
        JR_FREE(jr);
        return -1;
    }

    #define ACL_PRINT(item) \
        jv = JV(jo, item); \
        cli_print(cli, "%s: %s", item, JV_S(jv));

    for (i = 0; i < rule_num; i++) {
        json_object *jo, *jv;
        jo = JO(ja, i);
        ACL_PRINT("id");
        ACL_PRINT("enabled");
        ACL_PRINT("sip");
        ACL_PRINT("dip");
        ACL_PRINT("sp");
        ACL_PRINT("dp");
        ACL_PRINT("proto");
        ACL_PRINT("action");
        cli_print(cli, "%s", "");
    }

    #undef ACL_PRINT

    if (js) JS_FREE(js);
    if (jr) JR_FREE(jr);
    return ret;
}

static int 
acl_dump(struct cli_def *cli, const char *command, char *argv[], int argc) 
{
    char buffer[2048] = {0};
    _rte_acl_dump(m_acl_ctx, buffer);
    cli_print(cli, "command %s argv %s argc %d", command, argv[0], argc);
    cli_print(cli, "%s", buffer);
    return 0;
}

static void
acl_cli_register(config_t *config)
{
    struct cli_def *cli_def;
    struct cli_command *c;

    if (!config) {
        return;
    }

    cli_def = config->cli_def;
    if (!cli_def) {
        return;
    }

    c = cli_register_command(cli_def, NULL, "acl", NULL, PRIVILEGE_UNPRIVILEGED, MODE_EXEC, "access control list");
    cli_register_command(cli_def, c, "dump", acl_dump, PRIVILEGE_UNPRIVILEGED, MODE_EXEC, "dump acl context");
    cli_register_command(cli_def, c, "show", acl_show, PRIVILEGE_UNPRIVILEGED, MODE_EXEC, "show acl config");
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

    acl_cli_register(cfg);

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

// file format utf-8
// ident using space