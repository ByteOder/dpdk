#include <rte_lcore.h>
#include <rte_ethdev.h>

#include <json-c/json.h>

#include "../config.h"
#include "../module.h"
#include "interface.h"

interface_config_t interface_config;

struct rte_eth_conf g_port_conf = {
    .rxmode = {
        .split_hdr_size = 0,
    },
    .txmode = {
        .mq_mode = RTE_ETH_MQ_TX_NONE,
    },
};

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

static int
interface_json_load(void)
{
    const char *file = "interface.json";
    char f[MAX_FILE_PATH] = {0};
    long fsize;
    char *js = NULL;
    json_object *jr = NULL, *ja;
    int i, interface_num;
    int ret = 0;

    sprintf(f, "%s/%s", CONFIG_PATH, file);

    FILE *fd = fopen(f, "r");
    if (!fd) {
        printf("open file %s failed\n", f);
        ret = -1;
        goto done;
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
        ret = -1;
        goto done;
    }

    if (json_object_object_get_ex(jr, "port", &ja) == 0) {
        printf("can't find port array in file %s\n", f);
        ret = -1;
        goto done;
    }

    interface_num = json_object_array_length(ja);
    for (i = 0; i < interface_num; i++) {
        json_object *jo, *jv;
        port_config_t *port_config = &interface_config.ports[i];
        jo = json_object_array_get_idx(ja, i);

        if (!json_object_object_get_ex(jo, "id", &jv)) {
            printf("port id not found\n");
            ret = -1;
            goto done;
        }
        port_config->id = atoi(json_object_get_string(jv));

        if (!json_object_object_get_ex(jo, "type", &jv)) {
            printf("port type not found\n");
            ret = -1;
            goto done;
        }
        port_config->type = atoi(json_object_get_string(jv));

        if (!json_object_object_get_ex(jo, "bus", &jv)) {
            printf("port bus not found\n");
            ret = -1;
            goto done;
        }
        sprintf(port_config->bus, "%s", json_object_get_string(jv));

        if (!json_object_object_get_ex(jo, "mac", &jv)) {
            printf("port mac not found\n");
            ret = -1;
            goto done;
        }
        sprintf(port_config->mac, "%s", json_object_get_string(jv));

        printf("port %u type %u bus %s mac %s\n",
            port_config->id, port_config->type, port_config->bus, port_config->mac);

        interface_config.port_num ++;
    }

done:
    if (js) free(js);
    if (jr) json_object_put(jr);
    return ret;
}

int interface_init(__rte_unused void* cfg)
{
    config_t *config = (config_t *)cfg;
    struct rte_eth_dev_info dev_info;
    uint16_t ports, portid, rx_queues, tx_queues, i;
    uint16_t nb_rx_desc = 1024;
    uint16_t nb_tx_desc = 1024;
    int ret;

    if (interface_json_load()) {
        rte_exit(EXIT_FAILURE, "interface json load error");
    }

    ports = rte_eth_dev_count_avail();
    if (ports < 2) {
        rte_exit(EXIT_FAILURE, "need 2 port at least");
    }

    rx_queues = tx_queues = 0;

    RTE_ETH_FOREACH_DEV(portid) {
        ret = rte_eth_dev_info_get(portid, &dev_info);
        if (ret) {
            rte_exit(EXIT_FAILURE, "get dev info failed");
        }

        if (dev_info.max_rx_queues > config->worker_num) {
            rx_queues = config->worker_num;
        } else {
            rx_queues = dev_info.max_rx_queues;
        }

        if (dev_info.max_tx_queues > config->worker_num) {
            tx_queues = config->worker_num;
        } else {
            tx_queues = dev_info.max_tx_queues;
        }

        ret = rte_eth_dev_configure(portid, rx_queues, tx_queues, &g_port_conf);
        if (ret < 0) {
            rte_exit(EXIT_FAILURE, "config eth dev failed errcode %d id %u\n",
                ret, portid);
        }

        uint16_t rxds = nb_rx_desc * rx_queues;
        uint16_t txds = nb_tx_desc * tx_queues;

        ret = rte_eth_dev_adjust_nb_rx_tx_desc(portid, &rxds, &txds);
        if (ret < 0) {
            rte_exit(EXIT_FAILURE, "setup rx tx desc failed errcode %d port=%u\n",
                ret, portid);
        }

        for (i = 0; i < rx_queues; i++) {
            ret = rte_eth_rx_queue_setup(portid, i, nb_rx_desc, rte_eth_dev_socket_id(portid),
                &dev_info.default_rxconf, config->pktmbuf_pool);
            if (ret < 0) {
                rte_exit(EXIT_FAILURE, "setup rx queue failed errcode %d port %u\n",
                      ret, portid);
            }
        }

        for (i = 0; i < tx_queues; i++) {
            ret = rte_eth_tx_queue_setup(portid, i, nb_tx_desc, rte_eth_dev_socket_id(portid),
                &dev_info.default_txconf);
            if (ret < 0) {
                rte_exit(EXIT_FAILURE, "setup tx queue failed errcode %d port=%u\n",
                    ret, portid);
            }
        }

        ret = rte_eth_dev_set_ptypes(portid, RTE_PTYPE_UNKNOWN, NULL, 0);
        if (ret < 0) {
            printf("Port %u, Failed to disable Ptype parsing\n",
                    portid);
        }

        ret = rte_eth_dev_start(portid);
        if (ret < 0) {
            rte_exit(EXIT_FAILURE, "rte eth dev start errcode %d port %u\n", ret, portid);
        }

        if (config->promiscuous) {
            ret = rte_eth_promiscuous_enable(portid);
            if (ret != 0) {
                rte_exit(EXIT_FAILURE,
                    "rte eth promiscuous enable errcode %s port %u\n", rte_strerror(-ret), portid);
            }
        }

        struct rte_ether_addr mac1, mac2;       
        memset(&mac1, 0, sizeof(struct rte_ether_addr));
        memset(&mac2, 0, sizeof(struct rte_ether_addr));
        
        if (rte_eth_macaddr_get(portid, &mac1)) {
                rte_exit(EXIT_FAILURE,
                    "rte eth macaddr get errcode %s port %u\n", rte_strerror(-ret), portid);
        }

        if (portid < interface_config.port_num) {
            ret = rte_ether_unformat_addr(interface_config.ports[portid].mac, &mac2);
            if (ret) {
                rte_exit(EXIT_FAILURE,
                    "rte eth unformat addr errorcode %s port %u\n", rte_strerror(-ret), portid);
            }

            if (!rte_is_same_ether_addr(&mac1, &mac2)) {
                rte_exit(EXIT_FAILURE,
                    "rte eth unformat addr errorcode %s port %u\n", rte_strerror(-ret), portid);
            }
        }
    }

    config->interface_config = &interface_config;

    return 0;
}

mod_ret_t interface_proc(__rte_unused struct rte_mbuf *mbuf, mod_hook_t hook)
{
    printf("interface proc\n");

    if (hook == MOD_HOOK_INGRESS) {
        printf("recv\n");
    }

    if (hook == MOD_HOOK_EGRESS) {
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