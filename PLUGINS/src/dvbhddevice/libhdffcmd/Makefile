#
# Makefile for the HDFF firmware command interface library
#

VERSION = 0.1.0

INSTALL_PATH         ?= /usr/local
INSTALL_LIB_PATH     ?= $(INSTALL_PATH)/lib
INSTALL_INCLUDE_PATH ?= $(INSTALL_PATH)/include

LIB_NAME = libhdffcmd

LIB_OBJS = bitbuffer.o hdffcmd_av.o hdffcmd_base.o hdffcmd_generic.o \
           hdffcmd_hdmi.o hdffcmd_mux.o hdffcmd_osd.o hdffcmd_remote.o

LIB_HEADERS = hdffcmd.h hdffcmd_av.h hdffcmd_generic.h hdffcmd_hdmi.h \
              hdffcmd_mux.h hdffcmd_osd.h hdffcmd_remote.h

LIB_STATIC = $(LIB_NAME).a
LIB_SHARED = $(LIB_NAME)-$(VERSION).so

CC      ?= gcc
CFLAGS  ?= -g -O2 -fPIC -Wall
AR      ?= ar
ARFLAGS ?= r

### Implicit rules:

%.o: %.c
	$(CC) $(CFLAGS) -c $(DEFINES) $(INCLUDES) $<

### Dependencies:

MAKEDEP = $(CC) -MM -MG
DEPFILE = .dependencies
$(DEPFILE): Makefile
	@$(MAKEDEP) $(DEFINES) $(INCLUDES) $(LIB_OBJS:%.o=%.c) > $@

-include $(DEPFILE)

### Targets:

all: $(LIB_STATIC) $(LIB_SHARED)

$(LIB_STATIC): $(LIB_OBJS)
	$(AR) $(ARFLAGS) $(LIB_STATIC) $(LIB_OBJS)

$(LIB_SHARED): $(LIB_OBJS)
	$(CC) $(CFLAGS) $(LDFLAGS) -fPIC -shared -o $(LIB_SHARED) $(LIB_OBJS)
	ln -sf $(LIB_SHARED) $(LIB_NAME).so

clean:
	@-rm -f $(LIB_OBJS) $(DEPFILE) $(LIB_STATIC) $(LIB_NAME)*.so

install: $(LIB_SHARED)
	chown root $(LIB_SHARED)
	chgrp root $(LIB_SHARED)
	chmod 0755 $(LIB_SHARED)
	cp -f $(LIB_SHARED) $(INSTALL_LIB_PATH)/
	ln -sf $(LIB_SHARED) $(INSTALL_LIB_PATH)/$(LIB_NAME).so
	mkdir -p  $(INSTALL_INCLUDE_PATH)/libhdffcmd
	list='$(LIB_HEADERS)'; \
	for headerfile in $$list; do \
	  cp -f $$headerfile $(INSTALL_INCLUDE_PATH)/libhdffcmd/ ; \
	  chown root $(INSTALL_INCLUDE_PATH)/libhdffcmd/$$headerfile ; \
	  chgrp root $(INSTALL_INCLUDE_PATH)/libhdffcmd/$$headerfile ; \
	  chmod 0644 $(INSTALL_INCLUDE_PATH)/libhdffcmd/$$headerfile ; \
	done
