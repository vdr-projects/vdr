#
# Makefile for 'ac3dec'
#
# $Id: Makefile 1.1 2001/08/02 13:18:08 kls Exp $

OBJS = coeff.o decode.o exponent.o rematrix.o bit_allocate.o crc.o dither.o\
       imdct.o sanity_check.o bitstream.o debug.o downmix.o parse.o stats.o                                  

DEFINES += -DDOLBY_SURROUND

all: libac3.a

libac3.a: $(OBJS)
	ar -rc libac3.a $(OBJS)

# Implicit rules:

%.o: %.c
	gcc -g -O2 -Wall -m486 -c $(DEFINES) $<

clean:
	rm -f *~ libac3.a $(OBJS)
