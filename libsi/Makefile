#
# Makefile for a Video Disk Recorder plugin
#
# $Id: Makefile 1.1 2003/11/22 17:56:12 kls Exp $

VDRDIR = ../../../..

### The C++ compiler and options:

CXX      ?= g++
CXXFLAGS ?= -O2 -g -Wall -Woverloaded-virtual
AR = ar
ARFLAGS = ru
RANLIB = ranlib


### The directory environment:


INCLUDES += -I$(VDRDIR)/include -I..

DEFINES += 

LIBS +=

### The object files (add further files here):

OBJS = util.o si.o section.o descriptor.o

### Implicit rules:

%.o: %.c
	$(CXX) $(CXXFLAGS) -c $(DEFINES) $(INCLUDES) $<

# Dependencies:

MAKEDEP = g++ -MM -MG
DEPFILE = .dependencies
$(DEPFILE): Makefile
	@$(MAKEDEP) $(DEFINES) $(INCLUDES) $(OBJS:%.o=%.c) > $@

-include $(DEPFILE)

### Targets:

all: libsi.a

libsi.a : $(OBJS)
	$(AR) $(ARFLAGS) $@ $(OBJS)
#	$(CXX) $(CXXFLAGS) -shared $(OBJS) $(LIBS) -o $@ 

clean:
	@-rm -f $(OBJS) $(DEPFILE) *.a *.so *.tgz core* *~
   
dist:
	tar cvzf libsi.tar.gz -C .. libsi/util.c libsi/si.c libsi/section.c libsi/descriptor.c \
   libsi/util.h libsi/si.h libsi/section.h libsi/descriptor.h libsi/headers.h libsi/Makefile libsi/gendescr.pl
