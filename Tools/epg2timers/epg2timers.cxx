/*
 * epg2timers.cxx: Convert an EPG "merkliste" HTML page (http://tvtv.de)
 *                 to timers.conf format for Klaus Schmidinger's vdr 
 *                 (http://www.cadsoft.de/people/kls/vdr).
 *
 * Copyright (C) 2000, 2001 Carsten Koch
 * 
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * version 2 as published by the Free Software Foundation.
 * 
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 * 
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
 * Or, point your browser to http://www.gnu.org/copyleft/gpl.html
 * 
 * The author can be reached at Carsten.Koch@icem.de
 */
 
 
#include <malloc.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

// User-configurable options.

static const int stop_time_safety_margin = 10;  // add 10 minutes to stop time in case start was delayed
static const int recording_priority = 50;       // vdr recording priority setting for all timer entries generated
static const int recording_lifetime = 98;       // vdr recording life time setting for all timer entries generated


// Usually, you should not want to change any of these.

static const int max_title   = 256;             // maximum length+1 of title file name generated
static const int max_genre   = 32;              // maximum length+1 of genre text parsed
static const int max_line    = 1024;            // line buffer (not used when parsing summary text)
static const int max_summary = 9000;            // Summary can be up to 9000 bytes long (a bit shorter than vdr's SVDRP command buffer)
static const int max_vdr_channel = 1000;        // maximum size+1 of your channels.conf

// The following table maps http://tvtv.de channel names into Astra 19.2E PIDs.
// It is incomplete. Contributions welcome.

typedef struct
{
   const char * tvtv_name;
   unsigned short pnr;
} map_entry;


