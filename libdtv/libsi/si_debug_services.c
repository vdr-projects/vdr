//////////////////////////////////////////////////////////////
///                                                        ///
/// si_debug_services.c: debugging functions for libsi     ///
///                                                        ///
//////////////////////////////////////////////////////////////

// $Revision: 1.5 $
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
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "../liblx/liblx.h"
#include "libsi.h"
#include "si_tables.h"
#include "si_debug_services.h"


void siDebugServices (struct LIST *Services)
{
   struct Service *Service;

   if (!Services) return;

   xForeach (Services, Service)
   {
      printf ("Service\n=======\n");
      printf ("   ServiceID: %d\n", Service->ServiceID);
      printf ("   TransportStreamID: %d\n", Service->TransportStreamID);
      printf ("   OriginalNetworkID: %d\n", Service->OriginalNetworkID);
      printf ("   SdtVersion: %d\n", Service->SdtVersion);
      printf ("   Status: ");
      if (GetScheduleFlag (Service->Status))
         printf ("SCHEDULE_INFO ");
      if (GetPresentFollowing(Service->Status))
         printf ("PRESENT_FOLLOWING ");
      switch (GetRunningStatus (Service->Status))
      {
         case RUNNING_STATUS_NOT_RUNNING:
            printf ("RUNNING_STATUS_NOT_RUNNING\n");
         break;

         case RUNNING_STATUS_AWAITING:
            printf ("RUNNING_STATUS_AWAITING\n");
         break;

         case RUNNING_STATUS_PAUSING:
            printf ("RUNNING_STATUS_PAUSING\n");
         break;

         case RUNNING_STATUS_RUNNING:
            printf ("RUNNING_STATUS_RUNNING\n");
         break;
      }
      siDebugDescriptors ("   ", Service->Descriptors);
      siDebugEvents ("   ", Service->Events);
   }
   return;
}

void siDebugService (struct Service *Service)
{
   if (!Service) return;

   printf ("Service\r\n=======\r\n");
   printf ("   ServiceID: %d\r\n", Service->ServiceID);
   printf ("   TransportStreamID: %d\r\n", Service->TransportStreamID);
   printf ("   OriginalNetworkID: %d\r\n", Service->OriginalNetworkID);
   printf ("   SdtVersion: %d\r\n", Service->SdtVersion);
   printf ("   Status: ");
   if (GetScheduleFlag (Service->Status))
      printf ("SCHEDULE_INFO ");
   if (GetPresentFollowing(Service->Status))
      printf ("PRESENT_FOLLOWING ");
   switch (GetRunningStatus (Service->Status))
   {
      case RUNNING_STATUS_NOT_RUNNING:
         printf ("RUNNING_STATUS_NOT_RUNNING\r\n");
      break;

      case RUNNING_STATUS_AWAITING:
         printf ("RUNNING_STATUS_AWAITING\r\n");
      break;

      case RUNNING_STATUS_PAUSING:
         printf ("RUNNING_STATUS_PAUSING\r\n");
      break;

      case RUNNING_STATUS_RUNNING:
         printf ("RUNNING_STATUS_RUNNING\r\n");
      break;
   }
   siDebugDescriptors ("\r   ", Service->Descriptors);
   siDebugEvents ("\r   ", Service->Events);

   return;
}

