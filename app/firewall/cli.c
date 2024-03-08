#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <errno.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <sys/select.h>

#include "config.h"
#include "cli.h"

#define CLI_PORT 8000

unsigned int cli_regular_count;
unsigned int cli_regular_debug;

struct cli_def *m_cli_def;
int m_cli_sockfd;

static int 
cli_regular_callback(struct cli_def *cli) 
{
    cli_regular_count++;
    if (cli_regular_debug) {
        cli_print(cli, "Regular callback - %u times so far", cli_regular_count);
        cli_reprompt(cli);
    }
    return CLI_OK;
}

static int 
cli_check_auth(const char *username, const char *password) 
{
    if (strcasecmp(username, "alan") != 0) return CLI_ERROR;
    if (strcasecmp(password, "alan") != 0) return CLI_ERROR;
    return CLI_OK;
}

static int
cli_check_enable(const char *password) 
{
    return !strcasecmp(password, "superman");
}

static int
cli_idle_timeout(struct cli_def *cli) 
{
    cli_print(cli, "Custom idle timeout");
    return CLI_QUIT;
}

int _cli_init(void *cfg)
{
    config_t *_cfg = (config_t *)cfg;
    struct sockaddr_in addr;
    int on = 1;
    const char *banner = 
    "=====================================================================\n"
    "      _____ .__                                 .__   .__\n"   
    "    _/ ____\\|__|_______   ____ __  _  _______   |  |  |  |\n"  
    "    \\   __\\ |  |\\_  __ \\_/ __ \\ \\/ \\/ /\\__  \\  |  |  |  |\n"  
    "     |  |   |  | |  | \\/\\  ___/ \\     /  / __ \\_|  |__|  |__\n"
    "     |__|   |__| |__|    \\___  > \\/\\_/  (____  /|____/|____/\n"
    "                             \\/              \\/\n"
    "=====================================================================\n";

    if (m_cli_def || m_cli_sockfd) {
        return -1;
    }

    m_cli_def = cli_init();
    cli_set_banner(m_cli_def, banner);
    cli_set_hostname(m_cli_def, "sys");
    cli_telnet_protocol(m_cli_def, 1);
    cli_regular(m_cli_def, cli_regular_callback);
    cli_regular_interval(m_cli_def, 5);
    cli_set_idle_timeout_callback(m_cli_def, 60, cli_idle_timeout);
    cli_set_auth_callback(m_cli_def, cli_check_auth);
    cli_set_enable_callback(m_cli_def, cli_check_enable);

    _cfg->cli_def = m_cli_def;

    if ((m_cli_sockfd = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
        perror("socket");
        return -1;
    }

    if (setsockopt(m_cli_sockfd, SOL_SOCKET, SO_REUSEADDR, &on, sizeof(on))) {
        perror("setsockopt");
        return -1;
    }

    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = htonl(INADDR_ANY);
    addr.sin_port = htons(CLI_PORT);
    if (bind(m_cli_sockfd, (struct sockaddr *)&addr, sizeof(addr)) < 0) {
        perror("bind");
        return -1;
    }

    if (listen(m_cli_sockfd, 50) < 0) {
        perror("listen");
        return -1;
    }

    return 0;
}

int _cli_run(void)
{
    struct timeval timeout;
    fd_set fds;
    int r, x;

    timeout.tv_sec = 10;
    timeout.tv_usec = 0;
    FD_ZERO(&fds);
    FD_SET(m_cli_sockfd, &fds);

    r = select(m_cli_sockfd + 1, &fds, NULL, NULL, &timeout);
    if (r == -1) {
        return -1;
    }

    if (r == 0) {
        return -1;
    }

    x = accept(m_cli_sockfd, NULL, 0);
    if (x > 0) {
        cli_loop(m_cli_def, x);
        close(x);
    }

    return 0;
}

// file format utf-8
// ident using space