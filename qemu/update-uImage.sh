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

    OUTFILE="./sd.img"
    KERNEL="./uImage"
    

SIZE=1073741824 # 1G by default

function make_image()
{
    
    # Reverse-engineer the partition setup
    BYTES_PER_SECTOR="$(/sbin/fdisk -l -u ${OUTFILE} | grep Units | awk '{print $9}')"
    VFAT_SECTOR_OFFSET="$(/sbin/fdisk -l -u ${OUTFILE} | grep img1 | awk '{print $3}')"
    EXT3_SECTOR_OFFSET="$(/sbin/fdisk -l -u ${OUTFILE} | grep img2 | awk '{print $2}')"
}



function populate_image() 
{
    LOOP_DEV="/dev/loop1"
/sbin/losetup -v -o $(expr ${BYTES_PER_SECTOR} "*" ${VFAT_SECTOR_OFFSET}) ${LOOP_DEV} ${OUTFILE}
    
    echo "[ Copying files to vfat ]"
    mount ${LOOP_DEV} /mnt
    cp -v ${KERNEL} /mnt/uImage
sleep 1
    umount ${LOOP_DEV}
   
    echo "[ Clean up ]"
    /sbin/losetup -d ${LOOP_DEV}
}

make_image
populate_image
