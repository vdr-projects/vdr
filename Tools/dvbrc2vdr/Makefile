#
# Makefile for dvbrc2vdr utility
#

OBJS = dvbrc2vdr.o

%.o: %.c
	gcc -g -O2 -Wall -c $(DEFINES) $<

all: dvbrc2vdr

dvbrc2vdr: $(OBJS)
	gcc -g -O2 $(OBJS) -o dvbrc2vdr

clean:
	-rm -f $(OBJS) dvbrc2vdr
