//////////////////////////////////////////////////////////////
///                                                        ///
/// libsi.h: definitions for data structures of libsi      ///
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

#ifndef LIBSI_H
#define LIBSI_H

#include <time.h>
#include <sys/types.h>
#include <asm/types.h>

#define dvb_pid_t int


  /* Program Identifier */

#define PID_PAT               0x00           /* Program Association Table */
#define PID_BAT               0x01           /* Bouquet Association Table */
#define PID_CAT               0x01           /* Conditional Access Table */
#define PID_NIT               0x10           /* Network Information Table */
#define PID_SDT               0x11           /* Service Description Table */
#define PID_EIT               0x12           /* Event Information Table */
#define PID_RST               0x13           /* Running Status Table */
#define PID_TDT               0x14           /* Time Date Table */
#define PID_TOT               0x14           /* Time Offset Table */
#define PID_ST                0x14           /* Stuffing Table */
                           /* 0x15 - 0x1F */ /* Reserved for future use */

  /* Table Identifier */

#define TID_PAT               0x00           /* Program Association Section */
#define TID_CAT               0x01           /* Conditional Access Section */
#define TID_PMT               0x02           /* Conditional Access Section */
                           /* 0x03 - 0x3F */ /* Reserved for future use */
#define TID_NIT_ACT           0x40           /* Network Information Section -
                                                   actual */
#define TID_NIT_OTH           0x41           /* Network Information Section -
                                                   other */
#define TID_SDT_ACT           0x42           /* Service Description Section -
                                                   actual */
#define TID_SDT_OTH           0x46           /* Service Description Section -
                                                   other */
#define TID_EIT_ACT           0x4E           /* Event Information Section -
                                                   actual */
#define TID_EIT_OTH           0x4F           /* Event Information Section -
                                                   other */
#define TID_EIT_ACT_SCH       0x50           /* Event Information Section -
                                                   actual, schedule  */
#define TID_EIT_OTH_SCH       0x60           /* Event Information Section -
                                                   other, schedule */
#define TID_TDT               0x70           /* Time Date Section */
#define TID_TOT               0x73           /* Time Offset Section */
#define TID_CA_ECM_0          0x80
#define TID_CA_ECM_1          0x81

#define TID_BAT               0x01           /* Bouquet Association Section */

#define TID_EIT               0x12           /* Event Information Section */
#define TID_RST               0x13           /* Running Status Section */
#define TID_ST                0x14           /* Stuffung Section */
                           /* 0xFF */        /* Reserved for future use */

  /* Descriptor Identifier */

  /* defined by ISO/IEC 13818-1 */

#define DESCR_VIDEO_STREAM                   0x02
#define DESCR_AUDIO_STREAM                   0x03
#define DESCR_HIERARCHY                      0x04
#define DESCR_REGISTRATION                   0x05
#define DESCR_DATA_STREAM_ALIGN              0x06
#define DESCR_TARGET_BACKGRID                0x07
#define DESCR_VIDEO_WINDOW                   0x08
#define DESCR_CA                             0x09
#define DESCR_ISO_639_LANGUAGE               0x0A
#define DESCR_SYSTEM_CLOCK                   0x0B
#define DESCR_MULTIPLEX_BUFFER_UTIL          0x0C
#define DESCR_COPYRIGHT                      0x0D
#define DESCR_MAXIMUM_BITRATE                0x0E
#define DESCR_PRIVATE_DATA_IND               0x0F
#define DESCR_SMOOTHING_BUFFER               0x10
#define DESCR_STD                            0x11
#define DESCR_IBP                            0x12
                                          /* 0x13 - 0x3F */ /* Reserved */

  /* defined by ETSI */

