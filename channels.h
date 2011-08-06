/*
 * channels.h: Channel handling
 *
 * See the main source file 'vdr.c' for copyright information and
 * how to reach the author.
 *
 * $Id: channels.h 2.14 2011/08/06 09:56:13 kls Exp $
 */

#ifndef __CHANNELS_H
#define __CHANNELS_H

#include "config.h"
#include "sources.h"
#include "thread.h"
#include "tools.h"

#define ISTRANSPONDER(f1, f2)  (abs((f1) - (f2)) < 4) //XXX

#define CHANNELMOD_NONE     0x00
#define CHANNELMOD_ALL      0xFF
#define CHANNELMOD_NAME     0x01
#define CHANNELMOD_PIDS     0x02
#define CHANNELMOD_ID       0x04
#define CHANNELMOD_CA       0x10
#define CHANNELMOD_TRANSP   0x20
#define CHANNELMOD_LANGS    0x40
#define CHANNELMOD_RETUNE   (CHANNELMOD_PIDS | CHANNELMOD_CA | CHANNELMOD_TRANSP)

#define CHANNELSMOD_NONE    0
#define CHANNELSMOD_AUTO    1
#define CHANNELSMOD_USER    2

#define MAXAPIDS 32 // audio
#define MAXDPIDS 16 // dolby (AC3 + DTS)
#define MAXSPIDS 32 // subtitles
#define MAXCAIDS 12 // conditional access

#define MAXLANGCODE1 4 // a 3 letter language code, zero terminated
#define MAXLANGCODE2 8 // up to two 3 letter language codes, separated by '+' and zero terminated

#define CA_FTA           0x0000
#define CA_DVB_MIN       0x0001
#define CA_DVB_MAX       0x000F
#define CA_USER_MIN      0x0010
#define CA_USER_MAX      0x00FF
#define CA_ENCRYPTED_MIN 0x0100
#define CA_ENCRYPTED_MAX 0xFFFF

struct tChannelID {
private:
  int source;
  int nid; ///< actually the "original" network id
  int tid;
  int sid;
  int rid;
public:
  tChannelID(void) { source = nid = tid = sid = rid = 0; }
  tChannelID(int Source, int Nid, int Tid, int Sid, int Rid = 0) { source = Source; nid = Nid; tid = Tid; sid = Sid; rid = Rid; }
  bool operator== (const tChannelID &arg) const { return source == arg.source && nid == arg.nid && tid == arg.tid && sid == arg.sid && rid == arg.rid; }
  bool Valid(void) const { return (nid || tid) && sid; } // rid is optional and source may be 0//XXX source may not be 0???
  tChannelID &ClrRid(void) { rid = 0; return *this; }
  tChannelID &ClrPolarization(void);
  int Source(void)  const { return source; }
  int Nid(void)  const { return nid; }
  int Tid(void)  const { return tid; }
  int Sid(void)  const { return sid; }
  int Rid(void)  const { return rid; }
  static tChannelID FromString(const char *s);
  cString ToString(void) const;
  static const tChannelID InvalidID;
  };

class cChannel;

class cLinkChannel : public cListObject {
private:
  cChannel *channel;
public:
  cLinkChannel(cChannel *Channel) { channel = Channel; }
  cChannel *Channel(void) { return channel; }
  };

class cLinkChannels : public cList<cLinkChannel> {
  };

class cSchedule;

