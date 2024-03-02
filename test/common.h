#ifndef TEST_TELNET_SERVER_COMMON_H__
#define TEST_TELNET_SERVER_COMMON_H__

#include "sdkconfig.h"

#include "errno.h"
#include "esp_err.h"
#include "esp_heap_caps.h"
#include "esp_idf_version.h"
#include "esp_log.h"

void test_setup();
void test_teardown();

#endif // TEST_TELNET_SERVER_COMMON_H__