#define DESCR_NW_NAME                        0x40
#define DESCR_SERVICE_LIST                   0x41
#define DESCR_STUFFING                       0x42
#define DESCR_SAT_DEL_SYS                    0x43
#define DESCR_CABLE_DEL_SYS                  0x44
#define DESCR_VBI_DATA                       0x45
#define DESCR_VBI_TELETEXT                   0x46
#define DESCR_BOUQUET_NAME                   0x47
#define DESCR_SERVICE                        0x48
#define DESCR_COUNTRY_AVAIL                  0x49
#define DESCR_LINKAGE                        0x4A
#define DESCR_NVOD_REF                       0x4B
#define DESCR_TIME_SHIFTED_SERVICE           0x4C
#define DESCR_SHORT_EVENT                    0x4D
#define DESCR_EXTENDED_EVENT                 0x4E
#define DESCR_TIME_SHIFTED_EVENT             0x4F
#define DESCR_COMPONENT                      0x50
#define DESCR_MOSAIC                         0x51
#define DESCR_STREAM_ID                      0x52
#define DESCR_CA_IDENT                       0x53
#define DESCR_CONTENT                        0x54
#define DESCR_PARENTAL_RATING                0x55
#define DESCR_TELETEXT                       0x56
#define DESCR_TELEPHONE                      0x57
#define DESCR_LOCAL_TIME_OFF                 0x58
#define DESCR_SUBTITLING                     0x59
#define DESCR_TERR_DEL_SYS                   0x5A
#define DESCR_ML_NW_NAME                     0x5B
#define DESCR_ML_BQ_NAME                     0x5C
#define DESCR_ML_SERVICE_NAME                0x5D
#define DESCR_ML_COMPONENT                   0x5E
#define DESCR_PRIV_DATA_SPEC                 0x5F
#define DESCR_SERVICE_MOVE                   0x60
#define DESCR_SHORT_SMOOTH_BUF               0x61
#define DESCR_FREQUENCY_LIST                 0x62
#define DESCR_PARTIAL_TP_STREAM              0x63
#define DESCR_DATA_BROADCAST                 0x64
#define DESCR_CA_SYSTEM                      0x65
#define DESCR_DATA_BROADCAST_ID              0x66
#define DESCR_TRANSPORT_STREAM               0x67
#define DESCR_DSNG                           0x68
#define DESCR_PDC                            0x69
#define DESCR_AC3                            0x6A
#define DESCR_ANCILLARY_DATA                 0x6B
#define DESCR_CELL_LIST                      0x6C
#define DESCR_CELL_FREQ_LINK                 0x6D
#define DESCR_ANNOUNCEMENT_SUPPORT           0x6E


#define MAX_SECTION_BUFFER 4096


/* Strukturen zur Aufnahme der SDT und EIT Informationen */

struct Service {
   struct NODE          Node;
   int                  ServiceID;
   int                  TransportStreamID;
   int                  OriginalNetworkID;
   int                  SdtVersion;
   unsigned short       Status;
   struct LIST         *Descriptors;
   struct LIST         *Events;
};

#define EIT_SCHEDULE_FLAG               0x0001
#define GetScheduleFlag(x)              ((x)&EIT_SCHEDULE_FLAG)
#define SetScheduleFlag(x)              ((x)|=EIT_SCHEDULE_FLAG)
#define EIT_PRESENT_FOLLOWING_FLAG      0x0002
#define GetPresentFollowing(x)          ((x)&EIT_PRESENT_FOLLOWING_FLAG)
#define SetPresentFollowing(x)          ((x)|=EIT_PRESENT_FOLLOWING_FLAG)
#define RUNNING_STATUS_NOT_RUNNING      0x0000
#define RUNNING_STATUS_AWAITING         0x0004
#define RUNNING_STATUS_PAUSING          0x0008
#define RUNNING_STATUS_RUNNING          0x000C
#define GetRunningStatus(x)             ((x)&RUNNING_STATUS_RUNNING)
#define SetRunningStatus(x,s)           ((x)|=((s)&RUNNING_STATUS_RUNNING))
#define FREE_TO_AIR                     0x0000
#define CONDITIONAL_ACCESS              0x0010
#define GetConditionalAccess(x)         ((x)&CONDITIONAL_ACCESS)
#define SetConditionalAccess(x)         ((x)|=CONDITIONAL_ACCESS)

#define CreateService(service, svid, tsid, onid, vers, sta) \
   do \
   { \
      xCreateNode (service, NULL); \
      service->ServiceID = svid; \
      service->TransportStreamID = tsid; \
      service->OriginalNetworkID = onid; \
      service->SdtVersion = vers; \
      service->Status = sta; \
      service->Descriptors = xNewList (NULL); \
      service->Events = xNewList (NULL); \
   } while (0)


struct Event {
   struct NODE          Node;
   int                  EventID;
   int                  ServiceID;
   int                  EitVersion;
   int                  TransportStreamID;
   int                  OriginalNetworkID;
   time_t               StartTime;
   time_t               Duration;
   unsigned short       Status;
   struct LIST         *Descriptors;
};

#define CreateEvent(event, evid, svid, tsid, onid, vers, sta) \
   do \
   { \
      xCreateNode (event, NULL); \
      event->EventID = evid; \
      event->ServiceID = svid; \
      event->TransportStreamID = tsid; \
      event->OriginalNetworkID = onid; \
      event->EitVersion = vers; \
      event->Status = sta; \
      event->Descriptors = xNewList (NULL); \
   } while (0)


/* Strukturen zur Aufnahme der PAT und PMT Informationen */

struct Program {
   struct NODE          Node;
   int                  ProgramID;
   int                  TransportStreamID;
   int                  NetworkPID;
   int                  PatVersion;
   struct LIST         *Pids;
};

#define CreateProgram(program, pgid, tsid, npid, vers) \
   do \
   { \
      xCreateNode (program, NULL); \
      program->ProgramID = pgid; \
      program->TransportStreamID = tsid; \
      program->NetworkPID = npid; \
      program->PatVersion = vers; \
      program->Pids = xNewList (NULL); \
   } while (0)

