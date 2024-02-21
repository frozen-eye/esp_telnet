#pragma once

#include "sdkconfig.h"

#include <libtelnet.h>

static const telnet_telopt_t default_telopts[] = {
  {TELNET_TELOPT_COMPRESS2, TELNET_WILL, TELNET_DO}, {TELNET_TELOPT_ZMP, TELNET_WILL, TELNET_DO},
  {TELNET_TELOPT_MSSP, TELNET_WILL, TELNET_DONT},    {TELNET_TELOPT_NEW_ENVIRON, TELNET_WILL, TELNET_DONT},
  {TELNET_TELOPT_TTYPE, TELNET_WILL, TELNET_DONT},   {-1, 0, 0}};

struct user_t {
  char* name;
  int sock;
  telnet_t* telnet;
  char linebuf[255];
  int linepos;
};

#ifdef __cplusplus
extern "C" {
#endif

struct telnet_server_config {
  int port;
  int stack_size;
  int task_priority;
  int task_core;
  int redirect_logs;
  int max_connections;
  const telnet_telopt_t *telnet_opts;
};

/**
 * @brief Telnet Server Default Configuration
 *
 */
#define TELNET_SERVER_DEFAULT_CONFIG                         \
{                                                            \
    .port = CONFIG_TELNET_SERVER_DEFAULT_PORT,               \
    .stack_size = CONFIG_TELNET_SERVER_STACK_SIZE,           \
    .task_priority = CONFIG_TELNET_SERVER_TASK_PRIORITY,     \
    .task_core = CONFIG_TELNET_SERVER_TASK_CORE,             \
    .redirect_logs = CONFIG_TELNET_SERVER_REDIRECT_LOGS,     \
    .max_connections = CONFIG_TELNET_SERVER_MAX_CONNECTIONS, \
    .telnet_opts = default_telopts,                          \
}

typedef struct telnet_server_config telnet_server_config_t;

esp_err_t telnet_server_create(telnet_server_config_t* config);

#ifdef __cplusplus
}
#endif