void siDebugEvents (char *Prepend, struct LIST *EventList)
{
   struct Event *Event;
   char          NewPrepend[32];

   if (!EventList) return;

   xForeach (EventList, Event)
   {
      printf ("%sEvent\n%s=====\n", Prepend, Prepend);
      printf ("%s   EventID: %d\n", Prepend, Event->EventID);
      printf ("%s   ServiceID: %d\n", Prepend, Event->ServiceID);
      printf ("%s   TransportStreamID: %d\n", Prepend, Event->TransportStreamID);
      printf ("%s   OriginalNetworkID: %d\n", Prepend, Event->OriginalNetworkID);
      printf ("%s   EitVersion: %d\n", Prepend, Event->EitVersion);
      printf ("%s   StartTime: %s", Prepend, ctime (&Event->StartTime));
      printf ("%s   Duration: %d Minuten\n", Prepend, Event->Duration/60);
      printf ("%s   Status: ");
      switch (GetRunningStatus (Event->Status))
      {
         case RUNNING_STATUS_NOT_RUNNING:
            printf ("RUNNING_STATUS_NOT_RUNNING\n");
         break;

         case RUNNING_STATUS_AWAITING:
            printf ("RUNNING_STATUS_AWAITING\n");
         break;

         case RUNNING_STATUS_PAUSING:
            printf ("RUNNING_STATUS_PAUSING\n");
         break;

         case RUNNING_STATUS_RUNNING:
            printf ("RUNNING_STATUS_RUNNING\n");
         break;
      }
      sprintf (NewPrepend, "%s   ", Prepend);
      siDebugDescriptors (NewPrepend, Event->Descriptors);
   }
   return;
}


void siDebugPrograms (char *Prepend, struct LIST *ProgramList)
{
   struct Program *Program;
   char            NewPrepend[32];

   if (!ProgramList) return;

   xForeach (ProgramList, Program)
   {
      printf ("%sProgram\n%s=======\n", Prepend, Prepend);
      printf ("%s   ProgramID: %d\n", Prepend, Program->ProgramID);
      printf ("%s   TransportStreamID: %d\n", Prepend, Program->TransportStreamID);
      printf ("%s   NetworkPID: %d\n", Prepend, Program->NetworkPID);
      printf ("%s   PatVersion: %d\n", Prepend, Program->PatVersion);

      sprintf (NewPrepend, "%s   ", Prepend);
      siDebugPids (NewPrepend, Program->Pids);
   }
   return;
}

void siDebugProgram (struct Program *Program)
{
   if (!Program) return;

   printf ("Program\r\n=======\r\n");
   printf ("   ProgramID: %d\r\n", Program->ProgramID);
   printf ("   TransportStreamID: %d\r\n", Program->TransportStreamID);
   printf ("   NetworkPID: %d\r\n", Program->NetworkPID);
   printf ("   PatVersion: %d\r\n", Program->PatVersion);

   siDebugPids ("\r   ", Program->Pids);

   return;
}

void siDebugPids (char *Prepend, struct LIST *PidList)
{
   struct Pid         *Pid;
   struct PidInfo     *PidInfo;
   char                NewPrepend[32];
   int                 index;

   if (!PidList) return;

   xForeach (PidList, Pid)
   {
      printf ("%sPid\n%s===\n", Prepend, Prepend);
      printf ("%s   ProgramID: %d\n", Prepend, Pid->ProgramID);
      printf ("%s   PcrPid: %d\n", Prepend, Pid->PcrPID);
      printf ("%s   PmtVersion: %d\n", Prepend, Pid->PmtVersion);
      sprintf (NewPrepend, "%s      ", Prepend);
      siDebugDescriptors (NewPrepend, Pid->Descriptors);

      xForeach (Pid->InfoList, PidInfo)
      {
         printf ("%s   PidInfo\n%s   =======\n", Prepend, Prepend);
         index = PidInfo->StreamType;
         if (index > 0x0F && index <= 0x7F) index = 0x0E;
         if (index >= 0x80) index = 0x0F;
         printf ("%s      StreamType: %s\n", Prepend, StreamTypes[index]);
         printf ("%s      ElementaryPid: %d\n", Prepend, PidInfo->ElementaryPid);

         sprintf (NewPrepend, "%s         ", Prepend);
         siDebugDescriptors (NewPrepend, PidInfo->Descriptors);
      }
   }
   return;
}