static const map_entry channel_map[] =
{
   //   Deutschsprachig
   {"13th Street", 42},
   {"3sat", 28007},
   {"ARTE", 28109},
   {"B1", 28206},
   {"BR3", 28107},
   {"BR-alpha", 28112},
   {"ARD", 28106},
   {"Discovery", 14},
   {"Disney Channel", 34},
   {"Eins Extra", 28201},
   {"Eins Festival", 28202},
   {"Eins MuXx", 28203},
   {"Filmpalast", 516},
   {"FOX KIDS", 28},
   {"Heimatkanal", 517},
   {"HR", 28108},
   {"Junior", 19},
   {"Kabel 1", 899},
   {"Kinderkanal", 28008},
   {"Krimi&Co", 23},
   {"K-Toon", 12},
   {"Liberty TV.com", 12199},
   {"MDR", 28204},
   {"NDR", 28224},
   {"NEUN LIVE", 897},
   {"ORB", 28205},
   {"ORF1", 13001},
   {"ORF2", 13002},
   {"Phoenix", 28114},
   {"Planet", 13},
   {"Premiere 1", 10},
   {"Premiere 2", 11},
   {"Premiere 3", 43},
   {"Premiere Action", 20},
   {"Premiere Comedy", 29},
   {"Premiere SCI-FI", 41},
   {"Premiere Star", 9},
   {"PREMIERE WORLD", 8},
   {"ProSieben", 898},
   {"RTL", 12003},
   {"RTL2", 12020},
   {"SAT.1", 46},
   {"SeaSonS", 33},
   {"SR", 28110},
   {"Studio Universal", 36},
   {"Sunset", 16},
   {"Super RTL", 12040},
   {"Test-Z1", 28305},
   {"TW1", 13013},
   {"Via 1 - Schöner Reise", 44},
   {"VOX", 12060},
   {"WDR", 28111},
   {"ZDF", 28006},
   {"ZDF.doku", 28014},
   {"ZDF.info", 28011},
   //  Movies
   {"AXN", 29506},
   {"CANAL+", 29100},
   {"CANAL+ AZUL", 29101},
   {"CANAL+ ROJO", 29102},
   {"CANAL+ VERT", 8208},
   {"CANAL+ 16/9", 8204},
   {"CANAL+ 16|9", 29024},
   {"C+ROOD", 4005},
   {"CINE CINEMA I", 8206},
   {"CINE CINEMA II", 8002},
   {"CINE CINEMA III", 8003},
   {"CINE CLASSICS", 8709},
   {"CINE CINEMA 16/9", 8301},
   {"cinecinemas", 4008},
   {"CINECLASSICS", 29203},
   {"Cinedom 1", 176},
   {"Cinedom 1B", 178},
   {"Cinedom 1C", 180},
   {"Cinedom 1D", 190},
   {"Cinedom 2", 179},
   {"Cinedom 2B", 183},
   {"Cinedom 2C", 184},
   {"Cinedom 2D", 188},
   {"Cinedom 2E", 193},
   {"Cinedom 3", 182},
   {"Cinedom 3B", 185},
   {"Cinedom 3C", 192},
   {"Cinedom 3D", 195},
   {"Cinedom 4", 181},
   {"Cinedom 4B", 187},
   {"Cinedom 4C", 191},
   {"Cinedom 5", 186},
   {"Cinedom 5B", 194},
   {"Cindedom Deluxe", 189},
   {"CINEMANÍA AZUL", 29501},
   {"CINEMANÍA ROJO", 29605},
   {"CINEMANÍA", 29500},
   {"K1", 8401},
   {"K2", 8402},
   {"K3", 8403},
   {"K4", 8404},
   {"K5", 8405},
   {"K6", 8406},
   {"K7", 8407},
   {"K9", 8409},
   {"K12", 8412},
   {"TAQUILLA 1", 29206},
   {"TAQUILLA 2", 29207},
   {"TAQUILLA 3", 29502},
   {"TAQUILLA 4", 29503},
   {"TAQUILLA 5", 29504},
   {"TAQUILLA 6", 29301},
   {"TAQUILLA 7", 29302},
   {"TAQUILLA 8", 29303},
   {"TAQUILLA 11", 29316},
   {"TAQUILLA 12", 29610},
   {"TAQUILLA 13", 29402},
   {"TAQUILLA 14", 29212},
   {"TAQUILLA 16|9", 29606},
   //  Music
   {"40 LATINO", 29031},
   {"40 TV", 29110},
   {"CANAL+ JAUNE", 8203},
   {"CLASSICA", 15},
   {"GOLDSTAR TV", 518},
   {"MCM 2", 8305},
   {"MCM AFRICA", 8307},
   {"MCM", 8302},
   {"MTV 2", 28649},
   {"MTV 6", 28641},
   {"MTV Base", 28645},
   {"MTV Central", 28643},
   {"MTV F", 28642},
   {"MTV Hits", 28644},
   {"MUZZIK", 8007},
   {"RFM TV", 17008},
   {"TMF", 5015},
   {"VH1 Classic", 28647},
   {"VH1", 28646},
   {"Video Italia", 12220},
   {"VIVA ZWEI", 12120},
   {"VIVA", 12732},
   {"ZIK'/XXL", 17004},
   //  News
   {"BBC WORLD", 17007},
   {"Bloomberg TV", 12160},
   {"CNBC", 28010},
   {"CNBC", 35},
   {"CNBC-NBC", 29202},
   {"CNN", 28512},
   {"DW-tv", 9005},
   {"EuroNews", 28015},
   {"FOX NEWS", 29032},
   {"N24", 47},
   {"n-tv", 12730},
   {"Sky News", 3995},
   //  Netherlands
   {"NED1", 4011},
   {"NED2", 4012},
   {"NED3", 4013},
   {"NET5", 5004},
   {"RTL4", 2004},
   {"RTL5", 2005},
   {"SBS6", 5005},
   {"V8/Fox Kids", 5020},
   {"Yorin", 5010},
   //  Porn
   {"BEATE-UHSE.TV", 21},
   {"Blue Movie1", 513},
   {"Blue Movie2", 514},
   {"Blue Movie3", 515},
   {"K10", 8410},
   {"TAQUILLA X", 29213},
   {"TAQUILLA X", 29602},
   {"TAQUILLA XX", 29607},
   {"X-ZONE", 4009},
   //  Sports
   {"C+BLAUW", 4006},
   {"DSF", 900},
   {"EUROSPORT", 8101},
   {"Eurosport", 28009},
   {"EUROSPORT", 29310},
   {"EUROSPORTNEWS", 29037},
   {"PATHE SPORT|", 8009},
   {"PREMIERE SPORT 1", 17},
   {"PREMIERE SPORT 2", 27},
   {"SUPERDOM", 26},
   //  French
   {"13EME RUE", 8703},
   {"AB 1", 17001},
   {"AB MOTEURS", 17000},
   {"ACTION", 17010},
   {"ALLOCINE TV", 8308},
   {"ANIMAUX", 17002},
   {"ARTE", 9009},
   {"BLOOMBERG TV", 8004},
   {"CA TV", 8610},
   {"CANAL+", 8201},
   {"CANAL+ BLEU", 8202},
   {"CANAL J", 8108},
   {"CANAL JIMMY", 8006},
   {"CANALCLUB", 8812},
   {"Cartoon Network", 28511},
   {"CLUB TELEACHAT", 8303},
   {"COMEDIE !", 8702},
   {"CONTACT TV", 8804},
   {"CUISINE.TV", 8112},
   {"DEMAIN !", 8701},
   {"DISNEY CHANNEL", 8207},
   {"DT CSAT 10", 9159},
   {"ENCYCLOPEDIA", 17003},
   {"ESCALES", 17005},
   {"EURONEWS", 8505},
   {"FORUM", 8707},
   {"FRANCE 2", 8801},
   {"FRANCE 3", 8802},
   {"GAME ONE", 8717},
   {"i TELEVISION", 8010},
   {"KIOSQUE", 8704},
   {"KTO", 8304},
   {"LA CHAINE METEO", 8008},
   {"LA CINQUIEME", 8501},
   {"LaChaîneHistoire", 17006},
   {"LCI", 8107},
   {"LCP", 8506},
   {"L'EQUIPE TV", 8706},
   {"LibertyTV.com", 12280},
   {"MANGAS", 17011},
   {"MONTECARLO TMC", 8102},
   {"Motors TV", 12300},
   {"NAT GEOGRAPHIC", 8310},
   {"PAD", 8211},
   {"PARIS PREMIERE", 8104},
   {"PLANETE 2", 8507},
   {"PLANETE", 8103},
   {"PMU sur Canal+", 8210},
   {"RFO SAT", 8708},
   {"SANTE - VIE", 8110},
   {"SEASONS", 8001},
   {"TCM", 28515},
   {"TEST CDN 1", 8616},
   {"TEST CDN 3", 8627},
   {"TiJi", 8309},
   {"TV 5", 9001},
   {"TV BREIZH", 8502},
   {"TV Puls", 20601},
   {"TV5 Europe", 12240},
   {"VOYAGE", 8105},
   //  Spanish
   {"ANDALUCÍA TV", 29011},
   {"Bloomberg", 12721},
   {"CALLE 13", 29609},
   {"Canal Canarias", 29700},
   {"Cartoon Network", 29314},
   {"CNN+", 29020},
   {"DISCOVERY", 29116},
   {"DISNEY CHANNEL", 29111},
   {"DOCUMANÍA", 29200},
   {"ESTILO", 29305},
   {"ETB", 29035},
   {"FASHION TV", 29115},
   {"FOX KIDS", 29209},
   {"FOX", 29507},
   {"MOSAICO", 29315},
   {"MÉTEO", 29014},
   {"Nat Geo Channel", 29034},
   {"NICK-PARAMOUNT", 29312},
   {"RTPI", 9006},
   {"SEASONS", 29204},
   {"TAQUILLA 0", 29205},
   {"TCM.", 28516},
   {"TVC INT.", 29701},
   {"VIAJAR", 29306},
   //  Miscellaneous
   {"Alice", 12200},
   {"Canal Algerie", 9008},
   {"CANALPRO TV", 8516},
   {"ESC1 - EGYPTE", 9003},
   {"FASHION TV.COM", 17009},
   {"Home Shopping Euro", 45},
   {"Home Shopping Euro", 40},
   {"Kabel 1 Austria", 20004},
   {"Kabel 1 Schweiz", 20003},
   {"Polonia 1/Top Sho", 20366},
   {"ProSieben A", 20002},
   {"ProSieben Schweiz", 20001},
   {"QVC GERMANY", 12100},
   {"RAI 1", 9004},
   {"REAL MADRID TV", 29019},
   {"RealityTV", 20309},
   {"RTL TELE Letzebuerg", 3994},
   {"RTM - MAROC", 9002},
   {"SÜDWEST BW", 28113},
   {"SÜDWEST RP", 28231},
   {"Super 1", 20364},
   {"Travel", 28001},
   {"TV7", 9007},
   {"TV-NIEP II", 12740},
   {"Wishline", 12320}
};


