# created by Alan at 2022-09-30 15:17

#! /bin/bash

# set variable
WORK_PATH=`pwd`
KMOD_PATH=${WORK_PATH}/kmods/linux/igb_uio/
KMOD_INSTALL_PATH=${WORK_PATH}/build/kmods/

# remove cache
rm -fr build
mkdir build

# build all examples
meson -Dexamples=all build
ninja -C build

# build kernel module
mkdir -p ${KMOD_INSTALL_PATH}
cd ${KMOD_PATH}
make clean
make
cp igb_uio.ko ${KMOD_INSTALL_PATH}
cd ${WORK_PATH}
