//////////////////////////////////////////////////////////////
///                                                        ///
/// si_parser.c: main parsing functions of libsi           ///
///                                                        ///
//////////////////////////////////////////////////////////////

// $Revision: 1.8 $
// $Date: 2003/02/04 18:45:35 $
// $Author: hakenes $
//
//   (C) 2001-03 Rolf Hakenes <hakenes@hippomi.de>, under the
//               GNU GPL with contribution of Oleg Assovski,
//               www.satmania.com
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
#include <string.h>
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
      // fprintf (stderr, "PAT: wrong TID %d\n", Pat->table_id);
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


struct LIST *siParseCAT (u_char *Buffer) 
{
   cat_t               *Cat;
   u_char              *Ptr;
   int                  SectionLength;
   int                  TransportStreamID;
   int                  CatVersion;
   struct Descriptor   *Descriptor;
   struct LIST         *DescriptorList = NULL;

   if (!Buffer) return NULL;

   Cat = (cat_t *) Buffer; Ptr = Buffer;

   if (Cat->table_id != TID_CAT) {
      // fprintf (stderr, "CAT: wrong TID %d\n", Cat->table_id);
      return NULL;
   }

   SectionLength = HILO (Cat->section_length) + 3 - CAT_LEN - 4;

   if (crc32 (Ptr, HILO (Cat->section_length) + 3)) return (NULL);

   CatVersion = Cat->version_number;

   Ptr += CAT_LEN;

   if (SectionLength >= 0)
   {
      DescriptorList = xNewList (NULL);
      siParseDescriptors (DescriptorList, Ptr, SectionLength, Cat->table_id);
   }

   return (DescriptorList);
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
      // fprintf (stderr, "PMT: wrong TID %d\n", Pmt->table_id);
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


struct LIST *siParseNIT (u_char *Buffer) 
{
   nit_t               *Nit;
   nit_mid_t           *NitMid;
   nit_ts_t            *TSDesc;
   u_char              *Ptr;
   int                  SectionLength, LoopLength, Loop2Length;
   int                  TransportStreamID;
   int                  NitVersion;
   int                  NetworkID;
   struct TransportStream *TransportStream;
   struct LIST         *TSList = NULL;
   struct LIST         *Networks;
   struct NetworkInfo  *Network;  

   if (!Buffer) return NULL;

   Nit = (nit_t *) Buffer; 
   Ptr = Buffer;

   if (Nit->table_id != TID_NIT_ACT && Nit->table_id != TID_NIT_OTH && Nit->table_id != TID_BAT) {
      return NULL;
   }

   SectionLength = HILO (Nit->section_length) + 3 - NIT_LEN - 4;

   if (crc32 (Ptr, HILO (Nit->section_length) + 3)) return (NULL);

   NitVersion = Nit->version_number;
   NetworkID = HILO (Nit->network_id);
   if (NetworkID == 65535)
      NetworkID = 0;
   CreateNetworkInfo (Network, NetworkID);
   Networks = xNewList (NULL);
   xAddTail (Networks, Network);

   Ptr += NIT_LEN;

   LoopLength = HILO (Nit->network_descriptor_length);
//   fprintf (stderr, "table 0x%X, SectionLen = %d, LoopLen = %d\n",
//                              Nit->table_id, SectionLength, LoopLength);
   if (LoopLength > SectionLength - SDT_DESCR_LEN)
      return (Networks);

