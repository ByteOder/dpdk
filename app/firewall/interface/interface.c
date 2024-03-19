#include <rte_lcore.h>
#include <rte_ethdev.h>
#include <rte_ring.h>
#include <rte_log.h>

#include "../config.h"
#include "../module.h"
#include "../packet.h"
#include "../json.h"

#include "interface.h"
#include "vwire.h"

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
    .id = MOD_ID_INTERFACE,
    .enabled = true,
    .log = true,
    .logf = "/opt/firewall/log/interface.log",
    .init = interface_init,
    .proc = interface_proc,
    .priv = NULL
};

static int
interface_json_load(config_t *config)
{
    interface_config_t *itfc = config->interface_config;
    json_object *jr = NULL, *ja;
    int i, interface_num;
    int ret = 0;

    jr = JR(CONFIG_PATH, "interface.json");
    if (!jr) {
        printf("get json string failed\n");
        return -1;
    }

    interface_num = JA(jr, "ports", &ja);
    if (interface_num == -1) {
        printf("no ports found\n");
        ret = -1;
        goto done;
    }

    #define INTF_JV(item) \
        jv = JV(jo, item); \
        if (!jv) { \
            printf("parse %s failed\n", item); \
            ret = -1; \
            goto done; \
        }

    for (i = 0; i < interface_num; i++) {
        json_object *jo, *jv;
        port_config_t *port_config = &itfc->ports[i];
        jo = JO(ja, i);

        INTF_JV("id");
        port_config->id = JV_I(jv);

        INTF_JV("type");
        port_config->type = JV_I(jv);

        INTF_JV("bus");
        sprintf(port_config->bus, "%s", JV_S(jv));

        INTF_JV("mac")
        sprintf(port_config->mac, "%s", JV_S(jv));

        printf("port %u type %u bus %s mac %s\n",
            port_config->id, port_config->type, port_config->bus, port_config->mac);

        itfc->port_num ++;
    }

    #undef INTF_JV

done:
    if (jr) JR_FREE(jr);
    return ret;
}

int interface_init(void *config)
{
    config_t *c = config;
    struct rte_eth_dev_info dev_info;
    uint16_t portid, rx_queues, tx_queues, i;
    uint16_t nb_rx_desc = 1024;
    uint16_t nb_tx_desc = 1024;
    int ret;

    if (interface.log) {
        if (rte_log_init(interface.logf, MOD_ID_INTERFACE, RTE_LOG_ERR)) {
            printf("interface init rte log failed\n");
            return -1;
        }
    }

    c->interface_config = malloc(sizeof(interface_config_t));
    if (!c->interface_config) {
        printf("alloc interface config failed\n");
        return -1;
    }

    if (interface_json_load(c)) {
        printf("interface json load failed\n");
        return -1;
    }

    if (vwire_init(c)) {
        printf("vwire init failed\n");
        return -1;
    }

    rx_queues = tx_queues = 0;

    RTE_ETH_FOREACH_DEV(portid) {
        ret = rte_eth_dev_info_get(portid, &dev_info);
        if (ret) {
            printf("rte eth dev info get failed\n");
            return -1;
        }

        if (dev_info.max_rx_queues > c->worker_num) {
            rx_queues = c->worker_num;
        } else {
            rx_queues = dev_info.max_rx_queues;
        }

        if (dev_info.max_tx_queues > c->worker_num) {
            tx_queues = c->worker_num;
        } else {
            tx_queues = dev_info.max_tx_queues;
        }

        ret = rte_eth_dev_configure(portid, rx_queues, tx_queues, &g_port_conf);
        if (ret < 0) {
            printf("rte eth dev configure failed errcode %d id %u\n", ret, portid);
            return -1;
        }

        uint16_t rxds = nb_rx_desc * rx_queues;
        uint16_t txds = nb_tx_desc * tx_queues;

        ret = rte_eth_dev_adjust_nb_rx_tx_desc(portid, &rxds, &txds);
        if (ret < 0) {
            printf("rte eth dev adjust nb rx tx desc failed errcode %d port=%u\n", ret, portid);
            return -1;
        }

        for (i = 0; i < rx_queues; i++) {
            ret = rte_eth_rx_queue_setup(portid, i, nb_rx_desc, rte_eth_dev_socket_id(portid),
                &dev_info.default_rxconf, c->pktmbuf_pool);
            if (ret < 0) {
                printf("rte eth rx queue setup failed errcode %d port %u\n", ret, portid);
                return -1;
            }
        }

        for (i = 0; i < tx_queues; i++) {
            ret = rte_eth_tx_queue_setup(portid, i, nb_tx_desc, rte_eth_dev_socket_id(portid),
                &dev_info.default_txconf);
            if (ret < 0) {
                printf("rte eth tx queue setup failed errcode %d port=%u\n", ret, portid);
                return -1;
            }
        }

        ret = rte_eth_dev_set_ptypes(portid, RTE_PTYPE_UNKNOWN, NULL, 0);
        if (ret < 0) {
            printf("rte eth dev set ptypes failed\n");
            return -1;
        }

        ret = rte_eth_dev_start(portid);
        if (ret < 0) {
            printf("rte eth dev start failed errcode %d port %u\n", ret, portid);
            return -1;
        }

        if (c->promiscuous) {
            ret = rte_eth_promiscuous_enable(portid);
            if (ret != 0) {
                printf("rte eth promiscuous enable failed errcode %s port %u\n", rte_strerror(-ret), portid);
                return -1;
            }
        }
    }

    return 0;
}

