#
# Makefile for 'ac3dec'
#
# $Id: Makefile 1.2 2001/08/10 12:44:07 kls Exp $

#OBJS = coeff.o decode.o exponent.o rematrix.o bit_allocate.o crc.o dither.o\
#       imdct.o sanity_check.o bitstream.o debug.o downmix.o parse.o stats.o\
#       downmix_c.o imdct.o                             

OBJS = bit_allocate.o bitstream.o coeff.o cpu_accel.o crc.o debug.o decode.o\
       dither.o downmix.o downmix_c.o exponent.o imdct.o imdct_c.o imdct_kni.o\
       parse.o rematrix.o sanity_check.o srfft.o srfft_kni_c.o stats.o 


DEFINES += -DDOLBY_SURROUND

all: libac3.a

libac3.a: $(OBJS)
	ar -rc libac3.a $(OBJS)

# Implicit rules:

%.o: %.c
	gcc -g -O2 -Wall -m486 -I./ -c $(DEFINES) $<

clean:
	rm -f *~ libac3.a $(OBJS)