// Nothing user-configurable below this line.

static const char date_line[]       = "\t<td align=center valign=middle colspan=3><span id=fb-b10>";
static const char start_time_line[] = " \t\t<td id=\"jobview-box-date\" align=center><nobr>&nbsp;";
static const char stop_time_line[]  = "\t\t\t<tr><td id=\"line\" align=center><span id=\"fn-w9\">bis ";
static const char channel_line[]    = "\t\t\t<tr><td align=center><span id=\"fb-w9\">";
static const char title_line[]      = "\t\t\t\t<td align=left width=100%><span id=\"fb-w10\">";
static const char summary_line[]    = "<span id=\"fn-b8\">";
static const char genre_line[]      = "\t\t\t\t<td align=right valign=center nowrap><span id=\"fn-w10\">";

static const int month_lengths[12] = {31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};




char map_special_char(const char * const word)
      
{
   if (strcmp(word, "auml") == 0)
      return 'ä';
   else if (strcmp(word, "ouml") == 0)
      return 'ö';
   else if (strcmp(word, "uuml") == 0)
      return 'ü';
   else if (strcmp(word, "Auml") == 0)
      return 'Ä';
   else if (strcmp(word, "Ouml") == 0)
      return 'Ö';
   else if (strcmp(word, "Uuml") == 0)
      return 'Ü';
   else if (strcmp(word, "szlig") == 0)
      return 'ß';
   else if (strcmp(word, "nbsp") == 0)
      return ' ';
   else if (strcmp(word, "amp") == 0)
      return '&';
   return ' ';
}





