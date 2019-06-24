KCLI = wrapfs
obj-m += $(KCLI).o
$(KCLI)-objs += dentry.o file.o inode.o main.o super.o lookup.o mmap.o

KDIR := /lib/modules/$(shell uname -r)/build

all:
	make -C $(KDIR) SUBDIRS=$(shell pwd) modules

clean:
	make -C $(KDIR) SUBDIRS=$(shell pwd) clean
