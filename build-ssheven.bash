#!/bin/bash
set -e

# set this to the root of your Retro68 build folder, e.g. ~/src/Retro68-build/
RETRO68_PATH=change_me_please

echo "Set your Retro68 build path in this script and delete this line!" && exit

################################################################################
echo "building PPC..."

rm -rf build-ppc
mkdir build-ppc

cmake -S . -B build-ppc -DCMAKE_TOOLCHAIN_FILE=$RETRO68_PATH/toolchain/powerpc-apple-macos/cmake/retroppc.toolchain.cmake
cmake --build build-ppc

################################################################################
echo "building m68k..."

rm -rf build-m68k
mkdir build-m68k

cmake -S . -B build-m68k -DCMAKE_TOOLCHAIN_FILE=$RETRO68_PATH/toolchain/m68k-apple-macos/cmake/retro68.toolchain.cmake
cmake --build build-m68k

################################################################################
echo "Rez-ing it all together..."

rm -rf build-fat
mkdir build-fat

$RETRO68_PATH/toolchain/bin/Rez \
$RETRO68_PATH/toolchain/m68k-apple-macos/RIncludes/RetroPPCAPPL.r \
-I$RETRO68_PATH/toolchain/m68k-apple-macos/RIncludes \
-DCFRAG_NAME="\"ssheven\"" \
--copy build-m68k/ssheven.code.bin \
-o build-fat/ssheven-fat.bin \
--cc build-fat/ssheven-fat.dsk --cc build-fat/ssheven-fat.APPL --cc build-fat/%ssheven-fat.ad \
-t APPL -c SSH7 \
--data build-ppc/ssheven.pef build-ppc/ssheven.r.rsrc.bin

echo "done."
