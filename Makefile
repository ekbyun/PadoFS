CC = gcc

CFLAGS = -Wall -Werror -c -DTEST -DNODP

LIBS = -lpthread -lrt
INCLUDES = 

SRCS = inserver.c incont.c
OBJS = $(SRCS:%.c=%.o)
PROG = inodeserver
TEST = test

.SUFFIXES:.c .o

.c.o:
	$(CC) -c $(CFLAGS) $(INCLUDES) $< -o $@

.PHONY: all clean dep distclean


all:$(PROG) $(TEST)

$(PROG): $(OBJS)
	$(CC) $(INCLUDES) $(LIBS) -o $@ $^

$(TEST): incont.o test.o
	$(CC) $(INCLIDES) $(LIBS) -o $@ $^

clean:
	rm -f $(PROG) $(OBJS) $(TEST)

dep:
	gcc -M $(INCLUDES) $(SRCS) >.depend

distclean: clean
	rm -rf .depend

$(OBJS): inserver.h Makefile

ifeq (.depend,$(wildcard .depend))
include .depend
endif