void read_file_name_and_title(const char * const line, char * const file_name, char * const title)
      
{
   int line_index = sizeof(title_line) - 1;
   int title_index = 0;
   int file_name_index = 0;
   char ch;
   do
   {
      ch = line[line_index++];
      if (ch == '&')
      {
         char word[10];
         int i = 0;
         while ((line[line_index + i] != ';') && (i < 9))
         {
            word[i] = line[line_index + i]; i++;
         }
         if (line[line_index + i] == ';')
         {
            word[i] = 0;
            ch = map_special_char(word);
            line_index += i;
         }
      }
      switch (ch)
      {
         case 'ä': file_name[file_name_index++] = 'a'; file_name[file_name_index++] = 'e'; break;
         case 'ö': file_name[file_name_index++] = 'o'; file_name[file_name_index++] = 'e'; break;
         case 'ü': file_name[file_name_index++] = 'u'; file_name[file_name_index++] = 'e'; break;
         case 'Ä': file_name[file_name_index++] = 'A'; file_name[file_name_index++] = 'e'; break;
         case 'Ö': file_name[file_name_index++] = 'O'; file_name[file_name_index++] = 'e'; break;
         case 'Ü': file_name[file_name_index++] = 'U'; file_name[file_name_index++] = 'e'; break;
         case 'ß': file_name[file_name_index++] = 's'; file_name[file_name_index++] = 's'; break;
         case ' ': file_name[file_name_index++] = '_'; break;
         case '&': 
            file_name[file_name_index++] = 'u'; file_name[file_name_index++] = 'n'; file_name[file_name_index++] = 'd'; 
         break;
         default:
            if (((ch >= 'a') && (ch <= 'z')) || ((ch >= 'A') && (ch <= 'Z')) || ((ch >= '0') && (ch <= '9')) || (ch == '-'))
               file_name[file_name_index++] = ch;
      }
      title[title_index++] = ch;
   } while ((file_name_index < max_title-5) && (ch != '<') && (ch != 0));
   file_name[file_name_index] = 0;
   title[title_index-1] = 0;
}



