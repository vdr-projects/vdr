/* 
 *    decode.c
 *
 *	Copyright (C) Aaron Holtzman - May 1999
 *
 *      Added support for DVB-s PCI card by:
 *      Matjaz Thaler <matjaz.thaler@rd.iskraemeco.si> - November 2000
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
 *
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif 

#include <stdlib.h>
#include <stdio.h>
#include <errno.h>
#include <string.h>
#include <sys/time.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

#include "ac3.h"
#include "ac3_internal.h"
#include "bitstream.h"
#include "imdct.h"
#include "exponent.h"
#include "coeff.h"
#include "bit_allocate.h"
#include "parse.h"
#include "crc.h"
#include "stats.h"
#include "rematrix.h"
#include "sanity_check.h"
#include "downmix.h"
#include "debug.h"

#define AC3_BUFFER_SIZE (6*1024*16)      

//our global config structure
ac3_config_t ac3_config;
uint_32 error_flag = 0;

static audblk_t audblk;
static bsi_t bsi;
static syncinfo_t syncinfo;
static uint_32 frame_count = 0;
//static uint_32 is_output_initialized = 0;

//the floating point samples for one audblk
static stream_samples_t samples;

//the integer samples for the entire frame (with enough space for 2 ch out)
//if this size change, be sure to change the size when muting
static sint_16 s16_samples[2 * 6 * 256];


//Storage for the syncframe
#define SYNC_BUFFER_MAX_SIZE 4096
static uint_8 sync_buffer[SYNC_BUFFER_MAX_SIZE];
static uint_32 sync_buffer_size = 0;;

uint_32
decode_sync_buffer_syncframe(syncinfo_t *syncinfo, uint_8 **start,uint_8 *end)
{
	uint_8 *cur = *start;
	uint_16 syncword = syncinfo->syncword;
	uint_32 ret = 0;

	// 
	// Find an ac3 sync frame.
	// 
resync:

	while(syncword != 0x0b77)
	{
		if(cur >= end)
			goto done;
		syncword = (syncword << 8) + *cur++;
	}

	//need the next 3 bytes to decide how big the frame is
	while(sync_buffer_size < 3)
	{
		if(cur >= end)
			goto done;

		sync_buffer[sync_buffer_size++] = *cur++;
	}
	
	parse_syncinfo(syncinfo,sync_buffer);
	stats_print_syncinfo(syncinfo);

	while(sync_buffer_size < syncinfo->frame_size * 2 - 2)
	{
		if(cur >= end)
			goto done;

		sync_buffer[sync_buffer_size++] = *cur++;
	}
	
	// Check the crc over the entire frame 
	crc_init();
	crc_process_frame(sync_buffer,syncinfo->frame_size * 2 - 2);

	if(!crc_validate())
	{
		fprintf(stderr,"** CRC failed - skipping frame **\n");
		syncword = 0xffff;
		sync_buffer_size = 0;
		goto resync;
	}
	
	//
	//if we got to this point, we found a valid ac3 frame to decode
	//

	bitstream_init(sync_buffer);
	//get rid of the syncinfo struct as we already parsed it
	bitstream_get(24);

	//reset the syncword for next time
	syncword = 0xffff;
	sync_buffer_size = 0;
	ret = 1;

done:
	syncinfo->syncword = syncword;
	*start = cur;
	return ret;
}

void
decode_mute(void)
{
	fprintf(stderr,"muting frame\n");
	//mute the frame
	memset(s16_samples,0,sizeof(sint_16) * 256 * 2 * 6);
	error_flag = 0;
}


void
ac3_init(ac3_config_t *config)
{
	memcpy(&ac3_config,config,sizeof(ac3_config_t));

	imdct_init();
	sanity_check_init(&syncinfo,&bsi,&audblk);

	//	ac3_output = *foo;
}

uint_32 ac3_decode_data(uint_8 *data_start,uint_8 *data_end, int ac3reset, int *input_pointer, int *output_pointer, char *ac3_data)
{
	uint_32 i;
	int datasize;
	char *data;

	if(ac3reset != 0){
	  syncinfo.syncword = 0xffff;
	  sync_buffer_size = 0;
	}
	
	while(decode_sync_buffer_syncframe(&syncinfo,&data_start,data_end))
	{
		dprintf("(decode) begin frame %d\n",frame_count++);

		if(error_flag)
		{
			decode_mute();
			continue;
		}

		parse_bsi(&bsi);

		for(i=0; i < 6; i++)
		{
			//Initialize freq/time sample storage
			memset(samples,0,sizeof(float) * 256 * (bsi.nfchans + bsi.lfeon));

			// Extract most of the audblk info from the bitstream
			// (minus the mantissas 
			parse_audblk(&bsi,&audblk);

			// Take the differential exponent data and turn it into
			// absolute exponents 
			exponent_unpack(&bsi,&audblk); 
			if(error_flag)
				goto error;

			// Figure out how many bits per mantissa 
			bit_allocate(syncinfo.fscod,&bsi,&audblk);

			// Extract the mantissas from the stream and
			// generate floating point frequency coefficients
			coeff_unpack(&bsi,&audblk,samples);
			if(error_flag)
				goto error;

			if(bsi.acmod == 0x2)
				rematrix(&audblk,samples);

			// Convert the frequency samples into time samples 
			imdct(&bsi,&audblk,samples);

			// Downmix into the requested number of channels
			// and convert floating point to sint_16
			downmix(&bsi,samples,&s16_samples[i * 2 * 256]);

			sanity_check(&syncinfo,&bsi,&audblk);
			if(error_flag)
				goto error;

			continue;
		}
		
		parse_auxdata(&syncinfo);

		/*
		if(!is_output_initialized)
		{
			ac3_output.open(16,syncinfo.sampling_rate,2);
			is_output_initialized = 1;
		}
		*/
		data = (char *)s16_samples;
		datasize = 0;
		while(datasize < 6144){
		  if(((*input_pointer+1) % AC3_BUFFER_SIZE) != *output_pointer){    // There is room in the sync_buffer
		    ac3_data[*input_pointer]=data[datasize];
		    datasize++;
		    *input_pointer = (*input_pointer+1) % AC3_BUFFER_SIZE;
		  }
		  else{
		    *input_pointer = *output_pointer = 0;
		    break;
		  }
		}


		//write(1, s16_samples, 256 * 6 * 2* 2);
		//ac3_output.play(s16_samples, 256 * 6 * 2);
error:
		;
		//find a new frame
	}

	return 0;	
}
