cmake_minimum_required(VERSION 3.9)

add_application(ssheven ssheven.c CONSOLE)

set_target_properties(ssheven PROPERTIES COMPILE_OPTIONS -ffunction-sections)

IF(CMAKE_SYSTEM_NAME MATCHES Retro68)
# for 68k
set_target_properties(ssheven PROPERTIES LINK_FLAGS "-Wl,-gc-sections -Wl,--mac-strip-macsbug -Wl,--mac-segments -Wl,${CMAKE_CURRENT_SOURCE_DIR}/ssheven.segmap")
target_link_libraries(ssheven RetroConsole ssh2 mbedtls mbedx509 mbedcrypto OpenTransportApp OpenTransport OpenTptInet)
ELSE()
# for PPC
set_target_properties(ssheven PROPERTIES LINK_FLAGS "-Wl,-gc-sections")
target_link_libraries(ssheven RetroConsole ThreadsLib ssh2 mbedtls mbedx509 mbedcrypto OpenTransportAppPPC OpenTransportLib OpenTptInternetLib)
ENDIF()

