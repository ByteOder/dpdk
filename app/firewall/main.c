/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2010-2021 Intel Corporation
 */

#include <stdio.h>

#include <rte_common.h>
#include <rte_log.h>
#include <rte_eal.h>
#include <rte_launch.h>

#include "module.h"

static int
main_loop(__rte_unused void *arg)
{
    return 0;
}

int main(int argc, char **argv)
{
    int ret = 0;
    void *config = NULL;

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
     * load modules
     * */
    modules_load();

    /**
     * init modules
     * */
    modules_init(&config);
    
    /**
     * launch per-lcore init
     * */
    rte_eal_mp_remote_launch(main_loop, NULL, CALL_MAIN);

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