struct Pid {
   struct NODE          Node;
   int                  ProgramID;
   int                  PcrPID;
   int                  PmtVersion;
   struct LIST         *Descriptors;
   struct LIST         *InfoList;
};

#define CreatePid(pid, pgid, pcid, vers) \
   do \
   { \
      xCreateNode (pid, NULL); \
      pid->ProgramID = pgid; \
      pid->PcrPID = pcid; \
      pid->PmtVersion = vers; \
      pid->Descriptors = xNewList (NULL); \
      pid->InfoList = xNewList (NULL); \
   } while (0)

struct PidInfo {
   struct NODE          Node;
   int                  StreamType;
   dvb_pid_t            ElementaryPid;
   struct LIST         *Descriptors;
};

#define CreatePidInfo(pidinfo, styp, epid) \
   do \
   { \
      xCreateNode (pidinfo, NULL); \
      pidinfo->StreamType = styp; \
      pidinfo->ElementaryPid = (dvb_pid_t) epid; \
      pidinfo->Descriptors = xNewList (NULL); \
   } while (0)


#define STREAMTYPE_11172_VIDEO                   1
#define STREAMTYPE_13818_VIDEO                   2
#define STREAMTYPE_11172_AUDIO                   3
#define STREAMTYPE_13818_AUDIO                   4
#define STREAMTYPE_13818_PRIVATE                 5
#define STREAMTYPE_13818_PES_PRIVATE             6
#define STREAMTYPE_13522_MHPEG                   7
#define STREAMTYPE_13818_DSMCC                   8
#define STREAMTYPE_ITU_222_1                     9
#define STREAMTYPE_13818_A                      10
#define STREAMTYPE_13818_B                      11
#define STREAMTYPE_13818_C                      12
#define STREAMTYPE_13818_D                      13
#define STREAMTYPE_13818_AUX                    14

/* Descriptors */

#define DescriptorTag(x) ((struct Descriptor *)(x))->Tag

struct Descriptor {
   struct NODE          Node;
   unsigned short       Tag;
};


/* Iso639LanguageDescriptor */

struct Iso639LanguageDescriptor {
   struct NODE          Node;
   unsigned short       Tag;
   char                 LanguageCode[4];
};

#define CreateIso639LanguageDescriptor(descr, lc1, lc2, lc3) \
   do \
   { \
      xCreateNode (((struct Iso639LanguageDescriptor *)descr), NULL); \
      ((struct Iso639LanguageDescriptor *)descr)->Tag = DESCR_ISO_639_LANGUAGE; \
      ((struct Iso639LanguageDescriptor *)descr)->LanguageCode[0] = lc1; \
      ((struct Iso639LanguageDescriptor *)descr)->LanguageCode[1] = lc2; \
      ((struct Iso639LanguageDescriptor *)descr)->LanguageCode[2] = lc3; \
      ((struct Iso639LanguageDescriptor *)descr)->LanguageCode[3] = '\0'; \
   } while (0)


/* Ac3Descriptor */

#define AC3_TYPE_FLAG          0x0001
#define BS_ID_FLAG             0x0002
#define MAIN_ID_FLAG           0x0004
#define ASVC_FLAG              0x0008

struct Ac3Descriptor {
   struct NODE          Node;
   unsigned short       Tag;
   unsigned short       PresentFlags;
   unsigned short       Ac3Type;
   unsigned short       BsId;
   unsigned short       MainId;
   unsigned short       Asvc;
   unsigned short       Amount;        /* AdditionalData */
   unsigned char       *AdditionalData;
};

#define CreateAc3Descriptor(descr) \
   do \
   { \
      xCreateNode (((struct Ac3Descriptor *)descr), NULL); \
      ((struct Ac3Descriptor *)descr)->Tag = DESCR_AC3; \
   } while (0)

#define AddAc3FlagAndValue(descr, flg, val) \
   do \
   { \
      if ((flg) & AC3_TYPE_FLAG) { \
         ((struct Ac3Descriptor *)descr)->PresentFlags |= AC3_TYPE_FLAG; \
         ((struct Ac3Descriptor *)descr)->Ac3Type = (val); } \
      else if ((flg) & BS_ID_FLAG) { \
         ((struct Ac3Descriptor *)descr)->PresentFlags |= BS_ID_FLAG; \
         ((struct Ac3Descriptor *)descr)->BsId = (val); } \
      else if ((flg) & MAIN_ID_FLAG) { \
         ((struct Ac3Descriptor *)descr)->PresentFlags |= MAIN_ID_FLAG; \
         ((struct Ac3Descriptor *)descr)->MainId = (val); } \
      else if ((flg) & ASVC_FLAG) { \
         ((struct Ac3Descriptor *)descr)->PresentFlags |= ASVC_FLAG; \
         ((struct Ac3Descriptor *)descr)->Asvc = (val); } \
   } while (0)

