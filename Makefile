#
# Makefile for the Video Disk Recorder
#
# See the main source file 'vdr.c' for copyright information and
# how to reach the author.
#
# $Id: Makefile 1.94 2006/06/02 14:45:33 kls Exp $

.DELETE_ON_ERROR:

CC       ?= gcc
CFLAGS   ?= -g -O2 -Wall

CXX      ?= g++
CXXFLAGS ?= -g -O2 -Wall -Woverloaded-virtual

LSIDIR   = ./libsi
MANDIR   = /usr/local/man
BINDIR   = /usr/local/bin
LIBS     = -ljpeg -lpthread -ldl -lcap
INCLUDES =

PLUGINDIR= ./PLUGINS
PLUGINLIBDIR= $(PLUGINDIR)/lib

VIDEODIR = /video

DOXYGEN  = /usr/bin/doxygen
DOXYFILE = Doxyfile

-include Make.config

SILIB    = $(LSIDIR)/libsi.a

OBJS = audio.o channels.o ci.o config.o cutter.o device.o diseqc.o dvbdevice.o dvbosd.o\
       dvbplayer.o dvbspu.o eit.o eitscan.o epg.o filter.o font.o i18n.o interface.o keys.o\
       lirc.o menu.o menuitems.o nit.o osdbase.o osd.o pat.o player.o plugin.o rcu.o\
       receiver.o recorder.o recording.o remote.o remux.o ringbuffer.o sdt.o sections.o\
       skinclassic.o skins.o skinsttng.o sources.o spu.o status.o svdrp.o themes.o thread.o\
       timers.o tools.o transfer.o vdr.o videodir.o

FIXFONT_ISO8859_1 = -adobe-courier-bold-r-normal--25-*-100-100-m-*-iso8859-1
OSDFONT_ISO8859_1 = -adobe-helvetica-medium-r-normal--23-*-100-100-p-*-iso8859-1
SMLFONT_ISO8859_1 = -adobe-helvetica-medium-r-normal--18-*-100-100-p-*-iso8859-1

FIXFONT_ISO8859_2 = -adobe-courier-bold-r-normal--25-*-100-100-m-*-iso8859-2
OSDFONT_ISO8859_2 = -adobe-helvetica-medium-r-normal--24-*-75-75-p-*-iso8859-2
SMLFONT_ISO8859_2 = -adobe-helvetica-medium-r-normal--18-*-75-75-p-*-iso8859-2

FIXFONT_ISO8859_5 = -rfx-courier-bold-r-normal--24-*-75-75-m-*-iso8859-5
OSDFONT_ISO8859_5 = -rfx-helvetica-medium-r-normal--24-*-75-75-p-*-iso8859-5
SMLFONT_ISO8859_5 = -rfx-helvetica-medium-r-normal--18-*-75-75-p-*-iso8859-5

FIXFONT_ISO8859_7 = --user-medium-r-normal--26-171-110-110-m-140-iso8859-7
OSDFONT_ISO8859_7 = --user-medium-r-normal--23-179-85-85-m-120-iso8859-7
SMLFONT_ISO8859_7 = --user-medium-r-normal--19-160-72-72-m-110-iso8859-7

FIXFONT_ISO8859_15 = -adobe-courier-bold-r-normal--25-*-100-100-m-*-iso8859-15
OSDFONT_ISO8859_15 = -adobe-helvetica-medium-r-normal--23-*-100-100-p-*-iso8859-15
SMLFONT_ISO8859_15 = -adobe-helvetica-medium-r-normal--18-*-100-100-p-*-iso8859-15

ifndef NO_KBD
DEFINES += -DREMOTE_KBD
endif
ifdef REMOTE
DEFINES += -DREMOTE_$(REMOTE)
endif
ifdef VDR_USER
DEFINES += -DVDR_USER=\"$(VDR_USER)\"
endif

LIRC_DEVICE ?= /dev/lircd
RCU_DEVICE  ?= /dev/ttyS1

DEFINES += -DLIRC_DEVICE=\"$(LIRC_DEVICE)\" -DRCU_DEVICE=\"$(RCU_DEVICE)\"

DEFINES += -D_GNU_SOURCE

DEFINES += -DVIDEODIR=\"$(VIDEODIR)\"
DEFINES += -DPLUGINDIR=\"$(PLUGINLIBDIR)\"

# The version numbers of VDR and the plugin API (taken from VDR's "config.h"):

VDRVERSION = $(shell sed -ne '/define VDRVERSION/s/^.*"\(.*\)".*$$/\1/p' config.h)
APIVERSION = $(shell sed -ne '/define APIVERSION/s/^.*"\(.*\)".*$$/\1/p' config.h)

ifdef VFAT
# for people who want their video directory on a VFAT partition
DEFINES += -DVFAT
endif

all: vdr
font: genfontfile\
      fontfix-iso8859-1.c fontosd-iso8859-1.c fontsml-iso8859-1.c\
      fontfix-iso8859-2.c fontosd-iso8859-2.c fontsml-iso8859-2.c\
      fontfix-iso8859-5.c fontosd-iso8859-5.c fontsml-iso8859-5.c\
      fontfix-iso8859-7.c fontosd-iso8859-7.c fontsml-iso8859-7.c\
      fontfix-iso8859-15.c fontosd-iso8859-15.c fontsml-iso8859-15.c
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

vdr: $(OBJS) $(SILIB)
	$(CXX) $(CXXFLAGS) -rdynamic $(OBJS) $(NCURSESLIB) $(LIBS) $(LIBDIRS) $(SILIB) -o vdr

# The font files:

fontfix-iso8859-1.c:
	./genfontfile "cFont::tPixelData FontFix_iso8859_1" "$(FIXFONT_ISO8859_1)" > $@
