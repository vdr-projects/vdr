#
# Makefile for the Video Disk Recorder
#
# See the main source file 'vdr.c' for copyright information and
# how to reach the author.
#
# $Id: Makefile 2.27 2012/04/15 13:21:31 kls Exp $

.DELETE_ON_ERROR:

CC       ?= gcc
CFLAGS   ?= -g -O3 -Wall

CXX      ?= g++
CXXFLAGS ?= -g -O3 -Wall -Werror=overloaded-virtual -Wno-parentheses

LSIDIR   = ./libsi
DESTDIR ?=
PREFIX  ?= /usr/local
MANDIR  ?= $(PREFIX)/share/man
BINDIR  ?= $(PREFIX)/bin
INCDIR  ?= $(PREFIX)/include
LOCDIR  ?= ./locale
LIBS     = -ljpeg -lpthread -ldl -lcap -lrt $(shell pkg-config --libs freetype2 fontconfig)
INCLUDES ?= $(shell pkg-config --cflags freetype2 fontconfig)

PLUGINDIR= ./PLUGINS
PLUGINLIBDIR= $(PLUGINDIR)/lib

VIDEODIR = /video
CONFDIR  = $(VIDEODIR)

DOXYGEN ?= /usr/bin/doxygen
DOXYFILE = Doxyfile

PCDIR   ?= $(firstword $(subst :, , ${PKG_CONFIG_PATH}:$(shell pkg-config --variable=pc_path pkg-config):$(PREFIX)/lib/pkgconfig))

include Make.global
-include Make.config

SILIB    = $(LSIDIR)/libsi.a

OBJS = audio.o channels.o ci.o config.o cutter.o device.o diseqc.o dvbdevice.o dvbci.o\
       dvbplayer.o dvbspu.o dvbsubtitle.o eit.o eitscan.o epg.o filter.o font.o i18n.o interface.o keys.o\
       lirc.o menu.o menuitems.o nit.o osdbase.o osd.o pat.o player.o plugin.o\
       receiver.o recorder.o recording.o remote.o remux.o ringbuffer.o sdt.o sections.o shutdown.o\
       skinclassic.o skinlcars.o skins.o skinsttng.o sourceparams.o sources.o spu.o status.o svdrp.o themes.o thread.o\
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
ifdef BIDI
INCLUDES += $(shell pkg-config --cflags fribidi)
DEFINES += -DBIDI
LIBS += $(shell pkg-config --libs fribidi)
endif

LIRC_DEVICE ?= /var/run/lirc/lircd

DEFINES += -DLIRC_DEVICE=\"$(LIRC_DEVICE)\"

DEFINES += -D_GNU_SOURCE

DEFINES += -DVIDEODIR=\"$(VIDEODIR)\"
DEFINES += -DCONFDIR=\"$(CONFDIR)\"
DEFINES += -DPLUGINDIR=\"$(PLUGINLIBDIR)\"
DEFINES += -DLOCDIR=\"$(LOCDIR)\"

# The version numbers of VDR and the plugin API (taken from VDR's "config.h"):

VDRVERSION = $(shell sed -ne '/define VDRVERSION/s/^.*"\(.*\)".*$$/\1/p' config.h)
APIVERSION = $(shell sed -ne '/define APIVERSION/s/^.*"\(.*\)".*$$/\1/p' config.h)

all: vdr i18n vdr.pc

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
	$(CXX) $(CXXFLAGS) -rdynamic $(LDFLAGS) $(OBJS) $(LIBS) $(LIBDIRS) $(SILIB) -o vdr

# The libsi library:

$(SILIB):
	$(MAKE) -C $(LSIDIR) all

# pkg-config file:

vdr.pc: Makefile Make.global
	@echo "bindir=$(BINDIR)" > $@
	@echo "includedir=$(INCDIR)" >> $@
	@echo "configdir=$(CONFDIR)" >> $@
	@echo "videodir=$(VIDEODIR)" >> $@
	@echo "plugindir=$(PLUGINLIBDIR)" >> $@
	@echo "localedir=$(LOCDIR)" >> $@
	@echo "apiversion=$(APIVERSION)" >> $@
	@echo "cflags=$(CXXFLAGS) $(DEFINES) -I\$${includedir}" >> $@
	@echo "plugincflags=\$${cflags} -fPIC" >> $@
	@echo "" >> $@
	@echo "Name: VDR" >> $@
	@echo "Description: Video Disk Recorder" >> $@
	@echo "URL: http://www.tvdr.de/" >> $@
	@echo "Version: $(VDRVERSION)" >> $@
	@echo "Cflags: \$${cflags}" >> $@

# Internationalization (I18N):

PODIR     = po
LOCALEDIR = locale
I18Npo    = $(wildcard $(PODIR)/*.po)
I18Nmsgs  = $(addprefix $(LOCALEDIR)/, $(addsuffix /LC_MESSAGES/vdr.mo, $(notdir $(foreach file, $(I18Npo), $(basename $(file))))))
I18Npot   = $(PODIR)/vdr.pot

%.mo: %.po
	msgfmt -c -o $@ $<

$(I18Npot): $(wildcard *.c)
	xgettext -C -cTRANSLATORS --no-wrap --no-location -k -ktr -ktrNOOP --package-name=VDR --package-version=$(VDRVERSION) --msgid-bugs-address='<vdr-bugs@tvdr.de>' -o $@ `ls $^`

%.po: $(I18Npot)
	msgmerge -U --no-wrap --no-location --backup=none -q -N $@ $<
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

install: install-bin install-conf install-doc install-plugins install-i18n install-includes install-pc

# VDR binary:

install-bin: vdr
	@mkdir -p $(DESTDIR)$(BINDIR)
	@cp --remove-destination vdr svdrpsend $(DESTDIR)$(BINDIR)

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

# Includes:

install-includes: include-dir
	@mkdir -p $(DESTDIR)$(INCDIR)
	@cp -pLR include/vdr include/libsi $(DESTDIR)$(INCDIR)

# pkg-config file:

install-pc: vdr.pc
	if [ -n "$(PCDIR)" ] ; then \
	    mkdir -p $(DESTDIR)$(PCDIR) ; \
	    cp vdr.pc $(DESTDIR)$(PCDIR) ; \
	    fi

# Source documentation:

srcdoc:
	@cp $(DOXYFILE) $(DOXYFILE).tmp
	@echo PROJECT_NUMBER = $(VDRVERSION) >> $(DOXYFILE).tmp
	$(DOXYGEN) $(DOXYFILE).tmp
	@rm $(DOXYFILE).tmp

# Housekeeping:

clean:
	$(MAKE) -C $(LSIDIR) clean
	-rm -f $(OBJS) $(DEPFILE) vdr vdr.pc core* *~
	-rm -rf $(LOCALEDIR) $(PODIR)/*.mo $(PODIR)/*.pot
	-rm -rf include
	-rm -rf srcdoc
CLEAN: clean
