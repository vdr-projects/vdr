#
# Makefile for a Video Disk Recorder plugin
#
# $Id: Makefile 1.1 2002/05/09 15:17:44 kls Exp $

# The official name of this plugin.
# This name will be used in the '-P...' option of VDR to load the plugin.
# By default the main source file also carries this name.
#
PLUGIN = hello

### The version number of this plugin (taken from the main source file):

VERSION = `grep 'static const char \*VERSION *=' $(PLUGIN).c | awk '{ print $$6 }' | sed -e 's/[";]//g'`

### The directory environment:

DVBDIR = ../../../../DVB/ost/include
VDRDIR = ../../..
VDRINC = $(VDRDIR)/include
LIBDIR = ../../lib
TMPDIR = /tmp

### The version number of VDR (taken from VDR's "config.h"):

VDRVERSION = `grep 'define VDRVERSION ' $(VDRDIR)/config.h | awk '{ print $$3 }' | sed -e 's/"//g'`

### The name of the distribution archive:

ARCHIVE = vdr-$(PLUGIN)-$(VERSION)

### Includes and Defines (add further entries here):

INCLUDES = -I$(VDRINC) -I$(DVBDIR)

DEFINES = -DPLUGIN_NAME_I18N='"$(PLUGIN)"'

### The object files (add further files here):

OBJS = $(PLUGIN).o i18n.o

### The C++ compiler and options:

CXX      = g++
CXXFLAGS = -O2 -Wall -Woverloaded-virtual -m486

### Implicit rules:

%.o: %.c
	$(CXX) $(CXXFLAGS) -c $(DEFINES) $(INCLUDES) $<

# Dependencies:

MAKEDEP = g++ -MM -MG
DEPFILE = .dependencies
$(DEPFILE): Makefile
	@$(MAKEDEP) $(DEFINES) $(INCLUDES) $(OBJS:%.o=%.c) > $@

include $(DEPFILE)

### Targets:

all: libvdr-$(PLUGIN).so

libvdr-$(PLUGIN).so: $(OBJS)
	$(CXX) $(CXXFLAGS) -shared $(OBJS) -o $@
	@cp $@ $(LIBDIR)/$@.$(VDRVERSION)

package: clean
	@-rm -rf $(TMPDIR)/$(ARCHIVE)
	@mkdir $(TMPDIR)/$(ARCHIVE)
	@cp -a * $(TMPDIR)/$(ARCHIVE)
	@tar czf $(ARCHIVE).tgz -C $(TMPDIR) $(ARCHIVE)
	@-rm -rf $(TMPDIR)/$(ARCHIVE)
	@echo Distribution archive created as $(ARCHIVE).tgz

clean:
	@-rm -f $(OBJS) $(DEPFILE) *.so *.tgz core* *~
