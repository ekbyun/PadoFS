obj-m += bench.o

KDIR := /lib/modules/$(shell uname -r)/build
#TESTFLAGS := -DPRINT_TIME_PER_CPU

all:
	make -C $(KDIR) M=$(shell pwd) modules
	gcc -o test test.c -pthread $(TESTFLAGS)

clean:
	make -C $(KDIR) M=$(shell pwd) clean
	rm ./test