class cChannel : public cListObject {
  friend class cSchedules;
  friend class cMenuEditChannel;
  friend class cDvbSourceParam;
private:
  static cString ToText(const cChannel *Channel);
  char *name;
  char *shortName;
  char *provider;
  char *portalName;
  int __BeginData__;
  int frequency; // MHz
  int source;
  int srate;
  int vpid;
  int ppid;
  int vtype;
  int apids[MAXAPIDS + 1]; // list is zero-terminated
  int atypes[MAXAPIDS + 1]; // list is zero-terminated
  char alangs[MAXAPIDS][MAXLANGCODE2];
  int dpids[MAXDPIDS + 1]; // list is zero-terminated
  int dtypes[MAXAPIDS + 1]; // list is zero-terminated
  char dlangs[MAXDPIDS][MAXLANGCODE2];
  int spids[MAXSPIDS + 1]; // list is zero-terminated
  char slangs[MAXSPIDS][MAXLANGCODE2];
  uchar subtitlingTypes[MAXSPIDS];
  uint16_t compositionPageIds[MAXSPIDS];
  uint16_t ancillaryPageIds[MAXSPIDS];
  int tpid;
  int caids[MAXCAIDS + 1]; // list is zero-terminated
  int nid;
  int tid;
  int sid;
  int rid;
  int number;    // Sequence number assigned on load
  bool groupSep;
  int __EndData__;
  cString parameters;
  int modification;
  mutable const cSchedule *schedule;
  cLinkChannels *linkChannels;
  cChannel *refChannel;
  cString TransponderDataToString(void) const;
public:
  cChannel(void);
  cChannel(const cChannel &Channel);
  ~cChannel();
  cChannel& operator= (const cChannel &Channel);
  cString ToText(void) const;
  bool Parse(const char *s);
  bool Save(FILE *f);
  const char *Name(void) const { return name; }
  const char *ShortName(bool OrName = false) const { return (OrName && isempty(shortName)) ? name : shortName; }
  const char *Provider(void) const { return provider; }
  const char *PortalName(void) const { return portalName; }
  int Frequency(void) const { return frequency; } ///< Returns the actual frequency, as given in 'channels.conf'
  int Transponder(void) const;                    ///< Returns the transponder frequency in MHz, plus the polarization in case of sat
  static int Transponder(int Frequency, char Polarization); ///< builds the transponder from the given Frequency and Polarization
  int Source(void) const { return source; }
  int Srate(void) const { return srate; }
  int Vpid(void) const { return vpid; }
  int Ppid(void) const { return ppid; }
  int Vtype(void) const { return vtype; }
  const int *Apids(void) const { return apids; }
  const int *Dpids(void) const { return dpids; }
  const int *Spids(void) const { return spids; }
  int Apid(int i) const { return (0 <= i && i < MAXAPIDS) ? apids[i] : 0; }
  int Dpid(int i) const { return (0 <= i && i < MAXDPIDS) ? dpids[i] : 0; }
  int Spid(int i) const { return (0 <= i && i < MAXSPIDS) ? spids[i] : 0; }
  const char *Alang(int i) const { return (0 <= i && i < MAXAPIDS) ? alangs[i] : ""; }
  const char *Dlang(int i) const { return (0 <= i && i < MAXDPIDS) ? dlangs[i] : ""; }
  const char *Slang(int i) const { return (0 <= i && i < MAXSPIDS) ? slangs[i] : ""; }
  int Atype(int i) const { return (0 <= i && i < MAXAPIDS) ? atypes[i] : 0; }
  int Dtype(int i) const { return (0 <= i && i < MAXDPIDS) ? dtypes[i] : 0; }
  uchar SubtitlingType(int i) const { return (0 <= i && i < MAXSPIDS) ? subtitlingTypes[i] : uchar(0); }
  uint16_t CompositionPageId(int i) const { return (0 <= i && i < MAXSPIDS) ? compositionPageIds[i] : uint16_t(0); }
  uint16_t AncillaryPageId(int i) const { return (0 <= i && i < MAXSPIDS) ? ancillaryPageIds[i] : uint16_t(0); }
  int Tpid(void) const { return tpid; }
  const int *Caids(void) const { return caids; }
  int Ca(int Index = 0) const { return Index < MAXCAIDS ? caids[Index] : 0; }
  int Nid(void) const { return nid; }
  int Tid(void) const { return tid; }
  int Sid(void) const { return sid; }
  int Rid(void) const { return rid; }
  int Number(void) const { return number; }
  void SetNumber(int Number) { number = Number; }
  bool GroupSep(void) const { return groupSep; }
  const char *Parameters(void) const { return parameters; }
  const cLinkChannels* LinkChannels(void) const { return linkChannels; }
  const cChannel *RefChannel(void) const { return refChannel; }
  bool IsAtsc(void) const { return cSource::IsAtsc(source); }
  bool IsCable(void) const { return cSource::IsCable(source); }
  bool IsSat(void) const { return cSource::IsSat(source); }
  bool IsTerr(void) const { return cSource::IsTerr(source); }
  bool IsSourceType(char Source) const { return cSource::IsType(source, Source); }
  tChannelID GetChannelID(void) const { return tChannelID(source, nid, (nid || tid) ? tid : Transponder(), sid, rid); }
  bool HasTimer(void) const;
  int Modification(int Mask = CHANNELMOD_ALL);
  void CopyTransponderData(const cChannel *Channel);
  bool SetTransponderData(int Source, int Frequency, int Srate, const char *Parameters, bool Quiet = false);
  void SetId(int Nid, int Tid, int Sid, int Rid = 0);
  void SetName(const char *Name, const char *ShortName, const char *Provider);
  void SetPortalName(const char *PortalName);
  void SetPids(int Vpid, int Ppid, int Vtype, int *Apids, int *Atypes, char ALangs[][MAXLANGCODE2], int *Dpids, int *Dtypes, char DLangs[][MAXLANGCODE2], int *Spids, char SLangs[][MAXLANGCODE2], int Tpid);
  void SetCaIds(const int *CaIds); // list must be zero-terminated
  void SetCaDescriptors(int Level);
  void SetLinkChannels(cLinkChannels *LinkChannels);
  void SetRefChannel(cChannel *RefChannel);
  void SetSubtitlingDescriptors(uchar *SubtitlingTypes, uint16_t *CompositionPageIds, uint16_t *AncillaryPageIds);
  };

