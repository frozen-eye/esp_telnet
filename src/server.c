#include "freertos/FreeRTOS.h"
#include "sdkconfig.h"

#define LINEBUFFER_SIZE 256
#define AWAIT_TIMEOUT   10

#include <esp_log.h>
#include <libtelnet.h>
#include <lwip/def.h>
#include <lwip/sockets.h>
#include <telnet/server.h>

static const char* TAG = "telnet";
static struct user_t users[CONFIG_TELNET_SERVER_MAX_CONNECTIONS + 1];

static void linebuffer_push(char* buffer, size_t size, int* linepos, char ch,
                            void (*cb)(const char* line, size_t overflow, void* ud), void* ud)
{
  /* CRLF -- line terminator */
  if (ch == '\n' && *linepos > 0 && buffer[*linepos - 1] == '\r') {
    /* NUL terminate (replaces \r in buffer), notify app, clear */
    buffer[*linepos - 1] = 0;
    cb(buffer, 0, ud);
    *linepos = 0;

    /* CRNUL -- just a CR */
  }
  else if (ch == 0 && *linepos > 0 && buffer[*linepos - 1] == '\r') {
    /* do nothing, the CR is already in the buffer */

    /* anything else (including technically invalid CR followed by
     * anything besides LF or NUL -- just buffer if we have room
     * \r
     */
  }
  else if (*linepos != (int)size) {
    buffer[(*linepos)++] = ch;

    /* buffer overflow */
  }
  else {
    /* terminate (NOTE: eats a byte), notify app, clear buffer */
    buffer[size - 1] = 0;
    cb(buffer, size - 1, ud);
    *linepos = 0;
  }
}

static void _message(const char* from, const char* msg)
{
  int i;
  for (i = 0; i != CONFIG_TELNET_SERVER_MAX_CONNECTIONS; ++i) {
    if (users[i].sock != -1 && users[i].name != 0 && strcmp(users[i].name, from) != 0) {
      telnet_printf(users[i].telnet, "%s: \"%s\"\n", from, msg);
    }
  }
}

static void _broadcast(const char* from, const char* msg)
{
  int i;
  for (i = 0; i != CONFIG_TELNET_SERVER_MAX_CONNECTIONS; ++i) {
    if (users[i].sock != -1) {
      telnet_printf(users[i].telnet, "%s: \"%s\"\n", from, msg);
    }
  }
}

static void _send(int sock, const char* buffer, size_t size)
{
  int rs;

  /* ignore on invalid socket */
  if (sock == -1)
    return;

  /* send data */
  while (size > 0) {
    if ((rs = send(sock, buffer, (int)size, 0)) == -1) {
      if (errno != EINTR && errno != ECONNRESET) {
        ESP_LOGW(TAG, "send() failed: %s", strerror(errno));
        return;
      }
      else {
        return;
      }
    }
    else if (rs == 0) {
      ESP_LOGE(TAG, "send() unexpectedly returned 0");
      return;
    }

    /* update pointer and size to see if we've got more to send */
    buffer += rs;
    size -= rs;
  }
}

/* process input line */
static void _online(const char* line, size_t overflow, void* ud)
{
  struct user_t* user = (struct user_t*)ud;
  int i;

  (void)overflow;

  /* if the user has no name, this is his "login" */
  if (user->name == 0) {
    /* must not be empty, must be at least 32 chars */
    if (strlen(line) == 0 || strlen(line) > 32) {
      telnet_printf(user->telnet, "Invalid name. Enter name: ");
      return;
    }

    /* must not already exist */
    for (i = 0; i != CONFIG_TELNET_SERVER_MAX_CONNECTIONS; ++i) {
      if (users[i].name != 0 && strcmp(users[i].name, line) == 0) {
        telnet_printf(user->telnet, "Name already in use. Enter name: ");
        return;
      }
    }

    /* keep name */
    user->name = strdup(line);
    telnet_printf(user->telnet, "Welcome, %s!\n", line);
    return;
  }

  /* execute a command, need to send to the system */
  // _message(user->name, line);
}

static void _input(struct user_t* user, const char* buffer, size_t size)
{
  unsigned int i;
  for (i = 0; user->sock != -1 && i != size; ++i) {
    linebuffer_push(user->linebuf, sizeof(user->linebuf), &user->linepos, (char)buffer[i], _online, user);
  }
}

static void _event_handler(telnet_t* telnet, telnet_event_t* ev, void* user_data)
{
  struct user_t* user = (struct user_t*)user_data;

  switch (ev->type) {
  /* data received */
  case TELNET_EV_DATA:
    _input(user, ev->data.buffer, ev->data.size);
    // telnet_negotiate(telnet, TELNET_WONT, TELNET_TELOPT_ECHO);
    // telnet_negotiate(telnet, TELNET_WILL, TELNET_TELOPT_ECHO);
    break;
  /* data must be sent */
  case TELNET_EV_SEND: _send(user->sock, ev->data.buffer, ev->data.size); break;
  /* enable compress2 if accepted by client */
  case TELNET_EV_DO:
    if (ev->neg.telopt == TELNET_TELOPT_COMPRESS2)
      telnet_begin_compress2(telnet);
    break;
  /* error */
  case TELNET_EV_ERROR:
    close(user->sock);
    user->sock = -1;
    if (user->name != 0) {
      _message(user->name, "** HAS HAD AN ERROR **");
      free(user->name);
      user->name = 0;
    }
    telnet_free(user->telnet);
    break;
  default:
    /* ignore */
    break;
  }
}

