//////////////////////////////////////////////////////////////
///                                                        ///
/// libvdr.h: definitions necessary for the libvdr package ///
///                                                        ///
//////////////////////////////////////////////////////////////

// $Revision: 1.4 $
// $Date: 2001/10/06 15:33:46 $
// $Author: hakenes $
//
//   (C) 1992-2001 Rolf Hakenes <hakenes@hippomi.de>, under the GNU GPL.
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

#ifndef LIBVDR_H
#define LIBVDR_H

#ifdef __cplusplus
extern "C" {
#endif

struct LIST *createVdrProgramInfos (unsigned char *);

#ifdef __cplusplus
}
#endif

struct VdrProgramInfo {
   struct NODE          Node;
   int                  EventID;
   int                  TransportStreamID;
   int                  ServiceID;
   time_t               StartTime;
   time_t               Duration;
   unsigned short       Status;
   char                 LanguageCode[4];
   unsigned short       Rating;
   unsigned short       ContentNibble1;
   unsigned short       ContentNibble2;
   char                *ShortName;
   char                *ShortText;
   char                *ExtendedName;
   char                *ExtendedText;
   int                  ReferenceServiceID;
   int                  ReferenceEventID;
};


#define CreateVdrProgramInfo(cinf, evid, tpid, svid, stst, dura, sta) \
   do \
   { \
      xCreateNode (cinf, NULL); \
      cinf->EventID = evid; \
      cinf->TransportStreamID = tpid; \
      cinf->ServiceID = svid; \
      cinf->StartTime = stst; \
      cinf->Duration = dura; \
      cinf->Status = sta; \
      cinf->LanguageCode[0] = 0; \
      cinf->Rating = 0; \
      cinf->ContentNibble1 = 0; \
      cinf->ContentNibble2 = 0; \
      cinf->ShortName = NULL; \
      cinf->ShortText = NULL; \
      cinf->ExtendedName = NULL; \
      cinf->ExtendedText = NULL; \
      cinf->ReferenceServiceID = 0; \
      cinf->ReferenceEventID = 0; \
   } while (0)

#define AddToText(src, dest) \
   do { \
      if (dest) \
      { \
         char *tmbuf; \
         xMemAlloc (strlen (src) + strlen (dest) + 4, &tmbuf); \
         sprintf (tmbuf, "%s%s", (dest), (src)); \
         xMemFree (dest); (dest) = tmbuf; \
      } else { \
         (dest) = xSetText (src); \
      } \
   } while (0)


#define AddItemToText(src, dest) \
   do { \
      if (dest) \
      { \
         char *tmbuf; \
         xMemAlloc (strlen (src) + strlen (dest) + 4, &tmbuf); \
         sprintf (tmbuf, "%s|%s", (dest), (src)); \
         xMemFree (dest); (dest) = tmbuf; \
      } else { \
         (dest) = xSetText (src); \
      } \
   } while (0)

#endif
