/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2010-2021 Intel Corporation
 */

#include <stdio.h>
#include <signal.h>
#include <unistd.h>

#include <rte_common.h>
#include <rte_log.h>
#include <rte_eal.h>
#include <rte_launch.h>
#include <rte_ethdev.h>

#include "config.h"
#include "module.h"
#include "worker.h"
#include "interface/interface.h"

volatile bool force_quit;

config_t config = {
    .pktmbuf_pool = NULL,
    .promiscuous = 0,
    .worker_num = 0,
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
    int lcore_rtx = *(int *)arg;

    while (!force_quit) {
        printf("lcore %u is running, over!\n", lcore_id);
        
        if (lcore_rtx != -1) {
            if (lcore_rtx == lcore_id) {
                printf("RTX\n");
                RTX();
            } else {
                printf("WKR\n");
                WRK();
            }
        } else {
            printf("RTX_WKR\n");
            RTX_WRK();
        }

        sleep(5);
    }

    return 0;
}

int main(int argc, char **argv)
{
    int lcore_id;
    int lcore_rtx = -1;
    int lcore_mgt = -1;
    int ret = 0;

    printf("==== firewall buid at date 2023 12 01 =====\n");

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
    config.pktmbuf_pool = rte_pktmbuf_pool_create("mbuf_pool", 8192, 256, 0, 128 + 2048, rte_socket_id());
    if (!config.pktmbuf_pool) {
        rte_exit(EXIT_FAILURE, "create pktmbuf pool failed\n");
    }
    
    /**
     * lcore count check, at least need 2 cores, 1 for management plane and others
     * for data plane
     * */
    unsigned int lcores = rte_lcore_count();
    if (lcores < 2) {
        rte_exit(EXIT_FAILURE, "need 2 lcores at least\n");
    }

    lcore_mgt = rte_get_main_lcore();
    printf("mgt-core: %d\n", lcore_mgt);

    RTE_LCORE_FOREACH(lcore_id) {
        if (lcores == 2) {
            if (lcore_id != lcore_mgt) {
                printf("rtx-wkr-core: %d\n", lcore_id);
                config.worker_num ++;
            }
        }

        if (lcores > 2) {
            if (lcore_id != lcore_mgt) {
                if (lcore_rtx == -1) {
                    lcore_rtx = lcore_id;
                    printf("rtx-core: %d\n", lcore_rtx);
                } else {
                    printf("wkr-core: %d\n", lcore_id);
                    config.worker_num ++;
                }
            }
        }
    }

    modules_load();
    modules_init(&config);

    rte_eal_mp_remote_launch(main_loop, (void *)&lcore_rtx, SKIP_MAIN);

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