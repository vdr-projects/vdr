/*
 * filter.c: Section filter
 *
 * See the main source file 'vdr.c' for copyright information and
 * how to reach the author.
 *
 * $Id: filter.c 5.1 2021/03/16 15:10:54 kls Exp $
 */

#include "filter.h"
#include "sections.h"

// --- cSectionSyncer --------------------------------------------------------

cSectionSyncer::cSectionSyncer(bool Random)
{
  random = Random;
  Reset();
}

void cSectionSyncer::Reset(void)
{
  currentVersion = -1;
  currentSection = -1;
  synced = false;
  complete = false;
  segments = 0;
  memset(sections, 0x00, sizeof(sections));
}

bool cSectionSyncer::Check(uchar Version, int SectionNumber)
{
  if (Version != currentVersion) {
     Reset();
     currentVersion = Version;
     }
  if (complete)
     return false;
  if (!random) {
     if (!synced) {
        if (SectionNumber == 0) {
           currentSection = 0;
           synced = true;
           }
        else
           return false;
        }
     if (SectionNumber != currentSection)
        return false;
     }
  return !GetSectionFlag(SectionNumber);
}

bool cSectionSyncer::Processed(int SectionNumber, int LastSectionNumber, int SegmentLastSectionNumber)
{
  SetSectionFlag(SectionNumber, true); // the flag for this section
  if (!random)
     currentSection++;           // expect the next section
  int Index = SectionNumber / 8; // the segment (byte) in which this section lies
  uchar b = 0xFF;                // all sections in this segment
  if (SegmentLastSectionNumber < 0 && Index == LastSectionNumber / 8)
     SegmentLastSectionNumber = LastSectionNumber;
  if (SegmentLastSectionNumber >= 0) {
     b >>= 7 - (SegmentLastSectionNumber & 0x07); // limits them up to the last section in this segment
     if (!random && SectionNumber == SegmentLastSectionNumber)
        currentSection = (SectionNumber + 8) & ~0x07; // expect first section of next segment
     }
  if (sections[Index] == b)           // all expected sections in this segment have been received
     segments |= 1 << Index;          // so we set the respective bit in the segments flags
  uint32_t s = 0xFFFFFFFF;            // all segments
  s >>= 31 - (LastSectionNumber / 8); // limits them up to the last expected segment
  complete = segments == s;
  return complete;
}

#if DEPRECATED_SECTIONSYNCER_SYNC_REPEAT
void cSectionSyncer::Repeat(void)
{
  SetSectionFlag(currentSection, false);
  synced = false;
  complete = false;
}

bool cSectionSyncer::Sync(uchar Version, int Number, int LastNumber)
{
  if (Version != currentVersion) {
     Reset();
     currentVersion = Version;
     }
  if (!synced) {
     if (Number != 0)
        return false;
     else
        synced = true;
     }
  currentSection = Number;
  bool Result = !GetSectionFlag(Number);
  SetSectionFlag(Number, true);
  if (Number == LastNumber)
     complete = true;
  return Result;
}
#endif

// --- cFilterData -----------------------------------------------------------

cFilterData::cFilterData(void)
{
  pid = 0;
  tid = 0;
  mask = 0;
  sticky = false;
}

cFilterData::cFilterData(u_short Pid, u_char Tid, u_char Mask, bool Sticky)
{
  pid = Pid;
  tid = Tid;
  mask = Mask;
  sticky = Sticky;
}

cFilterData& cFilterData::operator= (const cFilterData &FilterData)
{
  pid = FilterData.pid;
  tid = FilterData.tid;
  mask = FilterData.mask;
  sticky = FilterData.sticky;
  return *this;
}

bool cFilterData::Is(u_short Pid, u_char Tid, u_char Mask)
{
  return pid == Pid && tid == Tid && mask == Mask;
}

bool cFilterData::Matches(u_short Pid, u_char Tid)
{
  return pid == Pid && tid == (Tid & mask);
}

// --- cFilter ---------------------------------------------------------------

cFilter::cFilter(void)
{
  sectionHandler = NULL;
  on = false;
}

cFilter::cFilter(u_short Pid, u_char Tid, u_char Mask)
{
  sectionHandler = NULL;
  on = false;
  Set(Pid, Tid, Mask);
}

cFilter::~cFilter()
{
  if (sectionHandler)
     sectionHandler->Detach(this);
}

int cFilter::Source(void)
{
  return sectionHandler ? sectionHandler->Source() : 0;
}

int cFilter::Transponder(void)
{
  return sectionHandler ? sectionHandler->Transponder() : 0;
}

const cChannel *cFilter::Channel(void)
{
  return sectionHandler ? sectionHandler->Channel() : NULL;
}

void cFilter::SetStatus(bool On)
{
  if (sectionHandler && on != On) {
     cFilterData *fd = data.First();
     while (fd) {
           if (On)
              sectionHandler->Add(fd);
           else {
              sectionHandler->Del(fd);
              if (!fd->sticky) {
                 cFilterData *next = data.Next(fd);
                 data.Del(fd);
                 fd = next;
                 continue;
                 }
              }
           fd = data.Next(fd);
           }
     on = On;
     }
}

bool cFilter::Matches(u_short Pid, u_char Tid)
{
  if (on) {
     for (cFilterData *fd = data.First(); fd; fd = data.Next(fd)) {
         if (fd->Matches(Pid, Tid))
            return true;
         }
     }
  return false;
}

void cFilter::Set(u_short Pid, u_char Tid, u_char Mask)
{
  Add(Pid, Tid, Mask, true);
}

void cFilter::Add(u_short Pid, u_char Tid, u_char Mask, bool Sticky)
{
  cFilterData *fd = new cFilterData(Pid, Tid, Mask, Sticky);
  data.Add(fd);
  if (sectionHandler && on)
     sectionHandler->Add(fd);
}

void cFilter::Del(u_short Pid, u_char Tid, u_char Mask)
{
  for (cFilterData *fd = data.First(); fd; fd = data.Next(fd)) {
      if (fd->Is(Pid, Tid, Mask)) {
         if (sectionHandler && on)
            sectionHandler->Del(fd);
         data.Del(fd);
         return;
         }
      }
}
