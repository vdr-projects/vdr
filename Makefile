#
# Makefile for the Video Disk Recorder
#
# See the main source file 'vdr.c' for copyright information and
# how to reach the author.
#
# $Id: Makefile 1.30 2002/02/01 14:40:09 kls Exp $

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

ifeq ($(REMOTE), KBD)
NCURSESLIB = -lncurses
endif

DEFINES += -DREMOTE_$(REMOTE)

DEFINES += -D_GNU_SOURCE

ifdef DEBUG_OSD
DEFINES += -DDEBUG_OSD
NCURSESLIB = -lncurses
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

MAKEDEP = g++ -MM -MG
DEPFILE = .dependencies
$(DEPFILE): Makefile
	@$(MAKEDEP) $(DEFINES) $(INCLUDES) $(OBJS:%.o=%.c) > $@

include $(DEPFILE)

# The main program:

vdr: $(OBJS) $(AC3LIB) $(DTVLIB)
	g++ -g -O2 $(OBJS) $(NCURSESLIB) -ljpeg -lpthread $(LIBDIRS) $(DVDLIB) $(AC3LIB) $(DTVLIB) -o vdr

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
	-rm -f $(OBJS) $(DEPFILE) vdr genfontfile genfontfile.o core* *~
fontclean:
	-rm -f fontfix.c fontosd.c
CLEAN: clean fontclean

