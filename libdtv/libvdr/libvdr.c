//////////////////////////////////////////////////////////////
///                                                        ///
/// libvdr.c: routines to parse the DVB-SI stream          ///
///                                                        ///
//////////////////////////////////////////////////////////////

// $Revision: 1.1 $
// $Date: 2001/10/07 10:25:33 $
// $Author: hakenes $
//
//   (C) 2001 Rolf Hakenes <hakenes@hippomi.de>, under the GNU GPL.
//
// libvdr is free software; you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation; either version 2, or (at your option)
// any later version.
//
// libvdr is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// You may have received a copy of the GNU General Public License
// along with libvdr; see the file COPYING.  If not, write to the
// Free Software Foundation, Inc., 59 Temple Place - Suite 330,
// Boston, MA 02111-1307, USA.

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/ioctl.h>
#include <fcntl.h>

#include <liblx.h>
#include <libsi.h>
#include <si_tables.h>
#include "libvdr.h"



struct LIST *createVdrProgramInfos (unsigned char *siBuffer)
{
   struct VdrProgramInfo      *VdrProgramInfo;
   struct LIST                *Result, *EventList;
   struct Event               *Event;
   struct Descriptor          *Descriptor;
   int GotVdrProgramInfo;
   
   if (!siBuffer) return (NULL);

   if (!(EventList = siParseEIT (siBuffer))) return (NULL);

   Result = xNewList (NULL);

   xForeach (EventList, Event)
   {
      VdrProgramInfo = NULL;
      GotVdrProgramInfo = 0;
      
      xForeach (Event->Descriptors, Descriptor)
      {
         if (!VdrProgramInfo)
         {
            CreateVdrProgramInfo(VdrProgramInfo,
               Event->EventID, Event->TransportStreamID,
               Event->ServiceID, Event->StartTime,
               Event->Duration, Event->Status);
         }
         
         switch (Descriptor->Tag)
         {
            case DESCR_SHORT_EVENT:
            {
               if (!xName(Descriptor) || !xName(Descriptor)[0])
                  break;

               VdrProgramInfo->ShortName =
                  xSetText (xName (Descriptor));
               VdrProgramInfo->ShortText =
                  xSetText (((struct ShortEventDescriptor
                     *)Descriptor)->Text);
               memcpy (VdrProgramInfo->LanguageCode, ((struct
                  ShortEventDescriptor *)Descriptor)->
                  LanguageCode, 4);
               GotVdrProgramInfo = 1;
            }      
            break;

            case DESCR_TIME_SHIFTED_EVENT:
            {
               struct tm *StartTime;

               VdrProgramInfo->ReferenceServiceID =
                  ((struct TimeShiftedEventDescriptor
                     *)Descriptor)->ReferenceServiceID;
               VdrProgramInfo->ReferenceEventID =
                  ((struct TimeShiftedEventDescriptor
                     *)Descriptor)->ReferenceEventID;
               GotVdrProgramInfo = 1;
            }
            break;

            case DESCR_EXTENDED_EVENT:
            {
               struct ExtendedEventItem *Item;

               if (xName (Descriptor))
                  AddToText (xName (Descriptor),
                     VdrProgramInfo->ExtendedName);
               xForeach (((struct ExtendedEventDescriptor*)
                  Descriptor)->Items, Item)
               {
                  AddItemToText (xName (Item),
                     VdrProgramInfo->ExtendedText);
                  AddItemToText (Item->Text,
                     VdrProgramInfo->ExtendedText);
               }
               GotVdrProgramInfo = 1;
            }
            break;

            case DESCR_CONTENT:
            {
               int i, j;

               for (j = 0; j < ((struct ContentDescriptor*)
                  Descriptor)->Amount; j++)
               {
                  VdrProgramInfo->ContentNibble1 =
                     GetContentContentNibble1(Descriptor, j);
                  VdrProgramInfo->ContentNibble2 =
                     GetContentContentNibble2(Descriptor, j);
               }
               GotVdrProgramInfo = 1;
            }
            break;

            case DESCR_PARENTAL_RATING:
            {
               struct ParentalRating *Rating;
               
               xForeach (((struct ParentalRatingDescriptor *)
                           Descriptor)->Ratings, Rating)
                  if (!strncmp (VdrProgramInfo->LanguageCode,
                          Rating->LanguageCode, 3))
                     VdrProgramInfo->Rating = Rating->Rating;
               GotVdrProgramInfo = 1;
            }
            break;
         }
      }      
      if (GotVdrProgramInfo) xAddTail (Result, VdrProgramInfo);
   }

   return (Result);
}
