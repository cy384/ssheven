ssheven
-------
A modern SSH client for Mac OS 7/8/9 on m68k and PPC machines.

Project status: can open the SSH connection, channel, send a command, get the response, and cleanly disconnect

* encryption libraries: ham-handedly ported and seem to work
* console emulation: nonexistent, planning the use of libvterm for escape codes etc., going to need to make a custom window type
* UI/UX: not yet considered at all

build
-----
More details to come as functionality is added.

Uses Retro68 and cmake.

Requires mbedtls and libssh2, see my (cy384's) ports of those libraries for details.

* `mkdir build && cd build`
* `cmake .. -DCMAKE_TOOLCHAIN_FILE=/your/path/to/Retro68-build/toolchain/powerpc-apple-macos/cmake/retroppc.toolchain.cmake` or `cmake .. -DCMAKE_TOOLCHAIN_FILE=/your/path/to/Retro68-build/toolchain/m68k-apple-macos/cmake/retro68.toolchain.cmake`
* `make`

license
-------
Licensed under the BSD 2 clause license, see LICENSE file.
