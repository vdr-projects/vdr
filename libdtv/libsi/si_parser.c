//////////////////////////////////////////////////////////////
///                                                        ///
/// si_parser.c: main parsing functions of libsi           ///
///                                                        ///
//////////////////////////////////////////////////////////////

// $Revision: 1.6 $
// $Date: 2002/01/30 17:04:13 $
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

#include <stdio.h>
#include <math.h>
#include <time.h>

#include "../liblx/liblx.h"
#include "libsi.h"
#include "si_tables.h"



struct LIST *siParsePAT (u_char *Buffer) 
{
   pat_t               *Pat;
   pat_prog_t          *PatProgram;
   u_char              *Ptr;
   int                  SectionLength;
   int                  TransportStreamID;
   int                  PatVersion;
   struct Program      *Program;
   struct LIST         *ProgramList = NULL;

   if (!Buffer) return NULL;

   Pat = (pat_t *) Buffer; Ptr = Buffer;

   if (Pat->table_id != TID_PAT) {
//      fprintf (stderr, "PAT: wrong TID %d\n", Pat->table_id);
      return NULL;
   }

   SectionLength = HILO (Pat->section_length) + 3 - PAT_LEN - 4;

   if (crc32 (Ptr, HILO (Pat->section_length) + 3)) return (NULL);

   TransportStreamID = HILO (Pat->transport_stream_id);
   PatVersion = Pat->version_number;

   Ptr += PAT_LEN;

   while (SectionLength > 0)
   {
      PatProgram = (pat_prog_t *) Ptr;

      CreateProgram (Program, HILO (PatProgram->program_number),
         TransportStreamID, HILO (PatProgram->network_pid), PatVersion);

      if (!ProgramList) ProgramList = xNewList (NULL);
      xAddTail (ProgramList, Program);

      SectionLength -= PAT_PROG_LEN;
      Ptr += PAT_PROG_LEN;
   }

   return (ProgramList);
}


struct Pid *siParsePMT (u_char *Buffer) 
{
   pmt_t               *Pmt;
   pmt_info_t          *PmtInfo;
   u_char              *Ptr;
   int                  SectionLength, ProgramInfoLength,
                        StreamLength, LoopLength;
   int                  ProgramID;
   int                  PcrID;
   int                  PmtVersion;
   struct Pid          *Pid;
   struct PidInfo      *PidInfo;

   if (!Buffer) return NULL;

   Pmt = (pmt_t *) Buffer; Ptr = Buffer;

   if (Pmt->table_id != TID_PMT) {
//      fprintf (stderr, "PMT: wrong TID %d\n", Pmt->table_id);
      return NULL;
   }

   SectionLength = HILO (Pmt->section_length) + 3 - 4;

   if (crc32 (Ptr, HILO (Pmt->section_length) + 3)) return (NULL);

   ProgramInfoLength = HILO (Pmt->program_info_length);
   StreamLength = SectionLength - ProgramInfoLength - PMT_LEN;

   ProgramID = HILO (Pmt->program_number);
   PmtVersion = Pmt->version_number;
   PcrID = HILO (Pmt->PCR_PID);

   Ptr += PMT_LEN;

   CreatePid (Pid, ProgramID, PcrID, PmtVersion);

   if (StreamLength >= 0) siParseDescriptors (Pid->Descriptors, Ptr,
                             ProgramInfoLength, Pmt->table_id);

   Ptr += ProgramInfoLength;

   while (StreamLength > 0)
   {
      PmtInfo = (pmt_info_t *) Ptr;

      CreatePidInfo (PidInfo, PmtInfo->stream_type,
                        HILO (PmtInfo->elementary_PID));

      LoopLength = HILO (PmtInfo->ES_info_length);
      Ptr += PMT_INFO_LEN;

      siParseDescriptors (PidInfo->Descriptors, Ptr, LoopLength, Pmt->table_id);

      xAddTail (Pid->InfoList, PidInfo);

      StreamLength -= LoopLength + PMT_INFO_LEN;
      Ptr += LoopLength;
   }

   return (Pid);
}


struct LIST *siParseSDT (u_char *Buffer) 
{
   sdt_t               *Sdt;
   sdt_descr_t         *SdtDescriptor;
   u_char              *Ptr;
   int                  SectionLength, LoopLength;
   int                  TransportStreamID;
   int                  SdtVersion;
   int                  OriginalNetworkID;
   struct Service      *Service;
   struct LIST         *ServiceList = NULL;

   if (!Buffer) return NULL;

   Sdt = (sdt_t *) Buffer; Ptr = Buffer;

   if (Sdt->table_id != TID_SDT_ACT && Sdt->table_id != TID_SDT_OTH) {
//      fprintf (stderr, "SDT: wrong TID %d\n", Sdt->table_id);
      return NULL;
   }

   SectionLength = HILO (Sdt->section_length) + 3 - SDT_LEN - 4;

   if (crc32 (Ptr, HILO (Sdt->section_length) + 3)) return (NULL);

   TransportStreamID = HILO (Sdt->transport_stream_id);
   SdtVersion = Sdt->version_number;
   OriginalNetworkID = HILO (Sdt->original_network_id);

   Ptr += SDT_LEN;

