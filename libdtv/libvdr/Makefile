##############################################################
###                                                        ###
### Makefile: local makefile for libvdr                    ###
###                                                        ###
##############################################################

## $Revision: 1.3 $
## $Date: 2001/10/06 15:33:46 $
## $Author: hakenes $
##
##   (C) 2001 Rolf Hakenes <hakenes@hippomi.de>, under the GNU GPL.
##
## libdtv is free software; you can redistribute it and/or modify
## it under the terms of the GNU General Public License as published by
## the Free Software Foundation; either version 2, or (at your option)
## any later version.
##
## libdtv is distributed in the hope that it will be useful,
## but WITHOUT ANY WARRANTY; without even the implied warranty of
## MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
## GNU General Public License for more details.
##
## You may have received a copy of the GNU General Public License
## along with libdtv; see the file COPYING.  If not, write to the
## Free Software Foundation, Inc., 59 Temple Place - Suite 330,
## Boston, MA 02111-1307, USA.
#
#
#
CC = gcc
CFLAGS = -O2 -g -Wmissing-prototypes -Wstrict-prototypes \
         -Wimplicit -D__USE_FIXED_PROTOTYPES__ -I../include # -DDEBUG


AR = ar
ARFLAGS = r
RANLIB = ranlib
RM = rm -f
CP = cp

VDRINCLUDE = libvdr.h
VDRLIB = libvdr.a
VDROBJS = libvdr.o

all : $(VDRLIB)

clean :
	@echo "cleaning workspace..."
	@$(RM) $(VDROBJS) $(VDRLIB)

new : clean all

$(VDRLIB) : $(VDROBJS)
	@echo "updating library..."
	@$(AR) $(ARFLAGS) $(VDRLIB) $(VDROBJS)
	@$(RANLIB) $(VDRLIB)

dist: all
	@echo "distributing libvdr.a and libvdr.h..."
	@$(CP) $(VDRLIB) ../lib
	@$(CP) $(VDRINCLUDE) ../include
    
