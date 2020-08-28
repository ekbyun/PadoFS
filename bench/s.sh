#!/bin/sh
insmod bench.ko
mknod /dev/bench0 c 99 0
#numactl --membind 1 ./test $@
#./test $@
perf stat -C 0 -e offcore_response.any_request.mcdram_near,offcore_response.any_request.mcdram_far numactl --membind 0 ./test $@
#perf stat -a -e offcore_response.any_request.mcdram_near,offcore_response.any_request.mcdram_far,offcore_response.any_request.ddr_near,offcore_response.any_request.ddr_far ./test $@
rm -f /dev/bench0
rmmod bench
