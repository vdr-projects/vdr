#
# Makefile for xtvrc2vdr utility
#

OBJS = xtvrc2vdr.o

%.o: %.c
	gcc -g -O2 -Wall -c $(DEFINES) $<

all: xtvrc2vdr

xtvrc2vdr: $(OBJS)
	gcc -g -O2 $(OBJS) -o xtvrc2vdr

clean:
	-rm -f $(OBJS) xtvrc2vdr
