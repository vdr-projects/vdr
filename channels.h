/*
 * channels.h: Channel handling
 *
 * See the main source file 'vdr.c' for copyright information and
 * how to reach the author.
 *
 * $Id: channels.h 5.1 2021/01/14 10:29:05 kls Exp $
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
#define CHANNELMOD_AUX      0x08
#define CHANNELMOD_CA       0x10
#define CHANNELMOD_TRANSP   0x20
#define CHANNELMOD_LANGS    0x40
#define CHANNELMOD_RETUNE   (CHANNELMOD_PIDS | CHANNELMOD_CA | CHANNELMOD_TRANSP)

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
class cChannels;

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
  int dtypes[MAXDPIDS + 1]; // list is zero-terminated
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
  int lcn;       // Logical channel number assigned by data stream (or -1 if not available)
  int number;    // Sequence number assigned on load
  bool groupSep;
  int __EndData__;
  mutable cString nameSource;
  mutable int nameSourceMode;
  mutable cString shortNameSource;
  cString parameters;
  mutable int modification;
  time_t seen; // When this channel was last seen in the SDT of its transponder
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
  const char *Name(void) const;
  const char *ShortName(bool OrName = false) const;
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
  int Lcn(void) const { return lcn; }
  int Number(void) const { return number; }
  void SetNumber(int Number) { number = Number; }
  bool GroupSep(void) const { return groupSep; }
  const char *Parameters(void) const { return parameters; }
  const cSchedule *Schedule(void) const { return schedule; }
  const cLinkChannels* LinkChannels(void) const { return linkChannels; }
  const cChannel *RefChannel(void) const { return refChannel; }
  bool IsAtsc(void) const { return cSource::IsAtsc(source); }
  bool IsCable(void) const { return cSource::IsCable(source); }
  bool IsSat(void) const { return cSource::IsSat(source); }
  bool IsTerr(void) const { return cSource::IsTerr(source); }
  bool IsSourceType(char Source) const { return cSource::IsType(source, Source); }
  tChannelID GetChannelID(void) const { return tChannelID(source, nid, (nid || tid) ? tid : Transponder(), sid, rid); }
  int Modification(int Mask = CHANNELMOD_ALL) const;
  time_t Seen(void) const { return seen; }
  void CopyTransponderData(const cChannel *Channel);
  bool SetTransponderData(int Source, int Frequency, int Srate, const char *Parameters, bool Quiet = false);
  bool SetSource(int Source);
  bool SetId(cChannels *Channels, int Nid, int Tid, int Sid, int Rid = 0);
  bool SetLcn(int Lcn);
  bool SetName(const char *Name, const char *ShortName, const char *Provider);
  bool SetPortalName(const char *PortalName);
  bool SetPids(int Vpid, int Ppid, int Vtype, int *Apids, int *Atypes, char ALangs[][MAXLANGCODE2], int *Dpids, int *Dtypes, char DLangs[][MAXLANGCODE2], int *Spids, char SLangs[][MAXLANGCODE2], int Tpid);
  bool SetCaIds(const int *CaIds); // list must be zero-terminated
  bool SetCaDescriptors(int Level);
  bool SetLinkChannels(cLinkChannels *LinkChannels);
  void SetRefChannel(cChannel *RefChannel);
  bool SetSubtitlingDescriptors(uchar *SubtitlingTypes, uint16_t *CompositionPageIds, uint16_t *AncillaryPageIds);
  void SetSeen(void);
  void DelLinkChannel(cChannel *LinkChannel);
  };

class cChannels : public cConfig<cChannel> {
private:
  static cChannels channels;
  static int maxNumber;
  static int maxChannelNameLength;
  static int maxShortChannelNameLength;
  int modifiedByUser;
  cHash<cChannel> channelsHashSid;
  void DeleteDuplicateChannels(void);
public:
  cChannels(void);
  static const cChannels *GetChannelsRead(cStateKey &StateKey, int TimeoutMs = 0);
      ///< Gets the list of channels for read access.
      ///< See cTimers::GetTimersRead() for details.
  static cChannels *GetChannelsWrite(cStateKey &StateKey, int TimeoutMs = 0);
      ///< Gets the list of channels for write access.
      ///< See cTimers::GetTimersWrite() for details.
  static bool Load(const char *FileName, bool AllowComments = false, bool MustExist = false);
  void HashChannel(cChannel *Channel);
  void UnhashChannel(cChannel *Channel);
  int GetNextGroup(int Idx) const;   ///< Get next channel group
  int GetPrevGroup(int Idx) const;   ///< Get previous channel group
  int GetNextNormal(int Idx) const;  ///< Get next normal channel (not group)
  int GetPrevNormal(int Idx) const;  ///< Get previous normal channel (not group)
  void ReNumber(void);               ///< Recalculate 'number' based on channel type
  bool MoveNeedsDecrement(cChannel *From, cChannel *To); // Detect special case when moving a channel (closely related to Renumber())
  void Del(cChannel *Channel);       ///< Delete the given Channel from the list
  const cChannel *GetByNumber(int Number, int SkipGap = 0) const;
  cChannel *GetByNumber(int Number, int SkipGap = 0) { return const_cast<cChannel *>(static_cast<const cChannels *>(this)->GetByNumber(Number, SkipGap)); }
  const cChannel *GetByServiceID(int Source, int Transponder, unsigned short ServiceID) const;
  cChannel *GetByServiceID(int Source, int Transponder, unsigned short ServiceID) { return const_cast<cChannel *>(static_cast<const cChannels *>(this)->GetByServiceID(Source, Transponder, ServiceID)); }
  const cChannel *GetByChannelID(tChannelID ChannelID, bool TryWithoutRid = false, bool TryWithoutPolarization = false) const;
  cChannel *GetByChannelID(tChannelID ChannelID, bool TryWithoutRid = false, bool TryWithoutPolarization = false) { return const_cast<cChannel *>(static_cast<const cChannels *>(this)->GetByChannelID(ChannelID, TryWithoutRid, TryWithoutPolarization)); }
  const cChannel *GetByTransponderID(tChannelID ChannelID) const;
  cChannel *GetByTransponderID(tChannelID ChannelID) { return const_cast<cChannel *>(static_cast<const cChannels *>(this)->GetByTransponderID(ChannelID)); }
  bool HasUniqueChannelID(const cChannel *NewChannel, const cChannel *OldChannel = NULL) const;
  bool SwitchTo(int Number) const;
  static int MaxNumber(void) { return maxNumber; }
  static int MaxChannelNameLength(void);
  static int MaxShortChannelNameLength(void);
  void SetModifiedByUser(void);
  bool ModifiedByUser(int &State) const;
      ///< Returns true if the channels have been modified by the user since the last call
      ///< to this function with the same State variable. State must be initialized with 0
      ///< and will be set to the current value of the list's internal state variable upon
      ///< return from this function.
  cChannel *NewChannel(const cChannel *Transponder, const char *Name, const char *ShortName, const char *Provider, int Nid, int Tid, int Sid, int Rid = 0);
  bool MarkObsoleteChannels(int Source, int Nid, int Tid);
  };

// Provide lock controlled access to the list:

DEF_LIST_LOCK(Channels);

// These macros provide a convenient way of locking the global channels list
// and making sure the lock is released as soon as the current scope is left
// (note that these macros wait forever to obtain the lock!):

#define LOCK_CHANNELS_READ  USE_LIST_LOCK_READ(Channels)
#define LOCK_CHANNELS_WRITE USE_LIST_LOCK_WRITE(Channels)

cString ChannelString(const cChannel *Channel, int Number);

#endif //__CHANNELS_H
