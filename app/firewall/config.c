#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include "config.h"
#include "module.h"

/** Hold two copy of configuration and initilize from _A
 * */
config_t config_A = {
    .pktmbuf_pool = NULL,
    .cli_def = NULL,
    .cli_show = NULL,
    .cli_sockfd = 0,
    .itf_cfg = NULL,
    .acl_ctx = NULL,
    .promiscuous = 1,
    .worker_num = 0,
    .port_num = 0,
    .mgt_core = -1,
    .rx_core = -1,
    .tx_core = -1,
    .rtx_core = -1,
    .rtx_worker_core = -1,
    .rx_queues = {0},
    .tx_queues = {{0}, {0}, {0}, {0}, {0}, {0}, {0}, {0}},
    .reload_mark = 0,
    .switch_mark = 0,
};

config_t config_B;

/** Indicator for config switch, when all workers' indicator equal to
 * the global indicator, a config switch process finished
 * */
int config_I = 0;
int _config_I[MAX_WORKER_NUM] = {-1, -1, -1, -1, -1, -1, -1, -1};

int config_reload(config_t *c)
{
    config_t *_c = (c == &config_A) ? &config_B : &config_A;
    
    memcpy(_c, c, sizeof(config_t));
    modules_conf(_c);
    _c->reload_mark = 0;
    _c->switch_mark = 0;
    config_I = (config_I <= 0) ? 1 : config_I + 1;
    return 1;
}

config_t *config_switch(config_t *c, int lcore_id)
{
    if (lcore_id == -1) {
        /** Management must wait all workers' config switch finished
         * */
        int i;
        while (1) {
            usleep(50000);
            
            for (i = 0; i < MAX_WORKER_NUM; i++) {
                if (_config_I[i] == -1) continue;
                if (_config_I[i] != config_I) break;
            }

            if (i == MAX_WORKER_NUM) {
                c->switch_mark = 0;
                break;
            }
        }
    } else {
        /** Each worker switch config immediately */
        _config_I[lcore_id] = (_config_I[lcore_id] <= 0) ? 1 : _config_I[lcore_id] + 1;
    }

    return (c == &config_A) ? &config_B : &config_A;
}

// file format utf-8
// ident using space