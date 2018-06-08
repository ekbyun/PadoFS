KCLI = padoclient
obj-m += $(KCLI).o
$(KCLI)-objs += testmodule.o convcli.o

KDIR := /lib/modules/$(shell uname -r)/build

all:
	make -C $(KDIR) SUBDIRS=$(shell pwd) modules

clean:
	make -C $(KDIR) SUBDIRS=$(shell pwd) clean
