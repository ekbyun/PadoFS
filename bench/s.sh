#!/bin/sh
insmod bench.ko
mknod /dev/bench0 c 99 0
for(( i = 0 ; i < 200; i++)) ; do
	perf stat -C 0 -e offcore_response.any_request.mcdram_near,offcore_response.any_request.mcdram_far numactl --membind 0 ./test $@
#	perf stat -a -e offcore_response.any_request.mcdram_near,offcore_response.any_request.mcdram_far,offcore_response.any_request.ddr_near,offcore_response.any_request.ddr_far ./test $@
#	for j in `seq 0 2 62` ;	do
#		perf stat -C $j -e offcore_response.any_request.mcdram_near,offcore_response.any_request.mcdram_far numactl --membind 0 ./test 1 $j
#	done
done
rm -f /dev/bench0
rmmod bench
