#
# Makefile for a Video Disk Recorder plugin
#
# $Id: Makefile 2.9 2012/12/23 10:04:16 kls Exp $

# The official name of this plugin.
# This name will be used in the '-P...' option of VDR to load the plugin.
# By default the main source file also carries this name.

PLUGIN1 = svccli
PLUGIN2 = svcsvr

### The version number of this plugin (taken from the main source file):

VERSION = $(shell grep 'static const char \*VERSION *=' $(PLUGIN1).c | awk '{ print $$6 }' | sed -e 's/[";]//g')

### The directory environment:

# Use package data if installed...otherwise assume we're under the VDR source directory:
PKGCFG = $(if $(VDRDIR),$(shell pkg-config --variable=$(1) $(VDRDIR)/vdr.pc),$(shell pkg-config --variable=$(1) vdr || pkg-config --variable=$(1) ../../../vdr.pc))
LIBDIR = $(DESTDIR)$(call PKGCFG,libdir)
#
TMPDIR ?= /tmp

### The compiler options:

export CFLAGS   = $(call PKGCFG,cflags)
export CXXFLAGS = $(call PKGCFG,cxxflags)

### The version number of VDR's plugin API:

APIVERSION = $(call PKGCFG,apiversion)

### The name of the distribution archive:

ARCHIVE = svcintf-$(VERSION)
PACKAGE = vdr-$(ARCHIVE)

### Includes and Defines (add further entries here):

INCLUDES +=

DEFINES += -DPLUGIN_NAME_I18N='"$(PLUGIN)"'

### The object files (add further files here):

OBJS = $(PLUGIN1).o $(PLUGIN2).o

### The main target:

all: libvdr-$(PLUGIN1).so libvdr-$(PLUGIN2).so

### Implicit rules:

%.o: %.c
	$(CXX) $(CXXFLAGS) -c $(DEFINES) $(INCLUDES) $<

### Dependencies:

MAKEDEP = $(CXX) -MM -MG
DEPFILE = .dependencies
$(DEPFILE): Makefile
	@$(MAKEDEP) $(DEFINES) $(INCLUDES) $(OBJS:%.o=%.c) > $@

-include $(DEPFILE)

### Targets:

libvdr-$(PLUGIN1).so: $(PLUGIN1).o
	$(CXX) $(CXXFLAGS) $(LDFLAGS) -shared $(PLUGIN1).o -o $@

libvdr-$(PLUGIN2).so: $(PLUGIN2).o
	$(CXX) $(CXXFLAGS) $(LDFLAGS) -shared $(PLUGIN2).o -o $@

install: libvdr-$(PLUGIN1).so libvdr-$(PLUGIN2).so
	install -D libvdr-$(PLUGIN1).so $(LIBDIR)/libvdr-$(PLUGIN1).so.$(APIVERSION)
	install -D libvdr-$(PLUGIN2).so $(LIBDIR)/libvdr-$(PLUGIN2).so.$(APIVERSION)

dist: clean
	@-rm -rf $(TMPDIR)/$(ARCHIVE)
	@mkdir $(TMPDIR)/$(ARCHIVE)
	@cp -a * $(TMPDIR)/$(ARCHIVE)
	@tar czf $(PACKAGE).tgz -C $(TMPDIR) $(ARCHIVE)
	@-rm -rf $(TMPDIR)/$(ARCHIVE)
	@echo Distribution package created as $(PACKAGE).tgz

clean:
	@-rm -f $(OBJS) $(DEPFILE) *.so *.tgz core* *~