class cChannels : public cRwLock, public cConfig<cChannel> {
private:
  int maxNumber;
  int modified;
  int beingEdited;
  cHash<cChannel> channelsHashSid;
  void DeleteDuplicateChannels(void);
public:
  cChannels(void);
  bool Load(const char *FileName, bool AllowComments = false, bool MustExist = false);
  void HashChannel(cChannel *Channel);
  void UnhashChannel(cChannel *Channel);
  int GetNextGroup(int Idx);   // Get next channel group
  int GetPrevGroup(int Idx);   // Get previous channel group
  int GetNextNormal(int Idx);  // Get next normal channel (not group)
  int GetPrevNormal(int Idx);  // Get previous normal channel (not group)
  void ReNumber(void);         // Recalculate 'number' based on channel type
  cChannel *GetByNumber(int Number, int SkipGap = 0);
  cChannel *GetByServiceID(int Source, int Transponder, unsigned short ServiceID);
  cChannel *GetByChannelID(tChannelID ChannelID, bool TryWithoutRid = false, bool TryWithoutPolarization = false);
  cChannel *GetByTransponderID(tChannelID ChannelID);
  int BeingEdited(void) { return beingEdited; }
  void IncBeingEdited(void) { beingEdited++; }
  void DecBeingEdited(void) { beingEdited--; }
  bool HasUniqueChannelID(cChannel *NewChannel, cChannel *OldChannel = NULL);
  bool SwitchTo(int Number);
  int MaxNumber(void) { return maxNumber; }
  void SetModified(bool ByUser = false);
  int Modified(void);
      ///< Returns 0 if no channels have been modified, 1 if an automatic
      ///< modification has been made, and 2 if the user has made a modification.
      ///< Calling this function resets the 'modified' flag to 0.
  cChannel *NewChannel(const cChannel *Transponder, const char *Name, const char *ShortName, const char *Provider, int Nid, int Tid, int Sid, int Rid = 0);
  };

extern cChannels Channels;

cString ChannelString(const cChannel *Channel, int Number);

#endif //__CHANNELS_H
