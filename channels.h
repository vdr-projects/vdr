/*
 * channels.h: Channel handling
 *
 * See the main source file 'vdr.c' for copyright information and
 * how to reach the author.
 *
 * $Id: channels.h 1.3 2002/10/20 11:50:36 kls Exp $
 */

#ifndef __CHANNELS_H
#define __CHANNELS_H

#include "config.h"
#include "sources.h"
#include "tools.h"

#define ISTRANSPONDER(f1, f2)  (abs((f1) - (f2)) < 4) //XXX

struct tChannelParameterMap {
  int userValue;
  int driverValue;
  };

//XXX into cChannel???
int MapToUser(int Value, const tChannelParameterMap *Map);
int MapToDriver(int Value, const tChannelParameterMap *Map);
int UserIndex(int Value, const tChannelParameterMap *Map);
int DriverIndex(int Value, const tChannelParameterMap *Map);

extern const tChannelParameterMap InversionValues[];
extern const tChannelParameterMap BandwidthValues[];
extern const tChannelParameterMap CoderateValues[];
extern const tChannelParameterMap ModulationValues[];
extern const tChannelParameterMap TransmissionValues[];
extern const tChannelParameterMap GuardValues[];
extern const tChannelParameterMap HierarchyValues[];

class cChannel : public cListObject {
  friend class cMenuEditChannel;
private:
  static char *buffer;
  static const char *ToText(cChannel *Channel);
  enum { MaxChannelName = 32 }; // 31 chars + terminating 0!
  char name[MaxChannelName];
  int frequency; // MHz
  int source;
  int srate;
  int vpid;
  int apid1, apid2;
  int dpid1, dpid2;
  int tpid;
  int ca;
  int sid;
  int number;    // Sequence number assigned on load
  bool groupSep;
  //XXX
  char polarization;
  int inversion;
  int bandwidth;
  int coderateH;
  int coderateL;
  int modulation;
  int transmission;
  int guard;
  int hierarchy;
  const char *ParametersToString(void);
  bool StringToParameters(const char *s);
public:
  cChannel(void);
  cChannel(const cChannel *Channel);
  const char *ToText(void);
  bool Parse(const char *s);
  bool Save(FILE *f);
  const char *Name(void) const { return name; }
  int Frequency(void) const { return frequency; }
  int Source(void) const { return source; }
  int Srate(void) const { return srate; }
  int Vpid(void) const { return vpid; }
  int Apid1(void) const { return apid1; }
  int Apid2(void) const { return apid2; }
  int Dpid1(void) const { return dpid1; }
  int Dpid2(void) const { return dpid2; }
  int Tpid(void) const { return tpid; }
  int Ca(void) const { return ca; }
  int Sid(void) const { return sid; }
  int Number(void) const { return number; }
  void SetNumber(int Number) { number = Number; }
  bool GroupSep(void) const { return groupSep; }
  //XXX
  char Polarization(void) const { return polarization; }
  int Inversion(void) const { return inversion; }
  int Bandwidth(void) const { return bandwidth; }
  int CoderateH(void) const { return coderateH; }
  int CoderateL(void) const { return coderateL; }
  int Modulation(void) const { return modulation; }
  int Transmission(void) const { return transmission; }
  int Guard(void) const { return guard; }
  int Hierarchy(void) const { return hierarchy; }
  //XXX
  bool IsCable(void) { return (source & cSource::st_Mask) == cSource::stCable; }
  bool IsSat(void) { return (source & cSource::st_Mask) == cSource::stSat; }
  bool IsTerr(void) { return (source & cSource::st_Mask) == cSource::stTerr; }
  };

class cChannels : public cConfig<cChannel> {
protected:
  int maxNumber;
public:
  cChannels(void) { maxNumber = 0; }
  virtual bool Load(const char *FileName, bool AllowComments = false);
  int GetNextGroup(int Idx);   // Get next channel group
  int GetPrevGroup(int Idx);   // Get previous channel group
  int GetNextNormal(int Idx);  // Get next normal channel (not group)
  void ReNumber(void);         // Recalculate 'number' based on channel type
  cChannel *GetByNumber(int Number, int SkipGap = 0);
  cChannel *GetByServiceID(unsigned short ServiceId);
  bool SwitchTo(int Number);
  int MaxNumber(void) { return maxNumber; }
  };

extern cChannels Channels;

#endif //__CHANNELS_H
