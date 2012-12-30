#
# Makefile for the Video Disk Recorder
#
# See the main source file 'vdr.c' for copyright information and
# how to reach the author.
#
# $Id: Makefile 2.47 2012/12/30 11:18:18 kls Exp $

.DELETE_ON_ERROR:

# Compiler flags:

CC       ?= gcc
CFLAGS   ?= -g -O3 -Wall

CXX      ?= g++
CXXFLAGS ?= $(CFLAGS) -Werror=overloaded-virtual -Wno-parentheses

CFLAGS   += -fPIC

CDEFINES  = -D_GNU_SOURCE
CDEFINES += -D_FILE_OFFSET_BITS=64 -D_LARGEFILE_SOURCE -D_LARGEFILE64_SOURCE

# Directories:

CWD      = .
LSIDIR   = ./libsi
DESTDIR ?=
PREFIX  ?= /usr/local
MANDIR  ?= $(PREFIX)/share/man
BINDIR  ?= $(PREFIX)/bin
INCDIR  ?= $(CWD)/include
LOCDIR  ?= $(CWD)/locale
LIBS     = -ljpeg -lpthread -ldl -lcap -lrt $(shell pkg-config --libs freetype2 fontconfig)
INCLUDES ?= $(shell pkg-config --cflags freetype2 fontconfig)

PLUGINDIR= $(CWD)/PLUGINS
LIBDIR   = $(PLUGINDIR)/lib

# By default VDR requires only one single directory to operate:
VIDEODIR     = /video
# See Make.config.template if you want to build VDR according to the FHS ("File system Hierarchy Standard")

DOXYGEN ?= /usr/bin/doxygen
DOXYFILE = Doxyfile

PCDIR   ?= $(firstword $(subst :, , ${PKG_CONFIG_PATH}:$(shell pkg-config --variable=pc_path pkg-config):$(PREFIX)/lib/pkgconfig))

-include Make.config

ifdef DVBDIR
CFLAGS += -I$(DVBDIR)
endif

UP3 = $(if $(findstring "$(LIBDIR)-$(LOCDIR)","$(CWD)/PLUGINS/lib-$(CWD)/locale"),../../../,)

SILIB    = $(LSIDIR)/libsi.a

OBJS = audio.o channels.o ci.o config.o cutter.o device.o diseqc.o dvbdevice.o dvbci.o\
       dvbplayer.o dvbspu.o dvbsubtitle.o eit.o eitscan.o epg.o filter.o font.o i18n.o interface.o keys.o\
       lirc.o menu.o menuitems.o nit.o osdbase.o osd.o pat.o player.o plugin.o\
       receiver.o recorder.o recording.o remote.o remux.o ringbuffer.o sdt.o sections.o shutdown.o\
       skinclassic.o skinlcars.o skins.o skinsttng.o sourceparams.o sources.o spu.o status.o svdrp.o themes.o thread.o\
       timers.o tools.o transfer.o vdr.o videodir.o

DEFINES += $(CDEFINES)

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
DEFINES += -DVIDEODIR=\"$(VIDEODIR)\"
DEFINES += -DCONFDIR=\"$(CONFDIR)\"
DEFINES += -DCACHEDIR=\"$(CACHEDIR)\"
DEFINES += -DRESDIR=\"$(RESDIR)\"
DEFINES += -DPLUGINDIR=\"$(LIBDIR)\"
DEFINES += -DLOCDIR=\"$(LOCDIR)\"

# Default values for directories:

CONFDIRDEF  = $(firstword $(CONFDIR)  $(VIDEODIR))
CACHEDIRDEF = $(firstword $(CACHEDIR) $(VIDEODIR))
RESDIRDEF   = $(firstword $(RESDIR)   $(CONFDIRDEF))

# The version numbers of VDR and the plugin API (taken from VDR's "config.h"):

VDRVERSION = $(shell sed -ne '/define VDRVERSION/s/^.*"\(.*\)".*$$/\1/p' config.h)
APIVERSION = $(shell sed -ne '/define APIVERSION/s/^.*"\(.*\)".*$$/\1/p' config.h)

all: vdr i18n plugins

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
	$(CXX) $(CXXFLAGS) -rdynamic $(LDFLAGS) $(OBJS) $(LIBS) $(SILIB) -o vdr

# The libsi library:

$(SILIB):
	$(MAKE) --no-print-directory -C $(LSIDIR) CXXFLAGS="$(CXXFLAGS)" DEFINES="$(CDEFINES)" all

# pkg-config file:

.PHONY: vdr.pc
vdr.pc:
	@echo "bindir=$(BINDIR)" > $@
	@echo "mandir=$(MANDIR)" >> $@
	@echo "configdir=$(CONFDIRDEF)" >> $@
	@echo "videodir=$(VIDEODIR)" >> $@
	@echo "cachedir=$(CACHEDIRDEF)" >> $@
	@echo "resdir=$(RESDIRDEF)" >> $@
	@echo "libdir=$(UP3)$(LIBDIR)" >> $@
	@echo "locdir=$(UP3)$(LOCDIR)" >> $@
	@echo "plgcfg=$(PLGCFG)" >> $@
	@echo "apiversion=$(APIVERSION)" >> $@
	@echo "cflags=$(CFLAGS) $(CDEFINES) -I$(UP3)$(INCDIR)" >> $@
	@echo "cxxflags=$(CXXFLAGS) $(CDEFINES) -I$(UP3)$(INCDIR)" >> $@
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
i18n: $(I18Nmsgs) $(I18Npot)

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

plugins: include-dir vdr.pc
	@failed="";\
	noapiv="";\
	for i in `ls $(PLUGINDIR)/src | grep -v '[^a-z0-9]'`; do\
	    echo "*** Plugin $$i:";\
	    if ! grep -q "\$$(LIBDIR)/.*\$$(APIVERSION)" "$(PLUGINDIR)/src/$$i/Makefile" ; then\
	       echo "ERROR: plugin $$i doesn't honor APIVERSION - not compiled!";\
	       noapiv="$$noapiv $$i";\
	       continue;\
	       fi;\
	    newmakefile=`grep "PKGCFG" "$(PLUGINDIR)/src/$$i/Makefile"`;\
	    if [ -z "$$newmakefile" ]; then\
	       echo "********************************************************************";\
	       echo "* Your plugin \"$$i\" is using an old Makefile!";\
	       echo "* While this currently still works, it is strongly recommended";\
	       echo "* that you convert that Makefile to the new style used since";\
	       echo "* VDR version 1.7.35. Support for old style Makefiles may be dropped";\
	       echo "* in future versions of VDR.";\
	       echo "********************************************************************";\
	       $(MAKE) --no-print-directory -C "$(PLUGINDIR)/src/$$i" CXXFLAGS="$(CXXFLAGS)" VDRDIR=$(UP3) LIBDIR=../../lib all || failed="$$failed $$i";\
	    else\
               target=all;\
	       if [ "$(LIBDIR)" = "$(CWD)/PLUGINS/lib" ] && [ "$(LOCDIR)" = "$(CWD)/locale" ]; then\
	          target="install";\
	          fi;\
	       includes=;\
	       if [ "$(INCDIR)" != "$(CWD)/include" ]; then\
	          includes="INCLUDES=-I$(UP3)/include";\
	          fi;\
	       $(MAKE) --no-print-directory -C "$(PLUGINDIR)/src/$$i" VDRDIR=$(UP3) $$includes $$target || failed="$$failed $$i";\
	       fi;\
	    done;\
	if [ -n "$$noapiv" ] ; then echo; echo "*** plugins without APIVERSION:$$noapiv"; echo; fi;\
	if [ -n "$$failed" ] ; then echo; echo "*** failed plugins:$$failed"; echo; exit 1; fi

clean-plugins:
	@for i in `ls $(PLUGINDIR)/src | grep -v '[^a-z0-9]'`; do $(MAKE) --no-print-directory -C "$(PLUGINDIR)/src/$$i" clean; done
	@-rm -f $(PLUGINDIR)/lib/lib*-*.so.$(APIVERSION)

# Install the files:

install: install-bin install-dirs install-conf install-doc install-plugins install-i18n install-includes install-pc

# VDR binary:

install-bin: vdr
	@mkdir -p $(DESTDIR)$(BINDIR)
	@cp --remove-destination vdr svdrpsend $(DESTDIR)$(BINDIR)

# Configuration files:

install-dirs:
	@mkdir -p $(DESTDIR)$(VIDEODIR)
	@mkdir -p $(DESTDIR)$(CONFDIRDEF)
	@mkdir -p $(DESTDIR)$(CACHEDIRDEF)
	@mkdir -p $(DESTDIR)$(RESDIRDEF)

install-conf:
	@cp -n *.conf $(DESTDIR)$(CONFDIRDEF)


# Documentation:

install-doc:
	@mkdir -p $(DESTDIR)$(MANDIR)/man1
	@mkdir -p $(DESTDIR)$(MANDIR)/man5
	@gzip -c vdr.1 > $(DESTDIR)$(MANDIR)/man1/vdr.1.gz
	@gzip -c vdr.5 > $(DESTDIR)$(MANDIR)/man5/vdr.5.gz

# Plugins:

install-plugins: plugins
	@for i in `ls $(PLUGINDIR)/src | grep -v '[^a-z0-9]'`; do\
	     $(MAKE) --no-print-directory -C "$(PLUGINDIR)/src/$$i" VDRDIR=$(UP3) DESTDIR=$(DESTDIR) install;\
	     done

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
	@$(MAKE) --no-print-directory -C $(LSIDIR) clean
	@-rm -f $(OBJS) $(DEPFILE) vdr vdr.pc core* *~
	@-rm -rf $(LOCALEDIR) $(PODIR)/*.mo $(PODIR)/*.pot
	@-rm -rf include
	@-rm -rf srcdoc
CLEAN: clean
distclean: clean-plugins clean
