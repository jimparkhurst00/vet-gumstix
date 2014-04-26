#!/bin/sh

make ARCH=arm CROSS_COMPILE=arm-linux-gnueabi- uImage -j4
#cp arch/arm/boot/uImage ~/Dropbox/
