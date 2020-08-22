![ssheven box](http://www.cy384.com/media/img/ssheven_box_front_small.png)

ssheven
-------
A modern SSH client for Mac OS 7/8/9.

Project status: as of 0.3.0 (see github releases), an actual SSH client with a zero-features "vanilla" fixed-size terminal

![ssheven screenshot](http://www.cy384.com/media/img/ssheven-screenshot.png)

system requirements
-------------------
* CPU: Any PPC processor.  Maybe a 33 or 40 MHz 68040 (or 68LC040).  68030 is too slow (for now).
* RAM: 2MB
* Disk space: 1MB for the fat binary
* System 7.5 recommended, earlier System 7 versions possible with the Thread Manager extension installed
* Open Transport networking required, version 1.1.1 or later recommended

to do
-----
* escape codes and related console emulation features (via libvterm)
* refactor libssh2 usage to handle errors and centralize network ops
* terminal window resizing
* nicer connection dialog
* password dialog that doesn't show the password
* preferences
* saving/loading connection settings
* key authentication
* check server keys/known hosts/keys
* text selection + copy
* improve 68k performance (rewrite `mbedtls_mpi_exp_mod` in assembly)
* font size options
* color/bold/underline/italic etc. fancy console features

build
-----
Uses Retro68 and cmake.

Requires mbedtls, libssh2, and libvterm, see my (cy384's) ports of those libraries for details.  Note that you need to build/install each for both platforms (m68k and PPC).

* `mkdir build && cd build`
* `cmake .. -DCMAKE_TOOLCHAIN_FILE=/your/path/to/Retro68-build/toolchain/powerpc-apple-macos/cmake/retroppc.toolchain.cmake` or `cmake .. -DCMAKE_TOOLCHAIN_FILE=/your/path/to/Retro68-build/toolchain/m68k-apple-macos/cmake/retro68.toolchain.cmake`
* `make`

Use Rez to build the fat binary: join the data fork from the PPC version and the resource fork from the m68k version.

I have some build scripts that I'll clean up and publish with the 1.0.0 release.

license
-------
Licensed under the BSD 2 clause license, see LICENSE file.