   if (LoopLength <= SectionLength) {
      if (SectionLength >= 0) siParseDescriptors (Network->Descriptors, Ptr, LoopLength, Nit->table_id);
      SectionLength -= LoopLength;
      Ptr += LoopLength;
      NitMid = (nit_mid_t *) Ptr; 
      LoopLength = HILO (NitMid->transport_stream_loop_length);
//      fprintf (stderr, "table 0x%X, TS LoopLen = %d\n",
//                              Nit->table_id, LoopLength);
      if ((SectionLength > 0) && (LoopLength <= SectionLength)) {
         SectionLength -= SIZE_NIT_MID;
         Ptr += SIZE_NIT_MID;
         while (LoopLength > 0) {
            TSDesc = (nit_ts_t *) Ptr;
            CreateTransportStream (TransportStream, HILO(TSDesc->transport_stream_id), HILO(TSDesc->original_network_id));
            if (TransportStream->TransportStreamID == 65535)
               TransportStream->TransportStreamID = 0;
            if (TransportStream->OriginalNetworkID == 65535)
               TransportStream->OriginalNetworkID = 0;
            Loop2Length = HILO (TSDesc->transport_descriptors_length);
//            fprintf (stderr, "table 0x%X, TSdesc LoopLen  = %d\n",
//                              Nit->table_id, Loop2Length);
            Ptr += NIT_TS_LEN;
            if (Loop2Length <= LoopLength) {
               if (LoopLength >= 0) siParseDescriptors (TransportStream->Descriptors, Ptr, Loop2Length, Nit->table_id);
            }
            if (!Network->TransportStreams)
               Network->TransportStreams = xNewList (NULL);
            xAddTail (Network->TransportStreams, TransportStream);
            LoopLength -= Loop2Length + NIT_TS_LEN;
            SectionLength -= Loop2Length + NIT_TS_LEN;
            Ptr += Loop2Length;
         }
      }
   }

   return (Networks);
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
      // fprintf (stderr, "SDT: wrong TID %d\n", Sdt->table_id);
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
      // fprintf (stderr, "EIT: wrong TID %d\n", Eit->table_id);
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
      // fprintf (stderr, "TDT: wrong TID %d\n", Tdt->table_id);
      return 0;
   }

   SectionLength = HILO (Tdt->section_length) + 3;  /* no CRC ?! */

   CurrentTime = MjdToEpochTime (Tdt->utc_mjd) +
                 BcdTimeToSeconds (Tdt->utc_time);

   return (CurrentTime);
}


struct Tot *siParseTOT (u_char *Buffer) 
{
   tot_t               *Tot;
   u_char              *Ptr;
   int                  SectionLength, LoopLength;
   struct Tot          *table;
   time_t               CurrentTime;

   if (!Buffer) return NULL;

   Tot = (tot_t *) Buffer;
   Ptr = Buffer;

   if (Tot->table_id != TID_TOT) {
      return NULL;
   }

   if (crc32 (Ptr, HILO (Tot->section_length) + 3)) return (NULL);
//   SectionLength = HILO (Tot->section_length) + 3 - TOT_LEN - 4;

   CurrentTime = MjdToEpochTime (Tot->utc_mjd) +
                 BcdTimeToSeconds (Tot->utc_time);
   LoopLength = HILO (Tot->descriptors_loop_length);
   if (!LoopLength)
      return NULL;

   CreateTot (table, CurrentTime);

   Ptr += TOT_LEN;

   siParseDescriptors (table->Descriptors, Ptr, LoopLength, Tot->table_id);

   // fprintf (stderr, "TOT Bias: %d\n", table->Bias);
   return (table);
}

static u_char TempTableID = 0;

