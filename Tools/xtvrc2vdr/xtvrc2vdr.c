/*
 *  * xtvrc2vdr.c: Converts 'xtvrc' files to 'vdr' channel format
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
} CHANNEL_DATA ;

void strlwr( char *s ){
   while ( s && *s ){
     *s = tolower(*s);
     s++;
   }
}

int ReadChannel( FILE *f, CHANNEL_DATA *channel ) {
   static char s[MAX_LINE_LEN+1];
   char *p;

   memset( channel, sizeof( CHANNEL_DATA ), 0 ) ;
   
   while ((p=fgets( s, MAX_LINE_LEN, f ))!=NULL){
//    printf("%s", s ) ;
    if (s[0] == '*')
       break ;
   }
   
   if ( !p ) { /* EOF? */
//      printf("EOF\n");
     return 0 ;
   }
     
   while (fgets( s, MAX_LINE_LEN, f )){
      if ( s[0] == '\n' )
	return  channel->freq ? 1 : 0;
      p = strtok( s, TOKS ) ;
      if ( !p ) {
	return 0;
      }
      strlwr( p ) ;
      if ( !strcmp( p, "channel" )){
	 p=strtok( NULL, NAMETOKS );
	 while ( p && *p==' ')
	   p++;
	 strcpy( channel->Name, p );
//	 printf("%d ", channel->freq ) ;
      } else if ( !strcmp( p, "frequency")) {
	 channel->freq = atoi( p=strtok( NULL, TOKS ));
//	 printf("%d ", channel->freq ) ;
      } else if ( !strcmp( p, "cbhc")) {
	 channel->color = atoi(p=strtok(NULL,TOKS));
	 channel->hue = atoi(p=strtok(NULL,TOKS));
	 channel->bright = atoi(p=strtok(NULL,TOKS));
	 channel->saturation = atoi(p=strtok(NULL,TOKS));
      } else if ( !strcmp( p, "ni")) {
	 channel->nitv = atoi(p=strtok(NULL,TOKS));
	 channel->input = atoi(p=strtok(NULL,TOKS));
      } else if ( !strcmp( p, "sat")) {
	 channel->pol = atoi(p=strtok(NULL,TOKS));
	 channel->srate = atoi(p=strtok(NULL,TOKS));
	 channel->fec = atoi(p=strtok(NULL,TOKS));
	 channel->vpid = atoi(p=strtok(NULL,TOKS));
	 channel->apid = atoi(p=strtok(NULL,TOKS));
	 channel->lnbnum = atoi(p=strtok(NULL,TOKS));
	 channel->type = atoi(p=strtok(NULL,TOKS));
      } else
	printf("Unknown token %s\n", p ) ;
   }
   return 1 ;
}

int main ( int argc, char *argv[] ){
   FILE *f, *fo ;
   int cnt = 0;
   CHANNEL_DATA channel ;
   
   if ( argc != 3 ){
      printf("USAGE: %s <xtvrc file> <vdr file>\n\n", argv[0] ) ;
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
	     0 ); //channel.fec ) ;
   }
	  
   printf( "%d channels read.\n\n", cnt ) ;
   
   fclose(f);
   fclose(fo);
   return 1;
}
