#
# Makefile for the Video Disk Recorder
#
# See the main source file 'vdr.c' for copyright information and
# how to reach the author.
#
# $Id: Makefile 1.33 2002/04/01 12:50:48 kls Exp $

.DELETE_ON_ERROR:

DVBDIR   = ../DVB
DTVDIR   = ./libdtv
MANDIR   = /usr/local/man
BINDIR   = /usr/local/bin

VIDEODIR = /video

INCLUDES = -I$(DVBDIR)/ost/include

DTVLIB   = $(DTVDIR)/libdtv.a

OBJS = config.o dvbapi.o dvbosd.o eit.o font.o i18n.o interface.o menu.o osd.o\
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

vdr: $(OBJS) $(DTVLIB)
	g++ -g -O2 $(OBJS) $(NCURSESLIB) -ljpeg -lpthread $(LIBDIRS) $(DTVLIB) -o vdr

# The font files:

fontfix.c:
	./genfontfile "cFont::tPixelData FontFix" "$(FIXFONT)" > $@
fontosd.c:
	./genfontfile "cFont::tPixelData FontOsd" "$(OSDFONT)" > $@

# The font file generator:

genfontfile: genfontfile.c
	gcc -o $@ -O2 -L/usr/X11R6/lib $< -lX11

# The libdtv library:

$(DTVLIB) $(DTVDIR)/libdtv.h:
	make -C $(DTVDIR) all

# Install the files:

install:
	@cp vdr runvdr $(BINDIR)
	@gzip -c vdr.1 > $(MANDIR)/man1/vdr.1.gz
	@gzip -c vdr.5 > $(MANDIR)/man5/vdr.5.gz
	@if [ ! -d $(VIDEODIR) ]; then\
            mkdir $(VIDEODIR);\
            cp *.conf $(VIDEODIR);\
            fi

# Housekeeping:

clean:
	make -C $(DTVDIR) clean
	-rm -f $(OBJS) $(DEPFILE) vdr genfontfile genfontfile.o core* *~
fontclean:
	-rm -f fontfix.c fontosd.c
CLEAN: clean fontclean

