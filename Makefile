CC = gcc
MAKE = make

MODULE_DIR := $(PWD)/module
SERVERS_DIR := $(PWD)

BUILD_DIR := $(PWD)

.PHONY: all servers module clean dep distclean

all:servers module

servers:
	$(MAKE) -C $(SERVERS_DIR) -f Makefile.servers all

module:
	$(MAKE) -C $(MODULE_DIR) all

clean:
	$(MAKE) -C $(SERVERS_DIR) -f Makefile.servers clean
	$(MAKE) -C $(MODULE_DIR) clean

dep:
	$(MAKE) -C $(SERVERS_DIR) -f Makefile.servers dep

distclean: clean
	rm -rf .depend
