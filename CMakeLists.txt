# ssheven
#
# Copyright (c) 2021 by cy384 <cy384@cy384.com>
# See LICENSE file for details

cmake_minimum_required(VERSION 3.9)

project(ssheven)
add_application(ssheven CREATOR "SSH7" ssheven.c ssheven-console.c ssheven.r ssheven-net.c ssheven-debug.c)

# set up and build mbedtls
set(ENABLE_PROGRAMS OFF CACHE BOOL "disable mbedtls programs" FORCE)
set(UNSAFE_BUILD ON CACHE BOOL "ignore mac os jank fro mbedtls" FORCE)
set(ENABLE_TESTING OFF CACHE BOOL "disable mbedtls tests" FORCE)
add_subdirectory(opentransport-mbedtls)

# set up and build libssh2
set(CRYPTO_BACKEND mbedTLS)
set(BUILD_SHARED_LIBS OFF CACHE BOOL "disable libssh2 shared libs" FORCE)
set(BUILD_EXAMPLES OFF CACHE BOOL "disable libssh2 examples" FORCE)
set(BUILD_TESTING OFF CACHE BOOL "disable libssh2 tests" FORCE)
add_subdirectory(opentransport-libssh2)

IF(CMAKE_SYSTEM_NAME MATCHES Retro68)
# for 68k
  set_target_properties(ssheven PROPERTIES COMPILE_FLAGS "-ffunction-sections -mcpu=68020 -O3 -Wall -Wextra -Wno-unused-parameter")
  set_target_properties(ssheven PROPERTIES LINK_FLAGS "-Wl,-gc-sections -Wl,--mac-segments -Wl,${CMAKE_CURRENT_SOURCE_DIR}/ssheven.segmap")
  target_link_libraries(ssheven libssh2 mbedtls mbedx509 mbedcrypto OpenTransportApp OpenTransport OpenTptInet vterm retrocrt)
ELSE()
# for PPC
  set_target_properties(ssheven PROPERTIES COMPILE_FLAGS "-ffunction-sections -mcpu=601 -O3 -Wall -Wextra -Wno-unused-parameter")
  set_target_properties(ssheven PROPERTIES LINK_FLAGS "-Wl,-gc-sections")
  target_link_libraries(ssheven ThreadsLib ssh2 mbedtls mbedx509 mbedcrypto OpenTransportAppPPC OpenTransportLib OpenTptInternetLib vterm retrocrt)
ENDIF()