static int
interface_proc_recv(config_t *config)
{
    struct rte_mbuf *pkts_burst[MAX_PKT_BURST];
    packet_t *p;
    int i, nb_rx, portid, queueid;

    RTE_ETH_FOREACH_DEV(portid) {
        for (queueid = 0; queueid < config->worker_num; queueid ++) {
            nb_rx = rte_eth_rx_burst(portid, queueid, pkts_burst, MAX_PKT_BURST);
            if (nb_rx) {
                rte_log_f(RTE_LOG_DEBUG, MOD_ID_INTERFACE, "\nrecv %d pkt from %d-%d\n", nb_rx, portid, queueid);

                for (i = 0; i < nb_rx; i++) {
                    p = rte_mbuf_to_priv(pkts_burst[i]);
                    if (p) {
                        p->iport = portid;
                    }
                }

                /**
                 * enqueue must sucess.
                 * */
                while (!rte_ring_enqueue_bulk(config->rx_queues[queueid], (void *const *)pkts_burst, nb_rx, NULL)) {;};
                rte_log_f(RTE_LOG_DEBUG, MOD_ID_INTERFACE, "enqueue worker rx queue %d\n", queueid);
            }
        }
    }

    return 0;
}

static int
interface_proc_prerouting(config_t *config, struct rte_mbuf *mbuf)
{
    packet_t *p = rte_mbuf_to_priv(mbuf);
    interface_config_t *itfc = config->interface_config;
    uint16_t portid;

    if (!p) {
        rte_log_f(RTE_LOG_ERR, MOD_ID_INTERFACE, "drop pkt which priv data is null\n");
        rte_pktmbuf_free(mbuf);
        return -1;
    }

    portid = p->iport;
    switch (itfc->ports[portid].type) {
        case PORT_TYPE_VWIRE:
            p->oport = vwire_pair(config, portid);
            break;
        default:
            break;
    }

    return 0;
}

static int
interface_proc_send(config_t *config)
{
    struct rte_mbuf *pkts_burst[MAX_PKT_BURST];
    int nb_tx, tx, portid, queueid;

    for (portid = 0; portid < config->port_num; portid ++) {
        for (queueid = 0; queueid < config->worker_num; queueid ++) {
            nb_tx = rte_ring_count(config->tx_queues[portid][queueid]);
            if (!nb_tx) {
                continue;
            }

            nb_tx = nb_tx > MAX_PKT_BURST ? MAX_PKT_BURST : nb_tx;
            nb_tx = rte_ring_dequeue_bulk(config->tx_queues[portid][queueid], (void **)pkts_burst, nb_tx, NULL);
            if (nb_tx) {
                rte_log_f(RTE_LOG_DEBUG, MOD_ID_INTERFACE, "dequeue %d pkt from worker tx queue %d-%d\n", nb_tx, portid, queueid);
                tx = rte_eth_tx_burst(portid, queueid, pkts_burst, nb_tx);
                if (tx < nb_tx) {
                    rte_log_f(RTE_LOG_ERR, MOD_ID_INTERFACE, "send failed %d pkts\n", nb_tx - tx);
                }
                rte_log_f(RTE_LOG_DEBUG, MOD_ID_INTERFACE, "send %d pkt to %d-%d\n", tx, portid, queueid);
            }
        }
    }

    return 0;
}


mod_ret_t interface_proc(void *config, struct rte_mbuf *mbuf, mod_hook_t hook)
{
    if (hook == MOD_HOOK_RECV) {
        interface_proc_recv(config);
    }

    if (hook == MOD_HOOK_PREROUTING) {
        interface_proc_prerouting(config, mbuf);
    }

    if (hook == MOD_HOOK_SEND) {
        interface_proc_send(config);
    }

    return MOD_RET_ACCEPT;
}

// file-format: utf-8
// ident using spaces