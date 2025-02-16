![ssheven box](http://www.cy384.com/media/img/ssheven_box_front_small.png)

ssheven
-------
A minimal new SSH client for Mac OS 7/8/9.

Project status: as of 0.8.0 ([see github releases](https://github.com/cy384/ssheven/releases)), fairly secure and usable, but without a polished UX or all planned features.  Versions prior to 1.0.0 should be considered alpha/beta quality.

![ssheven screenshot](http://www.cy384.com/media/img/ssheven-0.8.8.png)

system requirements
-------------------
* CPU: Any PPC processor, or at least a 68030 (68040 strongly recommended).
* RAM: 2MB.
* Disk space: fits on a floppy.
* System 7.1 or later. Versions below 7.5 require the Thread Manager extension.
* Open Transport networking required, latest version possible highly recommended.

features that might happen
--------------------------
* improved unicode support
* basic scp file transfer
* configurable terminal string
* configurable/auto-choosing SSH buffer size (improves feel for faster machines)
* nicer error presentation for more failure cases
* add `known_hosts` reset option
* read Apple HIG and clean up UI/UX
* all license info in an about box type thing
* finish and upload papercraft box, floppy sticker artwork, icon/logo svg
* more complete color support
* keyboard-interactive login
* better debug output
* multiple terminal windows open at once

known problems
--------------
* drawing the screen is somewhat slow
* input latency feels high because redrawing the screen is slow (along with all the encryption, which is also slow)
* receiving a large amount of data may break the channel or cause lockups (e.g. `cat /dev/zero`)
* non-US keyboard input may or may not have issues

build
-----
Uses [Retro68](https://github.com/autc04/Retro68/) (requires the Universal headers) and cmake.

Requires mbedtls, libssh2, and libvterm, see my (cy384's) hackjobs of those libraries for details.  These are now pulled in as submodules, and everything should get built with a single CMake build.

The script `build-ssheven.bash` can be used to build a fat binary, or just as reference for build commands.

license
-------
Licensed under the BSD 2 clause license, see `LICENSE` file.

