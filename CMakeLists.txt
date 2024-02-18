cmake_minimum_required(VERSION 3.10)

idf_component_register(
  INCLUDE_DIRS
  include
  src
  .
  SRCS
  src/libtelnet.c
  src/server.c
  REQUIRES
  PRIV_REQUIRES
)

# target_compile_options(${COMPONENT_LIB} PRIVATE -Wall -Wextra -Werror)
# target_compile_features(${COMPONENT_LIB} PRIVATE cxx_std_17)
