CC = gcc

CFLAGS = -Wall -Werror -c

LIBS = -lpthread -lrt
INCLUDES = 

SRCS = inserver.c incont.c
OBJS = $(SRCS:%.c=%.o)
PROG = inodeserver

.SUFFIXES:.c .o

.c.o:
	$(CC) -c $(CFLAGS) $(INCLUDES) $< -o $@

.PHONY: all clean dep distclean


all:$(PROG)

$(PROG): $(OBJS)
	$(CC) $(INCLUDES) $(LIBS) -o $@ $^

clean:
	rm -f $(PROG) $(OBJS)

dep:
	gcc -M $(INCLUDES) $(SRCS) >.depend

distclean: clean
	rm -rf .depend

$(OBJS): inserver.h Makefile

ifeq (.depend,$(wildcard .depend))
include .depend
endif