#define AddAc3AdditionalData(descr, ptr, len) \
   do \
   { \
      xMemAlloc ((len)+1, &(((struct Ac3Descriptor *) \
         descr)->AdditionalData)); \
      memcpy ((((struct Ac3Descriptor *)descr)->AdditionalData),(ptr),(len)); \
   } while (0)


/* AncillaryDataDescriptor */

struct AncillaryDataDescriptor {
   struct NODE          Node;
   unsigned short       Tag;
   unsigned short       Identifier;
};

#define ANCILLARY_DATA_DVD_VIDEO            0x0001
#define ANCILLARY_DATA_EXTENDED             0x0002
#define ANCILLARY_DATA_SWITCHING            0x0004
#define ANCILLARY_DATA_DAB                  0x0008
#define ANCILLARY_DATA_SCALE_FACTOR         0x0010

#define CreateAncillaryDataDescriptor(descr, id) \
   do \
   { \
      xCreateNode (((struct AncillaryDataDescriptor *)descr), NULL); \
      ((struct AncillaryDataDescriptor *)descr)->Tag = DESCR_ANCILLARY_DATA; \
      ((struct AncillaryDataDescriptor *)descr)->Identifier = id; \
   } while (0)


/* BouquetNameDescriptor */

struct BouquetNameDescriptor {
   struct NODE          Node;    /* Node enthält Namen */
   unsigned short       Tag;
};

#define CreateBouquetNameDescriptor(descr, text) \
   do \
   { \
      xCreateNode (((struct BouquetNameDescriptor *)descr), NULL); \
      ((struct NODE *)descr)->Name = text; \
      ((struct NODE *)descr)->HashKey = xHashKey (text); \
      ((struct BouquetNameDescriptor *)descr)->Tag = DESCR_BOUQUET_NAME; \
   } while (0)


/* CountryAvailabilityDescriptor */

struct CountryAvailabilityDescriptor {
   struct NODE          Node;
   unsigned short       Tag;
   unsigned short       AvailibilityFlag;
   unsigned short       Amount;           /* CountryCodes */
   char                *CountryCodes;
};

#define COUNTRIES_ARE_AVAILABLE   0x0001
#define COUNTRIES_ARE_UNAVAILABLE 0x0000

#define CreateCountryAvailabilityDescriptor(descr, ava) \
   do \
   { \
      xCreateNode (((struct CountryAvailabilityDescriptor *)descr), NULL); \
      ((struct CountryAvailabilityDescriptor *)descr)->Tag = DESCR_COUNTRY_AVAIL; \
      ((struct CountryAvailabilityDescriptor *)descr)->AvailibilityFlag = ava; \
      ((struct CountryAvailabilityDescriptor *)descr)->Amount = 0; \
      ((struct CountryAvailabilityDescriptor *)descr)->CountryCodes = NULL; \
   } while (0)

#define AddCountryAvailabilityCode(descr, lc1, lc2, lc3) \
   do \
   { \
      char tmpbuf[4], *tmpptr, *ttptr; \
      \
      tmpbuf[0] = lc1; tmpbuf[1] = lc2; \
      tmpbuf[2] = lc3; tmpbuf[3] = '\0'; \
      xMemAlloc (((struct CountryAvailabilityDescriptor *)descr)->Amount*4 + 8, &tmpptr); \
      ttptr = tmpptr; \
      if (((struct CountryAvailabilityDescriptor *)descr)->CountryCodes) { \
         memcpy (ttptr, ((struct CountryAvailabilityDescriptor *)descr)->CountryCodes, \
                        ((struct CountryAvailabilityDescriptor *)descr)->Amount*4); \
         ttptr += ((struct CountryAvailabilityDescriptor *)descr)->Amount*4; \
      } \
      memcpy (ttptr, tmpbuf, 4); \
      ((struct CountryAvailabilityDescriptor *)descr)->CountryCodes = tmpptr; \
   } while (0)


/* CaIdentifierDescriptor */

struct CaIdentifierDescriptor {
   struct NODE          Node;
   unsigned short       Tag;
   unsigned short       Amount;           /* SystemIDs */
   unsigned short      *SystemID;
};

#define CreateCaIdentifierDescriptor(descr, amo) \
   do \
   { \
      xCreateNode (((struct CaIdentifierDescriptor *)descr), NULL); \
      ((struct CaIdentifierDescriptor *)descr)->Tag = DESCR_CA_IDENT; \
      ((struct CaIdentifierDescriptor *)descr)->Amount = amo; \
      xMemAlloc (amo*2+2, &((struct CaIdentifierDescriptor *)descr)->SystemID); \
   } while (0)