fontosd-iso8859-1.c:
	./genfontfile "cFont::tPixelData FontOsd_iso8859_1" "$(OSDFONT_ISO8859_1)" > $@
fontsml-iso8859-1.c:
	./genfontfile "cFont::tPixelData FontSml_iso8859_1" "$(SMLFONT_ISO8859_1)" > $@

fontfix-iso8859-2.c:
	./genfontfile "cFont::tPixelData FontFix_iso8859_2" "$(FIXFONT_ISO8859_2)" > $@
fontosd-iso8859-2.c:
	./genfontfile "cFont::tPixelData FontOsd_iso8859_2" "$(OSDFONT_ISO8859_2)" > $@
fontsml-iso8859-2.c:
	./genfontfile "cFont::tPixelData FontSml_iso8859_2" "$(SMLFONT_ISO8859_2)" > $@

fontfix-iso8859-5.c:
	./genfontfile "cFont::tPixelData FontFix_iso8859_5" "$(FIXFONT_ISO8859_5)" > $@
fontosd-iso8859-5.c:
	./genfontfile "cFont::tPixelData FontOsd_iso8859_5" "$(OSDFONT_ISO8859_5)" > $@
fontsml-iso8859-5.c:
	./genfontfile "cFont::tPixelData FontSml_iso8859_5" "$(SMLFONT_ISO8859_5)" > $@

fontfix-iso8859-7.c:
	./genfontfile "cFont::tPixelData FontFix_iso8859_7" "$(FIXFONT_ISO8859_7)" > $@
fontosd-iso8859-7.c:
	./genfontfile "cFont::tPixelData FontOsd_iso8859_7" "$(OSDFONT_ISO8859_7)" > $@
fontsml-iso8859-7.c:
	./genfontfile "cFont::tPixelData FontSml_iso8859_7" "$(SMLFONT_ISO8859_7)" > $@

fontfix-iso8859-15.c:
	./genfontfile "cFont::tPixelData FontFix_iso8859_15" "$(FIXFONT_ISO8859_15)" > $@
fontosd-iso8859-15.c:
	./genfontfile "cFont::tPixelData FontOsd_iso8859_15" "$(OSDFONT_ISO8859_15)" > $@
fontsml-iso8859-15.c:
	./genfontfile "cFont::tPixelData FontSml_iso8859_15" "$(SMLFONT_ISO8859_15)" > $@

# The font file generator:

genfontfile: genfontfile.c
	$(CC) $(CFLAGS) -o $@ -L/usr/X11R6/lib $< -lX11

# The libsi library:

$(SILIB):
	$(MAKE) -C $(LSIDIR) all

# The 'include' directory (for plugins):

include-dir:
	@mkdir -p include/vdr
	@(cd include/vdr; for i in ../../*.h; do ln -fs $$i .; done)
	@mkdir -p include/libsi
	@(cd include/libsi; for i in ../../libsi/*.h; do ln -fs $$i .; done)

# Plugins:

plugins: include-dir
	@failed="";\
	noapiv="";\
	for i in `ls $(PLUGINDIR)/src | grep -v '[^a-z0-9]'`; do\
	    echo "Plugin $$i:";\
	    if ! grep -q "\$$(LIBDIR)/.*\$$(APIVERSION)" "$(PLUGINDIR)/src/$$i/Makefile" ; then\
	       echo "ERROR: plugin $$i doesn't honor APIVERSION - not compiled!";\
	       noapiv="$$noapiv $$i";\
	       continue;\
	       fi;\
	    $(MAKE) -C "$(PLUGINDIR)/src/$$i" all || failed="$$failed $$i";\
	    done;\
	if [ -n "$$noapiv" ] ; then echo; echo "*** plugins without APIVERSION:$$noapiv"; echo; fi;\
	if [ -n "$$failed" ] ; then echo; echo "*** failed plugins:$$failed"; echo; fi

clean-plugins:
	@for i in `ls $(PLUGINDIR)/src | grep -v '[^a-z0-9]'`; do $(MAKE) -C "$(PLUGINDIR)/src/$$i" clean; done
	@-rm -f $(PLUGINDIR)/lib/lib*-*.so.$(APIVERSION)

# Install the files:

install: install-bin install-conf install-doc install-plugins

# VDR binary:

install-bin: vdr
	@mkdir -p $(BINDIR)
	@cp vdr runvdr $(BINDIR)

# Configuration files:

install-conf:
	@if [ ! -d $(VIDEODIR) ]; then\
	    mkdir -p $(VIDEODIR);\
	    cp *.conf $(VIDEODIR);\
	    fi

# Documentation:

install-doc:
	@mkdir -p $(MANDIR)/man1
	@mkdir -p $(MANDIR)/man5
	@gzip -c vdr.1 > $(MANDIR)/man1/vdr.1.gz
	@gzip -c vdr.5 > $(MANDIR)/man5/vdr.5.gz

# Plugins:

install-plugins: plugins
	@mkdir -p $(PLUGINLIBDIR)
	@cp $(PLUGINDIR)/lib/lib*-*.so.$(APIVERSION) $(PLUGINLIBDIR)

# Source documentation:

srcdoc:
	@cp $(DOXYFILE) $(DOXYFILE).tmp
	@echo PROJECT_NUMBER = $(VDRVERSION) >> $(DOXYFILE).tmp
	$(DOXYGEN) $(DOXYFILE).tmp
	@rm $(DOXYFILE).tmp

# Housekeeping:

clean:
	$(MAKE) -C $(LSIDIR) clean
	-rm -f $(OBJS) $(DEPFILE) vdr genfontfile genfontfile.o core* *~
	-rm -rf include
	-rm -rf srcdoc
fontclean:
	-rm -f fontfix*.c fontosd*.c fontsml*.c
CLEAN: clean fontclean