void read_summary(char * const summary)
      
{
   int summary_index = 0;
   int ch;
   bool need_space = false;
   bool done = false;
   do
   {
      ch = getchar();
      switch (ch)
      {
         case '&':
         {
            char word[10];
            int i = 0;
            ch = getchar();
            while ((ch != ';') && (ch != EOF) && (i < 9)) 
            {
               word[i++] = ch;
               ch = getchar();
            }
            word[i] = 0;
            if (need_space) {summary[summary_index++] = ' '; need_space = false;}
            summary[summary_index++] = map_special_char(word);
         }
         break;
         case '<':
         {
            char word[6];
            int word_index = 0;
            do
            {
               ch = getchar();
               word[word_index++] = ch;
            } while ((word_index < 6) && (ch != '>') && (ch != EOF));
            while ((ch != '>') && (ch != EOF)) ch = getchar();
            if (strncmp("/span", word, 4) == 0)
               done = true;
         }
         break;
         default:
         {
            if (ch <= ' ')
            {
               if (summary_index > 0) need_space = true;
            }
            else
            {
               if (need_space) {summary[summary_index++] = ' '; need_space = false;}
               summary[summary_index++] = ch;
            }
         }
      }
   } while ((summary_index < max_summary - 2) && (!done) && (ch != EOF));
   summary[summary_index] = 0;
}



int find_channel_number(const unsigned short * const vdr_pnrs, const char * const channel_name)
      
{
   for (int tvtv_channel_number = 0; tvtv_channel_number < sizeof(channel_map)/sizeof(map_entry); tvtv_channel_number++)
      if (strcmp(channel_name, channel_map[tvtv_channel_number].tvtv_name) == 0)
         for (int vdr_channel_number = 0; vdr_pnrs[vdr_channel_number] != 0xFFFF; vdr_channel_number++)
            if (vdr_pnrs[vdr_channel_number] == channel_map[tvtv_channel_number].pnr)
               return vdr_channel_number;
   fprintf(stderr, "Error - channel '%s' not recognized.\n", channel_name);
   exit(1);
   /*NOTREACHED*/
}




unsigned short * read_vdr_pnrs(const char * const channels_conf_file_name)
      
{
   FILE * channels_conf = fopen(channels_conf_file_name, "r");
   if (channels_conf == NULL)
   {
      perror("unable to open channels.conf.");
      exit(1);
   }
   unsigned short * vdr_pnrs = (unsigned short *) malloc(max_vdr_channel * sizeof(unsigned short));
   int vdr_channel_number = 0;
   while (!feof(channels_conf) && (vdr_channel_number < max_vdr_channel-1))
   {
      char line[1024];
      fgets(line, sizeof(line)-1, channels_conf);
      int pnr;
      if ((line[0] != ':') && 
          (sscanf(line, "%*[^:]:%*[^:]:%*[^:]:%*[^:]:%*[^:]:%*[^:]:%*[^:]:%*[^:]:%*[^:]:%d", &pnr) == 1))
         vdr_pnrs[vdr_channel_number++] = pnr;
   }
   vdr_pnrs[vdr_channel_number++] = 0xFFFF; // sentinel
   fprintf(stderr, "%d pnrs.\n", vdr_channel_number);
   return (unsigned short *) realloc(vdr_pnrs, vdr_channel_number * sizeof(unsigned short));
}




void process_input(const unsigned short * const vdr_pnrs)
      
