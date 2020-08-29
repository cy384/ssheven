cmake_minimum_required(VERSION 3.9)

add_application(ssheven CREATOR "SSH7" ssheven.c ssheven-console.c ssheven.r)

set_target_properties(ssheven PROPERTIES COMPILE_OPTIONS -ffunction-sections)

IF(CMAKE_SYSTEM_NAME MATCHES Retro68)
# for 68k
#enable this once we figure out the compiler bug issue
#add_compile_options(-mcpu=68020)
#add_compile_options(-O2 -flto)
set_target_properties(ssheven PROPERTIES LINK_FLAGS "-Wl,-gc-sections -Wl,--mac-segments -Wl,${CMAKE_CURRENT_SOURCE_DIR}/ssheven.segmap")
target_link_libraries(ssheven ssh2 mbedtls mbedx509 mbedcrypto OpenTransportApp OpenTransport OpenTptInet vterm)
ELSE()
# for PPC
add_compile_options(-O2 -flto)
set_target_properties(ssheven PROPERTIES LINK_FLAGS "-Wl,-gc-sections")
target_link_libraries(ssheven ThreadsLib ssh2 mbedtls mbedx509 mbedcrypto OpenTransportAppPPC OpenTransportLib OpenTptInternetLib vterm)
ENDIF()