void telnet_task(void* arg)
{
  static char buffer[512];
  static struct sockaddr_in addr;
  static telnet_server_config_t config;

  // save the configuration
  memcpy(&config, arg, sizeof(telnet_server_config_t));

  int listen_sock;
  int client_sock;
  int rs;
  int i;
  socklen_t addrlen;
  struct pollfd pfd[config.max_connections + 1];

  /* initialize data structures */
  memset(&pfd, 0, sizeof(pfd));
  memset(users, 0, sizeof(users));
  for (i = 0; i != config.max_connections; ++i) {
    users[i].sock = -1;
  }

  /* create listening socket */
  if ((listen_sock = socket(AF_INET, SOCK_STREAM, 0)) == -1) {
    ESP_LOGE(TAG, "socket() failed: %s", strerror(errno));
    return;
  }

  /* reuse address option */
  rs = 1;
  setsockopt(listen_sock, SOL_SOCKET, SO_REUSEADDR, (char*)&rs, sizeof(rs));

  /* bind to listening addr/port */
  memset(&addr, 0, sizeof(addr));
  addr.sin_family = AF_INET;
  addr.sin_addr.s_addr = INADDR_ANY;
  addr.sin_port = htons(config.port);
  if (bind(listen_sock, (struct sockaddr*)&addr, sizeof(addr)) == -1) {
    ESP_LOGE(TAG, "bind() failed: %s", strerror(errno));
    close(listen_sock);
    return;
  }

  /* listen for clients */
  if (listen(listen_sock, 3) == -1) {
    ESP_LOGE(TAG, "listen() failed: %s", strerror(errno));
    close(listen_sock);
    return;
  }

  ESP_LOGI(TAG, "Telnet server listening on port %d", config.port);

  /* initialize listening descriptors */
  pfd[config.max_connections].fd = listen_sock;
  pfd[config.max_connections].events = POLLIN;

  /* loop for ever */
  while (true) {
    /* prepare for poll */
    for (i = 0; i != config.max_connections; ++i) {
      if (users[i].sock != -1) {
        pfd[i].fd = users[i].sock;
        pfd[i].events = POLLIN;
      }
      else {
        pfd[i].fd = -1;
        pfd[i].events = 0;
      }
    }

    /* poll */
    rs = poll(pfd, config.max_connections + 1, AWAIT_TIMEOUT);
    if (rs == -1 && errno != EINTR) {
      ESP_LOGE(TAG, "poll() failed: %s", strerror(errno));
      close(listen_sock);
      return;
    }

    /* new connection */
    if (pfd[config.max_connections].revents & (POLLIN | POLLRDNORM | POLLRDBAND | POLLPRI | POLLERR | POLLHUP)) {
      /* acept the sock */
      ESP_LOGW(TAG, "New connection");
      addrlen = sizeof(addr);
      if ((client_sock = accept(listen_sock, (struct sockaddr*)&addr, &addrlen)) == -1) {
        ESP_LOGE(TAG, "accept() failed: %s", strerror(errno));
        return;
      }

      ESP_LOGV(TAG, "Connection received");

      /* find a free user */
      for (i = 0; i != config.max_connections; ++i) {
        if (users[i].sock == -1) {
          break;
        }
      }

      if (i == config.max_connections) {
        ESP_LOGV(TAG, "  rejected (too many users)");
        _send(client_sock, "Too many users.\n", 16);
        close(client_sock);
      }

      /* init, welcome */
      users[i].sock = client_sock;
      users[i].telnet = telnet_init(config.telnet_opts, _event_handler, 0, &users[i]);
      telnet_negotiate(users[i].telnet, TELNET_WILL, TELNET_TELOPT_COMPRESS2);
      telnet_printf(users[i].telnet, "Enter name: ");

      // telnet_negotiate(users[i].telnet, TELNET_WILL, TELNET_TELOPT_ECHO);
    }

    /* read from client */
    for (i = 0; i != config.max_connections; ++i) {
      /* skip users that aren't actually connected */
      if (users[i].sock == -1) {
        continue;
      }

      if (pfd[i].revents & (POLLIN | POLLERR | POLLHUP)) {
        if ((rs = recv(users[i].sock, buffer, sizeof(buffer), 0)) > 0) {
          telnet_recv(users[i].telnet, buffer, rs);
        }
        else if (rs == 0) {
          ESP_LOGV(TAG, "Connection closed.");
          close(users[i].sock);
          users[i].sock = -1;
          if (users[i].name != 0) {
            // _message(users[i].name, "** HAS DISCONNECTED **");
            free(users[i].name);
            users[i].name = 0;
          }
          telnet_free(users[i].telnet);
        }
        else if (errno != EINTR) {
          ESP_LOGE(TAG, "recv(client) failed: %s", strerror(errno));
          return;
        }
      }
    }
  }
}

static TaskHandle_t xHandle = NULL;

esp_err_t telnet_server_create(telnet_server_config_t* config)
{
  if (config == NULL) {
    return ESP_ERR_INVALID_ARG;
  }

  BaseType_t xReturned = xTaskCreate(telnet_task, "telnet_task", config->stack_size, config, config->task_priority, &xHandle);
  if (xReturned == pdPASS) {
    ESP_LOGV(TAG, "Telnet task created successfully.");
    return ESP_OK;
  }

  ESP_LOGE(TAG, "Failed to create telnet task.");
  return ESP_FAIL;
}
