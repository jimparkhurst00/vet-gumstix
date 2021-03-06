#!/bin/sh

wget http://s3-us-west-2.amazonaws.com/gumstix-yocto/Releases/2013-08-09/overo/dev/MLO
wget http://s3-us-west-2.amazonaws.com/gumstix-yocto/Releases/2013-08-09/overo/dev/gumstix-console-image-overo.tar.bz2
wget http://s3-us-west-2.amazonaws.com/gumstix-yocto/Releases/2013-08-09/overo/dev/u-boot.img
wget --output-document=uImage.oem http://s3-us-west-2.amazonaws.com/gumstix-yocto/Releases/2013-08-09/overo/dev/uImage

mkdir tmpdir
cd tmpdir
MOD_INSTALL_DIR=`pwd`
cd ..

cd ../linux
make ARCH=arm CROSS_COMPILE=arm-linux-gnueabi- modules -j4
sudo INSTALL_MOD_PATH=${MOD_INSTALL_DIR} make ARCH=arm CROSS_COMPILE=arm-linux-gnueabi- modules_install
cd ../qemu

cd tmpdir
tar cfzv ../modules.tgz *
cd ..

sudo rm -r tmpdir
