#
# Makefile for the Video Disk Recorder
#
# See the main source file 'vdr.c' for copyright information and
# how to reach the author.
#
# $Id: Makefile 1.26 2001/08/15 13:56:11 kls Exp $

.DELETE_ON_ERROR:

DVBDIR   = ../DVB
DVDDIR   = ../DVD
AC3DIR   = ./ac3dec
DTVDIR   = ./libdtv

INCLUDES = -I$(DVBDIR)/ost/include

DTVLIB   = $(DTVDIR)/libdtv.a

ifdef DVD
INCLUDES += -I$(DVDDIR)/libdvdread
LIBDIRS  += -L$(DVDDIR)/libdvdread/dvdread/.libs
DEFINES  += -DDVDSUPPORT
DEFINES  += -D_LARGEFILE64_SOURCE # needed by libdvdread
AC3LIB    = $(AC3DIR)/libac3.a
DVDLIB    = -ldvdread
endif

OBJS = config.o dvbapi.o dvbosd.o dvd.o eit.o font.o i18n.o interface.o menu.o osd.o\
       recording.o remote.o remux.o ringbuffer.o svdrp.o thread.o tools.o vdr.o\
       videodir.o

OSDFONT = -adobe-helvetica-medium-r-normal--23-*-100-100-p-*-iso8859-1
FIXFONT = -adobe-courier-bold-r-normal--25-*-100-100-m-*-iso8859-1

ifndef REMOTE
REMOTE = KBD
endif

DEFINES += -DREMOTE_$(REMOTE)

ifdef DEBUG_OSD
DEFINES += -DDEBUG_OSD
endif

ifdef VFAT
# for people who want their video directory on a VFAT partition
DEFINES += -DVFAT
endif

all: vdr
font: genfontfile fontfix.c fontosd.c
	@echo "font files created."

# Implicit rules:

%.o: %.c
	g++ -g -O2 -Wall -Woverloaded-virtual -m486 -c $(DEFINES) $(INCLUDES) $<

# Dependencies:

config.o    : config.c config.h dvbapi.h dvbosd.h dvd.h eit.h font.h i18n.h interface.h remote.h svdrp.h thread.h tools.h
dvbapi.o    : dvbapi.c $(AC3DIR)/ac3.h config.h dvbapi.h dvbosd.h dvd.h eit.h font.h recording.h remux.h ringbuffer.h thread.h tools.h videodir.h
dvbosd.o    : dvbosd.c dvbosd.h font.h tools.h
dvd.o       : dvd.c dvd.h
eit.o       : eit.c config.h dvbapi.h dvbosd.h dvd.h eit.h font.h $(DTVDIR)/libdtv.h thread.h tools.h videodir.h
font.o      : font.c font.h fontfix.c fontosd.c tools.h
i18n.o      : i18n.c config.h dvbapi.h dvbosd.h dvd.h eit.h font.h i18n.h thread.h tools.h
interface.o : interface.c config.h dvbapi.h dvbosd.h dvd.h eit.h font.h i18n.h interface.h remote.h svdrp.h thread.h tools.h
menu.o      : menu.c config.h dvbapi.h dvbosd.h dvd.h eit.h font.h i18n.h interface.h menu.h osd.h recording.h remote.h svdrp.h thread.h tools.h
osd.o       : osd.c config.h dvbapi.h dvbosd.h dvd.h eit.h font.h i18n.h interface.h osd.h remote.h svdrp.h thread.h tools.h
recording.o : recording.c config.h dvbapi.h dvbosd.h dvd.h eit.h font.h interface.h recording.h remote.h svdrp.h thread.h tools.h videodir.h
remote.o    : remote.c config.h dvbapi.h dvbosd.h dvd.h eit.h font.h remote.h thread.h tools.h
remux.o     : remux.c remux.h thread.h tools.h
ringbuffer.o: ringbuffer.c ringbuffer.h thread.h tools.h
svdrp.o     : svdrp.c config.h dvbapi.h dvbosd.h dvd.h eit.h font.h interface.h remote.h svdrp.h thread.h tools.h
thread.o    : thread.c thread.h tools.h
tools.o     : tools.c tools.h
vdr.o       : vdr.c config.h dvbapi.h dvbosd.h dvd.h eit.h font.h i18n.h interface.h menu.h osd.h recording.h remote.h svdrp.h thread.h tools.h videodir.h
videodir.o  : videodir.c tools.h videodir.h

# The main program:

vdr: $(OBJS) $(AC3LIB) $(DTVLIB)
	g++ -g -O2 $(OBJS) -lncurses -ljpeg -lpthread $(LIBDIRS) $(DVDLIB) $(AC3LIB) $(DTVLIB) -o vdr

# The font files:

fontfix.c:
	./genfontfile "cFont::tPixelData FontFix" "$(FIXFONT)" > $@
fontosd.c:
	./genfontfile "cFont::tPixelData FontOsd" "$(OSDFONT)" > $@

# The font file generator:

genfontfile: genfontfile.c
	gcc -o $@ -O2 -L/usr/X11R6/lib $< -lX11

# The ac3dec library:

$(AC3LIB):
	make -C $(AC3DIR) all

# The libdtv library:

$(DTVLIB) $(DTVDIR)/libdtv.h:
	make -C $(DTVDIR) all

# Housekeeping:

clean:
	make -C $(AC3DIR) clean
	make -C $(DTVDIR) clean
	-rm -f $(OBJS) vdr genfontfile genfontfile.o core *~
fontclean:
	-rm -f fontfix.c fontosd.c
CLEAN: clean fontclean

