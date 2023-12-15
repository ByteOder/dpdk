#include <stdio.h>
#include <unistd.h>

#include <rte_lcore.h>
#include <rte_ring.h>
#include <rte_mbuf.h>

#include "worker.h"
#include "config.h"
#include "module.h"
#include "packet.h"

/**
 * mbuf flow between RX, WORKER, TX:
 * 
 *             RX               WORKER             TX
 *   ^---queue1----------v                ^--> port1-queue1
 * port1           >----->----> queue1----v
 *   v---queue2----+--------v          >--+--> port1-queue2
 *                 |        |          |  |
 *   ^---queue1----^        |          |  v--> port2-queue1
 * port2                    >-> queue2-|
 *   v---queue2-------------^          >-----> port2-queue2
 * ...
 * */

int worker_init(config_t *config)
{
    printf("WORKER init ...\n");
    char qname[128];
    int i, j;
    
    for (i = 0; i < config->worker_num; i++) {
        memset(qname, 0, 128);
        sprintf(qname, "%s-%d", "worker-rx-queue", i);

        config->rx_queues[i] = rte_ring_create(qname, 1024 * config->port_num, rte_socket_id(), 0);
        if (!config->rx_queues[i]) {
            goto error;
        }
    }

    for (i = 0; i < config->port_num; i++) {
        for (j = 0; j < config->worker_num; j++) {
            memset(qname, 0, 128);
            sprintf(qname, "%s-%d-%d", "worker-tx-queue", i, j);

            config->tx_queues[i][j] = rte_ring_create(qname, 1024, rte_socket_id(), 0);
            if (!config->tx_queues[i][j]) {
                goto error;
            }
        }
    }

    return 0;

error:
    for (i = 0; i < config->worker_num; i++) {
        if (config->rx_queues[i]) {
            rte_ring_free(config->rx_queues[i]);
        }
    }

    for (i = 0; i < config->port_num; i++) {
        for (j = 0; j < config->worker_num; j++) {
            if (config->tx_queues[i][j]) {
                rte_ring_free(config->tx_queues[i][j]);
            }
        }
    }

    return -1;
}

/** 
 * recv pkts then push to worker's rx queue 
 * */
int RX(__rte_unused config_t *config)
{
    modules_proc(NULL, MOD_HOOK_RECV);
    return 0;
}

/**
 * pop pkts from worker's tx queue and send
 * */
int TX(__rte_unused config_t *config)
{
    modules_proc(NULL, MOD_HOOK_SEND);
    return 0;
}

/**
 * do both RX() and TX() in one lcore
 * */
int RTX(config_t *config)
{
    RX(config);
    TX(config);
    return 0;
}

/**
 * 1、 pop mbuf from worker's rx queue
 * 2、 handle the mbuf
 * 3、 push the handled mbuf to worker's tx queue
 * */
int WORKER(config_t *config)
{
    struct rte_mbuf *mbuf;
    packet_t *p;
    int ret, hook, portid, queueid;

    queueid = rte_lcore_id() / config->worker_num;
    ret = rte_ring_dequeue(config->rx_queues[queueid], (void **)&mbuf);
    if (ret || !mbuf) {
        return 0;
    }

    for (hook = MOD_HOOK_INGRESS; hook <= MOD_HOOK_EGRESS; hook ++)
        modules_proc(mbuf, hook);

    p = rte_mbuf_to_priv(mbuf);
    if (!p) {
        rte_pktmbuf_free(mbuf);
        return -1;
    }
    portid = p->out_port;

    ret = rte_ring_enqueue(config->tx_queues[portid][queueid], mbuf);
    if (ret) {
        rte_pktmbuf_free(mbuf);
        return -1;
    }

    return 0;
}

/**
 * do all in one lcore
 * */
int RTX_WORKER(config_t *config)
{
    RX(config);
    WORKER(config);
    TX(config);
    return 0;
}

// file-format utf-8
// ident using space