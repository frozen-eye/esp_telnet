#include "common.h"
#include "unity.h"

void test_setup()
{
  printf("Test setup complete.\n");
}

void test_teardown()
{
  printf("Test teardown complete.\n");
}

TEST_CASE("telnet_server_create", "[telnet_server]")
{
  test_setup();
  test_teardown();
}