   while (SectionLength > 0)
   {
      SdtDescriptor = (sdt_descr_t *) Ptr;

      CreateService (Service, HILO (SdtDescriptor->service_id),
         TransportStreamID, OriginalNetworkID, SdtVersion,
         SdtDescriptor->free_ca_mode ? CONDITIONAL_ACCESS : FREE_TO_AIR);

      switch (SdtDescriptor->running_status)
      {
         case 0x01:
            SetRunningStatus (Service->Status, RUNNING_STATUS_NOT_RUNNING);
         break;

         case 0x02:
            SetRunningStatus (Service->Status, RUNNING_STATUS_AWAITING);
         break;

         case 0x03:
            SetRunningStatus (Service->Status, RUNNING_STATUS_PAUSING);
         break;

         case 0x04:
         default:
            SetRunningStatus (Service->Status, RUNNING_STATUS_RUNNING);
         break;
      }
      if (SdtDescriptor->eit_schedule_flag)
         SetScheduleFlag (Service->Status);
      if (SdtDescriptor->eit_present_following_flag)
         SetPresentFollowing (Service->Status);

      LoopLength = HILO (SdtDescriptor->descriptors_loop_length);
      if (LoopLength > SectionLength - SDT_DESCR_LEN)
         return (ServiceList);
      Ptr += SDT_DESCR_LEN;

      siParseDescriptors (Service->Descriptors, Ptr, LoopLength, Sdt->table_id);

      if (!ServiceList) ServiceList = xNewList (NULL);
      xAddTail (ServiceList, Service);

      SectionLength -= LoopLength + SDT_DESCR_LEN;
      Ptr += LoopLength;
   }

   return (ServiceList);
}


struct LIST *siParseEIT (u_char *Buffer) 
{
   eit_t               *Eit;
   eit_event_t         *EitEvent;
   u_char              *Ptr;
   int                  SectionLength, LoopLength;
   int                  ServiceID;
   int                  EitVersion;
   int                  TransportStreamID;
   int                  OriginalNetworkID;
   struct Event        *Event;
   struct LIST         *EventList = NULL;

   if (!Buffer) return NULL;

   Eit = (eit_t *) Buffer; Ptr = Buffer;

   if (Eit->table_id != TID_EIT_ACT && Eit->table_id != TID_EIT_OTH &&
       !(Eit->table_id >= TID_EIT_ACT_SCH &&
         Eit->table_id <= TID_EIT_ACT_SCH + 0x0F) &&
       !(Eit->table_id >= TID_EIT_OTH_SCH &&
         Eit->table_id <= TID_EIT_OTH_SCH + 0x0F)) {
//      fprintf (stderr, "EIT: wrong TID %d\n", Eit->table_id);
      return NULL;
   }

   SectionLength = HILO (Eit->section_length) + 3 - EIT_LEN - 4;

   if (crc32 (Ptr, HILO (Eit->section_length) + 3)) return (NULL);

   ServiceID = HILO (Eit->service_id);
   TransportStreamID = HILO (Eit->transport_stream_id);
   EitVersion = Eit->version_number;
   OriginalNetworkID = HILO (Eit->original_network_id);

   Ptr += EIT_LEN;

   while (SectionLength > 0)
   {
      struct tm thisTime;
      int year, month, day;
      double mjd;

      EitEvent = (eit_event_t *) Ptr;

      CreateEvent (Event, HILO (EitEvent->event_id), ServiceID,
         TransportStreamID, OriginalNetworkID, EitVersion,
         EitEvent->free_ca_mode ? CONDITIONAL_ACCESS : FREE_TO_AIR);

      switch (EitEvent->running_status)
      {
         case 0x01:
            SetRunningStatus (Event->Status, RUNNING_STATUS_NOT_RUNNING);
         break;

         case 0x02:
            SetRunningStatus (Event->Status, RUNNING_STATUS_AWAITING);
         break;

         case 0x03:
            SetRunningStatus (Event->Status, RUNNING_STATUS_PAUSING);
         break;

         case 0x04:
         default:
            SetRunningStatus (Event->Status, RUNNING_STATUS_RUNNING);
         break;
      }
      Event->StartTime = MjdToEpochTime (EitEvent->mjd) +
                         BcdTimeToSeconds (EitEvent->start_time);
      Event->Duration = BcdTimeToSeconds (EitEvent->duration);

      LoopLength = HILO (EitEvent->descriptors_loop_length);
      if (LoopLength > SectionLength - EIT_EVENT_LEN)
         return (EventList);
      Ptr += EIT_EVENT_LEN;

      siParseDescriptors (Event->Descriptors, Ptr, LoopLength, Eit->table_id);

      if (!EventList) EventList = xNewList (NULL);
      xAddTail (EventList, Event);

      SectionLength -= LoopLength + EIT_EVENT_LEN;
      Ptr += LoopLength;
   }

   return (EventList);
}


time_t siParseTDT (u_char *Buffer) 
{
   tdt_t               *Tdt;
   u_char              *Ptr;
   int                  SectionLength;
   int                  TdtVersion;
   time_t               CurrentTime;

   if (!Buffer) return 0;

   Tdt = (tdt_t *) Buffer; Ptr = Buffer;

   if (Tdt->table_id != TID_TDT) {
//      fprintf (stderr, "TDT: wrong TID %d\n", Tdt->table_id);
      return 0;
   }

   SectionLength = HILO (Tdt->section_length) + 3;  /* no CRC ?! */

   CurrentTime = MjdToEpochTime (Tdt->utc_mjd) +
                 BcdTimeToSeconds (Tdt->utc_time);

   return (CurrentTime);
}


