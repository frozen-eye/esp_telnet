#pragma once

#include "sdkconfig.h"
#include <libtelnet.h>

#ifdef __cplusplus
extern "C" {
#endif

static const telnet_telopt_t telopts[] = {
  {TELNET_TELOPT_COMPRESS2, TELNET_WILL, TELNET_DO},
  {TELNET_TELOPT_ZMP, TELNET_WILL, TELNET_DO},
  {TELNET_TELOPT_MSSP, TELNET_WILL, TELNET_DONT},
  {TELNET_TELOPT_NEW_ENVIRON, TELNET_WILL, TELNET_DONT},
  {TELNET_TELOPT_TTYPE, TELNET_WILL, TELNET_DONT},
  {-1, 0, 0}};

struct user_t {
  char* name;
  int sock;
  telnet_t* telnet;
  char linebuf[255];
  int linepos;
};

void telnet_server(void);

#ifdef __cplusplus
}
#endif
