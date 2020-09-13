cmake_minimum_required(VERSION 3.9)

add_application(ssheven CREATOR "SSH7" ssheven.c ssheven-console.c ssheven.r)

IF(CMAKE_SYSTEM_NAME MATCHES Retro68)
# for 68k
# add "-mcpu=68020" once we figure out retro68 bug
set_target_properties(ssheven PROPERTIES COMPILE_FLAGS "-ffunction-sections -O3 -Wall -Wextra -Wno-unused-parameter")
set_target_properties(ssheven PROPERTIES LINK_FLAGS "-Wl,-gc-sections -Wl,--mac-segments -Wl,${CMAKE_CURRENT_SOURCE_DIR}/ssheven.segmap")
target_link_libraries(ssheven ssh2 mbedtls mbedx509 mbedcrypto OpenTransportApp OpenTransport OpenTptInet vterm retrocrt)
ELSE()
# for PPC
set_target_properties(ssheven PROPERTIES COMPILE_FLAGS "-ffunction-sections -O3 -Wall -Wextra -Wno-unused-parameter")
set_target_properties(ssheven PROPERTIES LINK_FLAGS "-Wl,-gc-sections")
target_link_libraries(ssheven ThreadsLib ssh2 mbedtls mbedx509 mbedcrypto OpenTransportAppPPC OpenTransportLib OpenTptInternetLib vterm retrocrt)
ENDIF()