#define SetCaIdentifierID(descr, num, id) \
      ((struct CaIdentifierDescriptor *)descr)->SystemID[num] = id
#define GetCaIdentifierID(descr, num) (((struct CaIdentifierDescriptor *)descr)->SystemID[num])


/* StreamIdentifierDescriptor */

struct StreamIdentifierDescriptor {
   struct NODE          Node;
   unsigned short       Tag;
   unsigned short       ComponentTag;
};

#define CreateStreamIdentifierDescriptor(descr, ctag) \
   do \
   { \
      xCreateNode (((struct StreamIdentifierDescriptor *)descr), NULL); \
      ((struct StreamIdentifierDescriptor *)descr)->Tag = DESCR_STREAM_ID; \
      ((struct StreamIdentifierDescriptor *)descr)->ComponentTag = (ctag); \
   } while (0)


/* DataBroadcastDescriptor */

struct DataBroadcastDescriptor {
   struct NODE          Node;          /* Node enthält DescriptorText */
   unsigned short       Tag;
   unsigned short       DataBroadcastID;
   unsigned short       ComponentTag;
   unsigned short       SelectorLength;
   unsigned char       *SelectorBytes;
   char                 LanguageCode[4];
};

struct MosaicDescriptor {
   struct NODE          Node;
   unsigned short       Tag;
   /* to be defined */
};

struct MultiLingualServiceDescriptor {
   struct NODE          Node;
   unsigned short       Tag;
   /* to be defined */
};


/* NvodReferenceDescriptor */

struct NvodReferenceDescriptor {
   struct NODE          Node;
   unsigned short       Tag;
   struct LIST         *Items;
};

#define CreateNvodReferenceDescriptor(descr) \
   do \
   { \
      xCreateNode (((struct NvodReferenceDescriptor *)descr), NULL); \
      ((struct NvodReferenceDescriptor *)descr)->Tag = DESCR_NVOD_REF; \
      ((struct NvodReferenceDescriptor *)descr)->Items = xNewList (NULL); \
   } while (0)

struct NvodReferenceItem {
   struct NODE          Node;
   int                  TransportStreamID;
   int                  OriginalNetworkID;
   int                  ServiceID;
};

#define CreateNvodReferenceItem(itm, tpid, onid, svid) \
   do \
   { \
      xCreateNode (itm, NULL); \
      itm->TransportStreamID = tpid; \
      itm->OriginalNetworkID = onid; \
      itm->ServiceID = svid; \
   } while (0)

#define AddNvodReferenceItem(desc, tpid, onid, svid) \
   do \
   { \
      struct NvodReferenceItem *item; \
      \
      CreateNvodReferenceItem(item, tpid, onid, svid); \
      xAddTail (((struct NvodReferenceDescriptor *)desc)->Items, item); \
   } while (0)


/* LinkageDescriptor */

struct LinkageDescriptor {
   struct NODE          Node;
   unsigned short       Tag;
   int                  TransportStreamID;
   int                  OriginalNetworkID;
   int                  ServiceID;
   int                  LinkageType;
   int                  PrivateDataLength;
   unsigned char       *PrivateData;
};

#define CreateLinkageDescriptor(descr, tpid, onid, svid, ltyp, pdl, pdp) \
   do \
   { \
      xCreateNode (((struct LinkageDescriptor *)descr), NULL); \
      ((struct LinkageDescriptor *)descr)->Tag = DESCR_LINKAGE; \
      ((struct LinkageDescriptor *)descr)->TransportStreamID = tpid; \
      ((struct LinkageDescriptor *)descr)->OriginalNetworkID = onid; \
      ((struct LinkageDescriptor *)descr)->ServiceID = svid; \
      ((struct LinkageDescriptor *)descr)->LinkageType = ltyp; \
      ((struct LinkageDescriptor *)descr)->PrivateDataLength = pdl; \
      xMemAlloc ((pdl)+1, &(((struct LinkageDescriptor *) \
         descr)->PrivateData)); \
      memcpy ((((struct LinkageDescriptor *)descr)->PrivateData),(pdp),(pdl));\
   } while (0)


/* ServiceDescriptor */

struct ServiceDescriptor {
   struct NODE          Node;      /* Node enthält ServiceName */
   unsigned short       Tag;
   unsigned short       ServiceType;
   char                *ServiceProvider;
};

#define CreateServiceDescriptor(descr, styp, prov, name) \
   do \
   { \
      xCreateNode (((struct ServiceDescriptor *)descr), NULL); \
      ((struct NODE *)descr)->Name = name; \
      ((struct NODE *)descr)->HashKey = xHashKey (name); \
      ((struct ServiceDescriptor *)descr)->Tag = DESCR_SERVICE; \
      ((struct ServiceDescriptor *)descr)->ServiceType = styp; \
      ((struct ServiceDescriptor *)descr)->ServiceProvider = prov; \
   } while (0)



