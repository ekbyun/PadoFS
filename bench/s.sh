#!/bin/sh
insmod bench.ko
mknod /dev/bench0 c 99 0
./test
rm -f /dev/bench0
rmmod bench