{

   int channel    = -1;
   int day        = -1;
   int next_day   = -1;
   int start_time = -1;
   int stop_hour  = -1;
   int stop_minute= -1;
   char genre[max_genre] = {0};
   char summary[max_summary] = {0};
   char file_name[max_title] = {0};
   char title[max_title] = {0};

   while (!feof(stdin))
   {
      char line[max_line];
      fgets(line, max_line-1, stdin);
      line[max_line-1] = 0;
      if (strncmp(line, date_line, sizeof(date_line)-1) == 0)
      {
         const int month = (line[sizeof(date_line) + 6]- '0') * 10 + line[sizeof(date_line) + 7]-'0';
         day = (line[sizeof(date_line) + 3]- '0') * 10 + line[sizeof(date_line) + 4]-'0';
         next_day = day == month_lengths[month-1]? 1 : day + 1;
      }
      else if (strncmp(line, start_time_line, sizeof(start_time_line)-1) == 0)
         start_time = (line[sizeof(start_time_line) - 1] - '0') * 1000 +
                      (line[sizeof(start_time_line)    ] - '0') * 100 +
                      (line[sizeof(start_time_line) + 2] - '0') * 10 +
                      (line[sizeof(start_time_line) + 3] - '0');
      else if (strncmp(line, stop_time_line, sizeof(stop_time_line)-1) == 0)
      {
         stop_hour =   (line[sizeof(stop_time_line) - 1] - '0') * 10 +
                       (line[sizeof(stop_time_line)    ] - '0');
         stop_minute = (line[sizeof(stop_time_line) + 2] - '0') * 10 +
                       (line[sizeof(stop_time_line) + 3] - '0') + 
                       stop_time_safety_margin;
         if (stop_minute > 59)
         {
            stop_minute -= 60;
            if (stop_hour == 23)
               stop_hour = 0;
            else
               stop_hour++;
         }
         if ((day < 0) || (start_time < 0) || (file_name[0] == 0) || (channel == -1))
         {
            fprintf(stderr, "Input data error.\n");
            exit(1);
         }
         else
            printf("1:%03d:%02d:%04d:%02d%02d:%d:%d:%s:\"%s\" %s||%s||||||(epg2timers)\n", 
                   channel+1, start_time < 600? next_day : day, start_time, stop_hour, stop_minute, 
                   recording_priority, recording_lifetime, file_name, 
                   title, genre, summary);
         start_time = -1; channel = -1;
         file_name[0] = 0; summary[0] = 0; genre[0] = 0;
      }
      else if (strncmp(line, title_line, sizeof(title_line)-1) == 0)
         read_file_name_and_title(line, file_name, title);
      else if (strncmp(line, channel_line, sizeof(channel_line)-1) == 0)
      {
         int i = sizeof(channel_line);
         while ((line[i] != '<') && (line[i] != 0)) i++;
         line[i] = 0; // end of string
         channel = find_channel_number(vdr_pnrs, line + sizeof(channel_line) - 1);
      }
      else if (strncmp(line, summary_line, sizeof(summary_line)-1) == 0)
         read_summary(summary);
      else if (strncmp(line, genre_line, sizeof(genre_line)-1) == 0)
      {
         int genre_index;
         for (genre_index = 0; genre_index < max_genre-1; genre_index++)
         {
            const char ch = line[genre_index + sizeof(genre_line)-1];
            if ((ch == 0) || (ch == '&') || (ch == '<'))
               break;
            genre[genre_index] = ch;
         }
         genre[genre_index] = 0;
      }
   }
}




main(int argc, char *argv[])

{
   fprintf(stderr, "epg2timers Version 0.5, 15-Sep-2001.\n");
   
   if (argc != 2)
   {
      fprintf(stderr, "usage: %s channels.conf\n", argv[0]);
      exit(1);
   }
   
   const unsigned short * const vdr_pnrs = read_vdr_pnrs(argv[1]);
   process_input(vdr_pnrs);
   exit(0);
}
