CC = gcc

CFLAGS = -Wall -Werror -c -DTEST

LIBS = -lpthread -lrt
INCLUDES = 

INOS_SRCS = inserver.c incont.c
INOS_OBJS = $(INOS_SRCS:%.c=%.o)
INOS_PROG = inodeserver

TEST = test
CONV = convser

PROGS = $(INOS_PROG) $(TEST) $(CONV)

.SUFFIXES:.c .o

.c.o:
	$(CC) -c $(CFLAGS) $(INCLUDES) $< -o $@

.PHONY: all clean dep distclean


all:$(PROGS)

$(INOS_PROG): $(INOS_OBJS)
	$(CC) $(INCLUDES) $(LIBS) -o $@ $^

$(TEST): incont.o test.o
	$(CC) $(INCLUDES) $(LIBS) -o $@ $^

$(CONV): convser.o
	$(CC) $(INCLUDES) $(LIBS) -o $@ $^

clean:
	rm -f $(PROGS) *.o

dep:
	gcc -M $(INCLUDES) $(SRCS) >.depend

distclean: clean
	rm -rf .depend

$(OBJS): inserver.h Makefile

ifeq (.depend,$(wildcard .depend))
include .depend
endif
