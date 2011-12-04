/***************************************************************************
 *       Copyright (c) 2003 by Marcel Wiesweg                              *
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 *   $Id: si.c 2.5 2011/12/04 15:06:18 kls Exp $
 *                                                                         *
 ***************************************************************************/

#include "si.h"
#include <errno.h>
#include <iconv.h>
#include <malloc.h>
#include <stdlib.h> // for broadcaster stupidity workaround
#include <string.h>
#include "descriptor.h"

namespace SI {

Object::Object() {
}

Object::Object(CharArray &d) : data(d) {
}

void Object::setData(const unsigned char*d, int size, bool doCopy) {
   data.assign(d, size, doCopy);
}

void Object::setData(CharArray &d) {
   data=d;
}

bool Object::checkSize(int offset) {
   return data.checkSize(offset);
}

Section::Section(const unsigned char *data, bool doCopy) {
   setData(data, getLength(data), doCopy);
}

TableId Section::getTableId() const {
   return getTableId(data.getData());
}

int Section::getLength() {
   return getLength(data.getData());
}

TableId Section::getTableId(const unsigned char *d) {
   return (TableId)((const SectionHeader *)d)->table_id;
}

int Section::getLength(const unsigned char *d) {
   return HILO(((const SectionHeader *)d)->section_length)+sizeof(SectionHeader);
}

bool CRCSection::isCRCValid() {
   return CRC32::isValid((const char *)data.getData(), getLength()/*, data.FourBytes(getLength()-4)*/);
}

bool CRCSection::CheckCRCAndParse() {
   if (!isCRCValid())
      return false;
   CheckParse();
   return isValid();
}

int NumberedSection::getTableIdExtension() const {
   return getTableIdExtension(data.getData());
}

int NumberedSection::getTableIdExtension(const unsigned char *d) {
   return HILO(((const ExtendedSectionHeader *)d)->table_id_extension);
}

bool NumberedSection::getCurrentNextIndicator() const {
   return data.getData<ExtendedSectionHeader>()->current_next_indicator;
}

int NumberedSection::getVersionNumber() const {
   return data.getData<ExtendedSectionHeader>()->version_number;
}

int NumberedSection::getSectionNumber() const {
   return data.getData<ExtendedSectionHeader>()->section_number;
}

int NumberedSection::getLastSectionNumber() const {
   return data.getData<ExtendedSectionHeader>()->last_section_number;
}

int Descriptor::getLength() {
   return getLength(data.getData());
}

DescriptorTag Descriptor::getDescriptorTag() const {
   return getDescriptorTag(data.getData());
}

int Descriptor::getLength(const unsigned char *d) {
   return ((const DescriptorHeader*)d)->descriptor_length+sizeof(DescriptorHeader);
}

DescriptorTag Descriptor::getDescriptorTag(const unsigned char *d) {
   return (DescriptorTag)((const DescriptorHeader*)d)->descriptor_tag;
}

Descriptor *DescriptorLoop::getNext(Iterator &it) {
   if (isValid() && it.i<getLength()) {
      return createDescriptor(it.i, true);
   }
   return 0;
}

Descriptor *DescriptorLoop::getNext(Iterator &it, DescriptorTag tag, bool returnUnimplemetedDescriptor) {
   Descriptor *d=0;
   int len;
   if (isValid() && it.i<(len=getLength())) {
      const unsigned char *p=data.getData(it.i);
      const unsigned char *end=p+len-it.i;
      while (p < end) {
         if (Descriptor::getDescriptorTag(p) == tag) {
            d=createDescriptor(it.i, returnUnimplemetedDescriptor);
            if (d)
               break;
         }
         it.i+=Descriptor::getLength(p);
         p+=Descriptor::getLength(p);
      }
   }
   return d;
}

Descriptor *DescriptorLoop::getNext(Iterator &it, DescriptorTag *tags, int arrayLength, bool returnUnimplementedDescriptor) {
   Descriptor *d=0;
   int len;
   if (isValid() && it.i<(len=getLength())) {
      const unsigned char *p=data.getData(it.i);
      const unsigned char *end=p+len-it.i;
      while (p < end) {
         for (int u=0; u<arrayLength;u++)
            if (Descriptor::getDescriptorTag(p) == tags[u]) {
               d=createDescriptor(it.i, returnUnimplementedDescriptor);
               break;
            }
         if (d)
            break; //length is added to it.i by createDescriptor, break here
         it.i+=Descriptor::getLength(p);
         p+=Descriptor::getLength(p);
      }
   }
   return d;
}

Descriptor *DescriptorLoop::createDescriptor(int &i, bool returnUnimplemetedDescriptor) {
   if (!checkSize(Descriptor::getLength(data.getData(i))))
      return 0;
   Descriptor *d=Descriptor::getDescriptor(data+i, domain, returnUnimplemetedDescriptor);
   if (!d)
      return 0;
   i+=d->getLength();
   d->CheckParse();
   return d;
}

int DescriptorLoop::getNumberOfDescriptors() {
   const unsigned char *p=data.getData();
   const unsigned char *end=p+getLength();
   int count=0;
   while (p < end) {
      count++;
      p+=Descriptor::getLength(p);
   }
   return count;
}

DescriptorGroup::DescriptorGroup(bool del) {
   array=0;
   length=0;
   deleteOnDesctruction=del;
}

DescriptorGroup::~DescriptorGroup() {
   if (deleteOnDesctruction)
      Delete();
   delete[] array;
}

void DescriptorGroup::Delete() {
   for (int i=0;i<length;i++)
      if (array[i]!=0) {
         delete array[i];
         array[i]=0;
      }
}

void DescriptorGroup::Add(GroupDescriptor *d) {
   if (!array) {
      length=d->getLastDescriptorNumber()+1;
      array=new GroupDescriptor*[length]; //numbering is zero-based
      for (int i=0;i<length;i++)
         array[i]=0;
   } else if (length != d->getLastDescriptorNumber()+1)
      return; //avoid crash in case of misuse
   if (length <= d->getDescriptorNumber())
      return; // see http://www.vdr-portal.de/board60-linux/board14-betriebssystem/board69-c-t-vdr/p1025777-segfault-mit-vdr-1-7-21/#post1025777
   array[d->getDescriptorNumber()]=d;
}

bool DescriptorGroup::isComplete() {
   for (int i=0;i<length;i++)
      if (array[i]==0)
         return false;
   return true;
}

char *String::getText() {
   int len=getLength();
   if (len < 0 || len > 4095)
      return strdup("text error"); // caller will delete it!
   char *data=new char(len+1); // FIXME If this function is ever actually used, this buffer might
                               // need to be bigger in order to hold the string as UTF-8.
                               // Maybe decodeText should dynamically handle this? kls 2007-06-10
   decodeText(data, len+1);
   return data;
}

char *String::getText(char *buffer, int size) {
   int len=getLength();
   if (len < 0 || len >= size) {
      strncpy(buffer, "text error", size);
      buffer[size-1] = 0;
      return buffer;
   }
   decodeText(buffer, size);
   return buffer;
}

char *String::getText(char *buffer, char *shortVersion, int sizeBuffer, int sizeShortVersion) {
   int len=getLength();
   if (len < 0 || len >= sizeBuffer) {
      strncpy(buffer, "text error", sizeBuffer);
      buffer[sizeBuffer-1] = 0;
      *shortVersion = 0;
      return buffer;
   }
   decodeText(buffer, shortVersion, sizeBuffer, sizeShortVersion);
   return buffer;
}

static const char *CharacterTables1[] = {
  NULL,          // 0x00
  "ISO-8859-5",  // 0x01
  "ISO-8859-6",  // 0x02
  "ISO-8859-7",  // 0x03
  "ISO-8859-8",  // 0x04
  "ISO-8859-9",  // 0x05
  "ISO-8859-10", // 0x06
  "ISO-8859-11", // 0x07
  "ISO-8859-12", // 0x08
  "ISO-8859-13", // 0x09
  "ISO-8859-14", // 0x0A
  "ISO-8859-15", // 0x0B
  NULL,          // 0x0C
  NULL,          // 0x0D
  NULL,          // 0x0E
  NULL,          // 0x0F
  NULL,          // 0x10
  "UTF-16",      // 0x11
  "EUC-KR",      // 0x12
  "GB2312",      // 0x13
  "GBK",         // 0x14
  "UTF-8",       // 0x15
  NULL,          // 0x16
  NULL,          // 0x17
  NULL,          // 0x18
  NULL,          // 0x19
  NULL,          // 0x1A
  NULL,          // 0x1B
  NULL,          // 0x1C
  NULL,          // 0x1D
  NULL,          // 0x1E
  NULL,          // 0x1F
};

#define SingleByteLimit 0x0B

static const char *CharacterTables2[] = {
  NULL,          // 0x00
  "ISO-8859-1",  // 0x01
  "ISO-8859-2",  // 0x02
  "ISO-8859-3",  // 0x03
  "ISO-8859-4",  // 0x04
  "ISO-8859-5",  // 0x05
  "ISO-8859-6",  // 0x06
  "ISO-8859-7",  // 0x07
  "ISO-8859-8",  // 0x08
  "ISO-8859-9",  // 0x09
  "ISO-8859-10", // 0x0A
  "ISO-8859-11", // 0x0B
  NULL,          // 0x0C
  "ISO-8859-13", // 0x0D
  "ISO-8859-14", // 0x0E
  "ISO-8859-15", // 0x0F
};

#define NumEntries(Table) (sizeof(Table) / sizeof(char *))

static const char *SystemCharacterTable = NULL;
bool SystemCharacterTableIsSingleByte = true;

bool systemCharacterTableIsSingleByte(void)
{
  return SystemCharacterTableIsSingleByte;
}

bool SetSystemCharacterTable(const char *CharacterTable) {
   if (CharacterTable) {
      for (unsigned int i = 0; i < NumEntries(CharacterTables1); i++) {
         if (CharacterTables1[i] && strcasecmp(CharacterTable, CharacterTables1[i]) == 0) {
            SystemCharacterTable = CharacterTables1[i];
            SystemCharacterTableIsSingleByte = i <= SingleByteLimit;
            return true;
         }
      }
      for (unsigned int i = 0; i < NumEntries(CharacterTables2); i++) {
         if (CharacterTables2[i] && strcasecmp(CharacterTable, CharacterTables2[i]) == 0) {
            SystemCharacterTable = CharacterTables2[i];
            SystemCharacterTableIsSingleByte = true;
            return true;
         }
      }
   } else {
      SystemCharacterTable = NULL;
      SystemCharacterTableIsSingleByte = true;
      return true;
   }
   return false;
}

const char *getCharacterTable(const unsigned char *&buffer, int &length, bool *isSingleByte) {
   const char *cs = "ISO6937";
   // Workaround for broadcaster stupidity: according to
   // "ETSI EN 300 468" the default character set is ISO6937. But unfortunately some
   // broadcasters actually use ISO-8859-9, but fail to correctly announce that.
   static const char *CharsetOverride = getenv("VDR_CHARSET_OVERRIDE");
   if (CharsetOverride)
      cs = CharsetOverride;
   if (isSingleByte)
      *isSingleByte = false;
   if (length <= 0)
      return cs;
   unsigned int tag = buffer[0];
   if (tag >= 0x20)
      return cs;
   if (tag == 0x10) {
      if (length >= 3) {
         tag = (buffer[1] << 8) | buffer[2];
         if (tag < NumEntries(CharacterTables2) && CharacterTables2[tag]) {
            buffer += 3;
            length -= 3;
            if (isSingleByte)
               *isSingleByte = true;
            return CharacterTables2[tag];
         }
      }
   } else if (tag < NumEntries(CharacterTables1) && CharacterTables1[tag]) {
      buffer += 1;
      length -= 1;
      if (isSingleByte)
         *isSingleByte = tag <= SingleByteLimit;
      return CharacterTables1[tag];
   }
   return cs;
}

bool convertCharacterTable(const char *from, size_t fromLength, char *to, size_t toLength, const char *fromCode)
{
  if (SystemCharacterTable) {
     iconv_t cd = iconv_open(SystemCharacterTable, fromCode);
     if (cd != (iconv_t)-1) {
        char *fromPtr = (char *)from;
        while (fromLength > 0 && toLength > 1) {
           if (iconv(cd, &fromPtr, &fromLength, &to, &toLength) == size_t(-1)) {
              if (errno == EILSEQ) {
                 // A character can't be converted, so mark it with '?' and proceed:
                 fromPtr++;
                 fromLength--;
                 *to++ = '?';
                 toLength--;
              }
              else
                 break;
           }
        }
        *to = 0;
        iconv_close(cd);
        return true;
     }
  }
  return false;
}

// originally from libdtv, Copyright Rolf Hakenes <hakenes@hippomi.de>
void String::decodeText(char *buffer, int size) {
   const unsigned char *from=data.getData(0);
   char *to=buffer;
   int len=getLength();
   if (len <= 0) {
      *to = '\0';
      return;
      }
   bool singleByte;
   const char *cs = getCharacterTable(from, len, &singleByte);
   // FIXME Need to make this UTF-8 aware (different control codes).
   // However, there's yet to be found a broadcaster that actually
   // uses UTF-8 for the SI data... (kls 2007-06-10)
   for (int i = 0; i < len; i++) {
      if (*from == 0)
         break;
      if (    ((' ' <= *from) && (*from <= '~'))
           || (*from == '\n')
           || (0xA0 <= *from)
         )
         *to++ = *from;
      else if (*from == 0x8A)
         *to++ = '\n';
      from++;
      if (to - buffer >= size - 1)
         break;
   }
   *to = '\0';
   if (!singleByte || !SystemCharacterTableIsSingleByte) {
      char convBuffer[size];
      if (convertCharacterTable(buffer, strlen(buffer), convBuffer, sizeof(convBuffer), cs))
         strncpy(buffer, convBuffer, strlen(convBuffer) + 1);
   }
}

void String::decodeText(char *buffer, char *shortVersion, int sizeBuffer, int sizeShortVersion) {
   const unsigned char *from=data.getData(0);
   char *to=buffer;
   char *toShort=shortVersion;
   int IsShortName=0;
   int len=getLength();
   if (len <= 0) {
      *to = '\0';
      *toShort = '\0';
      return;
      }
   bool singleByte;
   const char *cs = getCharacterTable(from, len, &singleByte);
   // FIXME Need to make this UTF-8 aware (different control codes).
   // However, there's yet to be found a broadcaster that actually
   // uses UTF-8 for the SI data... (kls 2007-06-10)
   for (int i = 0; i < len; i++) {
      if (    ((' ' <= *from) && (*from <= '~'))
           || (*from == '\n')
           || (0xA0 <= *from)
         )
      {
         *to++ = *from;
         if (IsShortName)
            *toShort++ = *from;
      }
      else if (*from == 0x8A)
         *to++ = '\n';
      else if (*from == 0x86)
         IsShortName++;
      else if (*from == 0x87)
         IsShortName--;
      else if (*from == 0)
         break;
      from++;
      if (to - buffer >= sizeBuffer - 1 || toShort - shortVersion >= sizeShortVersion - 1)
         break;
   }
   *to = '\0';
   *toShort = '\0';
   if (!singleByte || !SystemCharacterTableIsSingleByte) {
      char convBuffer[sizeBuffer];
      if (convertCharacterTable(buffer, strlen(buffer), convBuffer, sizeof(convBuffer), cs))
         strncpy(buffer, convBuffer, strlen(convBuffer) + 1);
      char convShortVersion[sizeShortVersion];
      if (convertCharacterTable(shortVersion, strlen(shortVersion), convShortVersion, sizeof(convShortVersion), cs))
         strncpy(shortVersion, convShortVersion, strlen(convShortVersion) + 1);
   }
}

Descriptor *Descriptor::getDescriptor(CharArray da, DescriptorTagDomain domain, bool returnUnimplemetedDescriptor) {
   Descriptor *d=0;
   switch (domain) {
   case SI:
      switch ((DescriptorTag)da.getData<DescriptorHeader>()->descriptor_tag) {
         case CaDescriptorTag:
            d=new CaDescriptor();
            break;
         case CarouselIdentifierDescriptorTag:
            d=new CarouselIdentifierDescriptor();
            break;
         case NetworkNameDescriptorTag:
            d=new NetworkNameDescriptor();
            break;
         case ServiceListDescriptorTag:
            d=new ServiceListDescriptor();
            break;
         case SatelliteDeliverySystemDescriptorTag:
            d=new SatelliteDeliverySystemDescriptor();
            break;
         case CableDeliverySystemDescriptorTag:
            d=new CableDeliverySystemDescriptor();
            break;
         case TerrestrialDeliverySystemDescriptorTag:
            d=new TerrestrialDeliverySystemDescriptor();
            break;
         case BouquetNameDescriptorTag:
            d=new BouquetNameDescriptor();
            break;
         case ServiceDescriptorTag:
            d=new ServiceDescriptor();
            break;
         case NVODReferenceDescriptorTag:
            d=new NVODReferenceDescriptor();
            break;
         case TimeShiftedServiceDescriptorTag:
            d=new TimeShiftedServiceDescriptor();
            break;
         case ComponentDescriptorTag:
            d=new ComponentDescriptor();
            break;
         case StreamIdentifierDescriptorTag:
            d=new StreamIdentifierDescriptor();
            break;
         case SubtitlingDescriptorTag:
            d=new SubtitlingDescriptor();
            break;
         case MultilingualNetworkNameDescriptorTag:
            d=new MultilingualNetworkNameDescriptor();
            break;
         case MultilingualBouquetNameDescriptorTag:
            d=new MultilingualBouquetNameDescriptor();
            break;
         case MultilingualServiceNameDescriptorTag:
            d=new MultilingualServiceNameDescriptor();
            break;
         case MultilingualComponentDescriptorTag:
            d=new MultilingualComponentDescriptor();
            break;
         case PrivateDataSpecifierDescriptorTag:
            d=new PrivateDataSpecifierDescriptor();
            break;
         case ServiceMoveDescriptorTag:
            d=new ServiceMoveDescriptor();
            break;
         case FrequencyListDescriptorTag:
            d=new FrequencyListDescriptor();
            break;
         case ServiceIdentifierDescriptorTag:
            d=new ServiceIdentifierDescriptor();
            break;
         case CaIdentifierDescriptorTag:
            d=new CaIdentifierDescriptor();
            break;
         case ShortEventDescriptorTag:
            d=new ShortEventDescriptor();
            break;
         case ExtendedEventDescriptorTag:
            d=new ExtendedEventDescriptor();
            break;
         case TimeShiftedEventDescriptorTag:
            d=new TimeShiftedEventDescriptor();
            break;
         case ContentDescriptorTag:
            d=new ContentDescriptor();
            break;
         case ParentalRatingDescriptorTag:
            d=new ParentalRatingDescriptor();
            break;
         case TeletextDescriptorTag:
         case VBITeletextDescriptorTag:
            d=new TeletextDescriptor();
            break;
         case ApplicationSignallingDescriptorTag:
            d=new ApplicationSignallingDescriptor();
            break;
         case LocalTimeOffsetDescriptorTag:
            d=new LocalTimeOffsetDescriptor();
            break;
         case LinkageDescriptorTag:
            d=new LinkageDescriptor();
            break;
         case ISO639LanguageDescriptorTag:
            d=new ISO639LanguageDescriptor();
            break;
         case PDCDescriptorTag:
            d=new PDCDescriptor();
            break;
         case AncillaryDataDescriptorTag:
            d=new AncillaryDataDescriptor();
            break;
         case S2SatelliteDeliverySystemDescriptorTag:
            d=new S2SatelliteDeliverySystemDescriptor();
            break;
         case ExtensionDescriptorTag:
            d=new ExtensionDescriptor();
            break;
         case RegistrationDescriptorTag:
            d=new RegistrationDescriptor();
            break;
         case ContentIdentifierDescriptorTag:
            d=new ContentIdentifierDescriptor();
            break;
         case DefaultAuthorityDescriptorTag:
            d=new DefaultAuthorityDescriptor();
            break;

         //note that it is no problem to implement one
         //of the unimplemented descriptors.

         //defined in ISO-13818-1
         case VideoStreamDescriptorTag:
         case AudioStreamDescriptorTag:
         case HierarchyDescriptorTag:
         case DataStreamAlignmentDescriptorTag:
         case TargetBackgroundGridDescriptorTag:
         case VideoWindowDescriptorTag:
         case SystemClockDescriptorTag:
         case MultiplexBufferUtilizationDescriptorTag:
         case CopyrightDescriptorTag:
         case MaximumBitrateDescriptorTag:
         case PrivateDataIndicatorDescriptorTag:
         case SmoothingBufferDescriptorTag:
         case STDDescriptorTag:
         case IBPDescriptorTag:

         //defined in ETSI EN 300 468
         case StuffingDescriptorTag:
         case VBIDataDescriptorTag:
         case CountryAvailabilityDescriptorTag:
         case MocaicDescriptorTag:
         case TelephoneDescriptorTag:
         case CellListDescriptorTag:
         case CellFrequencyLinkDescriptorTag:
         case ServiceAvailabilityDescriptorTag:
         case ShortSmoothingBufferDescriptorTag:
         case PartialTransportStreamDescriptorTag:
         case DataBroadcastDescriptorTag:
         case DataBroadcastIdDescriptorTag:
         case ScramblingDescriptorTag:
         case AC3DescriptorTag:
         case DSNGDescriptorTag:
         case AnnouncementSupportDescriptorTag:
         case AdaptationFieldDataDescriptorTag:
         case TransportStreamDescriptorTag:

         //defined in ETSI EN 300 468 v 1.7.1
         case RelatedContentDescriptorTag:
         case TVAIdDescriptorTag:
         case TimeSliceFecIdentifierDescriptorTag:
         case ECMRepetitionRateDescriptorTag:
         case EnhancedAC3DescriptorTag:
         case DTSDescriptorTag:
         case AACDescriptorTag:
         default:
            if (!returnUnimplemetedDescriptor)
               return 0;
            d=new UnimplementedDescriptor();
            break;
      }
      break;
   case MHP:
      switch ((DescriptorTag)da.getData<DescriptorHeader>()->descriptor_tag) {
      // They once again start with 0x00 (see page 234, MHP specification)
         case MHP_ApplicationDescriptorTag:
            d=new MHP_ApplicationDescriptor();
            break;
         case MHP_ApplicationNameDescriptorTag:
            d=new MHP_ApplicationNameDescriptor();
            break;
         case MHP_TransportProtocolDescriptorTag:
            d=new MHP_TransportProtocolDescriptor();
            break;
         case MHP_DVBJApplicationDescriptorTag:
            d=new MHP_DVBJApplicationDescriptor();
            break;
         case MHP_DVBJApplicationLocationDescriptorTag:
            d=new MHP_DVBJApplicationLocationDescriptor();
            break;
      // 0x05 - 0x0A is unimplemented this library
         case MHP_ExternalApplicationAuthorisationDescriptorTag:
         case MHP_IPv4RoutingDescriptorTag:
         case MHP_IPv6RoutingDescriptorTag:
         case MHP_DVBHTMLApplicationDescriptorTag:
         case MHP_DVBHTMLApplicationLocationDescriptorTag:
         case MHP_DVBHTMLApplicationBoundaryDescriptorTag:
         case MHP_ApplicationIconsDescriptorTag:
         case MHP_PrefetchDescriptorTag:
         case MHP_DelegatedApplicationDescriptorTag:
         case MHP_ApplicationStorageDescriptorTag:
         default:
            if (!returnUnimplemetedDescriptor)
               return 0;
            d=new UnimplementedDescriptor();
            break;
      }
      break;
   case PCIT:
      switch ((DescriptorTag)da.getData<DescriptorHeader>()->descriptor_tag) {
         case ContentDescriptorTag:
            d=new ContentDescriptor();
            break;
         case ShortEventDescriptorTag:
            d=new ShortEventDescriptor();
            break;
         case ExtendedEventDescriptorTag:
            d=new ExtendedEventDescriptor();
            break;
         case PremiereContentTransmissionDescriptorTag:
            d=new PremiereContentTransmissionDescriptor();
            break;
         default:
            if (!returnUnimplemetedDescriptor)
               return 0;
            d=new UnimplementedDescriptor();
            break;
      }
      break;
   default: ; // unknown domain, nothing to do
   }
   d->setData(da);
   return d;
}

} //end of namespace
