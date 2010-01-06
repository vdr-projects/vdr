#
# Makefile for the Video Disk Recorder
#
# See the main source file 'vdr.c' for copyright information and
# how to reach the author.
#
# $Id: Makefile 2.5 2009/12/29 11:20:50 kls Exp $

.DELETE_ON_ERROR:

CC       ?= gcc
CFLAGS   ?= -g -O2 -Wall

CXX      ?= g++
CXXFLAGS ?= -g -O2 -Wall -Woverloaded-virtual -Wno-parentheses

LSIDIR   = ./libsi
DESTDIR ?=
PREFIX  ?= /usr/local
MANDIR   = $(PREFIX)/share/man
BINDIR   = $(PREFIX)/bin
LOCDIR   = ./locale
LIBS     = -ljpeg -lpthread -ldl -lcap -lrt -lfreetype -lfontconfig
INCLUDES = -I/usr/include/freetype2

PLUGINDIR= ./PLUGINS
PLUGINLIBDIR= $(PLUGINDIR)/lib

VIDEODIR = /video
CONFDIR  = $(VIDEODIR)

DOXYGEN  = /usr/bin/doxygen
DOXYFILE = Doxyfile

-include Make.config

SILIB    = $(LSIDIR)/libsi.a

OBJS = audio.o channels.o ci.o config.o cutter.o device.o diseqc.o dvbdevice.o dvbci.o\
       dvbplayer.o dvbspu.o dvbsubtitle.o eit.o eitscan.o epg.o filter.o font.o i18n.o interface.o keys.o\
       lirc.o menu.o menuitems.o nit.o osdbase.o osd.o pat.o player.o plugin.o rcu.o\
       receiver.o recorder.o recording.o remote.o remux.o ringbuffer.o sdt.o sections.o shutdown.o\
       skinclassic.o skins.o skinsttng.o sources.o spu.o status.o svdrp.o themes.o thread.o\
       timers.o tools.o transfer.o vdr.o videodir.o

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

DEFINES += -D_FILE_OFFSET_BITS=64 -D_LARGEFILE_SOURCE -D_LARGEFILE64_SOURCE

DEFINES += -DVIDEODIR=\"$(VIDEODIR)\"
DEFINES += -DCONFDIR=\"$(CONFDIR)\"
DEFINES += -DPLUGINDIR=\"$(PLUGINLIBDIR)\"
DEFINES += -DLOCDIR=\"$(LOCDIR)\"

# The version numbers of VDR and the plugin API (taken from VDR's "config.h"):

VDRVERSION = $(shell sed -ne '/define VDRVERSION/s/^.*"\(.*\)".*$$/\1/p' config.h)
APIVERSION = $(shell sed -ne '/define APIVERSION/s/^.*"\(.*\)".*$$/\1/p' config.h)

all: vdr i18n

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
	$(CXX) $(CXXFLAGS) -rdynamic $(OBJS) $(LIBS) $(LIBDIRS) $(SILIB) -o vdr

# The libsi library:

$(SILIB):
	$(MAKE) -C $(LSIDIR) all

# Internationalization (I18N):

PODIR     = po
LOCALEDIR = locale
I18Npo    = $(wildcard $(PODIR)/*.po)
I18Nmsgs  = $(addprefix $(LOCALEDIR)/, $(addsuffix /LC_MESSAGES/vdr.mo, $(notdir $(foreach file, $(I18Npo), $(basename $(file))))))
I18Npot   = $(PODIR)/vdr.pot

%.mo: %.po
	msgfmt -c -o $@ $<

$(I18Npot): $(wildcard *.c)
	xgettext -C -cTRANSLATORS --no-wrap --no-location -k -ktr -ktrNOOP --msgid-bugs-address='<vdr-bugs@tvdr.de>' -o $@ $^

%.po: $(I18Npot)
	msgmerge -U --no-wrap --no-location --backup=none -q $@ $<
	@touch $@

$(I18Nmsgs): $(LOCALEDIR)/%/LC_MESSAGES/vdr.mo: $(PODIR)/%.mo
	@mkdir -p $(dir $@)
	cp $< $@

.PHONY: i18n
i18n: $(I18Nmsgs)

install-i18n:
	@mkdir -p $(DESTDIR)$(LOCDIR)
	@(cd $(LOCALEDIR); cp -r --parents * $(DESTDIR)$(LOCDIR))

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
	if [ -n "$$failed" ] ; then echo; echo "*** failed plugins:$$failed"; echo; exit 1; fi

clean-plugins:
	@for i in `ls $(PLUGINDIR)/src | grep -v '[^a-z0-9]'`; do $(MAKE) -C "$(PLUGINDIR)/src/$$i" clean; done
	@-rm -f $(PLUGINDIR)/lib/lib*-*.so.$(APIVERSION)

# Install the files:

install: install-bin install-conf install-doc install-plugins install-i18n

# VDR binary:

install-bin: vdr
	@mkdir -p $(DESTDIR)$(BINDIR)
	@cp --remove-destination vdr runvdr svdrpsend.pl $(DESTDIR)$(BINDIR)

# Configuration files:

install-conf:
	@mkdir -p $(DESTDIR)$(VIDEODIR)
	@if [ ! -d $(DESTDIR)$(CONFDIR) ]; then\
	    mkdir -p $(DESTDIR)$(CONFDIR);\
	    cp *.conf $(DESTDIR)$(CONFDIR);\
	    fi

# Documentation:

install-doc:
	@mkdir -p $(DESTDIR)$(MANDIR)/man1
	@mkdir -p $(DESTDIR)$(MANDIR)/man5
	@gzip -c vdr.1 > $(DESTDIR)$(MANDIR)/man1/vdr.1.gz
	@gzip -c vdr.5 > $(DESTDIR)$(MANDIR)/man5/vdr.5.gz

# Plugins:

install-plugins: plugins
	@mkdir -p $(DESTDIR)$(PLUGINLIBDIR)
	@cp --remove-destination $(PLUGINDIR)/lib/lib*-*.so.$(APIVERSION) $(DESTDIR)$(PLUGINLIBDIR)

# Source documentation:

srcdoc:
	@cp $(DOXYFILE) $(DOXYFILE).tmp
	@echo PROJECT_NUMBER = $(VDRVERSION) >> $(DOXYFILE).tmp
	$(DOXYGEN) $(DOXYFILE).tmp
	@rm $(DOXYFILE).tmp

# Housekeeping:

clean:
	$(MAKE) -C $(LSIDIR) clean
	-rm -f $(OBJS) $(DEPFILE) vdr core* *~
	-rm -rf $(LOCALEDIR) $(PODIR)/*.mo $(PODIR)/*.pot
	-rm -rf include
	-rm -rf srcdoc
CLEAN: clean

