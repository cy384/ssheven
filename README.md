![ssheven box](http://www.cy384.com/media/img/ssheven_box_front_small.png)

ssheven
-------
A modern SSH client for Mac OS 7/8/9.

Project status: as of 0.3.0 (see github releases), an actual SSH client with a zero-features "vanilla" fixed-size terminal

![ssheven screenshot](http://www.cy384.com/media/img/ssheven-screenshot.png)

system requirements
-------------------
* CPU: 33 MHz 68040 (or 68LC040) might be fast enough to connect without timeouts (any PPC processor should be fine)
* RAM: requires approx. 2MB
* Disk space: currently about 1MB for the fat binary, or about 600KB for one platform
* System 7.5 recommended, earlier System 7 versions possible with the Thread Manager extension installed
* Open Transport networking required, version 1.1.1 recommended minimum

to do
-----
* terminal resizing
* proper region invalidation/redraw
* good console emulation (to be implemented with libvterm)
* saving/loading connection settings
* nicer connection dialog
* preferences
* better error checking
* key authentication
* check server keys/known keys
* copy/paste
* figure out how to improve 68k performance (possibly impossible)

build
-----
More details to come as functionality is added.

Uses Retro68 and cmake.

Requires mbedtls and libssh2, see my (cy384's) ports of those libraries for details.  Note that you need to make/install them for both platforms if you want to build for both platforms.

* `mkdir build && cd build`
* `cmake .. -DCMAKE_TOOLCHAIN_FILE=/your/path/to/Retro68-build/toolchain/powerpc-apple-macos/cmake/retroppc.toolchain.cmake` or `cmake .. -DCMAKE_TOOLCHAIN_FILE=/your/path/to/Retro68-build/toolchain/m68k-apple-macos/cmake/retro68.toolchain.cmake`
* `make`

license
-------
Licensed under the BSD 2 clause license, see LICENSE file.

