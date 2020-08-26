obj-m += bench.o

KDIR := /lib/modules/$(shell uname -r)/build

all:
	make -C $(KDIR) SUBDIRS=$(shell pwd) modules
	gcc -o test test.c -lpthread

clean:
	make -C $(KDIR) SUBDIRS=$(shell pwd) clean
	rm ./test
