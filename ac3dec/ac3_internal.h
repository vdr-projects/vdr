/* 
 *    ac3_internal.h
 *
 *	Copyright (C) Aaron Holtzman - May 1999
 *
 *  This file is part of ac3dec, a free Dolby AC-3 stream decoder.
 *	
 *  ac3dec is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2, or (at your option)
 *  any later version.
 *   
 *  ac3dec is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *   
 *  You should have received a copy of the GNU General Public License
 *  along with GNU Make; see the file COPYING.  If not, write to
 *  the Free Software Foundation, 675 Mass Ave, Cambridge, MA 02139, USA. 
 *
 */

#ifndef __GNUC__
#define inline 
#endif

#include <setjmp.h>

/* Exponent strategy constants */
#define EXP_REUSE (0)
#define EXP_D15   (1)
#define EXP_D25   (2)
#define EXP_D45   (3)

/* Delta bit allocation constants */
#define DELTA_BIT_REUSE (0)
#define DELTA_BIT_NEW (1)
#define DELTA_BIT_NONE (2)
#define DELTA_BIT_RESERVED (3)

/* samples work structure */
typedef float stream_samples_t[6][256];

/* global config structure */
extern ac3_config_t ac3_config;
/* global error flag */
extern jmp_buf error_jmp_mark;
#define HANDLE_ERROR() longjmp (error_jmp_mark, -1)

/* Everything you wanted to know about band structure */
/*
 * The entire frequency domain is represented by 256 real
 * floating point fourier coefficients. Only the lower 253
 * coefficients are actually utilized however. We use arrays
 * of 256 to be efficient in some cases.
 *
 * The 5 full bandwidth channels (fbw) can have their higher
 * frequencies coupled together. These coupled channels then
 * share their high frequency components.
 *
 * This coupling band is broken up into 18 sub-bands starting
 * at mantissa number 37. Each sub-band is 12 bins wide.
 *
 * There are 50 bit allocation sub-bands which cover the entire
 * frequency range. The sub-bands are of non-uniform width, and
 * approximate a 1/6 octave scale.
 */

/* The following structures are filled in by their corresponding parse_*
 * functions. See http://www.atsc.org/Standards/A52/a_52.pdf for
 * full details on each field. Indented fields are used to denote
 * conditional fields.
 */

typedef struct syncinfo_s
{
	uint32_t	magic;
	/* Sync word == 0x0B77 */
	 uint16_t   syncword; 
	/* crc for the first 5/8 of the sync block */
	/* uint16_t   crc1; */
	/* Stream Sampling Rate (kHz) 0 = 48, 1 = 44.1, 2 = 32, 3 = reserved */
	uint16_t		fscod;	
	/* Frame size code */
	uint16_t		frmsizecod;

	/* Information not in the AC-3 bitstream, but derived */
	/* Frame size in 16 bit words */
	uint16_t frame_size;
	/* Bit rate in kilobits */
	uint16_t bit_rate;
	/* sampling rate in hertz */
	uint32_t sampling_rate;

} syncinfo_t;

typedef struct bsi_s
{
	uint32_t	magic;
	/* Bit stream identification == 0x8 */
	uint16_t bsid;	
	/* Bit stream mode */
	uint16_t bsmod;
	/* Audio coding mode */
	uint16_t acmod;
	/* If we're using the centre channel then */
		/* centre mix level */
		uint16_t cmixlev;
	/* If we're using the surround channel then */
		/* surround mix level */
		uint16_t surmixlev;
	/* If we're in 2/0 mode then */
		/* Dolby surround mix level - NOT USED - */
		uint16_t dsurmod;
	/* Low frequency effects on */
	uint16_t lfeon;
	/* Dialogue Normalization level */
	uint16_t dialnorm;
	/* Compression exists */
	uint16_t compre;
		/* Compression level */
		uint16_t compr;
	/* Language code exists */
	uint16_t langcode;
		/* Language code */
		uint16_t langcod;
	/* Audio production info exists*/
	uint16_t audprodie;
		uint16_t mixlevel;
		uint16_t roomtyp;
	/* If we're in dual mono mode (acmod == 0) then extra stuff */
		uint16_t dialnorm2;
		uint16_t compr2e;
			uint16_t compr2;
		uint16_t langcod2e;
			uint16_t langcod2;
		uint16_t audprodi2e;
			uint16_t mixlevel2;
			uint16_t roomtyp2;
	/* Copyright bit */
	uint16_t copyrightb;
	/* Original bit */
	uint16_t origbs;
	/* Timecode 1 exists */
	uint16_t timecod1e;
		/* Timecode 1 */
		uint16_t timecod1;
	/* Timecode 2 exists */
	uint16_t timecod2e;
		/* Timecode 2 */
		uint16_t timecod2;
	/* Additional bit stream info exists */
	uint16_t addbsie;
		/* Additional bit stream length - 1 (in bytes) */
		uint16_t addbsil;
		/* Additional bit stream information (max 64 bytes) */
		uint8_t	addbsi[64];

	/* Information not in the AC-3 bitstream, but derived */
	/* Number of channels (excluding LFE)
	 * Derived from acmod */
	uint16_t nfchans;
} bsi_t;


