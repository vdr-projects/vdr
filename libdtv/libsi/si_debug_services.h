//////////////////////////////////////////////////////////////
///                                                        ///
/// si_debug_services.h: local debugging definitions       ///
///                                                        ///
//////////////////////////////////////////////////////////////

// $Revision: 1.2 $
// $Date: 2003/02/04 18:45:35 $
// $Author: hakenes $
//
//   (C) 2001 Rolf Hakenes <hakenes@hippomi.de>, under the GNU GPL.
//
// libsi is free software; you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation; either version 2, or (at your option)
// any later version.
//
// libsi is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// You may have received a copy of the GNU General Public License
// along with libsi; see the file COPYING.  If not, write to the
// Free Software Foundation, Inc., 59 Temple Place - Suite 330,
// Boston, MA 02111-1307, USA.


struct component_type {
   u_char   Content;
   u_char   Type;
   char    *Description;
};

static struct component_type ComponentTypes[] = {
   { 0x01, 0x01, "video, 4:3 aspect ratio, 25 Hz" },
   { 0x01, 0x02, "video, 16:9 aspect ratio with pan vectors, 25 Hz" },
   { 0x01, 0x03, "video, 16:9 aspect ratio without pan vectors, 25 Hz" },
   { 0x01, 0x04, "video, > 16:9 aspect ratio, 25 Hz" },
   { 0x01, 0x05, "video, 4:3 aspect ratio, 30 Hz" },
   { 0x01, 0x06, "video, 16:9 aspect ratio with pan vectors, 30 Hz" },
   { 0x01, 0x07, "video, 16:9 aspect ratio without pan vectors, 30 Hz" },
   { 0x01, 0x08, "video, > 16:9 aspect ratio, 30 Hz" },
   { 0x01, 0x09, "HD video, 4:3 aspect ratio, 25 Hz" },
   { 0x01, 0x0A, "HD video, 16:9 aspect ratio with pan vectors, 25 Hz" },
   { 0x01, 0x0B, "HD video, 16:9 aspect ratio without pan vectors, 25 Hz" },
   { 0x01, 0x0C, "HD video, > 16:9 aspect ratio, 25 Hz" },
   { 0x01, 0x0D, "HD video, 4:3 aspect ratio, 30 Hz" },
   { 0x01, 0x0E, "HD video, 16:9 aspect ratio with pan vectors, 30 Hz" },
   { 0x01, 0x0F, "HD video, 16:9 aspect ratio without pan vectors, 30 Hz" },
   { 0x01, 0x10, "HD video, > 16:9 aspect ratio, 30 Hz" },
   { 0x02, 0x01, "audio, single mono channel" },
   { 0x02, 0x02, "audio, dual mono channel" },
   { 0x02, 0x03, "audio, stereo (2 channel)" },
   { 0x02, 0x04, "audio, multi lingual, multi channel" },
   { 0x02, 0x05, "audio, surround sound" },
   { 0x02, 0x40, "audio description for the visually impaired" },
   { 0x02, 0x41, "audio for the hard of hearing" },
   { 0x03, 0x01, "EBU Teletext subtitles" },
   { 0x03, 0x02, "associated EBU Teletext" },
   { 0x03, 0x03, "VBI data" },
   { 0x03, 0x10, "DVB subtitles (normal), no aspect criticality" },
   { 0x03, 0x11, "DVB subtitles (normal), aspect 4:3 only" },
   { 0x03, 0x12, "DVB subtitles (normal), aspect 16:9 only" },
   { 0x03, 0x13, "DVB subtitles (normal), aspect 2.21:1 only" },
   { 0x03, 0x20, "DVB subtitles (hard of hearing), no aspect criticality" },
   { 0x03, 0x21, "DVB subtitles (hard of hearing), aspect 4:3 only" },
   { 0x03, 0x22, "DVB subtitles (hard of hearing), aspect 16:9 only" },
   { 0x03, 0x23, "DVB subtitles (hard of hearing), aspect 2.21:1 only" }
};
#define COMPONENT_TYPE_NUMBER 35


struct service_type {
   u_char   Type;
   char    *Description;
};

static struct service_type ServiceTypes[] = {
   { 0x01, "digital television service" },
   { 0x02, "digital radio sound service" },
   { 0x03, "Teletext service" },
   { 0x04, "NVOD reference service" },
   { 0x05, "NVOD time-shifted service" },
   { 0x06, "mosaic service" },
   { 0x07, "PAL coded signal" },
   { 0x08, "SECAM coded signal" },
   { 0x09, "D/D2-MAC" },
   { 0x0A, "FM Radio" },
   { 0x0B, "NTSC coded signal" },
   { 0x0C, "data broadcast service" },
   { 0x0D, "common interface data" },
   { 0x0E, "RCS Map" },
   { 0x0F, "RCS FLS" },
   { 0x10, "DVB MHP service" }
};
#define SERVICE_TYPE_NUMBER 16


struct content_type {
   u_char   Nibble1;
   u_char   Nibble2;
   char    *Description;
};

