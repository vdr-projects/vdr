#
# Makefile for the Video Disk Recorder
#
# See the main source file 'vdr.c' for copyright information and
# how to reach the author.
#
# $Id: Makefile 1.10 2000/09/17 10:19:44 kls Exp $

DVBDIR   = ../DVB

INCLUDES = -I$(DVBDIR)/driver
OBJS = config.o dvbapi.o eit.o interface.o menu.o osd.o recording.o remote.o svdrp.o tools.o vdr.o videodir.o

ifndef REMOTE
REMOTE = KBD
endif

DEFINES += -DREMOTE_$(REMOTE)

ifdef DEBUG_OSD
DEFINES += -DDEBUG_OSD
endif

%.o: %.c
	g++ -g -O2 -Wall -m486 -c $(DEFINES) $(INCLUDES) $<

all: vdr

config.o   : config.c config.h dvbapi.h eit.h interface.h tools.h
dvbapi.o   : dvbapi.c config.h dvbapi.h interface.h tools.h videodir.h
eit.o      : eit.c eit.h
interface.o: interface.c config.h dvbapi.h eit.h interface.h remote.h tools.h
menu.o     : menu.c config.h dvbapi.h interface.h menu.h osd.h recording.h tools.h
osd.o      : osd.c config.h dvbapi.h interface.h osd.h tools.h
vdr.o      : vdr.c config.h dvbapi.h interface.h menu.h osd.h recording.h svdrp.h tools.h videodir.h
recording.o: recording.c config.h dvbapi.h interface.h recording.h tools.h videodir.h
remote.o   : remote.c remote.h tools.h
svdrp.o    : svdrp.c svdrp.h config.h interface.h tools.h
tools.o    : tools.c tools.h
videodir.o : videodir.c tools.h videodir.h

vdr: $(OBJS)
	g++ -g -O2 $(OBJS) -lncurses -ljpeg -o vdr

clean:
	-rm $(OBJS) vdr
