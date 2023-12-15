/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2010-2021 Intel Corporation
 */

#include <stdio.h>
#include <signal.h>

#include <rte_common.h>
#include <rte_log.h>
#include <rte_eal.h>
#include <rte_launch.h>
#include <rte_ethdev.h>

#include "config.h"
#include "module.h"
#include "worker.h"
#include "packet.h"
#include "interface/interface.h"

volatile bool force_quit;

config_t config = {
    .pktmbuf_pool = NULL,
    .promiscuous = 1,
    .worker_num = 0,
    .port_num = 0,
    .mgt_core = -1,
    .rx_core = -1,
    .tx_core = -1,
    .rtx_core= -1,
    .rtx_worker_core = -1,
    .rx_queues = {0},
    .tx_queues = {{0}, {0}, {0}, {0}, {0}, {0}, {0}, {0}},
};

static void
signal_handler(int signum)
{
    if (signum == SIGINT || signum == SIGTERM) {
        printf("\n\nSignal %d received, preparing to exit...\n",
                signum);
        force_quit = true;
    }
}

static int
main_loop(__rte_unused void *arg)
{
    int lcore_id = rte_lcore_id();
    config_t *c = (config_t *)arg;

    while (!force_quit) {
        if (lcore_id == c->rx_core) RX(c);
        else if (lcore_id == c->tx_core) TX(c);
        else if (lcore_id == c->rtx_core) RTX(c);
        else if (lcore_id == c->rtx_worker_core) RTX_WORKER(c);
        else WORKER(c);
    }

    return 0;
}

int main(int argc, char **argv)
{
    int lcore_id;
    int ret = 0;

    printf("==== firewall built at 2023 12 15 =====\n");

    /**
     * init EAL
     * */
    ret = rte_eal_init(argc, argv);
    if (ret < 0) {
        rte_exit(EXIT_FAILURE, "invalid eal parameters\n");
    }
    argc -= ret;
    argv += ret;

    /**
     * register signal handlers
     * */
    force_quit = false;
    signal(SIGINT, signal_handler);
    signal(SIGTERM, signal_handler);

    /**
     * init mbuf pool
     * */
    config.pktmbuf_pool = rte_pktmbuf_pool_create("mbuf_pool", 8192, 256, sizeof(packet_t), 128 + 2048, rte_socket_id());
    if (!config.pktmbuf_pool) {
        rte_exit(EXIT_FAILURE, "create pktmbuf pool failed\n");
    }
    
    /**
     * lcores check and alloc
     * 2 = 1 mgt-core + 1 rtx-worker-core
     * 4 = 1 mgt-core + 1 rtx-core + 2 worker-core
     * 8 = 1 mgt-core + 1 rx-core + 1 tx-core + 5 worker-core
     * ...
     * */

    unsigned int lcores = rte_lcore_count();
    if (lcores < 2 || lcores > 8 || lcores % 2) {
        rte_exit(EXIT_FAILURE, "lcores must be multiple of 2, support 2,4,8 for now\n");
    }

    config.mgt_core = rte_get_main_lcore();
    printf("mgt-core: %d\n", config.mgt_core);

    RTE_LCORE_FOREACH(lcore_id) {
        if (lcores == 2) {
            if (lcore_id != config.mgt_core) {
                printf("rtx-worker-core: %d\n", lcore_id);
                config.rtx_worker_core = lcore_id;
                config.worker_num ++;
            }
        }

        if (lcores == 4) {
            if (lcore_id != config.mgt_core) {
                if (config.rtx_core == -1) {
                    config.rtx_core = lcore_id;
                    printf("rtx-core: %d\n", config.rtx_core);
                } else {
                    printf("worker-core: %d\n", lcore_id);
                    config.worker_num ++;
                }
            }
        }

        if (lcores == 8) {
            if (lcore_id != config.mgt_core) {
                if (config.rx_core == -1) {
                    config.rx_core = lcore_id;
                    printf("rx-core: %d\n", config.rx_core);
                    continue;
                } 

                if (config.tx_core == -1) {
                    config.tx_core = lcore_id;
                    printf("tx-core: %d\n", config.tx_core);
                    continue;
                }

                printf("worker-core: %d\n", lcore_id);
                config.worker_num ++;
            }
        }
    }

    /**
     * port check
     * */
    config.port_num = rte_eth_dev_count_avail();
    if (config.port_num < 2) {
        rte_exit(EXIT_FAILURE, "need 2 port at least");
    }

    /**
     * init worker
     * 1. create rx and tx queue for each worker
     * */
    ret = worker_init(&config);
    if (ret) {
        rte_exit(EXIT_FAILURE, "worker init erorr\n");
    }

    /**
     * modules register and initialize
     * */
    modules_load();
    ret = modules_init(&config);
    if (ret) {
        rte_exit(EXIT_FAILURE, "module init erorr\n");
    }

    /**
     * start up worker main loop on each lcore, except mgt-core
     * */
    rte_eal_mp_remote_launch(main_loop, (void *)&config, SKIP_MAIN);

    ret = 0;

    /**
     * wait lcore
     * */
    rte_eal_mp_wait_lcore();

    /**
     * cleanup EAL
     * */
    rte_eal_cleanup();

    return ret;
}

// file-format utf-8
// ident using space