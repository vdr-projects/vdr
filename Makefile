#
# Makefile for the Video Disk Recorder
#
# See the main source file 'vdr.c' for copyright information and
# how to reach the author.
#
# $Id: Makefile 1.5 2000/07/23 11:57:14 kls Exp $

OBJS = config.o dvbapi.o interface.o menu.o osd.o recording.o remote.o svdrp.o tools.o vdr.o

ifndef REMOTE
REMOTE = KBD
endif

DEFINES += -DREMOTE_$(REMOTE)

ifdef DEBUG_OSD
DEFINES += -DDEBUG_OSD
endif

%.o: %.c
	g++ -g -O2 -Wall -c $(DEFINES) $<

all: vdr

config.o   : config.c config.h dvbapi.h interface.h tools.h
dvbapi.o   : dvbapi.c config.h dvbapi.h interface.h tools.h
interface.o: interface.c config.h dvbapi.h interface.h remote.h tools.h
menu.o     : menu.c config.h dvbapi.h interface.h menu.h osd.h recording.h tools.h
osd.o      : osd.c config.h dvbapi.h interface.h osd.h tools.h
vdr.o      : vdr.c config.h dvbapi.h interface.h menu.h osd.h recording.h svdrp.h tools.h
recording.o: recording.c config.h dvbapi.h interface.h recording.h tools.h
remote.o   : remote.c remote.h tools.h
svdrp.o    : svdrp.c svdrp.h config.h interface.h tools.h
tools.o    : tools.c tools.h

vdr: $(OBJS)
	g++ -g -O2 $(OBJS) -lncurses -o vdr

clean:
	-rm $(OBJS) vdr
