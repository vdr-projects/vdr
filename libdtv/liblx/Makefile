##############################################################
###                                                        ###
### Makefile: local makefile for liblx                     ###
###                                                        ###
##############################################################

## $Revision: 1.2 $
## $Date: 2001/06/25 19:39:00 $
## $Author: hakenes $
##
##   (C) 2001 Rolf Hakenes <hakenes@hippomi.de>, under the GNU GPL.
##
## dtv_scan is free software; you can redistribute it and/or modify
## it under the terms of the GNU General Public License as published by
## the Free Software Foundation; either version 2, or (at your option)
## any later version.
##
## dtv_scan is distributed in the hope that it will be useful,
## but WITHOUT ANY WARRANTY; without even the implied warranty of
## MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
## GNU General Public License for more details.
##
## You may have received a copy of the GNU General Public License
## along with dtv_scan; see the file COPYING.  If not, write to the
## Free Software Foundation, Inc., 59 Temple Place - Suite 330,
## Boston, MA 02111-1307, USA.
#
#
#
CC = gcc
CFLAGS = -O2 -g -pedantic -Wmissing-prototypes -Wstrict-prototypes \
         -Wimplicit -D__USE_FIXED_PROTOTYPES__ # -DDEBUG


AR = ar
ARFLAGS = r
RANLIB = ranlib
RM = rm -f
CP = cp

LXINCLUDE = liblx.h
LXLIB = liblx.a
LXOBJS = xMemMgt.o xListFuncs.o

all : $(LXLIB)

clean :
	@echo "cleaning workspace..."
	@$(RM) $(LXOBJS) $(LXLIB)

new : clean all

$(LXLIB) : $(LXOBJS)
	@echo "updating library..."
	@$(AR) $(ARFLAGS) $(LXLIB) $(LXOBJS)
	@$(RANLIB) $(LXLIB)

dist: all
	@echo "distributing liblx.a and liblx.h..."
	@$(CP) $(LXLIB) ../lib
	@$(CP) $(LXINCLUDE) ../include
    
