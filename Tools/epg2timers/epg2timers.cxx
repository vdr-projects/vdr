/*
 * epg2timers.cxx: Convert an EPG "merkliste" page (http://www.tvtv.de) to a timers.conf 
 *                 file for Klaus Schmidinger's vdr (http://www.cadsoft.de/people/kls/vdr).
 *
 * Copyright (C) 2000 Carsten Koch
 * 
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
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
 
 
#include <stdio.h>
#include <string.h>


static const char date_line[]       = "\t<td align=center valign=middle colspan=3><span id=fb-b10>";
static const char start_time_line[] = " \t\t<td bgcolor=\"#7f98bf\" align=center><span id=\"fb-w14\"><nobr>&nbsp;";
static const char stop_time_line[]  = "\t\t\t<tr><td bgcolor=\"#002b64\" align=center><span id=\"fn-w9\">bis ";
static const char channel_line[]    = "\t\t\t<tr><td bgcolor=\"#002b64\" align=center><span id=\"fb-w9\">";
static const char title_line[]      = "\t\t\t\t<td bgcolor=\"#002b64\" align=left width=100%><span id=\"fb-w10\">";
static const char summary_line[]    = "\t\t\t<table border=0 cellpadding=10 cellspacing=0 bgcolor=\"white\" width=100%>";
static const char * const channel_names[] =
{"RTL", "SAT1", "PRO7", "RTL2", "ARD", "BR3", "HR3", "NDR", "SWF", "WDR", "BR Alpha", "SWR BW", "Phoenix",
 "ZDF", "3sat", "Kinderkanal", "ARTE", "phoenix", "ORF Sat", "ZDF.info", "CNN", "Super RTL", "VOX", "DW TV",
 "Kabel1", "TM3", "DSF", "HOT", "BloombergTV", "Sky News", "KinderNet", "Alice", "n-tv", "Grand Tour.", "TW1",
 "Eins Extra", "Eins Festival", "Eins MuXx", "MDR", "ORB", "B1", "ARD Online-Kanal", "Premiere World Promo",
 "Premiere", "Star Kino", "Cine Action", "Cine Comedy", "Sci Fantasy", "Romantic Movies", "Studio Universal",
 "TV Niepokalanow", "Mosaico", "Andalucia TV", "TVC Internacional", "Nasza TV", "WishLine test", "Pro 7 Austria", 
 "Kabel 1 Schweiz", "Kabel 1 Austria", "Pro 7 Schweiz", "Kiosque", "KTO", "TCM", "Cartoon Network France & Spain", 
 "TVBS Europe", "TVBS Europe", "Travel", "TCM Espania", "MTV Spain", "TCM France", "RTL2 CH",
 "La Cinquieme", "ARTE", "Post Filial TV", "Canal Canaris", "Canal Canaris", "Canal Canaris", "Canal Canaris",
 "AB Sat Passion promo", "AB Channel 1", "Taquilla 0", "CSAT", "Mosaique", "Mosaique 2", "Mosaique 3", "Le Sesame C+", 
 "FEED", "RTM 1", "ESC 1", "TV5 Europe", "TV7 Tunisia", "ARTE", "RAI Uno", "RTP International",
 "Fashion TV", "VideoService", "Beta Research promo", "Canal Canarias", "TVC International", "Fitur", "Astra Info 1", 
 "Astra Info 2", "Astra Vision 1", "Astra Vision 1", "Astra Vision 1", "Astra Vision 1", "Astra Vision 1", 
 "Astra Vision 1", "Astra Vision 1", "RTL Tele Letzebuerg", "Astra Mosaic", "MHP test", "Bloomberg TV Spain", 
 "Video Italia", "AC 3 promo", ""
};
static const int month_lengths[12] = {31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};

static const int max_channel =  sizeof(channel_names)/sizeof(char *);
static const int max_title = 50;                // maximum length of title file name generated
static const int max_line = 1024;               // line buffer (not used when parsing summary text)
static const int max_summary = 5000;            // Summary can be up to 5000 bytes long
static const int stop_time_safety_margin = 10;  // add 10 minutes to stop time in case start was delayed



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
   return ' ';
}





void read_file_name(const char * const line, char * const file_name)
      
{
   int line_index = sizeof(title_line) - 1;
   int title_index = 0;
   char ch = line[line_index++];
   do
   {
      if (ch == '&')
      {
         char word[10];
         int i = 0;
         while ((line[line_index + i] != ';') && (i < 9))
            word[i++] = line[line_index + i];
         word[i] = 0;
         ch = map_special_char(word);
         line_index += i;
      }
      switch (ch)
      {
         case 'ä': file_name[title_index++] = 'a'; file_name[title_index++] = 'e'; break;
         case 'ö': file_name[title_index++] = 'o'; file_name[title_index++] = 'e'; break;
         case 'ü': file_name[title_index++] = 'u'; file_name[title_index++] = 'e'; break;
         case 'Ä': file_name[title_index++] = 'A'; file_name[title_index++] = 'e'; break;
         case 'Ö': file_name[title_index++] = 'O'; file_name[title_index++] = 'e'; break;
         case 'Ü': file_name[title_index++] = 'U'; file_name[title_index++] = 'e'; break;
         case 'ß': file_name[title_index++] = 's'; file_name[title_index++] = 's'; break;
         default:
            if (((ch >= 'a') && (ch <= 'z')) || ((ch >= 'A') && (ch <= 'Z')) || ((ch >= '0') && (ch <= '9')))
               file_name[title_index++] = ch;
      }
      ch = int(line[line_index++]);
   } while ((title_index < max_title-1) && (ch != '<') && (ch != 0) && (line_index < max_line-1));
   file_name[title_index] = 0;
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
            if (strncmp("/table", word, 6) == 0)
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




main()

{
   int channel    =  0;
   int day        = -1;
   int next_day   = -1;
   int start_time = -1;
   int stop_time  = -1;
   char summary[max_summary] = {0};
   char file_name[max_title] = {0};

   while (!feof(stdin))
   {
      char line[max_line];
      fgets(line, max_line-1, stdin);
      if (strncmp(line, date_line, sizeof(date_line)-1) == 0)
      {
         const int month = (line[sizeof(date_line) + 6]- '0') * 10 + line[sizeof(date_line) + 7]-'0';
         day = (line[sizeof(date_line) + 3]- '0') * 10 + line[sizeof(date_line) + 4]-'0';
         next_day = day == month_lengths[month]? 1 : day + 1;
      }
      else if (strncmp(line, start_time_line, sizeof(start_time_line)-1) == 0)
      {
         start_time = (line[sizeof(start_time_line) - 1] - '0') * 1000 +
                      (line[sizeof(start_time_line)    ] - '0') * 100 +
                      (line[sizeof(start_time_line) + 2] - '0') * 10 +
                      (line[sizeof(start_time_line) + 3] - '0');
      }
      else if (strncmp(line, stop_time_line, sizeof(stop_time_line)-1) == 0)
      {
         stop_time = ((line[sizeof(stop_time_line) - 1] - '0') * 1000 +
                      (line[sizeof(stop_time_line)    ] - '0') * 100 +
                      (line[sizeof(stop_time_line) + 2] - '0') * 10 +
                      (line[sizeof(stop_time_line) + 3] - '0') + stop_time_safety_margin) % 2400;
         if ((day < 0) || (start_time < 0) || (file_name[0] == 0) || (channel == max_channel))
            fprintf(stderr, "Input data error.\n");
         else
            printf("1:%03d:%02d:%04d:%04d:2:7:%s:%s\n", channel+1, start_time < 600? next_day : day, start_time, stop_time, file_name, summary);
         start_time = -1; stop_time = -1; file_name[0] = 0; summary[0] = 0; channel = max_channel;
      }
      else if (strncmp(line, title_line, sizeof(title_line)-1) == 0)
         read_file_name(line, file_name);
      else if (strncmp(line, channel_line, sizeof(channel_line)-1) == 0)
      {
         int i = sizeof(channel_line);
         while ((i < max_line-1) && (line[i] != '<')) i++;
         line[i] = 0; // end of string
         for (channel = 0; (channel < max_channel) && 
                           (strcmp(line + sizeof(channel_line) - 1, channel_names[channel]) != 0);
              channel++);
         if (channel == max_channel)
            fprintf(stderr, "Error - channel '%s' not recognized.\n", line + sizeof(channel_line) - 1);
      }
      else if (strncmp(line, summary_line, sizeof(summary_line)-1) == 0)
         read_summary(summary);
   }
}