void siParseDescriptors (struct LIST *Descriptors, u_char *Buffer,
                         int Length, u_char TableID)
{
   int          DescriptorLength;
   u_char      *Ptr;

   DescriptorLength = 0;
   Ptr = Buffer;

   while (DescriptorLength < Length)
   {
      if ((GetDescriptorLength (Ptr) > Length - DescriptorLength) ||
          (GetDescriptorLength (Ptr) <= 0)) return;
      switch (TableID)
      {
         case TID_NIT_ACT: case TID_NIT_OTH:
            switch (GetDescriptorTag(Ptr))
            {
               case DESCR_NW_NAME:
               case DESCR_SERVICE_LIST:
               case DESCR_STUFFING:
               case DESCR_SAT_DEL_SYS:
               case DESCR_CABLE_DEL_SYS:
               case DESCR_LINKAGE:
               case DESCR_TERR_DEL_SYS:
               case DESCR_ML_NW_NAME:
               case DESCR_PRIV_DATA_SPEC:
               case DESCR_CELL_LIST:
               case DESCR_CELL_FREQ_LINK:
               case DESCR_ANNOUNCEMENT_SUPPORT:
                  siParseDescriptor (Descriptors, Ptr);
               break;

               default:
               /*   fprintf (stderr, "forbidden descriptor 0x%x in NIT\n",
                              GetDescriptorTag(Ptr));*/
               break;
            }
         break;

         case TID_BAT:
            switch (GetDescriptorTag(Ptr))
            {
               case DESCR_SERVICE_LIST:
               case DESCR_STUFFING:
               case DESCR_BOUQUET_NAME:
               case DESCR_SERVICE:
               case DESCR_COUNTRY_AVAIL:
               case DESCR_LINKAGE:
               case DESCR_CA_IDENT:
               case DESCR_ML_BQ_NAME:
               case DESCR_PRIV_DATA_SPEC:
                  siParseDescriptor (Descriptors, Ptr);
               break;

               default:
                  /*fprintf (stderr, "forbidden descriptor 0x%x in BAT\n",
                              GetDescriptorTag(Ptr));*/
               break;
            }
         break;

         case TID_SDT_ACT: case TID_SDT_OTH:
            switch (GetDescriptorTag(Ptr))
            {
               case DESCR_STUFFING:
               case DESCR_BOUQUET_NAME:
               case DESCR_SERVICE: 
               case DESCR_COUNTRY_AVAIL: 
               case DESCR_LINKAGE:
               case DESCR_NVOD_REF:
               case DESCR_TIME_SHIFTED_SERVICE:
               case DESCR_MOSAIC:
               case DESCR_CA_IDENT:
               case DESCR_TELEPHONE:
               case DESCR_ML_SERVICE_NAME:
               case DESCR_PRIV_DATA_SPEC:
               case DESCR_DATA_BROADCAST:
                  siParseDescriptor (Descriptors, Ptr);
               break;

               default:
                  /* fprintf (stderr, "forbidden descriptor 0x%x in SDT\n",
                              GetDescriptorTag(Ptr)); */
               break;
            }
         break;

         case TID_EIT_ACT: case TID_EIT_OTH:
         case TID_EIT_ACT_SCH: case TID_EIT_OTH_SCH:
         case TID_EIT_ACT_SCH+1: case TID_EIT_OTH_SCH+1:
         case TID_EIT_ACT_SCH+2: case TID_EIT_OTH_SCH+2:
         case TID_EIT_ACT_SCH+3: case TID_EIT_OTH_SCH+3:
         case TID_EIT_ACT_SCH+4: case TID_EIT_OTH_SCH+4:
         case TID_EIT_ACT_SCH+5: case TID_EIT_OTH_SCH+5:
         case TID_EIT_ACT_SCH+6: case TID_EIT_OTH_SCH+6:
         case TID_EIT_ACT_SCH+7: case TID_EIT_OTH_SCH+7:
         case TID_EIT_ACT_SCH+8: case TID_EIT_OTH_SCH+8:
         case TID_EIT_ACT_SCH+9: case TID_EIT_OTH_SCH+9:
         case TID_EIT_ACT_SCH+10: case TID_EIT_OTH_SCH+10:
         case TID_EIT_ACT_SCH+11: case TID_EIT_OTH_SCH+11:
         case TID_EIT_ACT_SCH+12: case TID_EIT_OTH_SCH+12:
         case TID_EIT_ACT_SCH+13: case TID_EIT_OTH_SCH+13:
         case TID_EIT_ACT_SCH+14: case TID_EIT_OTH_SCH+14:
         case TID_EIT_ACT_SCH+15: case TID_EIT_OTH_SCH+15:
            switch (GetDescriptorTag(Ptr))
            {
               case DESCR_STUFFING:
               case DESCR_LINKAGE:
               case DESCR_SHORT_EVENT:
               case DESCR_EXTENDED_EVENT:
               case DESCR_TIME_SHIFTED_EVENT:
               case DESCR_COMPONENT:
               case DESCR_CA_IDENT:
               case DESCR_CONTENT:
               case DESCR_PARENTAL_RATING:
               case DESCR_TELEPHONE:
               case DESCR_ML_COMPONENT:
               case DESCR_PRIV_DATA_SPEC:
               case DESCR_SHORT_SMOOTH_BUF:
               case DESCR_DATA_BROADCAST:
               case DESCR_PDC:
                  siParseDescriptor (Descriptors, Ptr);
               break;

               default:
                  /*fprintf (stderr, "forbidden descriptor 0x%x in EIT\n",
                              GetDescriptorTag(Ptr));*/
               break;
            }
         break;

         case TID_TOT:
            switch (GetDescriptorTag(Ptr))
            {
               case DESCR_LOCAL_TIME_OFF:
                  siParseDescriptor (Descriptors, Ptr);
               break;

               default:
                  /*fprintf (stderr, "forbidden descriptor 0x%x in TOT\n",
                              GetDescriptorTag(Ptr));*/
               break;
            }
         break;

         case TID_PMT:
            switch (GetDescriptorTag(Ptr))
            {
               case DESCR_VBI_DATA:
               case DESCR_VBI_TELETEXT:
               case DESCR_MOSAIC:
               case DESCR_STREAM_ID:
               case DESCR_TELETEXT:
               case DESCR_SUBTITLING:
               case DESCR_PRIV_DATA_SPEC:
               case DESCR_SERVICE_MOVE:
               case DESCR_CA_SYSTEM:
               case DESCR_DATA_BROADCAST_ID:
               case DESCR_AC3:
               case DESCR_ANCILLARY_DATA:
               case DESCR_VIDEO_STREAM:
               case DESCR_AUDIO_STREAM:
               case DESCR_HIERARCHY:
               case DESCR_REGISTRATION:
               case DESCR_DATA_STREAM_ALIGN:
               case DESCR_TARGET_BACKGRID:
               case DESCR_VIDEO_WINDOW:
               case DESCR_CA:
               case DESCR_ISO_639_LANGUAGE:
               case DESCR_SYSTEM_CLOCK:
               case DESCR_MULTIPLEX_BUFFER_UTIL:
               case DESCR_COPYRIGHT:
               case DESCR_MAXIMUM_BITRATE:
                  siParseDescriptor (Descriptors, Ptr);
               break;

               default:
                  /* fprintf (stderr, "forbidden descriptor 0x%x in PMT\n",
                              GetDescriptorTag(Ptr)); */
               break;
            }
         break;

         default:
            fprintf (stderr, "descriptor 0x%x in unsupported table 0x%x\n",
                         GetDescriptorTag(Ptr), TableID);
         break;
      }
      DescriptorLength += GetDescriptorLength (Ptr);
      Ptr += GetDescriptorLength (Ptr);
   }
   return;
}


