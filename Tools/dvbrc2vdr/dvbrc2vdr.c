/*
 *  * dvbrc2vdr.c: Converts 'xtvrc' files to 'vdr' channel format
 *  *
 *  * Copyright (C) 2000 Plamen Ganev
 *  * 
 *  * This program is free software; you can redistribute it and/or
 *  * modify it under the terms of the GNU General Public License
 *  * as published by the Free Software Foundation; either version 2
 *  * of the License, or (at your option) any later version.
 *  * 
 *  * This program is distributed in the hope that it will be useful,
 *  * but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  * GNU General Public License for more details.
 *  * 
 *  * You should have received a copy of the GNU General Public License
 *  * along with this program; if not, write to the Free Software
 *  * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
 *  * Or, point your browser to http://www.gnu.org/copyleft/gpl.html
 *  * 
 *  * The author can be reached at pganev@comm.it
 *  *
 *  */


#include <stdio.h>
#include <ctype.h>
#include <string.h>
#include <stdlib.h>

#define MAX_LINE_LEN 1024
#define MAX_NAME 100
#define TOKS ": \n\r"
#define NAMETOKS ":\n\r"

typedef struct {
   char Name[MAX_NAME+1];
   int freq;
   int color, hue, bright, saturation ;
   int nitv, input ;
   int pol, srate, fec, vpid, apid, lnbnum, type;
   int sid, pcrpid ;
} CHANNEL_DATA ;

void strupr( char *s ){
   while ( s && *s ){
     *s = toupper(*s);
     s++;
   }
}

/* Warning: This function uses the last strtol string! */
int GetTpInfo( CHANNEL_DATA *channel )
{
   // s is: ID x SATID x TYPE x FREQ x POL H/V SRATE x FEC x
   char *p ;
   
   p = strtok( NULL, TOKS ) ;  /* Skip ID */
   p = strtok( NULL, TOKS ) ;  /* Skip x  */
   p = strtok( NULL, TOKS ) ;  /* Skip SatId */
   p = strtok( NULL, TOKS ) ;  /* Skip x */
   p = strtok( NULL, TOKS ) ;  /* Skip Type */
   p = strtok( NULL, TOKS ) ;  /* Skip x */
   p = strtok( NULL, TOKS ) ;  /* Skip Freq */
   p = strtok( NULL, TOKS ) ;  /* Get Freq */
   channel->freq = atol( p ) / 1000 ; 
   p = strtok( NULL, TOKS ) ;  /* Skip Pol */
   p = strtok( NULL, TOKS ) ;  /* Get H/V */
   channel->pol = (*p=='V') ? 1 : 0 ;
   p = strtok( NULL, TOKS ) ;  /* Skip SRATE */
   p = strtok( NULL, TOKS ) ;  /* Get srate */
   channel->srate = atol(p) / 1000 ; /* Convert SRATE */
   return 0;
}

/* Warning: This function uses the last strtol string! */
int GetChInfo( CHANNEL_DATA *channel )
{
   /* s is: ID x NAME "name" SATID x TPID x SID x TYPE x VPID x APID x  */
   char *p, *q ;

   p = strtok( NULL, TOKS ) ;
   while ( p ) {
      if ( !strcmp( p, "ID" )) {
	 p = strtok( NULL, TOKS ) ;
      } else if ( !strcmp( p, "NAME")) {
	 while ( *p++ );     /* Jump to end of "NAME" */
	 p++ ;               /* One More */
	 while ( *p == '"' ) p++ ;  /* Skip the " */
	 q = channel->Name ;
	 while ( *p != '"' ) 
	   if ( *p == ':' )
	     *q++ = '|', p++ ;
	   else
   	     *q++ = *p++ ;     /* Copy the name */
	 *q = 0 ;
	 p++ ;
	 p = strtok( p, TOKS ) ;
	 channel->apid = 8190;
	 channel->vpid = 8190;
	 channel->pcrpid = 0 ;
	 channel->sid = 0;
      } else if ( !strcmp( p, "VPID")) {
	 p = strtok( NULL, TOKS ) ;
	 channel->vpid = strtol( p, NULL, 16 ) ;
	 p = strtok( NULL, TOKS ) ;
      } else if ( !strcmp( p, "APID")) {
	 p = strtok( NULL, TOKS ) ;
	 channel->apid = strtol( p, NULL, 16 ) ;
	 p = strtok( NULL, TOKS ) ;
      } else if ( !strcmp( p, "SID")) {
	 p = strtok( NULL, TOKS ) ;
	 channel->sid = strtol( p, NULL, 16 ) ;
	 p = strtok( NULL, TOKS ) ;
      } else if ( !strcmp( p, "PCRPID")) {
	 p = strtok( NULL, TOKS ) ;
	 channel->pcrpid = strtol( p, NULL, 16 ) ;
	 p = strtok( NULL, TOKS ) ;
      } else {
	 p = strtok( NULL, TOKS ) ;
      }
    }
   return 1;
}

int ReadChannel( FILE *f, CHANNEL_DATA *channel ) {
   static char s[MAX_LINE_LEN+1];
   char *p;

   while (fgets( s, MAX_LINE_LEN, f )){
      p = strtok( s, TOKS ) ;
      strupr( p ) ;
      
      if ( !strcmp( p, "TRANSPONDER" )){
	 GetTpInfo( channel ) ;
      } else if ( !strcmp( p, "CHANNEL" ) ) {
	 GetChInfo( channel ) ;
	 return 1 ;
      }
   }
   return 0 ;
}

int main ( int argc, char *argv[] ){
   FILE *f, *fo ;
   int cnt = 0;
   CHANNEL_DATA channel ;
   
   if ( argc != 3 ){
      printf("USAGE: %s <dvbrc file> <vdr file>\n\n", argv[0] ) ;
      return 0;
   }
   
   if ( !(f=fopen(argv[1], "rt"))){
      printf("Can't open %s for reading\n\n", argv[1]);
      return 0;
   }
   
   if ( !(fo=fopen(argv[2], "wt"))){
      printf("Can't open %s for writing\n\n", argv[2]);
      return 0;
   }
   
   while ( ReadChannel( f, &channel ) ) {
      cnt++;
      fprintf(fo, "%s:%d:%c:%d:%d:%d:%d:%d:%d\n",
	     channel.Name ,
	     channel.freq ,
	     channel.pol ? 'v' : 'h' ,
	     1, //channel.lnbnum ,
	     channel.srate ,
	     channel.vpid ,
	     channel.apid ,
	     0, //channel.type ,
	     channel.sid ) ;
   }
	  
   printf( "%d channels read.\n\n", cnt ) ;
   
   fclose(f);
   fclose(fo);
   return 1;
}