void siDebugDescriptors (char *Prepend, struct LIST *Descriptors)
{
   struct Descriptor *Descriptor;
   int i;

   xForeach (Descriptors, Descriptor)
   {
      switch (DescriptorTag (Descriptor))
      {
         case DESCR_ANCILLARY_DATA:
            printf ("%sDescriptor: Ancillary Data\n", Prepend);
            printf ("%s   Identifier: ", Prepend);
            if (((struct AncillaryDataDescriptor *)Descriptor)->
                   Identifier & ANCILLARY_DATA_DVD_VIDEO)
               printf ("DVD-Video Ancillary Data ");
            if (((struct AncillaryDataDescriptor *)Descriptor)->
                   Identifier & ANCILLARY_DATA_EXTENDED)
               printf ("Extended Ancillary Data ");
            if (((struct AncillaryDataDescriptor *)Descriptor)->
                   Identifier & ANCILLARY_DATA_SWITCHING)
               printf ("Announcement Switching Data ");
            if (((struct AncillaryDataDescriptor *)Descriptor)->
                   Identifier & ANCILLARY_DATA_DAB)
               printf ("DAB Ancillary Data ");
            if (((struct AncillaryDataDescriptor *)Descriptor)->
                   Identifier & ANCILLARY_DATA_SCALE_FACTOR)
               printf ("Scale Factor Error Check (ScF-CRC) ");
            printf ("\n");
         break;

         case DESCR_NW_NAME:
            printf ("%sDescriptor: Network Name\n", Prepend);
            printf ("%s   Name: %s\n", Prepend, xName (Descriptor));
         break;

         case DESCR_BOUQUET_NAME:
            printf ("%sDescriptor: Bouquet Name\n", Prepend);
            printf ("%s   Name: %s\n", Prepend, xName (Descriptor));
         break;

         case DESCR_COMPONENT:
            printf ("%sDescriptor: Component\n", Prepend);
            printf ("%s   Text: %s\n", Prepend, xName (Descriptor));
            printf ("%s   Content/Type: ", Prepend);
            for (i = 0; i < COMPONENT_TYPE_NUMBER; i++)
               if ((((struct ComponentDescriptor *)Descriptor)->
                        StreamContent == ComponentTypes[i].Content) &&
                   (((struct ComponentDescriptor *)Descriptor)->
                        ComponentType == ComponentTypes[i].Type))
               { printf ("%s\n", ComponentTypes[i].Description); break; }
            if (i == COMPONENT_TYPE_NUMBER) { printf ("unbekannt\n"); }
            printf ("%s   ComponentTag: 0x%02x\n", Prepend,
               ((struct ComponentDescriptor *)Descriptor)->ComponentTag);
            printf ("%s   LanguageCode: %s\n", Prepend,
               ((struct ComponentDescriptor *)Descriptor)->LanguageCode);
         break;

         case DESCR_SERVICE:
            printf ("%sDescriptor: Service\n", Prepend);
            printf ("%s   Name: %s\n", Prepend, xName (Descriptor));
            printf ("%s   ServiceType: ", Prepend);
            for (i = 0; i < SERVICE_TYPE_NUMBER; i++)
               if ((((struct ServiceDescriptor *)Descriptor)->
                        ServiceType == ServiceTypes[i].Type))
               { printf ("%s\n", ServiceTypes[i].Description); break; }
            if (i == SERVICE_TYPE_NUMBER) { printf ("unbekannt\n"); }
            printf ("%s   ServiceProvider: %s\n", Prepend,
               ((struct ServiceDescriptor *)Descriptor)->ServiceProvider);
         break;

         case DESCR_COUNTRY_AVAIL:
            printf ("%sDescriptor: Country Availability\n", Prepend);
            printf ("%s   Type: %s\n", Prepend, (((struct CountryAvailabilityDescriptor *)Descriptor)->
                       AvailibilityFlag == COUNTRIES_ARE_AVAILABLE) ? "countries are available" :
                       "countries are unavailable");
            {
               char *cptr = ((struct CountryAvailabilityDescriptor *)Descriptor)->CountryCodes; int j;
               for (j = 0; j < ((struct CountryAvailabilityDescriptor *)Descriptor)->Amount; j++)
                { printf ("%s   Country: %s\n", Prepend, cptr); cptr += 4; }
            }
         break;

         case DESCR_SHORT_EVENT:
            printf ("%sDescriptor: Short Event\n", Prepend);
            printf ("%s   Name: %s\n", Prepend, xName (Descriptor));
            printf ("%s   LanguageCode: %s\n", Prepend,
               ((struct ShortEventDescriptor *)Descriptor)->LanguageCode);
            printf ("%s   Text: %s\n", Prepend,
               ((struct ShortEventDescriptor *)Descriptor)->Text);
         break;

         case DESCR_EXTENDED_EVENT:
         {
            struct ExtendedEventItem *Item;

            printf ("%sDescriptor: Extended Event\n", Prepend);
            printf ("%s   Text: %s\n", Prepend, xName (Descriptor));
            printf ("%s   DescriptorNumber: %d\n", Prepend,
               ((struct ExtendedEventDescriptor *)Descriptor)->DescriptorNumber);
            printf ("%s   LastDescriptorNumber: %d\n", Prepend,
               ((struct ExtendedEventDescriptor *)Descriptor)->LastDescriptorNumber);
            printf ("%s   LanguageCode: %s\n", Prepend,
               ((struct ExtendedEventDescriptor *)Descriptor)->LanguageCode);
            xForeach (((struct ExtendedEventDescriptor *)Descriptor)->Items, Item)
            {
               printf ("%s   Item:\n", Prepend);
               printf ("%s      Description: %s\n", Prepend, xName(Item));
               printf ("%s      Text: %s\n", Prepend, Item->Text);
            }
         }
         break;

         case DESCR_CA_IDENT:
            printf ("%sDescriptor: Conditional Access Identity\n", Prepend);
            {
               int j,k;
               for (j = 0; j < ((struct CaIdentifierDescriptor *)Descriptor)->Amount; j++)
               {
                  printf ("%s   SystemID: 0x%04x", Prepend, GetCaIdentifierID (Descriptor, j));
                  k = GetCaIdentifierID (Descriptor, j) >> 8;
                  if (k < 0 || k > MAX_CA_IDENT) printf (" (unknown)\n");
                  else printf (" (%s)\n", CaIdents[k]);
               }
            }
         break;

         case DESCR_CA:
         {
            int j,k;

            printf ("%sDescriptor: Conditional Access\n", Prepend);
            printf ("%s   CA type: 0x%04x", Prepend, (((struct CaDescriptor *)Descriptor)->CA_type));
            k = (((struct CaDescriptor *)Descriptor)->CA_type) >> 8;
            if (k < 0 || k > MAX_CA_IDENT) printf (" (unknown)\n");
            else printf (" (%s)\n", CaIdents[k]);
            printf ("%s   CA PID: %d\n", Prepend, (((struct CaDescriptor *)Descriptor)->CA_PID));
            printf ("%s   ProviderID: 0x%04X\n", Prepend, (((struct CaDescriptor *)Descriptor)->ProviderID));
            if (((struct CaDescriptor *)Descriptor)->DataLength > 0)
            {
               printf ("%s   CA data:", Prepend);
               for (j = 0; j < ((struct CaDescriptor *)Descriptor)->DataLength; j++)
                  printf (" 0x%02x", GetCaData (Descriptor, j));
               printf ("\n");
            }
         }
         break;

         case DESCR_CONTENT:
            printf ("%sDescriptor: Content\n", Prepend);
            {
               int j;
               for (j = 0; j < ((struct ContentDescriptor *)Descriptor)->Amount; j++)
               {
                  printf ("%s   Content: ", Prepend);
                  for (i = 0; i < CONTENT_TYPE_NUMBER; i++)
                     if ((GetContentContentNibble1(Descriptor, j) == ContentTypes[i].Nibble1) &&
                         (GetContentContentNibble2(Descriptor, j) == ContentTypes[i].Nibble2))
                     { printf ("%s\n", ContentTypes[i].Description); break; }
                  if (i == CONTENT_TYPE_NUMBER) { printf ("unbekannt\n"); }
                  printf ("%s   User-Nibble 1: 0x%1x\n", Prepend, GetContentUserNibble1(Descriptor, j));
                  printf ("%s   User-Nibble 2: 0x%1x\n", Prepend, GetContentUserNibble2(Descriptor, j));
               }
            }
         break;

         case DESCR_PARENTAL_RATING:
         {
            struct ParentalRating *Rating;

            printf ("%sDescriptor: Parental Rating\n", Prepend);
            xForeach (((struct ParentalRatingDescriptor *)Descriptor)->Ratings, Rating)
            {
               printf ("%s   Rating:\n");
               printf ("%s      LanguageCode: %s\n", Rating->LanguageCode);
               printf ("%s      Rating: ");
               if (Rating->Rating == 0) printf ("(undefined)\n");
               else { if (Rating->Rating <= 0x10) printf ("minimum age is %d\n", Rating->Rating + 3);
                      else printf ("(rating is provider defined)\n"); }
            }
         }
         break;

         case DESCR_NVOD_REF:
         {
            struct NvodReferenceItem *Item;

            printf ("%sDescriptor: NVOD Reference\n", Prepend);
            xForeach (((struct NvodReferenceDescriptor *)Descriptor)->Items, Item)
            {
               printf ("%s   Item:\n", Prepend);
               printf ("%s      ServiceID: %d\n", Prepend, Item->ServiceID);
               printf ("%s      TransportStreamID: %d\n", Prepend, Item->TransportStreamID);
               printf ("%s      OriginalNetworkID: %d\n", Prepend, Item->OriginalNetworkID);
            }
         }
         break;

         case DESCR_TIME_SHIFTED_SERVICE:
            printf ("%sDescriptor: Time Shifted Service\n", Prepend);
            printf ("%s   ReferenceServiceID: %d\n", Prepend,
                                      ((struct TimeShiftedServiceDescriptor *)
                                         Descriptor)->ReferenceServiceID);
         break;

         case DESCR_TIME_SHIFTED_EVENT:
            printf ("%sDescriptor: Time Shifted Event\n", Prepend);
            printf ("%s   ReferenceServiceID: %d\n", Prepend,
                                      ((struct TimeShiftedEventDescriptor *)
                                         Descriptor)->ReferenceServiceID);
            printf ("%s   ReferenceEventID: %d\n", Prepend,
                                      ((struct TimeShiftedEventDescriptor *)
                                         Descriptor)->ReferenceEventID);
         break;

         case DESCR_ISO_639_LANGUAGE:
            printf ("%sDescriptor: ISO 639 Language\n", Prepend);
            printf ("%s   LanguageCode: %s\n", Prepend,
               ((struct Iso639LanguageDescriptor *)Descriptor)->LanguageCode);
         break;

         case DESCR_STREAM_ID:
            printf ("%sDescriptor: Stream Identifier\n", Prepend);
            printf ("%s   ComponentTag: %d\n", Prepend,
               ((struct StreamIdentifierDescriptor *)Descriptor)->ComponentTag);
         break;

         case DESCR_LINKAGE:
            printf ("%sDescriptor: Linkage\n", Prepend);
            printf ("%s   TransportStreamID: %d\n", Prepend,
               ((struct LinkageDescriptor *)Descriptor)->TransportStreamID);
            printf ("%s   OriginalNetworkID: %d\n", Prepend,
               ((struct LinkageDescriptor *)Descriptor)->OriginalNetworkID);
            printf ("%s   ServiceID: %d\n", Prepend,
               ((struct LinkageDescriptor *)Descriptor)->ServiceID);
            printf ("%s   LinkageType: %d\n", Prepend,
               ((struct LinkageDescriptor *)Descriptor)->LinkageType);
            if (((struct LinkageDescriptor *)Descriptor)->PrivateDataLength)
            {
               int j;
               printf ("%s   PrivateData: ", Prepend);
               for (j = 0; j < ((struct LinkageDescriptor *)
                        Descriptor)->PrivateDataLength; j++)
                  printf ("0x%02X ", ((struct LinkageDescriptor *)
                        Descriptor)->PrivateData[j]);
               printf ("\n");
            }
         break;

         case DESCR_TELETEXT:
         {
            struct TeletextItem *Item;

            printf ("%sDescriptor: Teletext\n", Prepend);
            xForeach (((struct TeletextDescriptor *)Descriptor)->Items, Item)
            {
               printf ("%s   Item:\n");
               printf ("%s      LanguageCode: %s\n", Prepend, Item->LanguageCode);
               printf ("%s      Type: ", Prepend);
               switch (Item->Type)
               {
                  case 0x01: printf ("initial Teletext page\n"); break;
                  case 0x02: printf ("Teletext subtitle page\n"); break;
                  case 0x03: printf ("additional information page\n"); break;
                  case 0x04: printf ("programme schedule page\n"); break;
                  case 0x05: printf ("Teletext subtitle page ");
                             printf ("for hearing impaired people\n"); break;
                  default: printf ("reserved for future use\n"); break;
               }
               printf ("%s      MagazineNumber: %x\n", Prepend, Item->MagazineNumber);
               printf ("%s      PageNumber: %x\n", Prepend, Item->PageNumber);
            }
         }
         break;

         case DESCR_SUBTITLING:
         {
            struct SubtitlingItem *Item;

            printf ("%sDescriptor: Subtitling\n", Prepend);
            xForeach (((struct SubtitlingDescriptor *)Descriptor)->Items, Item)
            {
               printf ("%s   Item:\n");
               printf ("%s      LanguageCode: %s\n", Prepend, Item->LanguageCode);
               printf ("%s      Type: ", Prepend);
               for (i = 0; i < COMPONENT_TYPE_NUMBER; i++)
                  if ((0x03 == ComponentTypes[i].Content) &&
                      (Item->Type == ComponentTypes[i].Type))
                  { printf ("%s\n", ComponentTypes[i].Description); break; }
               printf ("%s      CompositionPageId: %x\n", Prepend, Item->CompositionPageId);
               printf ("%s      AncillaryPageId: %x\n", Prepend, Item->AncillaryPageId);
            }
         }
         break;

         case DESCR_SAT_DEL_SYS:
         {
            struct SatelliteDeliverySystemDescriptor *sds =
                (struct SatelliteDeliverySystemDescriptor *)Descriptor;

            printf ("%sDescriptor: Satellite Delivery System\n", Prepend);
            printf ("%s   Frequency: %ld\n", Prepend, sds->Frequency);
            printf ("%s   OrbitalPosition: %d\n", Prepend, sds->OrbitalPosition);
            printf ("%s   Polarization: %c\n", Prepend, sds->Polarization);
            printf ("%s   Modulation: %c\n", Prepend, sds->Modulation);
            printf ("%s   SymbolRate: %ld\n", Prepend, sds->SymbolRate);
            printf ("%s   FEC: %c\n", Prepend, sds->FEC);
         }
         break;

         case DESCR_CABLE_DEL_SYS:
         {
            struct CableDeliverySystemDescriptor *cds =
                (struct CableDeliverySystemDescriptor *)Descriptor;

            printf ("%sDescriptor: Cable Delivery System\n", Prepend);
            printf ("%s   Frequency: %ld\n", Prepend, cds->Frequency);
            printf ("%s   SymbolRate: %ld\n", Prepend, cds->SymbolRate);
            printf ("%s   FEC outer: %d\n", Prepend, cds->FECouter);
            printf ("%s   FEC inner: %d\n", Prepend, cds->FECinner);
            printf ("%s   Modulation: %d\n", Prepend, cds->Modulation);
         }
         break;

         case DESCR_TERR_DEL_SYS:
         {
            struct TerrestrialDeliverySystemDescriptor *tds =
                (struct TerrestrialDeliverySystemDescriptor *)Descriptor;

            printf ("%sDescriptor: Terrestrial Delivery System\n", Prepend);
            printf ("%s   Frequency: %ld\n", Prepend, tds->Frequency);
            printf ("%s   Bandwidth: %d\n", Prepend, tds->Bandwidth);
            printf ("%s   Constellation: %d\n", Prepend, tds->Constellation);
            printf ("%s   Hierarchy: %d\n", Prepend, tds->Hierarchy);
            printf ("%s   CodeRateHP: %d\n", Prepend, tds->CodeRateHP);
            printf ("%s   CodeRateLP: %d\n", Prepend, tds->CodeRateLP);
            printf ("%s   GuardInterval: %d\n", Prepend, tds->GuardInterval);
            printf ("%s   TransmissionMode: %d\n", Prepend, tds->TransmissionMode);
            printf ("%s   OtherFrequencyFlag: %d\n", Prepend, tds->OtherFrequencyFlag);
         }
         break;

         case DESCR_SERVICE_LIST:
         {
            struct ServiceListEntry *Entry;

            printf ("%sDescriptor: Service List\n", Prepend);
            xForeach (((struct ServiceListDescriptor *)Descriptor)->ServiceList, Entry)
            {
               printf ("%s   Entry:\n");
               printf ("%s      ServiceID: %d\n", Prepend, Entry->ServiceID);
               printf ("%s      ServiceType: %04x\n", Prepend, Entry->ServiceType);
            }
         }
         break;

         case DESCR_LOCAL_TIME_OFF:
         {
            struct LocalTimeOffsetEntry *Offset;

            printf ("%sDescriptor: Local Time Offset\n", Prepend);
            xForeach (((struct LocalTimeOffsetDescriptor *)Descriptor)->LocalTimeOffsets, Offset)
            {
               printf ("%s   Offset:\n");
               printf ("%s      CountryCode: %s\n", Offset->CountryCode);
               printf ("%s      RegionID: %c\n", Offset->RegionID);
               printf ("%s      CurrentOffset: %ld\n", Offset->CurrentOffset);
               printf ("%s      ChangeTime: %ld\n", Offset->ChangeTime);
               printf ("%s      NextOffset: %ld\n", Offset->NextOffset);
            }
         }
         break;

         case DESCR_STUFFING:
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
         case DESCR_AC3:
         case DESCR_CELL_LIST:
         case DESCR_CELL_FREQ_LINK:
         case DESCR_ANNOUNCEMENT_SUPPORT:
         default:
            printf ("%sDescriptor: %02x (noch nicht unterstützt)\n", Prepend, DescriptorTag (Descriptor));
         break;
      }
   }
   return;
}

void siDumpDescriptor (void * Descriptor)
{
   int Length, i;
   unsigned char *ptr;

   Length = GetDescriptorLength (Descriptor);
   for (i = 0, ptr = (char*) Descriptor; i < Length; i++) {
        if ((i % 8) == 0) 
           printf ("\n");
        printf ("0x%02X ", (unsigned int) ptr[i]);
   }
   printf ( "\n");
}

void siDumpSection (void *Section)
{
   int Length, i;
   unsigned char *ptr;
   char str[9];

   Length = GetSectionLength (Section) + 3;
   for (i = 0, ptr = (unsigned char*) Section, memset (str, 0, 9); i < Length; i++) {
        if ((i % 8) == 0) 
        {
           printf ("  %s\n", str);
           memset (str, 0, 8);
        }
        printf ("0x%02X ", (unsigned int) ptr[i]);
        if (ptr[i] < 0x20 || (ptr[i] > 'z' && ptr[i] < ((unsigned char )'À')) )
           str[i % 8] = '.';
        else
           str[i % 8] = ptr[i];
   }
   printf ("  %*s\n", (8 - ((abs(i - 1) % 8) ? (abs(i - 1) % 8) : 8)) * 5, str);
}