void siParseDescriptor (struct LIST *Descriptors, u_char *Buffer)
{
   struct NODE     *Descriptor = NULL;
   char            *Text , *Text2;
   u_char          *Ptr;
   int              Length, i;

   if (!Descriptors || !Buffer) return;

   Ptr = Buffer;

   switch (GetDescriptorTag(Buffer))
   {
      case DESCR_ANCILLARY_DATA:
         CreateAncillaryDataDescriptor (Descriptor,
                   CastAncillaryDataDescriptor(Buffer)->ancillary_data_identifier);
      break;

      case DESCR_BOUQUET_NAME:
         Text = siGetDescriptorText (Buffer + DESCR_BOUQUET_NAME_LEN,
                   GetDescriptorLength (Buffer) - DESCR_BOUQUET_NAME_LEN);
         CreateBouquetNameDescriptor (Descriptor, Text);
      break;

      case DESCR_COMPONENT:
         Text = siGetDescriptorText (Buffer + DESCR_COMPONENT_LEN,
                   GetDescriptorLength (Buffer) - DESCR_COMPONENT_LEN);
         CreateComponentDescriptor (Descriptor,
                   CastComponentDescriptor(Buffer)->stream_content,
                   CastComponentDescriptor(Buffer)->component_type,
                   CastComponentDescriptor(Buffer)->component_tag,
                   CastComponentDescriptor(Buffer)->lang_code1,
                   CastComponentDescriptor(Buffer)->lang_code2,
                   CastComponentDescriptor(Buffer)->lang_code3, Text);
      break;

      case DESCR_SERVICE:
         Text = siGetDescriptorText (Buffer + DESCR_SERVICE_LEN,
                   CastServiceDescriptor(Buffer)->provider_name_length);
         Text2 = siGetDescriptorText (Buffer + DESCR_SERVICE_LEN +
                   CastServiceDescriptor(Buffer)->provider_name_length + 1,
                   *((u_char *)(Buffer + DESCR_SERVICE_LEN +
                   CastServiceDescriptor(Buffer)->provider_name_length)));
         CreateServiceDescriptor (Descriptor,
                   CastServiceDescriptor(Buffer)->service_type, Text, Text2);
      break;

      case DESCR_COUNTRY_AVAIL:
         CreateCountryAvailabilityDescriptor (Descriptor,
                   CastCountryAvailabilityDescriptor(Buffer)->country_availability_flag);
         Length = GetDescriptorLength (Buffer) - DESCR_COUNTRY_AVAILABILITY_LEN;
         Ptr += DESCR_COUNTRY_AVAILABILITY_LEN;
         while (Length > 0)
            { AddCountryAvailabilityCode(Descriptor,
                      Ptr[0], Ptr[1], Ptr[2]); Ptr += 3; Length -= 3; }
      break;

      case DESCR_SHORT_EVENT:
         Text = siGetDescriptorText (Buffer + DESCR_SHORT_EVENT_LEN,
                   CastShortEventDescriptor(Buffer)->event_name_length);
         Text2 = siGetDescriptorText (Buffer + DESCR_SHORT_EVENT_LEN +
                   CastShortEventDescriptor(Buffer)->event_name_length + 1,
                   *((u_char *)(Buffer + DESCR_SHORT_EVENT_LEN +
                   CastShortEventDescriptor(Buffer)->event_name_length)));
         CreateShortEventDescriptor (Descriptor, Text,
                   CastShortEventDescriptor(Buffer)->lang_code1,
                   CastShortEventDescriptor(Buffer)->lang_code2,
                   CastShortEventDescriptor(Buffer)->lang_code3, Text2);
      break;

      case DESCR_EXTENDED_EVENT:
         Text = siGetDescriptorText (Buffer + DESCR_EXTENDED_EVENT_LEN +
                   CastExtendedEventDescriptor(Buffer)->length_of_items + 1,
                   *((u_char *)(Buffer + DESCR_EXTENDED_EVENT_LEN +
                   CastExtendedEventDescriptor(Buffer)->length_of_items)));
         CreateExtendedEventDescriptor (Descriptor,
                   CastExtendedEventDescriptor(Buffer)->descriptor_number,
                   CastExtendedEventDescriptor(Buffer)->last_descriptor_number,
                   CastExtendedEventDescriptor(Buffer)->lang_code1,
                   CastExtendedEventDescriptor(Buffer)->lang_code2,
                   CastExtendedEventDescriptor(Buffer)->lang_code3, Text);
         Length = CastExtendedEventDescriptor(Buffer)->length_of_items;
         Ptr += DESCR_EXTENDED_EVENT_LEN;
         while ((Length > 0) && (Length < GetDescriptorLength (Buffer)))
         {
            Text = siGetDescriptorText (Ptr + ITEM_EXTENDED_EVENT_LEN,
                      CastExtendedEventItem(Ptr)->item_description_length);
            Text2 = siGetDescriptorText (Ptr + ITEM_EXTENDED_EVENT_LEN +
                      CastExtendedEventItem(Ptr)->item_description_length + 1,
                      *((u_char *)(Ptr + ITEM_EXTENDED_EVENT_LEN +
                      CastExtendedEventItem(Ptr)->item_description_length)));
            AddExtendedEventItem (Descriptor, Text2, Text);
            Length -= ITEM_EXTENDED_EVENT_LEN + CastExtendedEventItem(Ptr)->item_description_length +
                      *((u_char *)(Ptr + ITEM_EXTENDED_EVENT_LEN +
                        CastExtendedEventItem(Ptr)->item_description_length)) + 1;
            Ptr += ITEM_EXTENDED_EVENT_LEN + CastExtendedEventItem(Ptr)->item_description_length +
                      *((u_char *)(Ptr + ITEM_EXTENDED_EVENT_LEN +
                        CastExtendedEventItem(Ptr)->item_description_length)) + 1;
         }
      break;

      case DESCR_CA_IDENT:
         CreateCaIdentifierDescriptor (Descriptor,
                   (GetDescriptorLength(Buffer) - DESCR_CA_IDENTIFIER_LEN) / 2);
         Length = GetDescriptorLength (Buffer) - DESCR_CA_IDENTIFIER_LEN;
         Ptr += DESCR_CA_IDENTIFIER_LEN; i = 0;
         while (Length > 0)
            { SetCaIdentifierID(Descriptor, i, *((u_short *) Ptr));
              Length -= 2; Ptr += 2; i++; }
      break;

      case DESCR_CONTENT:
         CreateContentDescriptor (Descriptor,
                   (GetDescriptorLength(Buffer) - DESCR_CONTENT_LEN) / 2);
         Length = GetDescriptorLength (Buffer) - DESCR_CONTENT_LEN;
         Ptr += DESCR_CONTENT_LEN; i = 0;
         while (Length > 0)
            { SetContentID(Descriptor, i, CastContentNibble(Ptr)->content_nibble_level_1,
                              CastContentNibble(Ptr)->content_nibble_level_2,
                              CastContentNibble(Ptr)->user_nibble_1,
                              CastContentNibble(Ptr)->user_nibble_2);
              Length -= 2; Ptr += 2; i++; }
      break;

      case DESCR_STUFFING:
         /* intentionally ignored */
      break;

      case DESCR_PARENTAL_RATING:
         CreateParentalRatingDescriptor (Descriptor);
         Length = GetDescriptorLength (Buffer) - DESCR_PARENTAL_RATING_LEN;
         Ptr += DESCR_PARENTAL_RATING_LEN; i = 0;
         while (Length > 0)
            { AddParentalRating (Descriptor, CastParentalRating(Ptr)->lang_code1,
                 CastParentalRating(Ptr)->lang_code2, CastParentalRating(Ptr)->lang_code3,
                 CastParentalRating(Ptr)->rating);
              Length -= PARENTAL_RATING_LEN; Ptr += PARENTAL_RATING_LEN; i++; }
      break;

      case DESCR_NVOD_REF:
         CreateNvodReferenceDescriptor (Descriptor);
         Length = GetDescriptorLength (Buffer) - DESCR_NVOD_REFERENCE_LEN;
         Ptr += DESCR_NVOD_REFERENCE_LEN;
         while (Length > 0)
         {
            AddNvodReferenceItem (Descriptor,
               HILO (CastNvodReferenceItem(Ptr)->transport_stream_id),
               HILO (CastNvodReferenceItem(Ptr)->original_network_id),
               HILO (CastNvodReferenceItem(Ptr)->service_id));
            Length -= ITEM_NVOD_REFERENCE_LEN;
            Ptr += ITEM_NVOD_REFERENCE_LEN;
         }
      break;

      case DESCR_TIME_SHIFTED_SERVICE:
         CreateTimeShiftedServiceDescriptor (Descriptor,
            HILO (CastTimeShiftedServiceDescriptor(Ptr)->reference_service_id));
      break;

      case DESCR_TIME_SHIFTED_EVENT:
         CreateTimeShiftedEventDescriptor (Descriptor,
            HILO (CastTimeShiftedEventDescriptor(Ptr)->reference_service_id),
            HILO (CastTimeShiftedEventDescriptor(Ptr)->reference_event_id));
      break;

      case DESCR_ISO_639_LANGUAGE:
         CreateIso639LanguageDescriptor (Descriptor,
                   CastIso639LanguageDescriptor(Buffer)->lang_code1,
                   CastIso639LanguageDescriptor(Buffer)->lang_code2,
                   CastIso639LanguageDescriptor(Buffer)->lang_code3);
      break;

      case DESCR_STREAM_ID:
         CreateStreamIdentifierDescriptor (Descriptor,
                   CastStreamIdentifierDescriptor(Ptr)->component_tag);
      break;

      case DESCR_LINKAGE:
         CreateLinkageDescriptor (Descriptor,
                   HILO (CastLinkageDescriptor(Ptr)->transport_stream_id),
                   HILO (CastLinkageDescriptor(Ptr)->original_network_id),
                   HILO (CastLinkageDescriptor(Ptr)->service_id),
                   CastLinkageDescriptor(Ptr)->linkage_type,
                   GetDescriptorLength (Ptr) - DESCR_LINKAGE_LEN,
                   Ptr + DESCR_LINKAGE_LEN);
      break;

      case DESCR_TELETEXT:
         CreateTeletextDescriptor (Descriptor);
         Length = GetDescriptorLength (Buffer) - DESCR_TELETEXT_LEN;
         Ptr += DESCR_TELETEXT_LEN;
         while (Length > 0)
         {
            AddTeletextItem (Descriptor,
                   CastTeletextItem(Ptr)->type,
                   CastTeletextItem(Ptr)->magazine_number,
                   CastTeletextItem(Ptr)->page_number,
                   CastTeletextItem(Ptr)->lang_code1,
                   CastTeletextItem(Ptr)->lang_code2,
                   CastTeletextItem(Ptr)->lang_code3);
            Length -= ITEM_TELETEXT_LEN;
            Ptr += ITEM_TELETEXT_LEN;
         }
      break;

      case DESCR_AC3:
         CreateAc3Descriptor (Descriptor);
         Length = GetDescriptorLength (Buffer);
         if (CastAc3Descriptor(Buffer)->ac3_type_flag)
          { Length -= 1; Ptr += 1; AddAc3FlagAndValue (Descriptor,
            AC3_TYPE_FLAG, CastAc3Descriptor(Buffer)->ac3_type); }
         if (CastAc3Descriptor(Buffer)->bsid_flag)
          { Length -= 1; Ptr += 1; AddAc3FlagAndValue (Descriptor,
            BS_ID_FLAG, CastAc3Descriptor(Buffer)->bsid); }
         if (CastAc3Descriptor(Buffer)->mainid_flag)
          { Length -= 1; Ptr += 1; AddAc3FlagAndValue (Descriptor,
            MAIN_ID_FLAG, CastAc3Descriptor(Buffer)->mainid); }
         if (CastAc3Descriptor(Buffer)->asvc_flag)
          { Length -= 1; Ptr += 1; AddAc3FlagAndValue (Descriptor,
            ASVC_FLAG, CastAc3Descriptor(Buffer)->asvc); }
         Length -= DESCR_AC3_LEN;
         Ptr += DESCR_AC3_LEN;
         if (Length > 0) AddAc3AdditionalData (Descriptor, Ptr, Length);
      break;

      case DESCR_SUBTITLING:
         CreateSubtitlingDescriptor (Descriptor);
         Length = GetDescriptorLength (Buffer) - DESCR_SUBTITLING_LEN;
         Ptr += DESCR_SUBTITLING_LEN;
         while (Length > 0)
         {
            AddSubtitlingItem (Descriptor,
                   CastSubtitlingItem(Ptr)->subtitling_type,
                   HILO (CastSubtitlingItem(Ptr)->composition_page_id),
                   HILO (CastSubtitlingItem(Ptr)->ancillary_page_id),
                   CastSubtitlingItem(Ptr)->lang_code1,
                   CastSubtitlingItem(Ptr)->lang_code2,
                   CastSubtitlingItem(Ptr)->lang_code3);
            Length -= ITEM_SUBTITLING_LEN;
            Ptr += ITEM_SUBTITLING_LEN;
         }
      break;

      case DESCR_VIDEO_STREAM:
      case DESCR_AUDIO_STREAM:
      case DESCR_HIERARCHY:
      case DESCR_REGISTRATION:
      case DESCR_DATA_STREAM_ALIGN:
      case DESCR_TARGET_BACKGRID:
      case DESCR_VIDEO_WINDOW:
      case DESCR_CA:
      case DESCR_SYSTEM_CLOCK:
      case DESCR_MULTIPLEX_BUFFER_UTIL:
      case DESCR_COPYRIGHT:
      case DESCR_MAXIMUM_BITRATE:
      case DESCR_PRIVATE_DATA_IND:
      case DESCR_SMOOTHING_BUFFER:
      case DESCR_STD:
      case DESCR_IBP:
      case DESCR_NW_NAME:
      case DESCR_SERVICE_LIST:
      case DESCR_SAT_DEL_SYS:
      case DESCR_CABLE_DEL_SYS:
      case DESCR_VBI_DATA:
      case DESCR_VBI_TELETEXT:
      case DESCR_MOSAIC:
      case DESCR_TELEPHONE:
      case DESCR_LOCAL_TIME_OFF:
      case DESCR_TERR_DEL_SYS:
      case DESCR_ML_NW_NAME:
      case DESCR_ML_BQ_NAME:
      case DESCR_ML_SERVICE_NAME:
      case DESCR_ML_COMPONENT:
      case DESCR_PRIV_DATA_SPEC:
      case DESCR_SERVICE_MOVE:
      case DESCR_SHORT_SMOOTH_BUF:
      case DESCR_FREQUENCY_LIST:
      case DESCR_PARTIAL_TP_STREAM:
      case DESCR_DATA_BROADCAST:
      case DESCR_CA_SYSTEM:
      case DESCR_DATA_BROADCAST_ID:
      case DESCR_TRANSPORT_STREAM:
      case DESCR_DSNG:
      case DESCR_PDC:
      case DESCR_CELL_LIST:
      case DESCR_CELL_FREQ_LINK:
      case DESCR_ANNOUNCEMENT_SUPPORT:
      default:
         /* fprintf (stderr, "unsupported descriptor 0x%x\n",
                              GetDescriptorTag(Buffer)); */
      break;
   }
   if (Descriptor) xAddTail (Descriptors, Descriptor);
   return;
}


