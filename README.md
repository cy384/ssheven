![ssheven box](http://www.cy384.com/media/img/ssheven_box_front_small.png)

ssheven
-------
A modern SSH client for Mac OS 7/8/9.

Project status: as of 0.8.0 ([see github releases](https://github.com/cy384/ssheven/releases)), a functional (but not completely secure) SSH client with color terminal emulation, able to login via key or password.  See roadmap below for upcoming work (i.e., things that aren't done yet).

![ssheven screenshot](http://www.cy384.com/media/img/ssheven-0.6.1-screenshot.png)

system requirements
-------------------
* CPU: Any PPC processor, or at least a 25 MHz 68040/68LC040.  Presently, all 68030/68020 CPUs are too slow.
* RAM: 2MB.
* Disk space: 1MB for the fat binary.
* System 7.5 or later, earlier System 7 versions might be possible with the Thread Manager extension installed.
* Open Transport networking required, version 1.1.1 or later recommended.

feature/bug-fix roadmap
-----------------------
0.9.0
* clean up libssh2 network ops (write fn, read safety, don't allow send until connected, quit while connected mess)
* clean up/update versions of libssh and mbedtls (get as close to mainline as possible, revert unecessary changes)
* build/packaging scripts
* general ssheven code cleanup

1.0.0 (first "real" release)
* nicer error presentation for more failure cases
* read Apple HIG and obsessively optimize placement of all GUI elements
* improve RNG
* license info in an about box type thing
* finish and upload papercraft box, floppy sticker artwork, icon/logo svg

?.?.?
* solve 68k crashes/finicky build issues ([retro68 issue](https://github.com/autc04/Retro68/issues/38))
* initial key exchange is too slow for 68030 and 68020 systems (improve `mbedtls_mpi_exp_mod`)
* input latency seems kinda high? related to draw speed/frequency? (maybe try to use an "offscreen graphics world" framebuffer? big refactor)
* receiving a large amount of data at once causes a freakout and breaks the SSH connection
* font face and size options
* scp file transfer
* text selection + copy
* more complete color support (will need to use color quickdraw, currently uses an 8-color hack for traditional quickdraw)
* preference file sometimes doesn't have the icon (fix up the filetype association etc.)
* keyboard-interactive authentication
* hook scrolling into vterm to reduce redraws/blanking
* check all keycode translation (control combo weirdness?)

build
-----
Uses Retro68 and cmake.

Requires mbedtls, libssh2, and libvterm, see my (cy384's) ports of those libraries for details.  Note that you need to build/install each for both platforms (m68k and PPC).

* `mkdir build && cd build`
* `cmake .. -DCMAKE_TOOLCHAIN_FILE=/your/path/to/Retro68-build/toolchain/powerpc-apple-macos/cmake/retroppc.toolchain.cmake` or `cmake .. -DCMAKE_TOOLCHAIN_FILE=/your/path/to/Retro68-build/toolchain/m68k-apple-macos/cmake/retro68.toolchain.cmake`
* `make`

Use Rez to build the fat binary: join the data fork from the PPC version and the resource fork from the m68k version.

I have some build scripts that I'll clean up and publish with the 1.0.0 release.

note to self: binary resources can be extracted in MPW via: `DeRez "Macintosh HD:whatever" -skip "'CODE'" -skip "'DATA'" -skip "'RELA'" -skip "'SIZE'"` etc., this is especially useful for icons

license
-------
Licensed under the BSD 2 clause license, see LICENSE file.

