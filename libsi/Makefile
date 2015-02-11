#
# Makefile for a libsi
#
# $Id: Makefile 3.1 2015/02/11 10:24:54 kls Exp $

### The archiver options:

AR       ?= ar
ARFLAGS  ?= ru
RANLIB   ?= ranlib

-include ../Make.config

### The object files (add further files here):

OBJS = util.o si.o section.o descriptor.o

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

all: libsi.a
	@:

libsi.a : $(OBJS)
	$(AR) $(ARFLAGS) $@ $(OBJS)

clean:
	@-rm -f $(OBJS) $(DEPFILE) *.a *.so *.tgz core* *~

dist:
	tar cvzf libsi.tar.gz -C .. libsi/util.c libsi/si.c libsi/section.c libsi/descriptor.c \
   libsi/util.h libsi/si.h libsi/section.h libsi/descriptor.h libsi/headers.h libsi/Makefile libsi/gendescr