/*
 *  ToDo:  ETSI conformal text definition
 */
char *siGetDescriptorText (u_char *Buffer, int Length)
{
   char *tmp, *result;
   int i;

   if ((Length < 0) || (Length > 4095))
      return (xSetText ("text error"));
   if (*Buffer == 0x05 || (*Buffer >= 0x20 && *Buffer <= 0xff))
   {
      xMemAlloc (Length+1, &result);
      tmp = result;
      for (i = 0; i < Length; i++)
      {
         if (*Buffer == 0) break;

         if ((*Buffer >= ' ' && *Buffer <= '~') ||
             (*Buffer >= 0xa0 && *Buffer <= 0xff)) *tmp++ = *Buffer;
         if (*Buffer == 0x8A || *Buffer == '\n') *tmp++ = '\n';
         if (*Buffer == 0x86 || *Buffer == 0x87) *tmp++ = ' ';
         Buffer++;
      }
      *tmp = '\0';
   }
   else
   {
      switch (*Buffer)
      {
         case 0x01: result = xSetText ("Coding according to character table 1"); break;
         case 0x02: result = xSetText ("Coding according to character table 2"); break;
         case 0x03: result = xSetText ("Coding according to character table 3"); break;
         case 0x04: result = xSetText ("Coding according to character table 4"); break;
         case 0x10: result = xSetText ("Coding according to ISO/IEC 8859"); break;
         case 0x11: result = xSetText ("Coding according to ISO/IEC 10646"); break;
         case 0x12: result = xSetText ("Coding according to KSC 5601"); break;
         default: result = xSetText ("Unknown coding"); break;
      }
   }

   return (result);
}

