#
# Makefile for the Video Disk Recorder
#
# See the main source file 'vdr.c' for copyright information and
# how to reach the author.
#
# $Id: Makefile 1.46 2002/09/08 14:00:48 kls Exp $

.DELETE_ON_ERROR:

CC       = gcc
CFLAGS   = -O2

CXX      = g++
CXXFLAGS = -g -O2 -Wall -Woverloaded-virtual

DVBDIR   = ../DVB
DTVDIR   = ./libdtv
MANDIR   = /usr/local/man
BINDIR   = /usr/local/bin

PLUGINDIR= ./PLUGINS

VIDEODIR = /video

ifdef NEWSTRUCT
INCLUDES = -I$(DVBDIR)/include
DEFINES += -DNEWSTRUCT
else
INCLUDES = -I$(DVBDIR)/ost/include
endif

DTVLIB   = $(DTVDIR)/libdtv.a

OBJS = audio.o config.o cutter.o device.o dvbdevice.o dvbosd.o dvbplayer.o dvbspu.o eit.o eitscan.o font.o i18n.o\
       interface.o menu.o menuitems.o osdbase.o osd.o player.o plugin.o receiver.o\
       recorder.o recording.o remote.o remux.o ringbuffer.o spu.o status.o svdrp.o thread.o\
       tools.o transfer.o vdr.o videodir.o

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
	$(CXX) $(CXXFLAGS) -c $(DEFINES) $(INCLUDES) $<

# Dependencies:

MAKEDEP = $(CXX) -MM -MG
DEPFILE = .dependencies
$(DEPFILE): Makefile
	@$(MAKEDEP) $(DEFINES) $(INCLUDES) $(OBJS:%.o=%.c) > $@

-include $(DEPFILE)

# The main program:

vdr: $(OBJS) $(DTVLIB)
	$(CXX) $(CXXFLAGS) -rdynamic $(OBJS) $(NCURSESLIB) -ljpeg -lpthread -ldl $(LIBDIRS) $(DTVLIB) -o vdr

# The font files:

fontfix.c:
	./genfontfile "cFont::tPixelData FontFix" "$(FIXFONT)" > $@
fontosd.c:
	./genfontfile "cFont::tPixelData FontOsd" "$(OSDFONT)" > $@

# The font file generator:

genfontfile: genfontfile.c
	$(CC) $(CFLAGS) -o $@ -L/usr/X11R6/lib $< -lX11

# The libdtv library:

$(DTVLIB) $(DTVDIR)/libdtv.h:
	$(MAKE) -C $(DTVDIR) all

# The 'include' directory (for plugins):

include-dir:
	@mkdir -p include/vdr
	@(cd include/vdr; for i in ../../*.h; do ln -fs $$i .; done)

# Plugins:

plugins: include-dir
	@for i in `ls $(PLUGINDIR)/SRC | grep -v '[^a-z0-9]'`; do $(MAKE) -C "$(PLUGINDIR)/SRC/$$i" all; done

plugins-clean:
	@for i in `ls $(PLUGINDIR)/SRC | grep -v '[^a-z0-9]'`; do $(MAKE) -C "$(PLUGINDIR)/SRC/$$i" clean; done
	@-rm -f $(PLUGINDIR)/lib/*

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
	$(MAKE) -C $(DTVDIR) clean
	-rm -f $(OBJS) $(DEPFILE) vdr genfontfile genfontfile.o core* *~
	-rm -rf include
fontclean:
	-rm -f fontfix.c fontosd.c
CLEAN: clean fontclean

