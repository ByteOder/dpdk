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

#include "module.h"
#include "interface/interface.h"

volatile bool force_quit;

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
    unsigned lcore_id = rte_lcore_id();
    struct rte_mbuf *m = NULL;

    while (!force_quit) {
        printf("lcore %u, this is main loop, over!\n", lcore_id);
        modules_proc(m, MOD_HOOK_RECV);
        modules_proc(m, MOD_HOOK_SEND);
        sleep(5);
    }

    return 0;
}

int main(int argc, char **argv)
{
    int ret = 0;
    void *config = NULL;
    struct rte_eth_dev_info dev_info;
    uint16_t portid;


    printf("==== firewall version f3f3a356c82b333066156878a5ca59ae8264d132 date 2023 11 29 =====\n");

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
     * port info
     * */
    RTE_ETH_FOREACH_DEV(portid) {
        ret = rte_eth_dev_info_get(portid, &dev_info);
        if (ret) {
            rte_exit(EXIT_FAILURE, "get dev info failed");
        }

        printf("device name %s driver name %s max_rx_queues %u max_tx_queues %u\n",
            dev_info.device->name, dev_info.driver_name, dev_info.max_rx_queues, dev_info.max_tx_queues);
    }

    /**
     * load modules
     * */
    modules_load();

    /**
     * init modules
     * */
    modules_init(&config);
    
    /**
     * lcore count check, at least need 2 cores, 1 for management plane and others
     * for data plane
     * */
    unsigned int lcores = rte_lcore_count();
    if (lcores < 2) {
        rte_exit(EXIT_FAILURE, "need 2 lcores at least\n");
    }

    rte_eal_mp_remote_launch(main_loop, NULL, SKIP_MAIN);

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