// CRC32 lookup table for polynomial 0x04c11db7

static u_long crc_table[256] = {
	0x00000000, 0x04c11db7, 0x09823b6e, 0x0d4326d9, 0x130476dc, 0x17c56b6b,
	0x1a864db2, 0x1e475005, 0x2608edb8, 0x22c9f00f, 0x2f8ad6d6, 0x2b4bcb61,
	0x350c9b64, 0x31cd86d3, 0x3c8ea00a, 0x384fbdbd, 0x4c11db70, 0x48d0c6c7,
	0x4593e01e, 0x4152fda9, 0x5f15adac, 0x5bd4b01b, 0x569796c2, 0x52568b75,
	0x6a1936c8, 0x6ed82b7f, 0x639b0da6, 0x675a1011, 0x791d4014, 0x7ddc5da3,
	0x709f7b7a, 0x745e66cd, 0x9823b6e0, 0x9ce2ab57, 0x91a18d8e, 0x95609039,
	0x8b27c03c, 0x8fe6dd8b, 0x82a5fb52, 0x8664e6e5, 0xbe2b5b58, 0xbaea46ef,
	0xb7a96036, 0xb3687d81, 0xad2f2d84, 0xa9ee3033, 0xa4ad16ea, 0xa06c0b5d,
	0xd4326d90, 0xd0f37027, 0xddb056fe, 0xd9714b49, 0xc7361b4c, 0xc3f706fb,
	0xceb42022, 0xca753d95, 0xf23a8028, 0xf6fb9d9f, 0xfbb8bb46, 0xff79a6f1,
	0xe13ef6f4, 0xe5ffeb43, 0xe8bccd9a, 0xec7dd02d, 0x34867077, 0x30476dc0,
	0x3d044b19, 0x39c556ae, 0x278206ab, 0x23431b1c, 0x2e003dc5, 0x2ac12072,
	0x128e9dcf, 0x164f8078, 0x1b0ca6a1, 0x1fcdbb16, 0x018aeb13, 0x054bf6a4,
	0x0808d07d, 0x0cc9cdca, 0x7897ab07, 0x7c56b6b0, 0x71159069, 0x75d48dde,
	0x6b93dddb, 0x6f52c06c, 0x6211e6b5, 0x66d0fb02, 0x5e9f46bf, 0x5a5e5b08,
	0x571d7dd1, 0x53dc6066, 0x4d9b3063, 0x495a2dd4, 0x44190b0d, 0x40d816ba,
	0xaca5c697, 0xa864db20, 0xa527fdf9, 0xa1e6e04e, 0xbfa1b04b, 0xbb60adfc,
	0xb6238b25, 0xb2e29692, 0x8aad2b2f, 0x8e6c3698, 0x832f1041, 0x87ee0df6,
	0x99a95df3, 0x9d684044, 0x902b669d, 0x94ea7b2a, 0xe0b41de7, 0xe4750050,
	0xe9362689, 0xedf73b3e, 0xf3b06b3b, 0xf771768c, 0xfa325055, 0xfef34de2,
	0xc6bcf05f, 0xc27dede8, 0xcf3ecb31, 0xcbffd686, 0xd5b88683, 0xd1799b34,
	0xdc3abded, 0xd8fba05a, 0x690ce0ee, 0x6dcdfd59, 0x608edb80, 0x644fc637,
	0x7a089632, 0x7ec98b85, 0x738aad5c, 0x774bb0eb, 0x4f040d56, 0x4bc510e1,
	0x46863638, 0x42472b8f, 0x5c007b8a, 0x58c1663d, 0x558240e4, 0x51435d53,
	0x251d3b9e, 0x21dc2629, 0x2c9f00f0, 0x285e1d47, 0x36194d42, 0x32d850f5,
	0x3f9b762c, 0x3b5a6b9b, 0x0315d626, 0x07d4cb91, 0x0a97ed48, 0x0e56f0ff,
	0x1011a0fa, 0x14d0bd4d, 0x19939b94, 0x1d528623, 0xf12f560e, 0xf5ee4bb9,
	0xf8ad6d60, 0xfc6c70d7, 0xe22b20d2, 0xe6ea3d65, 0xeba91bbc, 0xef68060b,
	0xd727bbb6, 0xd3e6a601, 0xdea580d8, 0xda649d6f, 0xc423cd6a, 0xc0e2d0dd,
	0xcda1f604, 0xc960ebb3, 0xbd3e8d7e, 0xb9ff90c9, 0xb4bcb610, 0xb07daba7,
	0xae3afba2, 0xaafbe615, 0xa7b8c0cc, 0xa379dd7b, 0x9b3660c6, 0x9ff77d71,
	0x92b45ba8, 0x9675461f, 0x8832161a, 0x8cf30bad, 0x81b02d74, 0x857130c3,
	0x5d8a9099, 0x594b8d2e, 0x5408abf7, 0x50c9b640, 0x4e8ee645, 0x4a4ffbf2,
	0x470cdd2b, 0x43cdc09c, 0x7b827d21, 0x7f436096, 0x7200464f, 0x76c15bf8,
	0x68860bfd, 0x6c47164a, 0x61043093, 0x65c52d24, 0x119b4be9, 0x155a565e,
	0x18197087, 0x1cd86d30, 0x029f3d35, 0x065e2082, 0x0b1d065b, 0x0fdc1bec,
	0x3793a651, 0x3352bbe6, 0x3e119d3f, 0x3ad08088, 0x2497d08d, 0x2056cd3a,
	0x2d15ebe3, 0x29d4f654, 0xc5a92679, 0xc1683bce, 0xcc2b1d17, 0xc8ea00a0,
	0xd6ad50a5, 0xd26c4d12, 0xdf2f6bcb, 0xdbee767c, 0xe3a1cbc1, 0xe760d676,
	0xea23f0af, 0xeee2ed18, 0xf0a5bd1d, 0xf464a0aa, 0xf9278673, 0xfde69bc4,
	0x89b8fd09, 0x8d79e0be, 0x803ac667, 0x84fbdbd0, 0x9abc8bd5, 0x9e7d9662,
	0x933eb0bb, 0x97ffad0c, 0xafb010b1, 0xab710d06, 0xa6322bdf, 0xa2f33668,
	0xbcb4666d, 0xb8757bda, 0xb5365d03, 0xb1f740b4};

u_long crc32 (char *data, int len)
{
	register int i;
	u_long crc = 0xffffffff;

	for (i=0; i<len; i++)
		crc = (crc << 8) ^ crc_table[((crc >> 24) ^ *data++) & 0xff];

	return crc;
}
