#!/bin/sh

qemu-system-arm -M overo -m 256 -sd ./sd.img -clock unix -nographic