void siParseDescriptors (struct LIST *Descriptors, u_char *Buffer,
                         int Length, u_char TableID)
{
   int          DescriptorLength;
   u_char      *Ptr;

   DescriptorLength = 0;
   Ptr = Buffer;
   TempTableID = TableID;

   while (DescriptorLength < Length)
   {
      if ((GetDescriptorLength (Ptr) > Length - DescriptorLength) ||
          (GetDescriptorLength (Ptr) <= 0)) return;
      switch (TableID)
      {
         case TID_NIT_ACT: case TID_NIT_OTH:
            switch (GetDescriptorTag(Ptr))
            {
               case DESCR_SAT_DEL_SYS:
               case DESCR_CABLE_DEL_SYS:
               case DESCR_SERVICE_LIST:
               case DESCR_PRIV_DATA_SPEC:
//                  fprintf (stderr, "Got descriptor with tag = 0x%X\n", GetDescriptorTag(Ptr));
//                  siDumpDescriptor (Ptr);
               case DESCR_NW_NAME:
               case DESCR_STUFFING:
               case DESCR_LINKAGE:
               case DESCR_TERR_DEL_SYS:
               case DESCR_ML_NW_NAME:
               case DESCR_CELL_LIST:
               case DESCR_CELL_FREQ_LINK:
               case DESCR_ANNOUNCEMENT_SUPPORT:
                  siParseDescriptor (Descriptors, Ptr);
               break;

               default:
                  // fprintf (stderr, "forbidden descriptor 0x%x in NIT\n", GetDescriptorTag(Ptr));
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
 //                 fprintf (stderr, "Got descriptor with tag = 0x%X\n", GetDescriptorTag(Ptr));
                  siParseDescriptor (Descriptors, Ptr);
               break;

               default:
                  // fprintf (stderr, "forbidden descriptor 0x%x in BAT\n", GetDescriptorTag(Ptr));
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
                  // fprintf (stderr, "forbidden descriptor 0x%x in SDT\n", GetDescriptorTag(Ptr));
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
                  // fprintf (stderr, "forbidden descriptor 0x%x in EIT\n", GetDescriptorTag(Ptr));
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
                  // fprintf (stderr, "forbidden descriptor 0x%x in TOT\n", GetDescriptorTag(Ptr));
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
                  // fprintf (stderr, "forbidden descriptor 0x%x in PMT\n", GetDescriptorTag(Ptr));
               break;
            }
         break;

         case TID_CAT:
            switch (GetDescriptorTag(Ptr))
            {
               case DESCR_CA_SYSTEM:
               case DESCR_CA:
               case DESCR_CA_IDENT:
                  siParseDescriptor (Descriptors, Ptr);
               break;

               default:
                  // fprintf (stderr, "forbidden descriptor 0x%x in CAT\n", GetDescriptorTag(Ptr));
               break;
            }
         break;

         default:
            // fprintf (stderr, "descriptor 0x%x in unsupported table 0x%x\n", GetDescriptorTag(Ptr), TableID);
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
//   fprintf (stderr, "Got descriptor with tag = 0x%X\n", GetDescriptorTag(Buffer));

   switch (GetDescriptorTag(Buffer))
   {
      case DESCR_ANCILLARY_DATA:
         CreateAncillaryDataDescriptor (Descriptor,
                   CastAncillaryDataDescriptor(Buffer)->ancillary_data_identifier);
      break;

      case DESCR_NW_NAME:
      case DESCR_BOUQUET_NAME:
         Text = siGetDescriptorName (Buffer + DESCR_BOUQUET_NAME_LEN,
                   GetDescriptorLength (Buffer) - DESCR_BOUQUET_NAME_LEN);
//         fprintf (stderr, "Got descriptor with tag = 0x%X, text = '%s'\n", GetDescriptorTag(Buffer), Text);
         CreateBouquetNameDescriptor (Descriptor, Text, GetDescriptorTag(Buffer));
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
         Text = siGetDescriptorName (Buffer + DESCR_SERVICE_LEN,
                   CastServiceDescriptor(Buffer)->provider_name_length);
         Text2 = siGetDescriptorName (Buffer + DESCR_SERVICE_LEN +
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
         Text = siGetDescriptorName (Buffer + DESCR_SHORT_EVENT_LEN,
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
//         printf ("EEDesc #%d, %s\n", CastExtendedEventDescriptor(Buffer)->descriptor_number, Text);
         while ((Length > 0) && (Length < GetDescriptorLength (Buffer)))
         {
            Text = siGetDescriptorText (Ptr + ITEM_EXTENDED_EVENT_LEN,
                      CastExtendedEventItem(Ptr)->item_description_length);
            Text2 = siGetDescriptorText (Ptr + ITEM_EXTENDED_EVENT_LEN +
                      CastExtendedEventItem(Ptr)->item_description_length + 1,
                      *((u_char *)(Ptr + ITEM_EXTENDED_EVENT_LEN +
                      CastExtendedEventItem(Ptr)->item_description_length)));
//            printf ("EEItem #%d, %s, %s\n", CastExtendedEventDescriptor(Buffer)->descriptor_number, Text, Text2);
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
         Length = GetDescriptorLength (Buffer) - DESCR_CA_IDENTIFIER_LEN;
         CreateCaIdentifierDescriptor (Descriptor, Length / 2);
         Ptr += DESCR_CA_IDENTIFIER_LEN; i = 0;
         while (Length > 0)
            { SetCaIdentifierID(Descriptor, i, ((*((u_char *) Ptr)<<8) + *((u_char *) (Ptr+1))));
              Length -= 2; Ptr += 2; i++; }
      break;

      case DESCR_CA:
      {
         struct CaDescriptor *CD;

         Length = GetDescriptorLength (Buffer) - DESCR_CA_LEN;
         CreateCaDescriptor (Descriptor,
            HILO(CastCaDescriptor(Buffer)->CA_type),
            HILO(CastCaDescriptor(Buffer)->CA_PID), Length);
         Ptr += DESCR_CA_LEN; i = 0;
         while (Length > 0)
            { SetCaData(Descriptor, i, *Ptr);
              Length --; Ptr ++; i++; }

         /*
          * The following analyses are more or less directly copied from
          * MultiDec 8.4b Sources. Thanx to Espresso for his great work !!
          */
         CD = (struct CaDescriptor *) Descriptor;

         // fprintf (stderr, "TableID: %02x - CA - Type: 0x%04x, PID: %d\n", TempTableID, CD->CA_type, CD->CA_PID);
        
         if ((CD->CA_type >> 8) == 0x01) /* SECA */
         {
            CD->ProviderID = (GetCaData (CD, 0) << 8) | GetCaData (CD, 1);
         }
         else if ((CD->CA_type >> 8) == 0x05) /* Viaccess ? (France Telecom) */
         {
            i=0;
            while (i < CD->DataLength)
            {
               if ((GetCaData (CD, i) == 0x14) && (GetCaData (CD, i+1) == 0x03))
               {
                  CD->ProviderID = (GetCaData (CD, i+2) << 16) |
                                   (GetCaData (CD, i+3) << 8) |
                                   (GetCaData (CD, i+4) & 0xf0);
                  i = CD->DataLength;
               }
               i++;
            }
         }
         if (CD->CA_type==0x0100)  /* SECA 1 */
         {
         /*   bptr=MyPtr+19;

            i=19;
            while ( i+4 < ca_info->len ) {
               if ( (*bptr&0xE0) == 0xE0 ) {
                  CA_ECM=(( *bptr&0x1f)<<8)+*(bptr+1);
                  Prov_Ident = ( *(bptr+2)<<8) | *(bptr+3);
                  j=0;
                  while ( j < ProgrammNeu[ProgrammNummer].CA_Anzahl ) {
                     if (( ProgrammNeu[ProgrammNummer].CA_System[j].CA_Typ == CA_Typ )
                            && ( ProgrammNeu[ProgrammNummer].CA_System[j].ECM == CA_ECM )) break;
                     j++;
                  };

                  if ( j < MAX_CA_SYSTEMS ) {
                     if ( j >= ProgrammNeu[ProgrammNummer].CA_Anzahl )
                          ProgrammNeu[ProgrammNummer].CA_Anzahl++;
                     ProgrammNeu[ProgrammNummer].CA_System[j].CA_Typ =CA_Typ;
                     ProgrammNeu[ProgrammNummer].CA_System[j].ECM    =CA_ECM ;
                     ProgrammNeu[ProgrammNummer].CA_System[j].Provider_Id = Prov_Ident;
                  };
               }
               i+=0x0f;
               bptr+=0x0f;
            }; */
         }
      }
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

      case DESCR_SAT_DEL_SYS:
//         fprintf (stderr, "got descriptor 0x%x\n", GetDescriptorTag(Buffer));
      {
         descr_satellite_delivery_system_t *sds;
         sds = (descr_satellite_delivery_system_t *) Ptr;
         if (CheckBcdChar (sds->frequency1) && CheckBcdChar (sds->frequency2) &&
             CheckBcdChar (sds->frequency3) && CheckBcdChar (sds->frequency4) &&
             CheckBcdChar (sds->orbital_position1) &&
             CheckBcdChar (sds->orbital_position2) &&
             CheckBcdChar (sds->symbol_rate1) && CheckBcdChar (sds->symbol_rate1) &&
             CheckBcdChar (sds->symbol_rate3) && (sds->fec_inner != 0) && (sds->modulation == 1))
         {
           CreateSatelliteDeliverySystemDescriptor (Descriptor, 
            BcdCharToInt (sds->frequency1) * 10 * 1000 * 1000 +
            BcdCharToInt (sds->frequency2) * 100 * 1000 +
            BcdCharToInt (sds->frequency3) * 1000 +
            BcdCharToInt (sds->frequency4) * 10,
            (sds->west_east_flag ? 1 : -1) *
            (BcdCharToInt (sds->orbital_position1) * 100 +
            BcdCharToInt (sds->orbital_position2)),
            sds->modulation,
            sds->polarization,
            BcdCharToInt (sds->symbol_rate1) * 10 * 1000 +
            BcdCharToInt (sds->symbol_rate2) * 100 +
            BcdCharToInt (sds->symbol_rate3),
            sds->fec_inner);
         }
         /* else
         {
            fprintf (stderr, "Illegal sds descriptor\n");
            siDumpDescriptor (Buffer);
         } */
      }
      break;

      case DESCR_CABLE_DEL_SYS:
//         fprintf (stderr, "got descriptor 0x%x\n", GetDescriptorTag(Buffer));
      {
         descr_cable_delivery_system_t *cds;
         cds = (descr_cable_delivery_system_t *) Ptr;
         if (CheckBcdChar (cds->frequency1) && CheckBcdChar (cds->frequency2) &&
             CheckBcdChar (cds->frequency3) && CheckBcdChar (cds->frequency4) &&
             CheckBcdChar (cds->symbol_rate1) && CheckBcdChar (cds->symbol_rate1) &&
             CheckBcdChar (cds->symbol_rate3) && (cds->fec_inner != 0))
         {
           CreateCableDeliverySystemDescriptor (Descriptor,
            BcdCharToInt (cds->frequency1) * 100 * 1000 * 1000 +
            BcdCharToInt (cds->frequency2) * 1000 * 1000 +
            BcdCharToInt (cds->frequency3) * 10 * 1000 +
            BcdCharToInt (cds->frequency4) * 100,
            BcdCharToInt (cds->symbol_rate1) * 10 * 1000 +
            BcdCharToInt (cds->symbol_rate2) * 100 +
            BcdCharToInt (cds->symbol_rate3),
            cds->fec_outer,
            cds->fec_inner,
            cds->modulation
           );
         }
         /* else
         {
            fprintf (stderr, "Illegal cds descriptor\n");
            siDumpDescriptor (Buffer);
         } */
      }
      break;

      case DESCR_TERR_DEL_SYS:
//         fprintf (stderr, "got descriptor 0x%x\n", GetDescriptorTag(Buffer));
      {
         descr_terrestrial_delivery_system_t *tds;
         tds = (descr_terrestrial_delivery_system_t *) Ptr;
         if (CheckBcdChar (tds->frequency1) && CheckBcdChar (tds->frequency2) &&
             CheckBcdChar (tds->frequency3) && CheckBcdChar (tds->frequency4))
         {
           CreateTerrestrialDeliverySystemDescriptor (Descriptor,
            BcdCharToInt (tds->frequency1) * 100 * 1000 * 1000 +
            BcdCharToInt (tds->frequency2) * 1000 * 1000 +
            BcdCharToInt (tds->frequency3) * 10 * 1000 +
            BcdCharToInt (tds->frequency4) * 100,
            tds->bandwidth,
            tds->constellation,
          tds->hierarchy,
          tds->code_rate_HP,
          tds->code_rate_LP,
            tds->guard_interval,
            tds->transmission_mode,
            tds->other_frequency_flag
           );
         }
         /* else
         {
            fprintf (stderr, "Illegal cds descriptor\n");
            siDumpDescriptor (Buffer);
         } */
      }
      break;

      case DESCR_SERVICE_LIST:
//         fprintf (stderr, "got descriptor 0x%x\n", GetDescriptorTag(Buffer));
         CreateServiceListDescriptor (Descriptor);
         Length = GetDescriptorLength (Buffer) - DESCR_SERVICE_LIST_LEN;
         Ptr += DESCR_SERVICE_LIST_LEN;
         while (Length > 0)
         {
            AddServiceListEntry (Descriptor,
                   HILO (CastServiceListDescriptorLoop(Ptr)->service_id),
                   CastServiceListDescriptorLoop(Ptr)->service_type);
            Length -= DESCR_SERVICE_LIST_LEN;
            Ptr += DESCR_SERVICE_LIST_LEN;
         }
      break;

      case DESCR_LOCAL_TIME_OFF:
         CreateLocalTimeOffsetDescriptor (Descriptor);
         Length = GetDescriptorLength (Buffer) - DESCR_LOCAL_TIME_OFFSET_LEN;
         Ptr += DESCR_LOCAL_TIME_OFFSET_LEN;
         while (Length > 0)
         {
            time_t ct, co, no;
            ct = MjdToEpochTime (CastLocalTimeOffsetEntry(Ptr)->time_of_change_mjd) +
                 BcdTimeToSeconds (CastLocalTimeOffsetEntry(Ptr)->time_of_change_time);
            co = (BcdCharToInt(CastLocalTimeOffsetEntry(Ptr)->local_time_offset_h) * 3600 + 
                 BcdCharToInt(CastLocalTimeOffsetEntry(Ptr)->local_time_offset_m) * 60) * 
                 ((CastLocalTimeOffsetEntry(Ptr)->local_time_offset_polarity) ? -1 : 1); 
            no = (BcdCharToInt(CastLocalTimeOffsetEntry(Ptr)->next_time_offset_h) * 3600 + 
                 BcdCharToInt(CastLocalTimeOffsetEntry(Ptr)->next_time_offset_m) * 60) * 
                 ((CastLocalTimeOffsetEntry(Ptr)->local_time_offset_polarity) ? -1 : 1); 
            AddLocalTimeOffsetEntry (Descriptor,
                   CastLocalTimeOffsetEntry(Ptr)->country_code1, 
                   CastLocalTimeOffsetEntry(Ptr)->country_code2, 
                   CastLocalTimeOffsetEntry(Ptr)->country_code3, 
                   CastLocalTimeOffsetEntry(Ptr)->country_region_id, co, ct, no); 
            Length -= LOCAL_TIME_OFFSET_ENTRY_LEN;
            Ptr += LOCAL_TIME_OFFSET_ENTRY_LEN;
         }
      break;

      case DESCR_VIDEO_STREAM:
      case DESCR_AUDIO_STREAM:
      case DESCR_HIERARCHY:
      case DESCR_REGISTRATION:
      case DESCR_DATA_STREAM_ALIGN:
      case DESCR_TARGET_BACKGRID:
      case DESCR_VIDEO_WINDOW:
      case DESCR_SYSTEM_CLOCK:
      case DESCR_MULTIPLEX_BUFFER_UTIL:
      case DESCR_COPYRIGHT:
      case DESCR_MAXIMUM_BITRATE:
      case DESCR_PRIVATE_DATA_IND:
      case DESCR_SMOOTHING_BUFFER:
      case DESCR_STD:
      case DESCR_IBP:
      case DESCR_VBI_DATA:
      case DESCR_VBI_TELETEXT:
      case DESCR_MOSAIC:
      case DESCR_TELEPHONE:
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
//         fprintf (stderr, "Unsupported descriptor with tag = 0x%02X\n", GetDescriptorTag(Ptr));
//         siDumpDescriptor (Buffer);
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
#define GDT_TEXT_DESCRIPTOR 0
#define GDT_NAME_DESCRIPTOR 1
char *siGetDescriptorTextHandler (u_char *, int , int );

char *siGetDescriptorTextHandler (u_char *Buffer, int Length, int type)
{
   char *tmp, *result;
   int i;

   if ((Length < 0) || (Length > 4095))
      return (xSetText ("text error"));
/* ASSENIZATION: removing coding detection - suppose they are all ANSI */
   // if (*Buffer == 0x05 || (*Buffer >= 0x20 && *Buffer <= 0xff))
   {
      xMemAlloc (Length+1, &result);
      tmp = result;
      for (i = 0; i < Length; i++)
      {
         if (*Buffer == 0) break;

         if ((*Buffer >= ' ' && *Buffer <= '~') || (*Buffer == '\n') ||
             (*Buffer >= 0xa0 && *Buffer <= 0xff)) *tmp++ = *Buffer;
         if (*Buffer == 0x8A) *tmp++ = '\n';
         if ((*Buffer == 0x86 || *Buffer == 0x87) && !(GDT_NAME_DESCRIPTOR & type)) *tmp++ = ' ';
         Buffer++;
      }
      *tmp = '\0';
   }
   /* else
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
   } */

   return (result);
}

char *siGetDescriptorText (u_char *Buffer, int Length)
{
   return siGetDescriptorTextHandler (Buffer, Length, GDT_TEXT_DESCRIPTOR);
}

char *siGetDescriptorName (u_char *Buffer, int Length)
{
   return siGetDescriptorTextHandler (Buffer, Length, GDT_NAME_DESCRIPTOR);
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