struct TelephoneDescriptor {
   struct NODE          Node;
   unsigned short       Tag;
   /* to be defined */
};


/* TimeShiftedServiceDescriptor */

struct TimeShiftedServiceDescriptor {
   struct NODE          Node;
   unsigned short       Tag;
   int                  ReferenceServiceID;
};

#define CreateTimeShiftedServiceDescriptor(descr, svid) \
   do \
   { \
      xCreateNode (((struct TimeShiftedServiceDescriptor *)descr), NULL); \
      ((struct TimeShiftedServiceDescriptor *)descr)->Tag = DESCR_TIME_SHIFTED_SERVICE; \
      ((struct TimeShiftedServiceDescriptor *)descr)->ReferenceServiceID = svid; \
   } while (0)


/* TimeShiftedEventDescriptor */

struct TimeShiftedEventDescriptor {
   struct NODE          Node;
   unsigned short       Tag;
   int                  ReferenceServiceID;
   int                  ReferenceEventID;
};

#define CreateTimeShiftedEventDescriptor(descr, svid, evid) \
   do \
   { \
      xCreateNode (((struct TimeShiftedEventDescriptor *)descr), NULL); \
      ((struct TimeShiftedEventDescriptor *)descr)->Tag = DESCR_TIME_SHIFTED_EVENT; \
      ((struct TimeShiftedEventDescriptor *)descr)->ReferenceServiceID = svid; \
      ((struct TimeShiftedEventDescriptor *)descr)->ReferenceEventID = evid; \
   } while (0)


/* ComponentDescriptor */

struct ComponentDescriptor {
   struct NODE          Node;   /* Node enthält ComponentText */
   unsigned short       Tag;
   unsigned short       StreamContent;
   unsigned short       ComponentType;
   unsigned short       ComponentTag;
   char                 LanguageCode[4];
};

#define CreateComponentDescriptor(descr, scnt, ctyp, tag, lc1, lc2, lc3, txt) \
   do \
   { \
      xCreateNode (((struct ComponentDescriptor *)descr), NULL); \
      ((struct NODE *)descr)->Name = txt; \
      ((struct NODE *)descr)->HashKey = xHashKey (txt); \
      ((struct ComponentDescriptor *)descr)->Tag = DESCR_COMPONENT; \
      ((struct ComponentDescriptor *)descr)->StreamContent = scnt; \
      ((struct ComponentDescriptor *)descr)->ComponentType = ctyp; \
      ((struct ComponentDescriptor *)descr)->ComponentTag = tag; \
      ((struct ComponentDescriptor *)descr)->LanguageCode[0] = lc1; \
      ((struct ComponentDescriptor *)descr)->LanguageCode[1] = lc2; \
      ((struct ComponentDescriptor *)descr)->LanguageCode[2] = lc3; \
      ((struct ComponentDescriptor *)descr)->LanguageCode[3] = '\0'; \
   } while (0)


/* ContentDescriptor */

struct ContentDescriptor {
   struct NODE          Node;
   unsigned short       Tag;
   unsigned short       Amount;        /* ContentIDs */
   unsigned short      *ContentID;
};

#define CreateContentDescriptor(descr, amo) \
   do \
   { \
      xCreateNode (((struct ContentDescriptor *)descr), NULL); \
      ((struct ContentDescriptor *)descr)->Tag = DESCR_CONTENT; \
      ((struct ContentDescriptor *)descr)->Amount = amo; \
      xMemAlloc (amo*2+2, &((struct ContentDescriptor *)descr)->ContentID); \
   } while (0)

#define SetContentID(descr, num, cnib1, cnib2, unib1, unib2) \
   do \
   { \
      ((struct ContentDescriptor *)descr)->ContentID[num] = \
         ((cnib1&0xF) << 12) | ((cnib2&0xF) << 8) | \
         ((unib1&0xF) << 4) | (unib2&0xF); \
   } while (0)
#define GetContentContentNibble1(descr, num) ((((struct ContentDescriptor *)descr)->ContentID[num]&0xF000) >> 12)
#define GetContentContentNibble2(descr, num) ((((struct ContentDescriptor *)descr)->ContentID[num]&0x0F00) >> 8)
#define GetContentUserNibble1(descr, num) ((((struct ContentDescriptor *)descr)->ContentID[num]&0x00F0) >> 4)
#define GetContentUserNibble2(descr, num) (((struct ContentDescriptor *)descr)->ContentID[num]&0x000F)


/* ExtendedEventDescriptor */

struct ExtendedEventDescriptor {
   struct NODE          Node;    /* Node enthält EventText */
   unsigned short       Tag;
   unsigned short       DescriptorNumber;
   unsigned short       LastDescriptorNumber;
   char                 LanguageCode[4];
   struct LIST         *Items;
};

