##############################################################
###                                                        ###
### Makefile: global makefile for libdtv                   ###
###                                                        ###
##############################################################

## $Revision: 1.3 $
## $Date: 2001/06/25 12:51:41 $
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

#
#  adapt this to your GNU make executable
#
MAKE = make

AR = ar
ARFLAGS = ru
RANLIB = ranlib

SUBDIRS = liblx libsi libvdr

all: newdist

new: clean newdist

clean:
	@echo "making all clean..."
	@for i in $(SUBDIRS);\
	do \
	    ( echo $$i; cd $$i ;\
	    $(MAKE) clean ) ;\
	done
	@rm -rf lib include libdtv.*

newdist:
	@mkdir -p include lib
	@echo "making all distributions..."
	@for i in $(SUBDIRS) ;\
	do \
	    ( cd $$i ;\
	    $(MAKE) new dist ) ;\
	done
	@echo "making libdtv.a/libdtv.h..."
	@cat include/* > libdtv.h
	@mkdir -p tmp
	@for i in $(SUBDIRS) ;\
	do \
	    ( cd tmp;\
              $(AR) x ../lib/$$i.a;\
              $(AR) $(ARFLAGS) ../libdtv.a *;\
              rm -f *) ;\
	done
	@rm -rf lib include tmp