static struct content_type ContentTypes[] = {
   /*   Movie/Drama: */
   { 0x01, 0x00, "movie/drama (general)" },
   { 0x01, 0x01, "detective/thriller" },
   { 0x01, 0x02, "adventure/western/war" },
   { 0x01, 0x03, "science fiction/fantasy/horror" },
   { 0x01, 0x04, "comedy" },
   { 0x01, 0x05, "soap/melodrama/folkloric" },
   { 0x01, 0x06, "romance" },
   { 0x01, 0x07, "serious/classical/religious/historical movie/drama" },
   { 0x01, 0x08, "adult movie/drama" },
   /*   News/Current affairs: */
   { 0x02, 0x00, "news/current affairs (general)" },
   { 0x02, 0x01, "news/weather report" },
   { 0x02, 0x02, "news magazine" },
   { 0x02, 0x03, "documentary" },
   { 0x02, 0x04, "discussion/interview/debate" },
   /*   Show/Game show: */
   { 0x03, 0x00, "show/game show (general)" },
   { 0x03, 0x01, "game show/quiz/contest" },
   { 0x03, 0x02, "variety show" },
   { 0x03, 0x03, "talk show" },
   /*   Sports: */
   { 0x04, 0x00, "sports (general)" },
   { 0x04, 0x01, "special events (Olympic Games, World Cup etc.)" },
   { 0x04, 0x02, "sports magazines" },
   { 0x04, 0x03, "football/soccer" },
   { 0x04, 0x04, "tennis/squash" },
   { 0x04, 0x05, "team sports (excluding football)" },
   { 0x04, 0x06, "athletics" },
   { 0x04, 0x07, "motor sport" },
   { 0x04, 0x08, "water sport" },
   { 0x04, 0x09, "winter sports" },
   { 0x04, 0x0A, "equestrian" },
   { 0x04, 0x0B, "martial sports" },
   /*   Children's/Youth programmes: */
   { 0x05, 0x00, "children's/youth programmes (general)" },
   { 0x05, 0x01, "pre-school children's programmes" },
   { 0x05, 0x02, "entertainment programmes for 6 to14" },
   { 0x05, 0x03, "entertainment programmes for 10 to 16" },
   { 0x05, 0x04, "informational/educational/school programmes" },
   { 0x05, 0x05, "cartoons/puppets" },
   /*   Music/Ballet/Dance: */
   { 0x06, 0x00, "music/ballet/dance (general)" },
   { 0x06, 0x01, "rock/pop" },
   { 0x06, 0x02, "serious music/classical music" },
   { 0x06, 0x03, "folk/traditional music" },
   { 0x06, 0x04, "jazz" },
   { 0x06, 0x05, "musical/opera" },
   { 0x06, 0x06, "ballet" },
   /*   Arts/Culture (without music): */
   { 0x07, 0x00, "arts/culture (without music, general)" },
   { 0x07, 0x01, "performing arts" },
   { 0x07, 0x02, "fine arts" },
   { 0x07, 0x03, "religion" },
   { 0x07, 0x04, "popular culture/traditional arts" },
   { 0x07, 0x05, "literature" },
   { 0x07, 0x06, "film/cinema" },
   { 0x07, 0x07, "experimental film/video" },
   { 0x07, 0x08, "broadcasting/press" },
   { 0x07, 0x09, "new media" },
   { 0x07, 0x0A, "arts/culture magazines" },
   { 0x07, 0x0B, "fashion" },
   /*   Social/Political issues/Economics: */
   { 0x08, 0x00, "social/political issues/economics (general)" },
   { 0x08, 0x01, "magazines/reports/documentary" },
   { 0x08, 0x02, "economics/social advisory" },
   { 0x08, 0x03, "remarkable people" },
   /*   Children's/Youth programmes: */
   /*   Education/ Science/Factual topics: */
   { 0x09, 0x00, "education/science/factual topics (general)" },
   { 0x09, 0x01, "nature/animals/environment" },
   { 0x09, 0x02, "technology/natural sciences" },
   { 0x09, 0x03, "medicine/physiology/psychology" },
   { 0x09, 0x04, "foreign countries/expeditions" },
   { 0x09, 0x05, "social/spiritual sciences" },
   { 0x09, 0x06, "further education" },
   { 0x09, 0x07, "languages" },
   /*   Leisure hobbies: */
   { 0x0A, 0x00, "leisure hobbies (general)" },
   { 0x0A, 0x01, "tourism/travel" },
   { 0x0A, 0x02, "handicraft" },
   { 0x0A, 0x03, "motoring" },
   { 0x0A, 0x04, "fitness & health" },
   { 0x0A, 0x05, "cooking" },
   { 0x0A, 0x06, "advertisement/shopping" },
   { 0x0A, 0x07, "gardening" },
   { 0x0B, 0x00, "original language" },
   { 0x0B, 0x01, "black & white" },
   { 0x0B, 0x02, "unpublished" },
   { 0x0B, 0x03, "live broadcast" }
};
#define CONTENT_TYPE_NUMBER 79

static char *StreamTypes[] = {
   "ITU-T|ISO/IEC Reserved",
   "ISO/IEC Video",
   "13818-2 Video or 11172-2 constrained parameter video stream",
   "ISO/IEC 11172 Audio",
   "ISO/IEC 13818-3 Audio",
   "private_sections",
   "packets containing private data / Videotext",
   "ISO/IEC 13522 MPEG",
   "ITU-T Rec. H.222.1",
   "ISO/IEC 13818-6 type A",
   "ISO/IEC 13818-6 type B",
   "ISO/IEC 13818-6 type C",
   "ISO/IEC 13818-6 type D",
   "ISO/IEC 13818-1 auxiliary",
   "ITU-T Rec. H.222.0 | ISO 13818-1 Reserved",
   "User private"
};

static char *CaIdents[] = {
   "Standardized systems",
   "Canal Plus",
   "CCETT",
   "Deutsche Telecom",
   "Eurodec",
   "France Telecom",
   "Irdeto",
   "Jerrold/GI",
   "Matra Communication",
   "News Datacom",
   "Nokia",
   "Norwegian Telekom",
   "NTL",
   "Philips",
   "Scientific Atlanta",
   "Sony",
   "Tandberg Television",
   "Thomson",
   "TV/Com",
   "HPT - Croatian Post and Telecommunications",
   "HRT - Croatian Radio and Television",
   "IBM",
   "Nera",
   "BetaTechnik"
};
#define MAX_CA_IDENT 24