#define CreateExtendedEventDescriptor(descr, dnum, ldnb, lc1, lc2, lc3, text) \
   do \
   { \
      xCreateNode (((struct ExtendedEventDescriptor *)descr), NULL); \
      ((struct NODE *)descr)->Name = text; \
      ((struct NODE *)descr)->HashKey = xHashKey (text); \
      ((struct ExtendedEventDescriptor *)descr)->Tag = DESCR_EXTENDED_EVENT; \
      ((struct ExtendedEventDescriptor *)descr)->DescriptorNumber = dnum; \
      ((struct ExtendedEventDescriptor *)descr)->LastDescriptorNumber = ldnb; \
      ((struct ExtendedEventDescriptor *)descr)->LanguageCode[0] = lc1; \
      ((struct ExtendedEventDescriptor *)descr)->LanguageCode[1] = lc2; \
      ((struct ExtendedEventDescriptor *)descr)->LanguageCode[2] = lc3; \
      ((struct ExtendedEventDescriptor *)descr)->LanguageCode[3] = '\0'; \
      ((struct ExtendedEventDescriptor *)descr)->Items = xNewList (NULL); \
   } while (0)

struct ExtendedEventItem {
   struct NODE          Node;    /* Node enthält ItemDescription Text */
   char                *Text;
};

#define CreateExtendedEventItem(itm, dtxt, text) \
   do \
   { \
      xCreateNode (itm, NULL); \
      ((struct NODE *)itm)->Name = dtxt; \
      ((struct NODE *)itm)->HashKey = xHashKey (dtxt); \
      itm->Text = text; \
   } while (0)

#define AddExtendedEventItem(desc, dtxt, text) \
   do \
   { \
      struct ExtendedEventItem *item; \
      \
      CreateExtendedEventItem(item, dtxt, text); \
      xAddTail (((struct ExtendedEventDescriptor *)desc)->Items, item); \
   } while (0)


/* ParentalRatingDescriptor */

struct ParentalRatingDescriptor {
   struct NODE          Node;
   unsigned short       Tag;
   struct LIST         *Ratings;
};

#define CreateParentalRatingDescriptor(descr) \
   do \
   { \
      xCreateNode (((struct ParentalRatingDescriptor *)descr), NULL); \
      ((struct ParentalRatingDescriptor *)descr)->Tag = DESCR_PARENTAL_RATING; \
      ((struct ParentalRatingDescriptor *)descr)->Ratings = xNewList (NULL); \
   } while (0)

struct ParentalRating {
   struct NODE          Node;    /* Node enthält ItemDescription Text */
   char                 LanguageCode[4];
   char                 Rating;
};

#define CreateParentalRating(rat, lc1, lc2, lc3, val) \
   do \
   { \
      xCreateNode (rat, NULL); \
      rat->LanguageCode[0] = lc1; \
      rat->LanguageCode[1] = lc2; \
      rat->LanguageCode[2] = lc3; \
      rat->LanguageCode[3] = '\0'; \
      rat->Rating = val; \
   } while (0)

#define AddParentalRating(desc, lc1, lc2, lc3, val) \
   do \
   { \
      struct ParentalRating *item; \
      \
      CreateParentalRating(item, lc1, lc2, lc3, val); \
      xAddTail (((struct ParentalRatingDescriptor *)desc)->Ratings, item); \
   } while (0)

/* ShortEventDescriptor */

struct ShortEventDescriptor {
   struct NODE          Node;    /* Node enthält EventName */
   unsigned short       Tag;
   char                 LanguageCode[4];
   char                *Text;
};

#define CreateShortEventDescriptor(descr, name, lc1, lc2, lc3, text) \
   do \
   { \
      xCreateNode (((struct ShortEventDescriptor *)descr), NULL); \
      ((struct NODE *)descr)->Name = name; \
      ((struct NODE *)descr)->HashKey = xHashKey (name); \
      ((struct ShortEventDescriptor *)descr)->Tag = DESCR_SHORT_EVENT; \
      ((struct ShortEventDescriptor *)descr)->LanguageCode[0] = lc1; \
      ((struct ShortEventDescriptor *)descr)->LanguageCode[1] = lc2; \
      ((struct ShortEventDescriptor *)descr)->LanguageCode[2] = lc3; \
      ((struct ShortEventDescriptor *)descr)->LanguageCode[3] = '\0'; \
      ((struct ShortEventDescriptor *)descr)->Text = text; \
   } while (0)


/* TeletextDescriptor */

struct TeletextDescriptor {
   struct NODE          Node;
   unsigned short       Tag;
   struct LIST         *Items;
};

