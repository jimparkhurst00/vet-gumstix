#!/bin/bash
# This script generates SD card disk images suitable for use with QEMU.
#
# Copyright (C) 2011 Ash Charles
# Based on:
#   Narcissus - Online image builder for the angstrom distribution
#   Copyright (C) 2008 - 2011 Koen Kooi
#   Copyright (C) 2010        Denys Dmytriyenko
# and
#   Linaro Images Tools.
#   Author: Guilherme Salgado <guilherme.salgado@linaro.org>
#
# This program is free software; you can redistribute it and/or modify
# it under the terms of the GNU General Public License version 2 as
# published by the Free Software Foundation.
#
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.
#
# You should have received a copy of the GNU General Public License along
# with this program; if not, write to the Free Software Foundation, Inc.,
# 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.

LC_ALL=C
set -e

function usage()
{
    echo "This utility generates SD card images suitable for use with QEMU."
    echo "Usage:"
    echo "  $0 <output name> <mlo> <u-boot> <kernel> <rootfs>"
    echo "Example:"
    echo "  $0 sd.img ~/MLO ~/u-boot.bin ~/uImage ~/rootfs.tar.bz2"
}

function check_args()
{
    echo $#
    if [ $# -ne 6 ]; then
        usage
        exit 1
    fi
    OUTFILE=$1
    MLO=$2
    UBOOT=$3
    KERNEL=$4
    ROOTFS=$5
    MODULEFS=$6
    
    if ! [[ -e ${MLO} ]]; then
        echo "MLO not found at ${MLO}! Quitting..."
        exit 1
    fi
    if ! [[ -e ${UBOOT} ]]; then
        echo "U-boot not found at ${UBOOT}! Quitting..."
        exit 1
    fi
    if ! [[ -e ${KERNEL} ]]; then
        echo "Kernel not found at ${KERNEL}! Quitting..."
        exit 1
    fi
    if ! [[ -e ${ROOTFS} ]]; then
        echo "Rootfs not found at ${ROOTFS}! Quitting..."
        exit 1
    fi
    if ! [[ -e ${MODULEFS} ]]; then
        echo "Modulefs not found at ${MODULEFS}! Quitting..."
        exit 1
    fi
}

SIZE=1073741824 # 1G by default

function make_image()
{
    qemu-img create -f raw ${OUTFILE} ${SIZE}
    
    CYLINDERS=`echo ${SIZE}/255/63/512 | bc`
    {
    echo ,9,0x0C,*
    echo ,,,-
    } | sfdisk -D -H 255 -S 63 -C ${CYLINDERS} ${OUTFILE} &> /dev/null
    
    # Reverse-engineer the partition setup
    BYTES_PER_SECTOR="$(/sbin/fdisk -l -u ${OUTFILE} | grep Units | awk '{print $9}')"
    VFAT_SECTOR_OFFSET="$(/sbin/fdisk -l -u ${OUTFILE} | grep img1 | awk '{print $3}')"
    EXT3_SECTOR_OFFSET="$(/sbin/fdisk -l -u ${OUTFILE} | grep img2 | awk '{print $2}')"
}



function populate_image() 
{
    LOOP_DEV="/dev/loop1"
    LOOP_DEV_FS="/dev/loop2"
    
    echo "[ Format vfat partition ]"
    /sbin/losetup -v -o $(expr ${BYTES_PER_SECTOR} "*" ${VFAT_SECTOR_OFFSET}) ${LOOP_DEV} ${OUTFILE}
    mkfs.vfat -F 32 -n "boot" ${LOOP_DEV}
    
    echo "[ Format ext3 partition ]"
    /sbin/losetup -v -o $(expr ${BYTES_PER_SECTOR} "*" ${EXT3_SECTOR_OFFSET}) ${LOOP_DEV_FS} ${OUTFILE}
    /sbin/mkfs.ext3 -L rootfs ${LOOP_DEV_FS}
    
    echo "[ Copying files to vfat ]"
    mount ${LOOP_DEV} /mnt
    cp -v ${MLO} /mnt/MLO
    cp -v ${UBOOT} /mnt/u-boot.img
    cp -v ${KERNEL} /mnt/uImage
sleep 1
    umount ${LOOP_DEV}
   
    echo "[ Copying file system ]"
    mount ${LOOP_DEV_FS} /mnt
    tar xafv ${ROOTFS} -C /mnt
#    tar xafv ${MODULEFS} -C /mnt
sleep 1
    umount ${LOOP_DEV_FS}
    
    echo "[ Clean up ]"
    /sbin/losetup -d ${LOOP_DEV}
    /sbin/losetup -d ${LOOP_DEV_FS}
}

ARGS=$*
check_args $ARGS
make_image
populate_image
