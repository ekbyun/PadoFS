obj-m += bench.o

KDIR := /lib/modules/$(shell uname -r)/build

all:
	make -C $(KDIR) M=$(shell pwd) modules
	gcc -o test test.c -pthread

clean:
	make -C $(KDIR) M=$(shell pwd) clean
	rm ./test