#define CreateTeletextDescriptor(descr) \
   do \
   { \
      xCreateNode (((struct TeletextDescriptor *)descr), NULL); \
      ((struct TeletextDescriptor *)descr)->Tag = DESCR_TELETEXT; \
      ((struct TeletextDescriptor *)descr)->Items = xNewList (NULL); \
   } while (0)

#define TELETEXT_TYPE_INITIAL_PAGE         0x0001
#define TELETEXT_TYPE_SUBTITLE_PAGE        0x0002
#define TELETEXT_TYPE_ADDITIONAL_INFO      0x0003
#define TELETEXT_TYPE_PROGRAM_SCHEDULE     0x0004
#define TELETEXT_TYPE_HEARING_IMPAIRED     0x0005

struct TeletextItem {
   struct NODE          Node;
   char                 LanguageCode[4];
   unsigned short       Type;
   unsigned short       MagazineNumber;
   unsigned short       PageNumber;
};

#define CreateTeletextItem(itm, tp, mg, pg, lc1, lc2, lc3) \
   do \
   { \
      xCreateNode (itm, NULL); \
      ((struct TeletextItem *)itm)->Type = (tp); \
      ((struct TeletextItem *)itm)->MagazineNumber = (mg); \
      ((struct TeletextItem *)itm)->PageNumber = (mg); \
      ((struct TeletextItem *)itm)->LanguageCode[0] = (lc1); \
      ((struct TeletextItem *)itm)->LanguageCode[1] = (lc2); \
      ((struct TeletextItem *)itm)->LanguageCode[2] = (lc3); \
      ((struct TeletextItem *)itm)->LanguageCode[3] = '\0'; \
   } while (0)

#define AddTeletextItem(desc, tp, mg, pg, lc1, lc2, lc3) \
   do \
   { \
      struct TeletextItem *item; \
      \
      CreateTeletextItem(item, tp, mg, pg, lc1, lc2, lc3); \
      xAddTail (((struct TeletextDescriptor *)desc)->Items, item); \
   } while (0)


/* SubtitlingDescriptor */

struct SubtitlingDescriptor {
   struct NODE          Node;
   unsigned short       Tag;
   struct LIST         *Items;
};

#define CreateSubtitlingDescriptor(descr) \
   do \
   { \
      xCreateNode (((struct SubtitlingDescriptor *)descr), NULL); \
      ((struct SubtitlingDescriptor *)descr)->Tag = DESCR_SUBTITLING; \
      ((struct SubtitlingDescriptor *)descr)->Items = xNewList (NULL); \
   } while (0)

struct SubtitlingItem {
   struct NODE          Node;
   char                 LanguageCode[4];
   unsigned char        Type;
   unsigned short       CompositionPageId;
   unsigned short       AncillaryPageId;
};

#define CreateSubtitlingItem(itm, tp, cp, ap, lc1, lc2, lc3) \
   do \
   { \
      xCreateNode (itm, NULL); \
      ((struct SubtitlingItem *)itm)->Type = (tp); \
      ((struct SubtitlingItem *)itm)->CompositionPageId = (cp); \
      ((struct SubtitlingItem *)itm)->AncillaryPageId = (ap); \
      ((struct SubtitlingItem *)itm)->LanguageCode[0] = (lc1); \
      ((struct SubtitlingItem *)itm)->LanguageCode[1] = (lc2); \
      ((struct SubtitlingItem *)itm)->LanguageCode[2] = (lc3); \
      ((struct SubtitlingItem *)itm)->LanguageCode[3] = '\0'; \
   } while (0)

#define AddSubtitlingItem(desc, tp, cp, ap, lc1, lc2, lc3) \
   do \
   { \
      struct SubtitlingItem *item; \
      \
      CreateSubtitlingItem(item, tp, cp, ap, lc1, lc2, lc3); \
      xAddTail (((struct SubtitlingDescriptor *)desc)->Items, item); \
   } while (0)



/* Prototypes */

#ifdef __cplusplus
extern "C" {
#endif

/* si_parser.c */

struct LIST *siParsePAT (u_char *);
struct Pid *siParsePMT (u_char *);
struct LIST *siParseSDT (u_char *);
struct LIST *siParseEIT (u_char *);
time_t siParseTDT (u_char *);
void siParseDescriptors (struct LIST *, u_char *, int, u_char);
void siParseDescriptor (struct LIST *, u_char *);
char *siGetDescriptorText (u_char *, int);
u_long crc32 (char *data, int len);

/* si_debug_services.c */

void siDebugServices (struct LIST *);
void siDebugService (struct Service *);
void siDebugEvents (char *, struct LIST *);
void siDebugPrograms (char *, struct LIST *);
void siDebugProgram (struct Program *);
void siDebugPids (char *, struct LIST *);
void siDebugDescriptors (char *, struct LIST *);
void siDebugEitServices (struct LIST *);
void siDebugEitEvents (char *, struct LIST *);

#ifdef __cplusplus
}
#endif

#endif
