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
#include <rte_per_lcore.h>

#include "config.h"
#include "module.h"
#include "worker.h"
#include "packet.h"
#include "cli.h"
#include "interface/interface.h"

extern config_t config_A, config_B;
extern int _config_I[MAX_WORKER_NUM], config_I;

config_t *m_cfg = &config_A;
__thread config_t *_m_cfg;

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
cli_show_conf(struct cli_def *cli, const char *command, char *argv[], int argc) 
{
    CLI_PRINT(cli, "command %s argv[0] %s argc %d", command, argv[0], argc);

    config_t *c = cli_get_context(cli);
    CLI_PRINT(cli, "working with config %s\n", (c == &config_A) ? "A" : "B");
    CLI_PRINT(cli, "indicator [%d %d %d %d %d %d %d %d] %d\n", 
        _config_I[0], _config_I[1], _config_I[2], _config_I[3],
        _config_I[4], _config_I[5], _config_I[6], _config_I[7], config_I);
    return 0;
}

static int
main_loop(__rte_unused void *arg)
{
    int lcore_id = rte_lcore_id();
    _m_cfg = (config_t *)arg;
    _config_I[lcore_id] = 0;

    while (!force_quit) {
        if (_m_cfg->switch_mark) {
            _m_cfg = config_switch(_m_cfg, lcore_id);
        }

        if (lcore_id == _m_cfg->rx_core) RX(_m_cfg);
        else if (lcore_id == _m_cfg->tx_core) TX(_m_cfg);
        else if (lcore_id == _m_cfg->rtx_core) RTX(_m_cfg);
        else if (lcore_id == _m_cfg->rtx_worker_core) RTX_WORKER(_m_cfg);
        else WORKER(_m_cfg);
    }

    return 0;
}

static void
mgmt_loop(__rte_unused config_t *c)
{
    config_t *_c = c;

    while (!force_quit) {
        /** When a reload mark set, a config switch process started, included steps below:
         * 1. reload config into 'free' one, eg. working with _A now then reload witch _B
         * 2. tell worker to switch config. eg. working with _A now then switch to _B
         * 3. wait all worker switch config done, switch config of it's own(sync by config indicator)
         * 4. update user context of cli for exist terminal(it is safe cause cli def never changed)
         * */
        if (_c->reload_mark) {
            if (config_reload(_c)) {
                _c->reload_mark = 0;
                _c->switch_mark = 1;
                _c = config_switch(_c, -1);
                cli_set_context(_c->cli_def, _c);
            }
        }
        _cli_run(_c);
    }
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
    m_cfg->pktmbuf_pool = rte_pktmbuf_pool_create("mbuf_pool", 8192, 256, sizeof(packet_t), 128 + 2048, rte_socket_id());
    if (!m_cfg->pktmbuf_pool) {
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

    m_cfg->mgt_core = rte_get_main_lcore();
    printf("mgt-core: %d\n", m_cfg->mgt_core);

    RTE_LCORE_FOREACH(lcore_id) {
        if (lcores == 2) {
            if (lcore_id != m_cfg->mgt_core) {
                printf("rtx-worker-core: %d\n", lcore_id);
                m_cfg->rtx_worker_core = lcore_id;
                m_cfg->worker_num ++;
            }
        }

        if (lcores == 4) {
            if (lcore_id != m_cfg->mgt_core) {
                if (m_cfg->rtx_core == -1) {
                    m_cfg->rtx_core = lcore_id;
                    printf("rtx-core: %d\n", m_cfg->rtx_core);
                } else {
                    printf("worker-core: %d\n", lcore_id);
                    m_cfg->worker_num ++;
                }
            }
        }

        if (lcores == 8) {
            if (lcore_id != m_cfg->mgt_core) {
                if (m_cfg->rx_core == -1) {
                    m_cfg->rx_core = lcore_id;
                    printf("rx-core: %d\n", m_cfg->rx_core);
                    continue;
                } 

                if (m_cfg->tx_core == -1) {
                    m_cfg->tx_core = lcore_id;
                    printf("tx-core: %d\n", m_cfg->tx_core);
                    continue;
                }

                printf("worker-core: %d\n", lcore_id);
                m_cfg->worker_num ++;
            }
        }
    }

    /**
     * port check
     * */
    m_cfg->port_num = rte_eth_dev_count_avail();
    if (m_cfg->port_num < 2) {
        rte_exit(EXIT_FAILURE, "need 2 port at least");
    }

    /**
     * init worker
     * 1. create rx and tx queue for each worker
     * */
    ret = worker_init(m_cfg);
    if (ret) {
        rte_exit(EXIT_FAILURE, "worker init erorr\n");
    }

    /**
     * init command line
     * */
    ret = _cli_init(m_cfg);
    if (ret) {
        rte_exit(EXIT_FAILURE, "cli init erorr\n");
    }

    CLI_CMD_C(m_cfg->cli_def, m_cfg->cli_show, "config", cli_show_conf, "global configuration");

    /**
     * modules register and initialize
     * */
    modules_load();
    ret = modules_init(m_cfg);
    if (ret) {
        rte_exit(EXIT_FAILURE, "module init erorr\n");
    }

    /**
     * start up worker loop on each lcore, except mgt-core
     * */
    rte_eal_mp_remote_launch(main_loop, (void *)m_cfg, SKIP_MAIN);

    /**
     * start up management loop on mgt-core
     * */
    mgmt_loop(m_cfg);

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