/* more pain */
typedef struct audblk_s
{
	uint32_t	magic1;
	/* block switch bit indexed by channel num */
	uint16_t blksw[5];
	/* dither enable bit indexed by channel num */
	uint16_t dithflag[5];
	/* dynamic range gain exists */
	uint16_t dynrnge;
		/* dynamic range gain */
		uint16_t dynrng;
	/* if acmod==0 then */
	/* dynamic range 2 gain exists */
	uint16_t dynrng2e;
		/* dynamic range 2 gain */
		uint16_t dynrng2;
	/* coupling strategy exists */
	uint16_t cplstre;
		/* coupling in use */
		uint16_t cplinu;
			/* channel coupled */
			uint16_t chincpl[5];
			/* if acmod==2 then */
				/* Phase flags in use */
				uint16_t phsflginu;
			/* coupling begin frequency code */
			uint16_t cplbegf;
			/* coupling end frequency code */
			uint16_t cplendf;
			/* coupling band structure bits */
			uint16_t cplbndstrc[18];
			/* Do coupling co-ords exist for this channel? */
			uint16_t cplcoe[5];
			/* Master coupling co-ordinate */
			uint16_t mstrcplco[5];
			/* Per coupling band coupling co-ordinates */
			uint16_t cplcoexp[5][18];
			uint16_t cplcomant[5][18];
			/* Phase flags for dual mono */
			uint16_t phsflg[18];
	/* Is there a rematrixing strategy */
	uint16_t rematstr;
		/* Rematrixing bits */
		uint16_t rematflg[4];
	/* Coupling exponent strategy */
	uint16_t cplexpstr;
	/* Exponent strategy for full bandwidth channels */
	uint16_t chexpstr[5];
	/* Exponent strategy for lfe channel */
	uint16_t lfeexpstr;
	/* Channel bandwidth for independent channels */
	uint16_t chbwcod[5];
		/* The absolute coupling exponent */
		uint16_t cplabsexp;
		/* Coupling channel exponents (D15 mode gives 18 * 12 /3  encoded exponents */
		uint16_t cplexps[18 * 12 / 3];
	/* Sanity checking constant */
	uint32_t	magic2;
	/* fbw channel exponents */
	uint16_t exps[5][252 / 3];
	/* channel gain range */
	uint16_t gainrng[5];
	/* low frequency exponents */
	uint16_t lfeexps[3];

	/* Bit allocation info */
	uint16_t baie;
		/* Slow decay code */
		uint16_t sdcycod;
		/* Fast decay code */
		uint16_t fdcycod;
		/* Slow gain code */
		uint16_t sgaincod;
		/* dB per bit code */
		uint16_t dbpbcod;
		/* masking floor code */
		uint16_t floorcod;

	/* SNR offset info */
	uint16_t snroffste;
		/* coarse SNR offset */
		uint16_t csnroffst;
		/* coupling fine SNR offset */
		uint16_t cplfsnroffst;
		/* coupling fast gain code */
		uint16_t cplfgaincod;
		/* fbw fine SNR offset */
		uint16_t fsnroffst[5];
		/* fbw fast gain code */
		uint16_t fgaincod[5];
		/* lfe fine SNR offset */
		uint16_t lfefsnroffst;
		/* lfe fast gain code */
		uint16_t lfefgaincod;
	
	/* Coupling leak info */
	uint16_t cplleake;
		/* coupling fast leak initialization */
		uint16_t cplfleak;
		/* coupling slow leak initialization */
		uint16_t cplsleak;
	
	/* delta bit allocation info */
	uint16_t deltbaie;
		/* coupling delta bit allocation exists */
		uint16_t cpldeltbae;
		/* fbw delta bit allocation exists */
		uint16_t deltbae[5];
		/* number of cpl delta bit segments */
		uint16_t cpldeltnseg;
			/* coupling delta bit allocation offset */
			uint16_t cpldeltoffst[8];
			/* coupling delta bit allocation length */
			uint16_t cpldeltlen[8];
			/* coupling delta bit allocation length */
			uint16_t cpldeltba[8];
		/* number of delta bit segments */
		uint16_t deltnseg[5];
			/* fbw delta bit allocation offset */
			uint16_t deltoffst[5][8];
			/* fbw delta bit allocation length */
			uint16_t deltlen[5][8];
			/* fbw delta bit allocation length */
			uint16_t deltba[5][8];

	/* skip length exists */
	uint16_t skiple;
		/* skip length */
		uint16_t skipl;

	//Removed Feb 2000 -ah
	//added Jul 2000 ++dent
	/* channel mantissas */
	uint16_t chmant[5][256];

	/* coupling mantissas */
//	uint16_t cplmant[256];

	//Added Jun 2000 -MaXX
	/* coupling floats */
	float cpl_flt[ 256 ];

	//Removed Feb 2000 -ah
	//added Jul 2000 ++dent
	/* coupling mantissas */
	uint16_t lfemant[7];


	/*  -- Information not in the bitstream, but derived thereof  -- */

	/* Number of coupling sub-bands */
	uint16_t ncplsubnd;

	/* Number of combined coupling sub-bands
	 * Derived from ncplsubnd and cplbndstrc */
	uint16_t ncplbnd;

	/* Number of exponent groups by channel
	 * Derived from strmant, endmant */
	uint16_t nchgrps[5];

	/* Number of coupling exponent groups
	 * Derived from cplbegf, cplendf, cplexpstr */
	uint16_t ncplgrps;
			
	/* End mantissa numbers of fbw channels */
	uint16_t endmant[5];

	/* Start and end mantissa numbers for the coupling channel */
	uint16_t cplstrtmant;
	uint16_t cplendmant;

	/* Decoded exponent info */
	uint16_t fbw_exp[5][256];
	uint16_t cpl_exp[256];
	uint16_t lfe_exp[7];

	/* Bit allocation pointer results */
	uint16_t fbw_bap[5][256];
	uint16_t cpl_bap[256];
	uint16_t lfe_bap[7];
	
	uint32_t	magic3;
} audblk_t;


