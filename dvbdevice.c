/*
 * dvbdevice.c: The DVB device tuner interface
 *
 * See the main source file 'vdr.c' for copyright information and
 * how to reach the author.
 *
 * $Id: dvbdevice.c 5.8 2024/09/09 08:53:57 kls Exp $
 */

#include "dvbdevice.h"
#include <ctype.h>
#include <errno.h>
#include <limits.h>
#include <linux/dvb/dmx.h>
#include <linux/dvb/frontend.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include "channels.h"
#include "diseqc.h"
#include "dvbci.h"
#include "menuitems.h"
#include "sourceparams.h"

static int DvbApiVersion = 0x0000; // the version of the DVB driver actually in use (will be determined by the first device created)

#define BANDWIDTH_HZ_AUTO 0 // missing in DVB API 5

#define DVBS_TUNE_TIMEOUT  9000 //ms
#define DVBS_LOCK_TIMEOUT  2000 //ms
#define DVBC_TUNE_TIMEOUT  9000 //ms
#define DVBC_LOCK_TIMEOUT  2000 //ms
#define DVBT_TUNE_TIMEOUT  9000 //ms
#define DVBT_LOCK_TIMEOUT  2000 //ms
#define ATSC_TUNE_TIMEOUT  9000 //ms
#define ATSC_LOCK_TIMEOUT  2000 //ms

#define SCR_RANDOM_TIMEOUT  500 // ms (add random value up to this when tuning SCR device to avoid lockups)

#define TSBUFFERSIZE MEGABYTE(16)

// --- DVB Parameter Maps ----------------------------------------------------

const tDvbParameterMap PilotValues[] = {
  {   0, PILOT_OFF,  trNOOP("off") },
  {   1, PILOT_ON,   trNOOP("on") },
  { 999, PILOT_AUTO, trNOOP("auto") },
  {  -1, 0, NULL }
  };

const tDvbParameterMap InversionValues[] = {
  {   0, INVERSION_OFF,  trNOOP("off") },
  {   1, INVERSION_ON,   trNOOP("on") },
  { 999, INVERSION_AUTO, trNOOP("auto") },
  {  -1, 0, NULL }
  };

const tDvbParameterMap BandwidthValues[] = {
  {    5,  5000000, "5 MHz" },
  {    6,  6000000, "6 MHz" },
  {    7,  7000000, "7 MHz" },
  {    8,  8000000, "8 MHz" },
  {   10, 10000000, "10 MHz" },
  { 1712,  1712000, "1.712 MHz" },
  {  999, BANDWIDTH_HZ_AUTO, trNOOP("auto") },
  {  -1, 0, NULL }
  };

const tDvbParameterMap CoderateValues[] = {
  {   0, FEC_NONE, trNOOP("none") },
  {  12, FEC_1_2,  "1/2" },
  {  23, FEC_2_3,  "2/3" },
  {  34, FEC_3_4,  "3/4" },
  {  35, FEC_3_5,  "3/5" },
  {  45, FEC_4_5,  "4/5" },
  {  56, FEC_5_6,  "5/6" },
  {  67, FEC_6_7,  "6/7" },
  {  78, FEC_7_8,  "7/8" },
  {  89, FEC_8_9,  "8/9" },
  { 910, FEC_9_10, "9/10" },
  { 999, FEC_AUTO, trNOOP("auto") },
  {  -1, 0, NULL }
  };

const tDvbParameterMap ModulationValues[] = {
  {  16, QAM_16,   "QAM16" },
  {  32, QAM_32,   "QAM32" },
  {  64, QAM_64,   "QAM64" },
  { 128, QAM_128,  "QAM128" },
  { 256, QAM_256,  "QAM256" },
  {   2, QPSK,     "QPSK" },
  {   5, PSK_8,    "8PSK" },
  {   6, APSK_16,  "16APSK" },
  {   7, APSK_32,  "32APSK" },
  {  10, VSB_8,    "VSB8" },
  {  11, VSB_16,   "VSB16" },
  {  12, DQPSK,    "DQPSK" },
  { 999, QAM_AUTO, trNOOP("auto") },
  {  -1, 0, NULL }
  };

#define DVB_SYSTEM_1 0 // see also nit.c
#define DVB_SYSTEM_2 1

const tDvbParameterMap SystemValuesSat[] = {
  {   0, DVB_SYSTEM_1, "DVB-S" },
  {   1, DVB_SYSTEM_2, "DVB-S2" },
  {  -1, 0, NULL }
  };

const tDvbParameterMap SystemValuesTerr[] = {
  {   0, DVB_SYSTEM_1, "DVB-T" },
  {   1, DVB_SYSTEM_2, "DVB-T2" },
  {  -1, 0, NULL }
  };

const tDvbParameterMap TransmissionValues[] = {
  {   1, TRANSMISSION_MODE_1K,   "1K" },
  {   2, TRANSMISSION_MODE_2K,   "2K" },
  {   4, TRANSMISSION_MODE_4K,   "4K" },
  {   8, TRANSMISSION_MODE_8K,   "8K" },
  {  16, TRANSMISSION_MODE_16K,  "16K" },
  {  32, TRANSMISSION_MODE_32K,  "32K" },
  { 999, TRANSMISSION_MODE_AUTO, trNOOP("auto") },
  {  -1, 0, NULL }
  };

const tDvbParameterMap GuardValues[] = {
  {     4, GUARD_INTERVAL_1_4,    "1/4" },
  {     8, GUARD_INTERVAL_1_8,    "1/8" },
  {    16, GUARD_INTERVAL_1_16,   "1/16" },
  {    32, GUARD_INTERVAL_1_32,   "1/32" },
  {   128, GUARD_INTERVAL_1_128,  "1/128" },
  { 19128, GUARD_INTERVAL_19_128, "19/128" },
  { 19256, GUARD_INTERVAL_19_256, "19/256" },
  {   999, GUARD_INTERVAL_AUTO,   trNOOP("auto") },
  {  -1, 0, NULL }
  };

const tDvbParameterMap HierarchyValues[] = {
  {   0, HIERARCHY_NONE, trNOOP("none") },
  {   1, HIERARCHY_1,    "1" },
  {   2, HIERARCHY_2,    "2" },
  {   4, HIERARCHY_4,    "4" },
  { 999, HIERARCHY_AUTO, trNOOP("auto") },
  {  -1, 0, NULL }
  };

const tDvbParameterMap RollOffValues[] = {
  {   0, ROLLOFF_AUTO, trNOOP("auto") },
  {  20, ROLLOFF_20, "0.20" },
  {  25, ROLLOFF_25, "0.25" },
  {  35, ROLLOFF_35, "0.35" },
  {  -1, 0, NULL }
  };

int UserIndex(int Value, const tDvbParameterMap *Map)
{
  const tDvbParameterMap *map = Map;
  while (map && map->userValue != -1) {
        if (map->userValue == Value)
           return map - Map;
        map++;
        }
  return -1;
}

int DriverIndex(int Value, const tDvbParameterMap *Map)
{
  const tDvbParameterMap *map = Map;
  while (map && map->userValue != -1) {
        if (map->driverValue == Value)
           return map - Map;
        map++;
        }
  return -1;
}

int MapToUser(int Value, const tDvbParameterMap *Map, const char **String)
{
  int n = DriverIndex(Value, Map);
  if (n >= 0) {
     if (String)
        *String = tr(Map[n].userString);
     return Map[n].userValue;
     }
  return -1;
}

const char *MapToUserString(int Value, const tDvbParameterMap *Map)
{
  int n = DriverIndex(Value, Map);
  if (n >= 0)
     return Map[n].userString;
  return "???";
}

int MapToDriver(int Value, const tDvbParameterMap *Map)
{
  int n = UserIndex(Value, Map);
  if (n >= 0)
     return Map[n].driverValue;
  return -1;
}

// --- cDvbTransponderParameters ---------------------------------------------

cDvbTransponderParameters::cDvbTransponderParameters(const char *Parameters)
{
  Parse(Parameters);
}

int cDvbTransponderParameters::PrintParameter(char *p, char Name, int Value) const
{
  return Value >= 0 && Value != 999 ? sprintf(p, "%c%d", Name, Value) : 0;
}

cString cDvbTransponderParameters::ToString(char Type) const
{
#define ST(s) if (strchr(s, Type) && (strchr(s, '0' + system + 1) || strchr(s, '*')))
  char buffer[64];
  char *q = buffer;
  *q = 0;
  ST("  S *")  q += sprintf(q, "%c", polarization);
  ST("   T*")  q += PrintParameter(q, 'B', MapToUser(bandwidth, BandwidthValues));
  ST(" CST*")  q += PrintParameter(q, 'C', MapToUser(coderateH, CoderateValues));
  ST("   T*")  q += PrintParameter(q, 'D', MapToUser(coderateL, CoderateValues));
  ST("   T*")  q += PrintParameter(q, 'G', MapToUser(guard, GuardValues));
  ST("ACST*")  q += PrintParameter(q, 'I', MapToUser(inversion, InversionValues));
  ST("ACST*")  q += PrintParameter(q, 'M', MapToUser(modulation, ModulationValues));
  ST("  S 2")  q += PrintParameter(q, 'N', MapToUser(pilot, PilotValues));
  ST("  S 2")  q += PrintParameter(q, 'O', MapToUser(rollOff, RollOffValues));
  ST("  ST2")  q += PrintParameter(q, 'P', streamId);
  ST("   T2")  q += PrintParameter(q, 'Q', t2systemId);
  ST("  ST*")  q += PrintParameter(q, 'S', MapToUser(system, SystemValuesSat)); // we only need the numerical value, so Sat or Terr doesn't matter
  ST("   T*")  q += PrintParameter(q, 'T', MapToUser(transmission, TransmissionValues));
  ST("   T2")  q += PrintParameter(q, 'X', sisoMiso);
  ST("   T*")  q += PrintParameter(q, 'Y', MapToUser(hierarchy, HierarchyValues));
  return buffer;
}

const char *cDvbTransponderParameters::ParseParameter(const char *s, int &Value, const tDvbParameterMap *Map)
{
  if (*++s) {
     char *p = NULL;
     errno = 0;
     int n = strtol(s, &p, 10);
     if (!errno && p != s) {
        Value = Map ? MapToDriver(n, Map) : n;
        if (Value >= 0)
           return p;
        }
     }
  esyslog("ERROR: invalid value for parameter '%c'", *(s - 1));
  return NULL;
}

bool cDvbTransponderParameters::Parse(const char *s)
{
  polarization = 0;
  inversion    = INVERSION_AUTO;
  bandwidth    = BANDWIDTH_HZ_AUTO;
  coderateH    = FEC_AUTO;
  coderateL    = FEC_AUTO;
  modulation   = QAM_AUTO;
  system       = DVB_SYSTEM_1;
  transmission = TRANSMISSION_MODE_AUTO;
  guard        = GUARD_INTERVAL_AUTO;
  hierarchy    = HIERARCHY_AUTO;
  rollOff      = ROLLOFF_AUTO;
  streamId     = 0;
  t2systemId   = 0;
  sisoMiso     = 0;
  pilot        = PILOT_AUTO;
  while (s && *s) {
        switch (toupper(*s)) {
          case 'B': s = ParseParameter(s, bandwidth, BandwidthValues); break;
          case 'C': s = ParseParameter(s, coderateH, CoderateValues); break;
          case 'D': s = ParseParameter(s, coderateL, CoderateValues); break;
          case 'G': s = ParseParameter(s, guard, GuardValues); break;
          case 'H': polarization = 'H'; s++; break;
          case 'I': s = ParseParameter(s, inversion, InversionValues); break;
          case 'L': polarization = 'L'; s++; break;
          case 'M': s = ParseParameter(s, modulation, ModulationValues); break;
          case 'N': s = ParseParameter(s, pilot, PilotValues); break;
          case 'O': s = ParseParameter(s, rollOff, RollOffValues); break;
          case 'P': s = ParseParameter(s, streamId); break;
          case 'Q': s = ParseParameter(s, t2systemId); break;
          case 'R': polarization = 'R'; s++; break;
          case 'S': s = ParseParameter(s, system, SystemValuesSat); break; // we only need the numerical value, so Sat or Terr doesn't matter
          case 'T': s = ParseParameter(s, transmission, TransmissionValues); break;
          case 'V': polarization = 'V'; s++; break;
          case 'X': s = ParseParameter(s, sisoMiso); break;
          case 'Y': s = ParseParameter(s, hierarchy, HierarchyValues); break;
          default: esyslog("ERROR: unknown parameter key '%c'", *s);
                   return false;
          }
        }
  return true;
}

// --- cDvbFrontend ----------------------------------------------------------

const char *DeliverySystemNames[] = {
  "???",
  "DVB-C",
  "DVB-C",
  "DVB-T",
  "DSS",
  "DVB-S",
  "DVB-S2",
  "DVB-H",
  "ISDBT",
  "ISDBS",
  "ISDBC",
  "ATSC",
  "ATSCMH",
  "DTMB",
  "CMMB",
  "DAB",
  "DVB-T2",
  "TURBO",
  "DVB-C",
  "DVB-C2",
  NULL
  };

static const int DeliverySystemNamesMax = sizeof(DeliverySystemNames) / sizeof(DeliverySystemNames[0]) - 2; // -1 to get the maximum allowed index & -1 for the NULL => -2

static const char *GetDeliverySystemName(int Index)
{
  if (Index > DeliverySystemNamesMax)
     Index = 0;
  return DeliverySystemNames[Index];
};

#define MAXFRONTENDCMDS 16
#define SETCMD(c, d) { Props[CmdSeq.num].cmd = (c);\
                       Props[CmdSeq.num].u.data = (d);\
                       if (CmdSeq.num++ > MAXFRONTENDCMDS) {\
                          esyslog("ERROR: too many tuning commands on frontend %d/%d", adapter, frontend);\
                          return false;\
                          }\
                     }

class cDvbFrontend {
private:
  int adapter, frontend;
  int fd_frontend;
  uint32_t subsystemId;
  dvb_frontend_info frontendInfo;
  cVector<int> deliverySystems;
  int numModulations;
  bool QueryDeliverySystems(void);
public:
  cDvbFrontend(int Adapter, int Frontend);
  ~cDvbFrontend();
  int Open(void);
  void Close(void);
  const char *FrontendName(void) { return frontendInfo.name; }
  bool ProvidesDeliverySystem(int DeliverySystem) const;
  bool ProvidesModulation(int System, int StreamId, int Modulation) const;
  int NumDeliverySystems(void) const { return deliverySystems.Size(); }
  int NumModulations(void) const { return numModulations; }
  uint32_t SubsystemId(void) const { return subsystemId; }
  };

cDvbFrontend::cDvbFrontend(int Adapter, int Frontend)
{
  adapter = Adapter;
  frontend = Frontend;
  fd_frontend = -1;
  subsystemId = cDvbDeviceProbe::GetSubsystemId(adapter, frontend);
  memset(&frontendInfo, 0, sizeof(frontendInfo));
  strn0cpy(frontendInfo.name, "???", sizeof(frontendInfo.name));
  numModulations = 0;
  Open();
  QueryDeliverySystems();
  Close();
}

cDvbFrontend::~cDvbFrontend()
{
  Close();
}

int cDvbFrontend::Open(void)
{
  Close();
  fd_frontend = DvbOpen(DEV_DVB_FRONTEND, adapter, frontend, O_RDWR | O_NONBLOCK, true);
  return fd_frontend;
}

void cDvbFrontend::Close(void)
{
  if (fd_frontend >= 0) {
     if (close(fd_frontend) != 0)
        esyslog("ERROR: frontend %d/%d: %m (%s:%d)", adapter, frontend, __FILE__, __LINE__);
     fd_frontend = -1;
     }
}

bool cDvbFrontend::ProvidesDeliverySystem(int DeliverySystem) const
{
  for (int i = 0; i < deliverySystems.Size(); i++) {
      if (deliverySystems[i] == DeliverySystem)
         return true;
      }
  return false;
}

bool cDvbFrontend::ProvidesModulation(int System, int StreamId, int Modulation) const
{
  if (StreamId != 0 && !(frontendInfo.caps & FE_CAN_MULTISTREAM))
     return false;
  if (Modulation == QPSK     && !(frontendInfo.caps & FE_CAN_QPSK) ||
      Modulation == QAM_16   && !(frontendInfo.caps & FE_CAN_QAM_16) ||
      Modulation == QAM_32   && !(frontendInfo.caps & FE_CAN_QAM_32) ||
      Modulation == QAM_64   && !(frontendInfo.caps & FE_CAN_QAM_64) ||
      Modulation == QAM_128  && !(frontendInfo.caps & FE_CAN_QAM_128) ||
      Modulation == QAM_256  && !(frontendInfo.caps & FE_CAN_QAM_256) ||
      Modulation == QAM_AUTO && !(frontendInfo.caps & FE_CAN_QAM_AUTO) ||
      Modulation == VSB_8    && !(frontendInfo.caps & FE_CAN_8VSB) ||
      Modulation == VSB_16   && !(frontendInfo.caps & FE_CAN_16VSB) ||
      Modulation == PSK_8    && !(frontendInfo.caps & FE_CAN_TURBO_FEC) && System == SYS_DVBS) // "turbo fec" is a non standard FEC used by North American broadcasters - this is a best guess to de
     return false;
  return true;
}

bool cDvbFrontend::QueryDeliverySystems(void)
{
  deliverySystems.Clear();
  numModulations = 0;
  if (ioctl(fd_frontend, FE_GET_INFO, &frontendInfo) < 0) {
     LOG_ERROR;
     return false;
     }
  dtv_property Props[1];
  dtv_properties CmdSeq;
  // Determine the version of the running DVB API:
  if (!DvbApiVersion) {
     memset(&Props, 0, sizeof(Props));
     memset(&CmdSeq, 0, sizeof(CmdSeq));
     CmdSeq.props = Props;
     SETCMD(DTV_API_VERSION, 0);
     if (ioctl(fd_frontend, FE_GET_PROPERTY, &CmdSeq) != 0) {
        LOG_ERROR;
        return false;
        }
     DvbApiVersion = Props[0].u.data;
     isyslog("DVB API version is 0x%04X (VDR was built with 0x%04X)", DvbApiVersion, DVBAPIVERSION);
     }
  // Determine the types of delivery systems this device provides:
  bool LegacyMode = true;
  if (DvbApiVersion >= 0x0505) {
     memset(&Props, 0, sizeof(Props));
     memset(&CmdSeq, 0, sizeof(CmdSeq));
     CmdSeq.props = Props;
     SETCMD(DTV_ENUM_DELSYS, 0);
     int Result = ioctl(fd_frontend, FE_GET_PROPERTY, &CmdSeq);
     if (Result == 0) {
        for (uint i = 0; i < Props[0].u.buffer.len; i++) {
            // activate this line to simulate a multi-frontend device if you only have a single-frontend device with DVB-S and DVB-S2:
            //if (frontend == 0 && Props[0].u.buffer.data[i] != SYS_DVBS || frontend == 1 && Props[0].u.buffer.data[i] != SYS_DVBS2)
            deliverySystems.Append(Props[0].u.buffer.data[i]);
            }
        LegacyMode = false;
        }
     else {
        esyslog("ERROR: can't query delivery systems on frontend %d/%d - falling back to legacy mode", adapter, frontend);
        }
     }
  if (LegacyMode) {
     // Legacy mode (DVB-API < 5.5):
     switch (frontendInfo.type) {
       case FE_QPSK: deliverySystems.Append(SYS_DVBS);
                     if (frontendInfo.caps & FE_CAN_2G_MODULATION)
                        deliverySystems.Append(SYS_DVBS2);
                     break;
       case FE_OFDM: deliverySystems.Append(SYS_DVBT);
                     if (frontendInfo.caps & FE_CAN_2G_MODULATION)
                        deliverySystems.Append(SYS_DVBT2);
                     break;
       case FE_QAM:  deliverySystems.Append(SYS_DVBC_ANNEX_AC); break;
       case FE_ATSC: deliverySystems.Append(SYS_ATSC); break;
       default: esyslog("ERROR: unknown frontend type %d on frontend %d/%d", frontendInfo.type, adapter, frontend);
       }
     }
  if (deliverySystems.Size() > 0) {
     cString ds("");
     for (int i = 0; i < deliverySystems.Size(); i++)
         ds = cString::sprintf("%s%s%s", *ds, i ? "," : "", GetDeliverySystemName(deliverySystems[i]));
     cString ms("");
     if (frontendInfo.caps & FE_CAN_QPSK)      { numModulations++; ms = cString::sprintf("%s%s%s", *ms, **ms ? "," : "", MapToUserString(QPSK, ModulationValues)); }
     if (frontendInfo.caps & FE_CAN_QAM_16)    { numModulations++; ms = cString::sprintf("%s%s%s", *ms, **ms ? "," : "", MapToUserString(QAM_16, ModulationValues)); }
     if (frontendInfo.caps & FE_CAN_QAM_32)    { numModulations++; ms = cString::sprintf("%s%s%s", *ms, **ms ? "," : "", MapToUserString(QAM_32, ModulationValues)); }
     if (frontendInfo.caps & FE_CAN_QAM_64)    { numModulations++; ms = cString::sprintf("%s%s%s", *ms, **ms ? "," : "", MapToUserString(QAM_64, ModulationValues)); }
     if (frontendInfo.caps & FE_CAN_QAM_128)   { numModulations++; ms = cString::sprintf("%s%s%s", *ms, **ms ? "," : "", MapToUserString(QAM_128, ModulationValues)); }
     if (frontendInfo.caps & FE_CAN_QAM_256)   { numModulations++; ms = cString::sprintf("%s%s%s", *ms, **ms ? "," : "", MapToUserString(QAM_256, ModulationValues)); }
     if (frontendInfo.caps & FE_CAN_8VSB)      { numModulations++; ms = cString::sprintf("%s%s%s", *ms, **ms ? "," : "", MapToUserString(VSB_8, ModulationValues)); }
     if (frontendInfo.caps & FE_CAN_16VSB)     { numModulations++; ms = cString::sprintf("%s%s%s", *ms, **ms ? "," : "", MapToUserString(VSB_16, ModulationValues)); }
     if (frontendInfo.caps & FE_CAN_TURBO_FEC) { numModulations++; ms = cString::sprintf("%s%s%s", *ms, **ms ? "," : "", "TURBO_FEC"); }
     if (!**ms)
        ms = "unknown modulations";
     isyslog("frontend %d/%d provides %s with %s (\"%s\")", adapter, frontend, *ds, *ms, frontendInfo.name);
     return true;
     }
  else
     esyslog("ERROR: frontend %d/%d doesn't provide any delivery systems", adapter, frontend);
  return false;
}

// --- cDvbTuner -------------------------------------------------------------

#define TUNER_POLL_TIMEOUT  10 // ms

static int GetRequiredDeliverySystem(const cChannel *Channel, const cDvbTransponderParameters *Dtp)
{
  int ds = SYS_UNDEFINED;
  if (Channel->IsAtsc())
     ds = SYS_ATSC;
  else if (Channel->IsCable())
     ds = SYS_DVBC_ANNEX_AC;
  else if (Channel->IsSat())
     ds = Dtp->System() == DVB_SYSTEM_1 ? SYS_DVBS : SYS_DVBS2;
  else if (Channel->IsTerr())
     ds = Dtp->System() == DVB_SYSTEM_1 ? SYS_DVBT : SYS_DVBT2;
  else
     esyslog("ERROR: can't determine frontend type for channel %d (%s)", Channel->Number(), Channel->Name());
  return ds;
}

class cDvbTuner : public cThread {
private:
  static cMutex bondMutex;
  enum eTunerStatus { tsIdle, tsSet, tsPositioning, tsTuned, tsLocked };
  int frontendType;
  const cDvbDevice *device;
  mutable int fd_frontend;
  int adapter;
  mutable int frontend;
  cVector<cDvbFrontend *> dvbFrontends;
  mutable cDvbFrontend *dvbFrontend;
  int numDeliverySystems;
  int numModulations;
  int tuneTimeout;
  int lockTimeout;
  time_t lastTimeoutReport;
  mutable uint32_t lastUncValue;
  mutable uint32_t lastUncDelta;
  mutable time_t lastUncChange;
  cChannel channel;
  mutable const cDiseqc *lastDiseqc;
  int diseqcOffset;
  mutable int lastSource;
  cPositioner *positioner;
  const cScr *scr;
  mutable bool lnbPowerTurnedOn;
  eTunerStatus tunerStatus;
  mutable cMutex mutex;
  cCondVar locked;
  cCondVar newSet;
  cDvbTuner *bondedTuner;
  bool bondedMaster;
  cString GetBondingParams(const cChannel *Channel = NULL) const;
  cDvbTuner *GetBondedMaster(void);
  bool IsBondedMaster(void) const { return !bondedTuner || bondedMaster; }
  void ClearEventQueue(void) const;
  bool GetFrontendStatus(fe_status_t &Status) const;
  cPositioner *GetPositioner(void);
  void ExecuteDiseqc(const cDiseqc *Diseqc, int *Frequency);
  void ResetToneAndVoltage(void);
  bool SetFrontend(void);
  virtual void Action(void);
public:
  cDvbTuner(const cDvbDevice *Device, int Adapter, int Frontend);
  virtual ~cDvbTuner();
  bool ProvidesDeliverySystem(int DeliverySystem) const;
  bool ProvidesModulation(int System, int StreamId, int Modulation) const;
  bool ProvidesFrontend(const cChannel *Channel, bool Activate = false) const;
  int Frontend(void) const { return frontend; }
  int FrontendType(void) const { return frontendType; }
  const char *FrontendName(void) { return dvbFrontend->FrontendName(); }
  int NumProvidedSystems(void) const { return numDeliverySystems + numModulations; }
  bool Bond(cDvbTuner *Tuner);
  void UnBond(void);
  bool BondingOk(const cChannel *Channel, bool ConsiderOccupied = false) const;
  const cChannel *GetTransponder(void) const { return &channel; }
  uint32_t SubsystemId(void) const { return dvbFrontend->SubsystemId(); }
  bool IsTunedTo(const cChannel *Channel) const;
  void SetChannel(const cChannel *Channel);
  bool Locked(int TimeoutMs = 0);
  const cPositioner *Positioner(void) const { return positioner; }
  bool GetSignalStats(int &Valid, double *Strength = NULL, double *Cnr = NULL, double *BerPre = NULL, double *BerPost = NULL, double *Per = NULL, int *Status = NULL) const;
  int GetSignalStrength(void) const;
  int GetSignalQuality(void) const;
  void SetPowerSaveMode(bool On);
  };

cMutex cDvbTuner::bondMutex;

cDvbTuner::cDvbTuner(const cDvbDevice *Device, int Adapter, int Frontend)
{
  frontendType = SYS_UNDEFINED;
  device = Device;
  fd_frontend = -1;
  adapter = Adapter;
  frontend = Frontend;
  dvbFrontend = NULL;
  tuneTimeout = 0;
  lockTimeout = 0;
  lastTimeoutReport = 0;
  lastUncValue = 0;
  lastUncDelta = 0;
  lastUncChange = 0;
  lastDiseqc = NULL;
  diseqcOffset = 0;
  lastSource = 0;
  positioner = NULL;
  scr = NULL;
  lnbPowerTurnedOn = false;
  tunerStatus = tsIdle;
  bondedTuner = NULL;
  bondedMaster = false;
  cDvbFrontend *fe = new cDvbFrontend(adapter, frontend);
  dvbFrontends.Append(fe);
  numDeliverySystems = fe->NumDeliverySystems();
  numModulations = fe->NumModulations();
  cString FrontendNumbers = cString::sprintf("%d", frontend);
  // Check for multiple frontends:
  if (frontend == 0) {
     for (int i = 1; ; i++) {
         if (access(DvbName(DEV_DVB_FRONTEND, adapter, i), F_OK) == 0) {
            if (access(DvbName(DEV_DVB_DEMUX, adapter, i), F_OK) != 0) {
               fe = new cDvbFrontend(adapter, i);
               dvbFrontends.Append(fe);
               numDeliverySystems += fe->NumDeliverySystems();
               //numModulations += fe->NumModulations(); // looks like in multi frontend devices all frontends report the same modulations
               FrontendNumbers = cString::sprintf("%s+%d", *FrontendNumbers, i);
               }
            }
         else
            break;
         }
     }
  // Open default frontend:
  dvbFrontend = dvbFrontends[0];
  fd_frontend = dvbFrontend->Open();
  SetDescription("frontend %d/%s tuner", adapter, *FrontendNumbers);
  Start();
}

cDvbTuner::~cDvbTuner()
{
  tunerStatus = tsIdle;
  newSet.Broadcast();
  locked.Broadcast();
  Cancel(3);
  UnBond();
  /* looks like this irritates the SCR switch, so let's leave it out for now
  if (lastDiseqc && lastDiseqc->IsScr()) {
     unsigned int Frequency = 0;
     ExecuteDiseqc(lastDiseqc, &Frequency);
     }
  */
  for (int i = 0; i < dvbFrontends.Size(); i++)
      delete dvbFrontends[i];
}

bool cDvbTuner::ProvidesDeliverySystem(int DeliverySystem) const
{
  for (int i = 0; i < dvbFrontends.Size(); i++) {
      if (dvbFrontends[i]->ProvidesDeliverySystem(DeliverySystem))
         return true;
      }
  return false;
}

bool cDvbTuner::ProvidesModulation(int System, int StreamId, int Modulation) const
{
  for (int i = 0; i < dvbFrontends.Size(); i++) {
      if (dvbFrontends[i]->ProvidesModulation(System, StreamId, Modulation))
         return true;
      }
  return false;
}

bool cDvbTuner::ProvidesFrontend(const cChannel *Channel, bool Activate) const
{
  cDvbTransponderParameters dtp(Channel->Parameters());
  int DeliverySystem = GetRequiredDeliverySystem(Channel, &dtp);
  for (int i = 0; i < dvbFrontends.Size(); i++) {
      if (dvbFrontends[i]->ProvidesDeliverySystem(DeliverySystem) && dvbFrontends[i]->ProvidesModulation(dtp.System(), dtp.StreamId(), dtp.Modulation())) {
         if (Activate && dvbFrontend != dvbFrontends[i]) {
            cMutexLock MutexLock(&mutex);
            dvbFrontend->Close();
            dvbFrontend = dvbFrontends[i];
            fd_frontend = dvbFrontend->Open();
            frontend = i;
            dsyslog("using frontend %d/%d", adapter, frontend);
            lastDiseqc = NULL;
            lastSource = 0;
            lastUncValue = 0;
            lastUncDelta = 0;
            lastUncChange = 0;
            lnbPowerTurnedOn = false;
            }
         return true;
         }
      }
  return false;
}

bool cDvbTuner::Bond(cDvbTuner *Tuner)
{
  cMutexLock MutexLock(&bondMutex);
  if (!bondedTuner) {
     ResetToneAndVoltage();
     bondedMaster = false; // makes sure we don't disturb an existing master
     bondedTuner = Tuner->bondedTuner ? Tuner->bondedTuner : Tuner;
     Tuner->bondedTuner = this;
     dsyslog("tuner %d/%d bonded with tuner %d/%d", adapter, frontend, bondedTuner->adapter, bondedTuner->frontend);
     return true;
     }
  else
     esyslog("ERROR: tuner %d/%d already bonded with tuner %d/%d, can't bond with tuner %d/%d", adapter, frontend, bondedTuner->adapter, bondedTuner->frontend, Tuner->adapter, Tuner->frontend);
  return false;
}

void cDvbTuner::UnBond(void)
{
  cMutexLock MutexLock(&bondMutex);
  if (cDvbTuner *t = bondedTuner) {
     dsyslog("tuner %d/%d unbonded from tuner %d/%d", adapter, frontend, bondedTuner->adapter, bondedTuner->frontend);
     while (t->bondedTuner != this)
           t = t->bondedTuner;
     if (t == bondedTuner)
        t->bondedTuner = NULL;
     else
        t->bondedTuner = bondedTuner;
     bondedMaster = false; // another one will automatically become master whenever necessary
     bondedTuner = NULL;
     }
}

cString cDvbTuner::GetBondingParams(const cChannel *Channel) const
{
  if (!Channel)
     Channel = &channel;
  cDvbTransponderParameters dtp(Channel->Parameters());
  if (Setup.DiSEqC) {
     if (const cDiseqc *diseqc = Diseqcs.Get(device->DeviceNumber() + 1, Channel->Source(), Channel->Frequency(), dtp.Polarization(), NULL))
        return diseqc->Commands();
     }
  else {
     bool ToneOff = Channel->Frequency() < Setup.LnbSLOF;
     bool VoltOff = dtp.Polarization() == 'V' || dtp.Polarization() == 'R';
     return cString::sprintf("%c %c", ToneOff ? 't' : 'T', VoltOff ? 'v' : 'V');
     }
  return "";
}

bool cDvbTuner::BondingOk(const cChannel *Channel, bool ConsiderOccupied) const
{
  cMutexLock MutexLock(&bondMutex);
  if (cDvbTuner *t = bondedTuner) {
     cString BondingParams = GetBondingParams(Channel);
     do {
        if (t->device->Priority() > IDLEPRIORITY || ConsiderOccupied && t->device->Occupied()) {
           if (strcmp(BondingParams, t->GetBondedMaster()->GetBondingParams()) != 0)
              return false;
           }
        t = t->bondedTuner;
        } while (t != bondedTuner);
     }
  return true;
}

cDvbTuner *cDvbTuner::GetBondedMaster(void)
{
  if (!bondedTuner)
     return this; // an unbonded tuner is always "master"
  cMutexLock MutexLock(&bondMutex);
  if (bondedMaster)
     return this;
  // This tuner is bonded, but it's not the master, so let's see if there is a master at all:
  if (cDvbTuner *t = bondedTuner) {
     while (t != this) {
           if (t->bondedMaster)
              return t;
           t = t->bondedTuner;
           }
     }
  // None of the other bonded tuners is master, so make this one the master:
  bondedMaster = true;
  dsyslog("tuner %d/%d is now bonded master", adapter, frontend);
  return this;
}

bool cDvbTuner::IsTunedTo(const cChannel *Channel) const
{
  if (tunerStatus == tsIdle)
     return false; // not tuned to
  if (channel.Source() != Channel->Source() || channel.Transponder() != Channel->Transponder() || channel.Srate() != Channel->Srate())
     return false; // sufficient mismatch
  // Polarization is already checked as part of the Transponder.
  return strcmp(channel.Parameters(), Channel->Parameters()) == 0;
}

void cDvbTuner::SetChannel(const cChannel *Channel)
{
  if (Channel) {
     if (bondedTuner) {
        cMutexLock MutexLock(&bondMutex);
        cDvbTuner *BondedMaster = GetBondedMaster();
        if (BondedMaster == this) {
           if (strcmp(GetBondingParams(Channel), GetBondingParams()) != 0) {
              // switching to a completely different band, so set all others to idle:
              for (cDvbTuner *t = bondedTuner; t && t != this; t = t->bondedTuner)
                  t->SetChannel(NULL);
              }
           }
        else if (strcmp(GetBondingParams(Channel), BondedMaster->GetBondingParams()) != 0)
           BondedMaster->SetChannel(Channel);
        }
     cMutexLock MutexLock(&mutex);
     if (!IsTunedTo(Channel))
        tunerStatus = tsSet;
     diseqcOffset = 0;
     channel = *Channel;
     lastTimeoutReport = 0;
     newSet.Broadcast();
     }
  else {
     cMutexLock MutexLock(&mutex);
     tunerStatus = tsIdle;
     ResetToneAndVoltage();
     }
  if (bondedTuner && device->IsPrimaryDevice())
     cDevice::PrimaryDevice()->DelLivePids(); // 'device' is const, so we must do it this way
}

bool cDvbTuner::Locked(int TimeoutMs)
{
  bool isLocked = (tunerStatus >= tsLocked);
  if (isLocked || !TimeoutMs)
     return isLocked;

  cMutexLock MutexLock(&mutex);
  if (TimeoutMs && tunerStatus < tsLocked)
     locked.TimedWait(mutex, TimeoutMs);
  return tunerStatus >= tsLocked;
}

void cDvbTuner::ClearEventQueue(void) const
{
  cPoller Poller(fd_frontend);
  if (Poller.Poll(TUNER_POLL_TIMEOUT)) {
     dvb_frontend_event Event;
     while (ioctl(fd_frontend, FE_GET_EVENT, &Event) == 0)
           ; // just to clear the event queue - we'll read the actual status below
     }
}

bool cDvbTuner::GetFrontendStatus(fe_status_t &Status) const
{
  if (fd_frontend == -1)
     return false;
  ClearEventQueue();
  Status = (fe_status_t)0; // initialize here to fix buggy drivers
  while (1) {
        if (ioctl(fd_frontend, FE_READ_STATUS, &Status) != -1)
           return true;
        if (errno != EINTR)
           break;
        }
  return false;
}

//#define DEBUG_SIGNALSTATS
//#define DEBUG_SIGNALSTRENGTH
//#define DEBUG_SIGNALQUALITY

bool cDvbTuner::GetSignalStats(int &Valid, double *Strength, double *Cnr, double *BerPre, double *BerPost, double *Per, int *Status) const
{
  if (fd_frontend == -1)
     return false;
  ClearEventQueue();
  fe_status_t FeStatus = (fe_status_t)0; // initialize here to fix buggy drivers
  dtv_property Props[MAXFRONTENDCMDS];
  dtv_properties CmdSeq;
  memset(&Props, 0, sizeof(Props));
  memset(&CmdSeq, 0, sizeof(CmdSeq));
  CmdSeq.props = Props;
  Valid = DTV_STAT_VALID_NONE;
  if (ioctl(fd_frontend, FE_READ_STATUS, &FeStatus) != 0) {
     esyslog("ERROR: frontend %d/%d: %m (%s:%d)", adapter, frontend, __FILE__, __LINE__);
     return false;
     }
  if (Status) {
     *Status = DTV_STAT_HAS_NONE;
     if (FeStatus & FE_HAS_SIGNAL)  *Status |= DTV_STAT_HAS_SIGNAL;
     if (FeStatus & FE_HAS_CARRIER) *Status |= DTV_STAT_HAS_CARRIER;
     if (FeStatus & FE_HAS_VITERBI) *Status |= DTV_STAT_HAS_VITERBI;
     if (FeStatus & FE_HAS_SYNC)    *Status |= DTV_STAT_HAS_SYNC;
     if (FeStatus & FE_HAS_LOCK)    *Status |= DTV_STAT_HAS_LOCK;
     Valid |= DTV_STAT_VALID_STATUS;
     }
  if (Strength)   SETCMD(DTV_STAT_SIGNAL_STRENGTH, 0);
  if (Cnr)        SETCMD(DTV_STAT_CNR, 0);
  if (BerPre)   { SETCMD(DTV_STAT_PRE_ERROR_BIT_COUNT, 0);
                  SETCMD(DTV_STAT_PRE_TOTAL_BIT_COUNT, 0); }
  if (BerPost)  { SETCMD(DTV_STAT_POST_ERROR_BIT_COUNT, 0);
                  SETCMD(DTV_STAT_POST_TOTAL_BIT_COUNT, 0); }
  if (Per)      { SETCMD(DTV_STAT_ERROR_BLOCK_COUNT, 0);
                  SETCMD(DTV_STAT_TOTAL_BLOCK_COUNT, 0); }
  if (CmdSeq.num && ioctl(fd_frontend, FE_GET_PROPERTY, &CmdSeq) != 0) {
     esyslog("ERROR: frontend %d/%d: %m (%s:%d)", adapter, frontend, __FILE__, __LINE__);
     return false;
     }
  int i = 0;
  if (Strength) {
     if (Props[i].u.st.len > 0) {
        switch (Props[i].u.st.stat[0].scale) {
          case FE_SCALE_DECIBEL:  *Strength = double(Props[i].u.st.stat[0].svalue) / 1000;
                                  Valid |= DTV_STAT_VALID_STRENGTH;
                                  break;
          default: ;
          }
        }
     i++;
     }
  if (Cnr) {
     if (Props[i].u.st.len > 0) {
        switch (Props[i].u.st.stat[0].scale) {
          case FE_SCALE_DECIBEL:  *Cnr = double(Props[i].u.st.stat[0].svalue) / 1000;
                                  Valid |= DTV_STAT_VALID_CNR;
                                  break;
          default: ;
          }
        }
     i++;
     }
  if (BerPre) {
     if (Props[i].u.st.len > 0 && Props[i + 1].u.st.len > 0) {
        if (Props[i].u.st.stat[0].scale == FE_SCALE_COUNTER && Props[i + 1].u.st.stat[0].scale == FE_SCALE_COUNTER) {
           uint64_t ebc = Props[i].u.st.stat[0].uvalue; // error bit count
           uint64_t tbc = Props[i + 1].u.st.stat[0].uvalue; // total bit count
           if (tbc > 0) {
              *BerPre = double(ebc) / tbc;
              Valid |= DTV_STAT_VALID_BERPRE;
              }
           }
        }
     i += 2;
     }
  if (BerPost) {
     if (Props[i].u.st.len > 0 && Props[i + 1].u.st.len > 0) {
        if (Props[i].u.st.stat[0].scale == FE_SCALE_COUNTER && Props[i + 1].u.st.stat[0].scale == FE_SCALE_COUNTER) {
           uint64_t ebc = Props[i].u.st.stat[0].uvalue; // error bit count
           uint64_t tbc = Props[i + 1].u.st.stat[0].uvalue; // total bit count
           if (tbc > 0) {
              *BerPost = double(ebc) / tbc;
              Valid |= DTV_STAT_VALID_BERPOST;
              }
           }
        }
     i += 2;
     }
  if (Per) {
     if (Props[i].u.st.len > 0 && Props[i + 1].u.st.len > 0) {
        if (Props[i].u.st.stat[0].scale == FE_SCALE_COUNTER && Props[i + 1].u.st.stat[0].scale == FE_SCALE_COUNTER) {
           uint64_t ebc = Props[i].u.st.stat[0].uvalue; // error block count
           uint64_t tbc = Props[i + 1].u.st.stat[0].uvalue; // total block count
           if (tbc > 0) {
              *Per = double(ebc) / tbc;
              Valid |= DTV_STAT_VALID_PER;
              }
           }
        }
     i += 2;
     }
#ifdef DEBUG_SIGNALSTATS
  fprintf(stderr, "FE %d/%d: API5 %04X", adapter, frontend, Valid);
  if ((Valid & DTV_STAT_VALID_STATUS)   != 0) fprintf(stderr, " STAT=%04X",     *Status);
  if ((Valid & DTV_STAT_VALID_STRENGTH) != 0) fprintf(stderr, " STR=%1.1fdBm",  *Strength);
  if ((Valid & DTV_STAT_VALID_CNR)      != 0) fprintf(stderr, " CNR=%1.1fdB",   *Cnr);
  if ((Valid & DTV_STAT_VALID_BERPRE)   != 0) fprintf(stderr, " BERPRE=%1.1e",  *BerPre);
  if ((Valid & DTV_STAT_VALID_BERPOST)  != 0) fprintf(stderr, " BERPOST=%1.1e", *BerPost);
  if ((Valid & DTV_STAT_VALID_PER)      != 0) fprintf(stderr, " PER=%1.1e",     *Per);
  fprintf(stderr, "\n");
#endif
  return Valid != DTV_STAT_VALID_NONE;
}

int dB1000toPercent(int dB1000, int Low, int High)
{
  // Convert the given value, which is in 1/1000 dBm, to a percentage in the
  // range 0..100. Anything below Low is considered 0%, and anything above
  // High counts as 100%.
  if (dB1000 < Low)
     return 0;
  if (dB1000 > High)
     return 100;
  // return 100 - 100 * (High - dB1000) / (High - Low); // linear conversion
  // return 100 - 100 * sqr(dB1000 - High) / sqr(Low - High); // quadratic conversion, see https://www.adriangranados.com/blog/dbm-to-percent-conversion
  double v = 10.0 * (dB1000 - High) / (Low - High); // avoids the sqr() function
  return 100 - v * v;
}

#define REF_S1(q1)                 (mod == QPSK)   ? q1 : 0
#define REF_S2(q1, q2, q3, q4)     (mod == QPSK)   ? q1 : (mod == PSK_8)  ? q2 : (mod == APSK_16) ? q3 : (mod == APSK_32) ? q4 : 0
#define REF_T1(q1, q2, q3)         (mod == QPSK)   ? q1 : (mod == QAM_16) ? q2 : (mod == QAM_64)  ? q3 : 0
#define REF_T2(q1, q2, q3, q4)     (mod == QPSK)   ? q1 : (mod == QAM_16) ? q2 : (mod == QAM_64)  ? q3 : (mod == QAM_256) ? q4 : 0
#define REF_C1(q1, q2, q3, q4, q5) (mod == QAM_16) ? q1 : (mod == QAM_32) ? q2 : (mod == QAM_64)  ? q3 : (mod == QAM_128) ? q4 : (mod == QAM_256) ? q5: 0

int StrengthToSSI(const cChannel *Channel, int Strength, int FeModulation, int FeCoderateH, int FeFec)
{
  // Strength in 0.001dBm (dBm x 1000)
  cDvbTransponderParameters dtp(Channel->Parameters());
  int ssi = 0; // 0-100
  int mod = (FeModulation >= 0) ? FeModulation : dtp.Modulation();
  int cod = (FeCoderateH >= 0) ? FeCoderateH : dtp.CoderateH(); // DVB-T
  int fec = (FeFec >= 0) ? FeFec : dtp.CoderateH();
  if (Channel->IsTerr()) {
     int pref = 0;
     // NorDig Unified Ver. 2.6 - 3.4.4.6 Page 43 ff.
     // reference values : pref-15dBm = 0%, pref+35dBm = 100%
     if (dtp.System() == DVB_SYSTEM_1) { // DVB-T
        fec = cod; // adjustment for DVB-T
        if (mod == QAM_AUTO) mod = QPSK;
        switch (fec) {  // dBm:        Q4  Q16  Q64
          case FEC_1_2: pref = REF_T1(-93, -87, -82); break;
          default:
          case FEC_2_3: pref = REF_T1(-91, -85, -80); break;
          case FEC_3_4: pref = REF_T1(-90, -84, -78); break;
          case FEC_5_6: pref = REF_T1(-89, -83, -77); break;
          case FEC_7_8: pref = REF_T1(-88, -82, -76); break;
          }
        }
     else { // DVB-T2
        if (mod == QAM_AUTO) mod = QAM_64;
        switch (fec) {  // dBm:        Q4  Q16  Q64 Q256
          case FEC_1_2: pref = REF_T2(-96, -91, -86, -82); break;
          default:
          case FEC_3_5: pref = REF_T2(-95, -89, -85, -80); break;
          case FEC_2_3: pref = REF_T2(-94, -88, -83, -78); break;
          case FEC_3_4: pref = REF_T2(-93, -87, -82, -76); break;
          case FEC_4_5: pref = REF_T2(-92, -86, -81, -75); break;
          case FEC_5_6: pref = REF_T2(-92, -86, -80, -74); break;
          }
        }
     if (pref) {
        int prel = (Strength / 1000) - pref;
        ssi = (prel < -15) ? 0 :
              (prel <   0) ? (prel + 15) * 2 / 3 :       //  0% -  10%
              (prel <  20) ? prel * 4 + 10 :             // 10% -  90%
              (prel <  35) ? (prel - 20) * 2 / 3 + 90 :  // 90% - 100%
              100;
#ifdef DEBUG_SIGNALSTRENGTH
        fprintf(stderr, "SSI-T: STR:%d, Pref:%d, Prel:%d, ssi:%d%%(sys:%d, mod:%d, fec:%d)\n", Strength, pref, prel, ssi, dtp.System(), mod, fec);
#endif
        }
     }
  else if (Channel->IsCable()) { // ! COMPLETELY UNTESTED !
     // Formula: pref(dB) = -174.0 + NoiseFigure + SymRef + CnRef
     // NoiseFigure = 6.5 dB;               -> Tuner specific - range: 3.5 .. 9.0 dB
     // SymRef = 10*log(6900000) = 68.5 dB; -> for Symbolrate of 6900 kSym/sec (TV: 6900, 6750 or 6111 kSym/sec)
     // ==> pref(dB) = -174.0 + 6.5 + 68.5 + CnRef[modulation]{20,23,26,29,32}; (+/- 3 dB tuner specific)
     if (mod == QAM_AUTO) mod = QAM_256;
     //                Q16  Q32  Q64 Q128 Q256
     int pref = REF_C1(-79, -76, -73, -70, -67);
     if (pref) {
        int prel = (Strength / 1000) - pref;
        ssi = (prel < -15) ? 0 :
              (prel <   0) ? (prel + 15) * 2 / 3 :       //  0% -  10%
              (prel <  20) ? prel * 4 + 10 :             // 10% -  90%
              (prel <  35) ? (prel - 20) * 2 / 3 + 90 :  // 90% - 100%
              100;
#ifdef DEBUG_SIGNALSTRENGTH
        fprintf(stderr, "SSI-C: STR:%d, Pref:%d, Prel:%d, ssi:%d%%(mod:%d)\n", Strength, pref, prel, ssi, mod);
#endif
        }
     }
  else if (Channel->IsSat())
     ssi = dB1000toPercent(Strength, -95000, -20000); // defaults
  return ssi;
}

// Due to missing values or the different meanings of the reported error rate, ber_sqi is currently not taken into account
#define IGNORE_BER 1
#define BER_ERROR_FREE (1000*1000*1000) // 1/10^-9

int SignalToSQI(const cChannel *Channel, int Signal, int Ber, int FeModulation, int FeCoderateH, int FeFec)
{
#if IGNORE_BER
  Ber = BER_ERROR_FREE; // assume/pretend to be biterror free
#endif
  // Signal in 0.001dB (dB x 1000)
  cDvbTransponderParameters dtp(Channel->Parameters());
  int sqi = 0; // 0-100
  int mod = (FeModulation >= 0) ? FeModulation : dtp.Modulation();
  int cod = (FeCoderateH >= 0) ? FeCoderateH : dtp.CoderateH(); // DVB-T
  int fec = (FeFec >= 0) ? FeFec : dtp.CoderateH();
  if (Channel->IsTerr()) { // QEF: BER 10^-6
     int cnref = 0;
     // NorDig Unified Ver. 2.6 - 3.4.4.7 Page 45 ff.
     // reference values for QEF (BER 10^-11 at MPEG2 demux input)
     if (dtp.System() == DVB_SYSTEM_1) { // DVB-T
        fec = cod; // adjustment for DVB-T
        if (mod == QAM_AUTO) mod = QPSK;
        switch (fec) {  // 0.1 dB      Q4  Q16  Q64 (Hierarchy=None)
          case FEC_1_2: cnref = REF_T1(51, 108, 165); break;
          default:
          case FEC_2_3: cnref = REF_T1(69, 131, 187); break;
          case FEC_3_4: cnref = REF_T1(79, 146, 202); break;
          case FEC_5_6: cnref = REF_T1(89, 156, 216); break;
          case FEC_7_8: cnref = REF_T1(97, 160, 225); break;
          }
        }
     else { // DVB-T2
        if (mod == QAM_AUTO) mod = QAM_64;
        switch (fec) {  // 0.1 dB      Q4  Q16  Q64 Q256
          case FEC_1_2: cnref = REF_T2(35,  87, 130, 170); break;
          default:
          case FEC_3_5: cnref = REF_T2(47, 101, 148, 194); break;
          case FEC_2_3: cnref = REF_T2(56, 114, 162, 208); break;
          case FEC_3_4: cnref = REF_T2(66, 125, 177, 229); break;
          case FEC_4_5: cnref = REF_T2(72, 133, 187, 243); break;
          case FEC_5_6: cnref = REF_T2(77, 138, 194, 251); break;
          }
        }
     if (cnref) {
        int cnrel = (Signal/100) - cnref; // 0.1 dB
        int ber_sqi = 100; // 100%
        int cnr_sqi = 0;   // 0%
        if (dtp.System() == DVB_SYSTEM_1) { // DVB-T
            ber_sqi = (Ber < 1000) ? 0 :           // > 10^-3
                      (Ber >= 10000000) ? 100 :    // <= 10^-7
                      (int)(20 * log10(Ber)) - 40; // 20*log10(1/BER)-40 -> 20% .. 100%
            // scale: -7dB/+3dB to reference-value
            cnr_sqi = (cnrel < -70) ? 0 :
                      (cnrel < +30) ? (100 + (cnrel - 30)) :
                      100;
            sqi = (cnr_sqi * ber_sqi) / 100;
            // alternative: stretched scale: cnref-7dB = 0%, 30dB = 100%
            // sqi = dB1000toPercent(Signal, (100*cnref)-7000, 30000);
            }
        else { // DVB-T2
            ber_sqi = (Ber < 10000)     ? 0 :             // > 10^-4
                      (Ber >= 10000000) ? 100 * 100 / 6 : // <= 10^-7 : 16.67% -> SQI 0% .. 100%
                      (100 * 100 / 15);                   //             6.67% -> SQI 0% .. 40% || 100%
            // scale: -3dB/+3dB to reference-value
            sqi = (cnrel <  -30) ? 0 :
                  (cnrel <= +30) ? (cnrel + 30) * ber_sqi / 1000 : // (0 .. 6) * 16,67 || 6.67
                  100;
            // alternative: stretched scale: cnref-3dB = 0%, 32dB = 100%
            // sqi = dB1000toPercent(Signal, (100*cnref)-3000, 32000);
            }
#ifdef DEBUG_SIGNALQUALITY
        fprintf(stderr, "SQI-T: SIG:%d, BER:%d, CNref:%d, CNrel:%d, bersqi:%d, sqi:%d%%(sys:%d, mod:%d, fec:%d)\n", Signal, Ber, cnref, cnrel, ber_sqi, sqi, dtp.System(), mod, fec);
#endif
        }
     }
  else if (Channel->IsCable()) { // ! COMPLETELY UNTESTED !
     if (mod == QAM_AUTO) mod = QAM_256;
         // 0.1 dB      Q16  Q32  Q64 Q128 Q256
     int cnref = REF_C1(200, 230, 260, 290, 320); // minimum for BER<10^-4
     if (cnref) {
        int cnrel = (Signal / 100) - cnref; // 0.1 dB
        int ber_sqi = (Ber < 1000)      ? 0 :      // > 10^-3
                      (Ber >= 10000000) ? 100 :    // <= 10^-7
                      (int)(20 * log10(Ber)) - 40; // 20*log10(1/BER)-40 -> 20% .. 100%
        // scale: -7dB/+3dB to reference-value
        int cnr_sqi = (cnrel < -70) ? 0 :
                      (cnrel < +30) ? (100 + (cnrel - 30)) :
                      100;
        sqi = (cnr_sqi * ber_sqi) / 100;
        // alternative: stretched scale: cnref-7dB = 0%, 40dB = 100%
        // sqi = dB1000toPercent(Signal, (100*cnref)-7000, 40000);
#ifdef DEBUG_SIGNALQUALITY
        dsyslog("SQI-C: SIG:%d, BER:%d, CNref:%d, CNrel:%d, bersqi:%d, sqi:%d%%(sys:%d, mod:%d, fec:%d)\n", Signal, Ber, cnref, cnrel, ber_sqi, sqi, dtp.System(), mod, fec);
#endif
        }
     }
  else if (Channel->IsSat()) {
    int cnref = 0;
    if (dtp.System() == DVB_SYSTEM_1) { // DVB-S
        if (mod == QAM_AUTO) mod = QPSK;
        switch (fec) {  // 0.1 dB:     Q4 : 10^-7
          case FEC_1_2: cnref = REF_S1(38); break;
          default:
          case FEC_2_3: cnref = REF_S1(56); break;
          case FEC_3_4: cnref = REF_S1(67); break;
          case FEC_5_6: cnref = REF_S1(77); break;
          case FEC_7_8: cnref = REF_S1(84); break;
          }
        if (cnref) {
           //cnrel = (Signal/100) - cnref; // 0.1 dB
           // scale: cnref-4dB = 0%, 15dB = 100%
           sqi = dB1000toPercent(Signal, (100*cnref)-4000, 15000);
#ifdef DEBUG_SIGNALQUALITY
           dsyslog("SQI-S1: SIG:%d, BER:%d, CNref:%d, sqi:%d%%(mod:%d, fec:%d)\n", Signal, Ber, cnref, sqi, mod, fec);
#endif
           }
        }
    else { // DVB-S2
        if (mod == QAM_AUTO) mod = QAM_64;
        switch (fec) {   // 0.1 dB       Q4   Q8 16A* 32A*
        //case FEC_1_4:  cnref = REF_S2(-14,  65,  90, 126); break;
        //case FEC_1_3:  cnref = REF_S2( -2,  65,  90, 126); break;
          case FEC_2_5:  cnref = REF_S2(  7,  65,  90, 126); break;
          case FEC_1_2:  cnref = REF_S2( 20,  65,  90, 126); break;
          case FEC_3_5:  cnref = REF_S2( 32,  65,  90, 126); break;
          default:
          case FEC_2_3:  cnref = REF_S2( 41,  76,  90, 126); break;
          case FEC_3_4:  cnref = REF_S2( 50,  66, 102, 126); break;
          case FEC_4_5:  cnref = REF_S2( 57,  89, 110, 136); break;
          case FEC_5_6:  cnref = REF_S2( 62, 104, 116, 143); break;
          case FEC_8_9:  cnref = REF_S2( 72, 117, 129, 157); break;
          case FEC_9_10: cnref = REF_S2( 74, 120, 131, 161); break;
          }
        if (cnref) {
           // cnrel = (Signal/100) - cnref; // 0.1 dB
           // scale: cnref-4dB = 0%, 20dB = 100%
           sqi = dB1000toPercent(Signal, (100*cnref)-4000, 20000);
#ifdef DEBUG_SIGNALQUALITY
           dsyslog("SQI-S2: SIG:%d, BER:%d, CNref:%d, sqi:%d%%(mod:%d, fec:%d)\n", Signal, Ber, cnref, sqi, mod, fec);
#endif
           }
        }
  }
  return sqi;
}

int cDvbTuner::GetSignalStrength(void) const
{
  if (fd_frontend == -1)
     return 0;
  ClearEventQueue();
  // Try DVB API 5:
  for (int i = 0; i < 1; i++) { // just a trick to break out with 'continue' ;-)
      dtv_property Props[MAXFRONTENDCMDS];
      dtv_properties CmdSeq;
      memset(&Props, 0, sizeof(Props));
      memset(&CmdSeq, 0, sizeof(CmdSeq));
      CmdSeq.props = Props;
      SETCMD(DTV_STAT_SIGNAL_STRENGTH, 0);
      SETCMD(DTV_MODULATION, 0);
      SETCMD(DTV_CODE_RATE_HP, 0); // DVB-T only
      SETCMD(DTV_INNER_FEC, 0);
      if (ioctl(fd_frontend, FE_GET_PROPERTY, &CmdSeq) != 0) {
         esyslog("ERROR: frontend %d/%d: %m (%s:%d)", adapter, frontend, __FILE__, __LINE__);
         return -1;
         }
      int FeMod = (Props[1].u.st.len > 0) ? (int)Props[1].u.data : -1;
      int FeCod = (Props[2].u.st.len > 0) ? (int)Props[2].u.data : -1;
      int FeFec = (Props[3].u.st.len > 0) ? (int)Props[3].u.data : -1;
      int Signal = 0;
      if (Props[0].u.st.len > 0) {
         switch (Props[0].u.st.stat[0].scale) {
           case FE_SCALE_DECIBEL:  Signal = StrengthToSSI(&channel, Props[0].u.st.stat[0].svalue, FeMod, FeCod, FeFec);
                                   break;
           case FE_SCALE_RELATIVE: Signal = 100 * Props[0].u.st.stat[0].uvalue / 0xFFFF;
                                   break;
           default: ;
           }
#ifdef DEBUG_SIGNALSTRENGTH
         fprintf(stderr, "FE %d/%d: API5 %d %08X %.1f S = %d\n", adapter, frontend, Props[0].u.st.stat[0].scale, int(Props[0].u.st.stat[0].svalue), int(Props[0].u.st.stat[0].svalue) / 1000.0, Signal);
#endif
         }
      else
         continue;
      return Signal;
      }
  // Fall back to DVB API 3:
  uint16_t Signal;
  while (1) {
        if (ioctl(fd_frontend, FE_READ_SIGNAL_STRENGTH, &Signal) != -1)
           break;
        if (errno != EINTR)
           return -1;
        }
  uint16_t MaxSignal = 0xFFFF; // Let's assume the default is using the entire range.
  // Use the subsystemId to identify individual devices in case they need
  // special treatment to map their Signal value into the range 0...0xFFFF.
  switch (dvbFrontend->SubsystemId()) {
    case 0x13C21019: // TT-budget S2-3200 (DVB-S/DVB-S2)
    case 0x1AE40001: // TechniSat SkyStar HD2 (DVB-S/DVB-S2)
                     MaxSignal = 670; break;
    }
  int s = int(Signal) * 100 / MaxSignal;
  if (s > 100)
     s = 100;
#ifdef DEBUG_SIGNALSTRENGTH
  fprintf(stderr, "FE %d/%d: API3 %08X S = %04X %04X %3d%%\n", adapter, frontend, dvbFrontend->SubsystemId(), MaxSignal, Signal, s);
#endif
  return s;
}

#define LOCK_THRESHOLD 5 // indicates that all 5 FE_HAS_* flags are set

int cDvbTuner::GetSignalQuality(void) const
{
  if (fd_frontend == -1)
     return 0;
  // Try DVB API 5:
  for (int i = 0; i < 1; i++) { // just a trick to break out with 'continue' ;-)
      dtv_property Props[MAXFRONTENDCMDS];
      dtv_properties CmdSeq;
      memset(&Props, 0, sizeof(Props));
      memset(&CmdSeq, 0, sizeof(CmdSeq));
      CmdSeq.props = Props;
      SETCMD(DTV_STAT_CNR, 0);
      SETCMD(DTV_MODULATION, 0);
      SETCMD(DTV_CODE_RATE_HP, 0); // DVB-T only
      SETCMD(DTV_INNER_FEC, 0);
      SETCMD(DTV_STAT_POST_ERROR_BIT_COUNT, 0);
      SETCMD(DTV_STAT_POST_TOTAL_BIT_COUNT, 0);
      if (ioctl(fd_frontend, FE_GET_PROPERTY, &CmdSeq) != 0) {
         esyslog("ERROR: frontend %d/%d: %m (%s:%d)", adapter, frontend, __FILE__, __LINE__);
         return -1;
         }
      int FeMod = (Props[1].u.st.len > 0) ? (int)Props[1].u.data : -1;
      int FeCod = (Props[2].u.st.len > 0) ? (int)Props[2].u.data : -1;
      int FeFec = (Props[3].u.st.len > 0) ? (int)Props[3].u.data : -1;
      int Ber = BER_ERROR_FREE; // 1/10^-9
      if (Props[4].u.st.len > 0 && Props[4].u.st.stat[0].scale == FE_SCALE_COUNTER && Props[5].u.st.len > 0 && Props[5].u.st.stat[0].scale == FE_SCALE_COUNTER) {
         uint64_t ebc = Props[4].u.st.stat[0].uvalue; // error bit count
         uint64_t tbc = Props[5].u.st.stat[0].uvalue; // total bit count
         if (ebc > 0) {
            uint64_t BerRev = tbc / ebc; // reversed, for integer arithmetic
            if (BerRev < BER_ERROR_FREE)
               Ber = (int)BerRev;
            }
         }
      int Cnr = 0;
      if (Props[0].u.st.len > 0) {
         switch (Props[0].u.st.stat[0].scale) {
           case FE_SCALE_DECIBEL:  Cnr = SignalToSQI(&channel, Props[0].u.st.stat[0].svalue, Ber, FeMod, FeCod, FeFec);
                                   break;
           case FE_SCALE_RELATIVE: Cnr = 100 * Props[0].u.st.stat[0].uvalue / 0xFFFF;
                                   break;
           default: ;
           }
#ifdef DEBUG_SIGNALQUALITY
         fprintf(stderr, "FE %d/%d: API5 %d %08X %.1f Q = %d\n", adapter, frontend, Props[0].u.st.stat[0].scale, int(Props[0].u.st.stat[0].svalue), int(Props[0].u.st.stat[0].svalue) / 1000.0, Cnr);
#endif
         }
      else
         continue;
      return Cnr;
      }
  // Fall back to DVB API 3:
  fe_status_t Status;
  if (GetFrontendStatus(Status)) {
     // Actually one would expect these checks to be done from FE_HAS_SIGNAL to FE_HAS_LOCK, but some drivers (like the stb0899) are broken, so FE_HAS_LOCK is the only one that (hopefully) is generally reliable...
     if ((Status & FE_HAS_LOCK) == 0) {
        if ((Status & FE_HAS_SIGNAL) == 0)
           return 0;
        if ((Status & FE_HAS_CARRIER) == 0)
           return 1;
        if ((Status & FE_HAS_VITERBI) == 0)
           return 2;
        if ((Status & FE_HAS_SYNC) == 0)
           return 3;
        return 4;
        }
#ifdef DEBUG_SIGNALQUALITY
     bool HasSnr = true;
#endif
     uint16_t Snr;
     while (1) {
           if (ioctl(fd_frontend, FE_READ_SNR, &Snr) != -1)
              break;
           if (errno != EINTR) {
              Snr = 0xFFFF;
#ifdef DEBUG_SIGNALQUALITY
              HasSnr = false;
#endif
              break;
              }
           }
#ifdef DEBUG_SIGNALQUALITY
     bool HasBer = true;
#endif
     uint32_t Ber;
     while (1) {
           if (ioctl(fd_frontend, FE_READ_BER, &Ber) != -1)
              break;
           if (errno != EINTR) {
              Ber = 0;
#ifdef DEBUG_SIGNALQUALITY
              HasBer = false;
#endif
              break;
              }
           }
#ifdef DEBUG_SIGNALQUALITY
     bool HasUnc = true;
#endif
     uint32_t Unc;
     while (1) {
           if (ioctl(fd_frontend, FE_READ_UNCORRECTED_BLOCKS, &Unc) != -1) {
              if (Unc != lastUncValue) {
#ifdef DEBUG_SIGNALQUALITY
                 fprintf(stderr, "FE %d/%d: API3 UNC = %u %u %u\n", adapter, frontend, Unc, lastUncValue, lastUncDelta);
#endif
                 lastUncDelta = (Unc >= lastUncValue) ? Unc - lastUncValue : lastUncValue - Unc;
                 lastUncValue = Unc;
                 lastUncChange = time(NULL);
                 }
              // The number of uncorrected blocks is a counter, which is normally
              // at a constant value and only increases if there are new uncorrected
              // blocks. So a change in the Unc value indicates reduced signal quality.
              // Whenever the Unc counter changes, we take the delta between the old
              // and new value into account for calculating the overall signal quality.
              // The impact of Unc is considered for 2 seconds, and after that it is
              // bisected with every passing second in order to phase it out. Otherwise
              // once one or more uncorrected blocks occur, the signal quality would
              // be considered low even if there haven't been any more uncorrected bocks
              // for quite a while.
              Unc = lastUncDelta;
              if (Unc > 0) {
                 int t = time(NULL) - lastUncChange - 2;
                 if (t > 0)
                    Unc >>= min(t, int(sizeof(Unc) * 8 - 1));
                 if (Unc == 0)
                    lastUncDelta = 0;
#ifdef DEBUG_SIGNALQUALITY
                 fprintf(stderr, "FE %d/%d: API3 UNC = %u\n", adapter, frontend, Unc);
#endif
                 }
              break;
              }
           if (errno != EINTR) {
              Unc = 0;
#ifdef DEBUG_SIGNALQUALITY
              HasUnc = false;
#endif
              break;
              }
           }
     uint16_t MinSnr = 0x0000;
     uint16_t MaxSnr = 0xFFFF; // Let's assume the default is using the entire range.
     // Use the subsystemId to identify individual devices in case they need
     // special treatment to map their Snr value into the range 0...0xFFFF.
     switch (dvbFrontend->SubsystemId()) {
       case 0x13C21019: // TT-budget S2-3200 (DVB-S/DVB-S2)
       case 0x1AE40001: // TechniSat SkyStar HD2 (DVB-S/DVB-S2)
                        if (frontendType == SYS_DVBS2) {
                           MinSnr = 10;
                           MaxSnr = 70;
                           }
                        else
                           MaxSnr = 200;
                        break;
       case 0x20130245: // PCTV Systems PCTV 73ESE
       case 0x2013024F: // PCTV Systems nanoStick T2 290e
                        MaxSnr = 255; break;
       }
     int a = int(constrain(Snr, MinSnr, MaxSnr)) * 100 / (MaxSnr - MinSnr);
     int b = 100 - (Unc * 10 + (Ber / 256) * 5);
     if (b < 0)
        b = 0;
     int q = LOCK_THRESHOLD + a * b * (100 - LOCK_THRESHOLD) / 100 / 100;
     if (q > 100)
        q = 100;
#ifdef DEBUG_SIGNALQUALITY
     fprintf(stderr, "FE %d/%d: API3 %08X Q = %04X %04X %d %5d %5d %3d%%\n", adapter, frontend, dvbFrontend->SubsystemId(), MaxSnr, Snr, HasSnr, HasBer ? int(Ber) : -1, HasUnc ? int(Unc) : -1, q);
#endif
     return q;
     }
  return -1;
}

static unsigned int FrequencyToHz(unsigned int f)
{
  while (f && f < 1000000)
        f *= 1000;
  return f;
}

cPositioner *cDvbTuner::GetPositioner(void)
{
  if (!positioner) {
     positioner = cPositioner::GetPositioner();
     positioner->SetFrontend(fd_frontend);
     }
  return positioner;
}

void cDvbTuner::ExecuteDiseqc(const cDiseqc *Diseqc, int *Frequency)
{
  if (!lnbPowerTurnedOn) {
     CHECK(ioctl(fd_frontend, FE_SET_VOLTAGE, SEC_VOLTAGE_13)); // must explicitly turn on LNB power
     lnbPowerTurnedOn = true;
     }
  static cMutex Mutex;
  if (Diseqc->IsScr())
     Mutex.Lock();
  struct dvb_diseqc_master_cmd cmd;
  const char *CurrentAction = NULL;
  cPositioner *Positioner = NULL;
  bool Break = false;
  for (int i = 0; !Break; i++) {
      cmd.msg_len = sizeof(cmd.msg);
      cDiseqc::eDiseqcActions da = Diseqc->Execute(&CurrentAction, cmd.msg, &cmd.msg_len, scr, Frequency);
      if (da == cDiseqc::daNone) {
         diseqcOffset = 0;
         break;
         }
      bool d = i >= diseqcOffset;
      switch (da) {
        case cDiseqc::daToneOff:   if (d) CHECK(ioctl(fd_frontend, FE_SET_TONE, SEC_TONE_OFF)); break;
        case cDiseqc::daToneOn:    if (d) CHECK(ioctl(fd_frontend, FE_SET_TONE, SEC_TONE_ON)); break;
        case cDiseqc::daVoltage13: if (d) CHECK(ioctl(fd_frontend, FE_SET_VOLTAGE, SEC_VOLTAGE_13)); break;
        case cDiseqc::daVoltage18: if (d) CHECK(ioctl(fd_frontend, FE_SET_VOLTAGE, SEC_VOLTAGE_18)); break;
        case cDiseqc::daMiniA:     if (d) CHECK(ioctl(fd_frontend, FE_DISEQC_SEND_BURST, SEC_MINI_A)); break;
        case cDiseqc::daMiniB:     if (d) CHECK(ioctl(fd_frontend, FE_DISEQC_SEND_BURST, SEC_MINI_B)); break;
        case cDiseqc::daCodes:     if (d) CHECK(ioctl(fd_frontend, FE_DISEQC_SEND_MASTER_CMD, &cmd)); break;
        case cDiseqc::daPositionN: if ((Positioner = GetPositioner()) != NULL) {
                                      if (d) {
                                         Positioner->GotoPosition(Diseqc->Position(), cSource::Position(channel.Source()));
                                         Break = Positioner->IsMoving();
                                         }
                                      }
                                   break;
        case cDiseqc::daPositionA: if ((Positioner = GetPositioner()) != NULL) {
                                      if (d) {
                                         Positioner->GotoAngle(cSource::Position(channel.Source()));
                                         Break = Positioner->IsMoving();
                                         }
                                      }
                                   break;
        case cDiseqc::daScr:
        case cDiseqc::daWait:      break;
        default: esyslog("ERROR: unknown diseqc command %d", da);
        }
      if (Break)
         diseqcOffset = i + 1;
      }
  positioner = Positioner;
  if (scr && !Break)
     ResetToneAndVoltage(); // makes sure we don't block the bus!
  if (Diseqc->IsScr())
     Mutex.Unlock();
}

void cDvbTuner::ResetToneAndVoltage(void)
{
  if (fd_frontend == -1)
     return;
  CHECK(ioctl(fd_frontend, FE_SET_VOLTAGE, bondedTuner ? SEC_VOLTAGE_OFF : SEC_VOLTAGE_13));
  CHECK(ioctl(fd_frontend, FE_SET_TONE, SEC_TONE_OFF));
}

bool cDvbTuner::SetFrontend(void)
{
  dtv_property Props[MAXFRONTENDCMDS];
  memset(&Props, 0, sizeof(Props));
  dtv_properties CmdSeq;
  memset(&CmdSeq, 0, sizeof(CmdSeq));
  CmdSeq.props = Props;
  SETCMD(DTV_CLEAR, 0);
  if (ioctl(fd_frontend, FE_SET_PROPERTY, &CmdSeq) < 0) {
     esyslog("ERROR: frontend %d/%d: %m (%s:%d)", adapter, frontend, __FILE__, __LINE__);
     return false;
     }
  CmdSeq.num = 0;

  cDvbTransponderParameters dtp(channel.Parameters());

  // Determine the required frontend type:
  frontendType = GetRequiredDeliverySystem(&channel, &dtp);
  if (frontendType == SYS_UNDEFINED)
     return false;

  SETCMD(DTV_DELIVERY_SYSTEM, frontendType);
  if (frontendType == SYS_DVBS || frontendType == SYS_DVBS2) {
     int frequency = channel.Frequency();
     if (Setup.DiSEqC) {
        if (const cDiseqc *diseqc = Diseqcs.Get(device->DeviceNumber() + 1, channel.Source(), frequency, dtp.Polarization(), &scr)) {
           frequency -= diseqc->Lof();
           if (diseqc != lastDiseqc || diseqc->IsScr() || diseqc->Position() >= 0 && channel.Source() != lastSource) {
              if (IsBondedMaster()) {
                 ExecuteDiseqc(diseqc, &frequency);
                 if (frequency == 0)
                    return false;
                 }
              else
                 ResetToneAndVoltage();
              lastDiseqc = diseqc;
              lastSource = channel.Source();
              }
           }
        else {
           esyslog("ERROR: no DiSEqC parameters found for channel %d (%s)", channel.Number(), channel.Name());
           return false;
           }
        }
     else {
        int tone = SEC_TONE_OFF;
        if (frequency < Setup.LnbSLOF) {
           frequency -= Setup.LnbFrequLo;
           tone = SEC_TONE_OFF;
           }
        else {
           frequency -= Setup.LnbFrequHi;
           tone = SEC_TONE_ON;
           }
        int volt = (dtp.Polarization() == 'V' || dtp.Polarization() == 'R') ? SEC_VOLTAGE_13 : SEC_VOLTAGE_18;
        if (!IsBondedMaster()) {
           tone = SEC_TONE_OFF;
           volt = SEC_VOLTAGE_13;
           }
        CHECK(ioctl(fd_frontend, FE_SET_VOLTAGE, volt));
        CHECK(ioctl(fd_frontend, FE_SET_TONE, tone));
        }
     frequency = abs(frequency); // Allow for C-band, where the frequency is less than the LOF

     // DVB-S/DVB-S2 (common parts)
     SETCMD(DTV_FREQUENCY, frequency * 1000UL);
     SETCMD(DTV_MODULATION, dtp.Modulation());
     SETCMD(DTV_SYMBOL_RATE, channel.Srate() * 1000UL);
     SETCMD(DTV_INNER_FEC, dtp.CoderateH());
     SETCMD(DTV_INVERSION, dtp.Inversion());
     if (frontendType == SYS_DVBS2) {
        // DVB-S2
        SETCMD(DTV_PILOT, dtp.Pilot());
        SETCMD(DTV_ROLLOFF, dtp.RollOff());
        if (DvbApiVersion >= 0x0508)
           SETCMD(DTV_STREAM_ID, dtp.StreamId());
        }
     else {
        // DVB-S
        SETCMD(DTV_ROLLOFF, ROLLOFF_35); // DVB-S always has a ROLLOFF of 0.35
        }

     tuneTimeout = DVBS_TUNE_TIMEOUT;
     lockTimeout = DVBS_LOCK_TIMEOUT;
     }
  else if (frontendType == SYS_DVBC_ANNEX_AC || frontendType == SYS_DVBC_ANNEX_B) {
     // DVB-C
     SETCMD(DTV_FREQUENCY, FrequencyToHz(channel.Frequency()));
     SETCMD(DTV_INVERSION, dtp.Inversion());
     SETCMD(DTV_SYMBOL_RATE, channel.Srate() * 1000UL);
     SETCMD(DTV_INNER_FEC, dtp.CoderateH());
     SETCMD(DTV_MODULATION, dtp.Modulation());

     tuneTimeout = DVBC_TUNE_TIMEOUT;
     lockTimeout = DVBC_LOCK_TIMEOUT;
     }
  else if (frontendType == SYS_DVBT || frontendType == SYS_DVBT2) {
     // DVB-T/DVB-T2 (common parts)
     SETCMD(DTV_FREQUENCY, FrequencyToHz(channel.Frequency()));
     SETCMD(DTV_INVERSION, dtp.Inversion());
     SETCMD(DTV_BANDWIDTH_HZ, dtp.Bandwidth());
     SETCMD(DTV_CODE_RATE_HP, dtp.CoderateH());
     SETCMD(DTV_CODE_RATE_LP, dtp.CoderateL());
     SETCMD(DTV_MODULATION, dtp.Modulation());
     SETCMD(DTV_TRANSMISSION_MODE, dtp.Transmission());
     SETCMD(DTV_GUARD_INTERVAL, dtp.Guard());
     SETCMD(DTV_HIERARCHY, dtp.Hierarchy());
     if (frontendType == SYS_DVBT2) {
        // DVB-T2
        SETCMD(DTV_INNER_FEC, dtp.CoderateH());
        if (DvbApiVersion >= 0x0508) {
           SETCMD(DTV_STREAM_ID, dtp.StreamId());
           }
        else if (DvbApiVersion >= 0x0503)
           SETCMD(DTV_DVBT2_PLP_ID_LEGACY, dtp.StreamId());
        }
     tuneTimeout = DVBT_TUNE_TIMEOUT;
     lockTimeout = DVBT_LOCK_TIMEOUT;
     }
  else if (frontendType == SYS_ATSC) {
     // ATSC
     SETCMD(DTV_FREQUENCY, FrequencyToHz(channel.Frequency()));
     SETCMD(DTV_INVERSION, dtp.Inversion());
     SETCMD(DTV_MODULATION, dtp.Modulation());

     tuneTimeout = ATSC_TUNE_TIMEOUT;
     lockTimeout = ATSC_LOCK_TIMEOUT;
     }
  else {
     esyslog("ERROR: attempt to set channel with unknown DVB frontend type");
     return false;
     }
  SETCMD(DTV_TUNE, 0);
  if (ioctl(fd_frontend, FE_SET_PROPERTY, &CmdSeq) < 0) {
     esyslog("ERROR: frontend %d/%d: %m (%s:%d)", adapter, frontend, __FILE__, __LINE__);
     return false;
     }
  return true;
}

void cDvbTuner::Action(void)
{
  cTimeMs Timer;
  bool LostLock = false;
  fe_status_t Status = (fe_status_t)0;
  while (Running()) {
        int WaitTime = 1000;
        fe_status_t NewStatus;
        if (GetFrontendStatus(NewStatus))
           Status = NewStatus;
        cMutexLock MutexLock(&mutex);
        switch (tunerStatus) {
          case tsIdle:
               break; // we want the TimedWait() below!
          case tsSet:
               tunerStatus = SetFrontend() ? tsPositioning : tsIdle;
               continue;
          case tsPositioning:
               if (positioner) {
                  if (positioner->IsMoving())
                     break; // we want the TimedWait() below!
                  else if (diseqcOffset) {
                     lastDiseqc = NULL;
                     tunerStatus = tsSet; // have it process the rest of the DiSEqC sequence
                     continue;
                     }
                  }
               tunerStatus = tsTuned;
               device->SectionHandler()->SetStatus(true); // may have been turned off when retuning
               Timer.Set(tuneTimeout + (scr ? rand() % SCR_RANDOM_TIMEOUT : 0));
               if (positioner)
                  continue;
               // otherwise run directly into tsTuned...
          case tsTuned:
               if (Timer.TimedOut()) {
                  tunerStatus = tsSet;
                  lastDiseqc = NULL;
                  lastSource = 0;
                  if (time(NULL) - lastTimeoutReport > 60) { // let's not get too many of these
                     if (channel.Number()) // no need to log this for transponders that are announced in the NIT but are not currently broadcasting
                        isyslog("frontend %d/%d timed out while tuning to channel %d (%s), tp %d", adapter, frontend, channel.Number(), channel.Name(), channel.Transponder());
                     lastTimeoutReport = time(NULL);
                     }
                  continue;
                  }
               WaitTime = 100; // allows for a quick change from tsTuned to tsLocked
               // run into tsLocked...
          case tsLocked:
               if (Status & FE_REINIT) {
                  tunerStatus = tsSet;
                  lastDiseqc = NULL;
                  lastSource = 0;
                  isyslog("frontend %d/%d was reinitialized", adapter, frontend);
                  lastTimeoutReport = 0;
                  continue;
                  }
               else if (Status & FE_HAS_LOCK) {
                  if (LostLock) {
                     isyslog("frontend %d/%d regained lock on channel %d (%s), tp %d", adapter, frontend, channel.Number(), channel.Name(), channel.Transponder());
                     LostLock = false;
                     }
                  if (device->SdtFilter()->TransponderWrong()) {
                     isyslog("frontend %d/%d is not receiving transponder %d for channel %d (%s) - retuning", adapter, frontend, channel.Transponder(), channel.Number(), channel.Name());
                     device->SectionHandler()->SetStatus(false);
                     tunerStatus = tsSet;
                     lastDiseqc = NULL;
                     lastSource = 0;
                     continue;
                     }
                  tunerStatus = tsLocked;
                  locked.Broadcast();
                  lastTimeoutReport = 0;
                  }
               else if (tunerStatus == tsLocked) {
                  LostLock = true;
                  isyslog("frontend %d/%d lost lock on channel %d (%s), tp %d", adapter, frontend, channel.Number(), channel.Name(), channel.Transponder());
                  tunerStatus = tsTuned;
                  Timer.Set(lockTimeout);
                  lastTimeoutReport = 0;
                  continue;
                  }
               break;
          default: esyslog("ERROR: unknown tuner status %d", tunerStatus);
          }
        newSet.TimedWait(mutex, WaitTime);
        }
}

void cDvbTuner::SetPowerSaveMode(bool On)
{
  cMutexLock MutexLock(&mutex);
  if (On) {
     if (fd_frontend != -1) {
        dsyslog("closing frontend %d/%d", adapter, frontend);
        tunerStatus = tsIdle;
        dvbFrontend->Close();
        fd_frontend = -1;
        channel = cChannel();
        }
     }
  else {
     if (fd_frontend == -1) {
        dsyslog("opening frontend %d/%d", adapter, frontend);
        fd_frontend = dvbFrontend->Open();
        lastDiseqc = NULL;
        lastSource = 0;
        lastUncValue = 0;
        lastUncDelta = 0;
        lastUncChange = 0;
        lnbPowerTurnedOn = false;
        }
     }
}

// --- cDvbSourceParam -------------------------------------------------------

class cDvbSourceParam : public cSourceParam {
private:
  int param;
  int srate;
  cDvbTransponderParameters dtp;
public:
  cDvbSourceParam(char Source, const char *Description);
  virtual void SetData(cChannel *Channel);
  virtual void GetData(cChannel *Channel);
  virtual cOsdItem *GetOsdItem(void);
  };

cDvbSourceParam::cDvbSourceParam(char Source, const char *Description)
:cSourceParam(Source, Description)
{
  param = 0;
  srate = 0;
}

void cDvbSourceParam::SetData(cChannel *Channel)
{
  srate = Channel->Srate();
  dtp.Parse(Channel->Parameters());
  param = 0;
}

void cDvbSourceParam::GetData(cChannel *Channel)
{
  Channel->SetTransponderData(Channel->Source(), Channel->Frequency(), srate, dtp.ToString(Source()), true);
}

cOsdItem *cDvbSourceParam::GetOsdItem(void)
{
  char type = Source();
  const tDvbParameterMap *SystemValues = type == 'S' ? SystemValuesSat : SystemValuesTerr;
#undef ST
#define ST(s) if (strchr(s, type))
  switch (param++) {
    case  0: ST("  S ")  return new cMenuEditChrItem( tr("Polarization"), &dtp.polarization, "HVLR");             else return GetOsdItem();
    case  1: ST("  ST")  return new cMenuEditMapItem( tr("System"),       &dtp.system,       SystemValues);       else return GetOsdItem();
    case  2: ST(" CS ")  return new cMenuEditIntItem( tr("Srate"),        &srate);                                else return GetOsdItem();
    case  3: ST("ACST")  return new cMenuEditMapItem( tr("Inversion"),    &dtp.inversion,    InversionValues);    else return GetOsdItem();
    case  4: ST(" CST")  return new cMenuEditMapItem( tr("CoderateH"),    &dtp.coderateH,    CoderateValues);     else return GetOsdItem();
    case  5: ST("   T")  return new cMenuEditMapItem( tr("CoderateL"),    &dtp.coderateL,    CoderateValues);     else return GetOsdItem();
    case  6: ST("ACST")  return new cMenuEditMapItem( tr("Modulation"),   &dtp.modulation,   ModulationValues);   else return GetOsdItem();
    case  7: ST("   T")  return new cMenuEditMapItem( tr("Bandwidth"),    &dtp.bandwidth,    BandwidthValues);    else return GetOsdItem();
    case  8: ST("   T")  return new cMenuEditMapItem( tr("Transmission"), &dtp.transmission, TransmissionValues); else return GetOsdItem();
    case  9: ST("   T")  return new cMenuEditMapItem( tr("Guard"),        &dtp.guard,        GuardValues);        else return GetOsdItem();
    case 10: ST("   T")  return new cMenuEditMapItem( tr("Hierarchy"),    &dtp.hierarchy,    HierarchyValues);    else return GetOsdItem();
    case 11: ST("  S ")  return new cMenuEditMapItem( tr("Rolloff"),      &dtp.rollOff,      RollOffValues);      else return GetOsdItem();
    case 12: ST("  ST")  return new cMenuEditIntItem( tr("StreamId"),     &dtp.streamId,     0, 255);             else return GetOsdItem();
    case 13: ST("  S ")  return new cMenuEditMapItem( tr("Pilot"),        &dtp.pilot,        PilotValues);        else return GetOsdItem();
    case 14: ST("   T")  return new cMenuEditIntItem( tr("T2SystemId"),   &dtp.t2systemId,   0, 65535);           else return GetOsdItem();
    case 15: ST("   T")  return new cMenuEditIntItem( tr("SISO/MISO"),    &dtp.sisoMiso,     0, 1);               else return GetOsdItem();
    default: return NULL;
    }
  return NULL;
}

// --- cDvbDevice ------------------------------------------------------------

bool cDvbDevice::useDvbDevices = true;
int cDvbDevice::setTransferModeForDolbyDigital = 1;
cMutex cDvbDevice::bondMutex;

cDvbDevice::cDvbDevice(int Adapter, int Frontend)
{
  adapter = Adapter;
  frontend = Frontend;
  ciAdapter = NULL;
  dvbTuner = NULL;
  bondedDevice = NULL;
  needsDetachBondedReceivers = false;
  tsBuffer = NULL;

  // Common Interface:

  fd_ca = DvbOpen(DEV_DVB_CA, adapter, frontend, O_RDWR);
  if (fd_ca >= 0)
     ciAdapter = cDvbCiAdapter::CreateCiAdapter(this, fd_ca);
  checkTsBuffer = false;

  // The DVR device (will be opened and closed as needed):

  fd_dvr = -1;

  // We only check the devices that must be present - the others will be checked before accessing them://XXX

  StartSectionHandler();

  dvbTuner = new cDvbTuner(this, adapter, frontend);
}

cDvbDevice::~cDvbDevice()
{
  delete dvbTuner;
  delete ciAdapter;
  StopSectionHandler();
  UnBond();
  // We're not explicitly closing any device files here, since this sometimes
  // caused segfaults. Besides, the program is about to terminate anyway...
}

cString DvbName(const char *Name, int Adapter, int Frontend)
{
  return cString::sprintf("%s/%s%d/%s%d", DEV_DVB_BASE, DEV_DVB_ADAPTER, Adapter, Name, Frontend);
}

int DvbOpen(const char *Name, int Adapter, int Frontend, int Mode, bool ReportError)
{
  cString FileName = DvbName(Name, Adapter, Frontend);
  int fd = open(FileName, Mode);
  if (fd < 0 && ReportError)
     LOG_ERROR_STR(*FileName);
  return fd;
}

int cDvbDevice::Frontend(void) const
{
  return dvbTuner ? dvbTuner->Frontend() : frontend;
}

bool cDvbDevice::Exists(int Adapter, int Frontend)
{
  cString FileName = DvbName(DEV_DVB_FRONTEND, Adapter, Frontend);
  if (access(FileName, F_OK) == 0) {
     int f = open(FileName, O_RDONLY);
     if (f >= 0) {
        close(f);
        return true;
        }
     else if (errno != ENODEV && errno != EINVAL)
        LOG_ERROR_STR(*FileName);
     }
  else if (errno != ENOENT)
     LOG_ERROR_STR(*FileName);
  return false;
}

bool cDvbDevice::Probe(int Adapter, int Frontend)
{
  cString FileName = DvbName(DEV_DVB_FRONTEND, Adapter, Frontend);
  dsyslog("probing %s", *FileName);
  for (cDvbDeviceProbe *dp = DvbDeviceProbes.First(); dp; dp = DvbDeviceProbes.Next(dp)) {
      if (dp->Probe(Adapter, Frontend))
         return true; // a plugin has created the actual device
      }
  dsyslog("creating cDvbDevice");
  new cDvbDevice(Adapter, Frontend); // it's a "budget" device
  return true;
}

cString cDvbDevice::DeviceType(void) const
{
  if (dvbTuner)
     return GetDeliverySystemName(dvbTuner->FrontendType());
  return "";
}

cString cDvbDevice::DeviceName(void) const
{
  if (dvbTuner)
     return dvbTuner->FrontendName();
  return "";
}

bool cDvbDevice::Initialize(void)
{
  new cDvbSourceParam('A', "ATSC");
  new cDvbSourceParam('C', "DVB-C");
  new cDvbSourceParam('S', "DVB-S");
  new cDvbSourceParam('T', "DVB-T");
  cStringList Nodes;
  cReadDir DvbDir(DEV_DVB_BASE);
  if (DvbDir.Ok()) {
     struct dirent *a;
     while ((a = DvbDir.Next()) != NULL) {
           if (strstr(a->d_name, DEV_DVB_ADAPTER) == a->d_name) {
              int Adapter = strtol(a->d_name + strlen(DEV_DVB_ADAPTER), NULL, 10);
              cReadDir AdapterDir(AddDirectory(DEV_DVB_BASE, a->d_name));
              if (AdapterDir.Ok()) {
                 struct dirent *f;
                 while ((f = AdapterDir.Next()) != NULL) {
                       if (strstr(f->d_name, DEV_DVB_FRONTEND) == f->d_name) {
                          int Frontend = strtol(f->d_name + strlen(DEV_DVB_FRONTEND), NULL, 10);
                          if (access(DvbName(DEV_DVB_DEMUX, Adapter, Frontend), F_OK) == 0) { // we only create devices for actual demuxes
                             dsyslog("detected /dev/dvb/adapter%d/frontend%d", Adapter, Frontend);
                             Nodes.Append(strdup(cString::sprintf("%2d %2d", Adapter, Frontend)));
                             }
                          }
                       }
                 }
              }
           }
     }
  int Found = 0;
  int Used = 0;
  if (Nodes.Size() > 0) {
     Nodes.Sort();
     for (int i = 0; i < Nodes.Size(); i++) {
         int Adapter;
         int Frontend;
         if (2 == sscanf(Nodes[i], "%d %d", &Adapter, &Frontend)) {
            if (Exists(Adapter, Frontend)) {
               if (Found < MAXDEVICES) {
                  Found++;
                  if (useDvbDevices && UseDevice(NextCardIndex())) {
                     if (Probe(Adapter, Frontend))
                        Used++;
                     }
                  else {
                     dsyslog("skipped /dev/dvb/adapter%d/frontend%d", Adapter, Frontend);
                     NextCardIndex(1); // skips this one
                     }
                  }
               }
            }
         }
     }
  if (Found > 0) {
     isyslog("found %d DVB device%s", Found, Found > 1 ? "s" : "");
     if (Used != Found)
        isyslog("using only %d DVB device%s", Used, Used != 1 ? "s" : "");
     }
  else
     isyslog("no DVB device found");
  return Found > 0;
}

bool cDvbDevice::BondDevices(const char *Bondings)
{
  UnBondDevices();
  if (Bondings) {
     cSatCableNumbers SatCableNumbers(MAXDEVICES, Bondings);
     for (int i = 0; i < cDevice::NumDevices(); i++) {
         int d = SatCableNumbers.FirstDeviceIndex(i);
         if (d >= 0) {
            int ErrorDevice = 0;
            if (cDevice *Device1 = cDevice::GetDevice(i)) {
               if (cDevice *Device2 = cDevice::GetDevice(d)) {
                  if (cDvbDevice *DvbDevice1 = dynamic_cast<cDvbDevice *>(Device1)) {
                     if (cDvbDevice *DvbDevice2 = dynamic_cast<cDvbDevice *>(Device2)) {
                        if (!DvbDevice1->Bond(DvbDevice2))
                           return false; // Bond() has already logged the error
                        }
                     else
                        ErrorDevice = d + 1;
                     }
                  else
                     ErrorDevice = i + 1;
                  if (ErrorDevice) {
                     esyslog("ERROR: device '%d' in device bondings '%s' is not a cDvbDevice", ErrorDevice, Bondings);
                     return false;
                     }
                  }
               else
                  ErrorDevice = d + 1;
               }
            else
               ErrorDevice = i + 1;
            if (ErrorDevice) {
               esyslog("ERROR: unknown device '%d' in device bondings '%s'", ErrorDevice, Bondings);
               return false;
               }
            }
         }
     }
  return true;
}

void cDvbDevice::UnBondDevices(void)
{
  for (int i = 0; i < cDevice::NumDevices(); i++) {
      if (cDvbDevice *d = dynamic_cast<cDvbDevice *>(cDevice::GetDevice(i)))
         d->UnBond();
      }
}

bool cDvbDevice::Bond(cDvbDevice *Device)
{
  cMutexLock MutexLock(&bondMutex);
  if (!bondedDevice) {
     if (Device != this) {
        if ((ProvidesDeliverySystem(SYS_DVBS) || ProvidesDeliverySystem(SYS_DVBS2)) && (Device->ProvidesDeliverySystem(SYS_DVBS) || Device->ProvidesDeliverySystem(SYS_DVBS2))) {
           if (dvbTuner && Device->dvbTuner && dvbTuner->Bond(Device->dvbTuner)) {
              bondedDevice = Device->bondedDevice ? Device->bondedDevice : Device;
              Device->bondedDevice = this;
              dsyslog("device %d bonded with device %d", DeviceNumber() + 1, bondedDevice->DeviceNumber() + 1);
              return true;
              }
           }
        else
           esyslog("ERROR: can't bond device %d with device %d (only DVB-S(2) devices can be bonded)", DeviceNumber() + 1, Device->DeviceNumber() + 1);
        }
     else
        esyslog("ERROR: can't bond device %d with itself", DeviceNumber() + 1);
     }
  else
     esyslog("ERROR: device %d already bonded with device %d, can't bond with device %d", DeviceNumber() + 1, bondedDevice->DeviceNumber() + 1, Device->DeviceNumber() + 1);
  return false;
}

void cDvbDevice::UnBond(void)
{
  cMutexLock MutexLock(&bondMutex);
  if (cDvbDevice *d = bondedDevice) {
     if (dvbTuner)
        dvbTuner->UnBond();
     dsyslog("device %d unbonded from device %d", DeviceNumber() + 1, bondedDevice->DeviceNumber() + 1);
     while (d->bondedDevice != this)
           d = d->bondedDevice;
     if (d == bondedDevice)
        d->bondedDevice = NULL;
     else
        d->bondedDevice = bondedDevice;
     bondedDevice = NULL;
     }
}

bool cDvbDevice::BondingOk(const cChannel *Channel, bool ConsiderOccupied) const
{
  cMutexLock MutexLock(&bondMutex);
  if (bondedDevice || Positioner())
     return dvbTuner && dvbTuner->BondingOk(Channel, ConsiderOccupied);
  return true;
}

bool cDvbDevice::HasCi(void)
{
  return ciAdapter;
}

bool cDvbDevice::SetPid(cPidHandle *Handle, int Type, bool On)
{
  if (Handle->pid) {
     dmx_pes_filter_params pesFilterParams;
     memset(&pesFilterParams, 0, sizeof(pesFilterParams));
     if (On) {
        if (Handle->handle < 0) {
           Handle->handle = DvbOpen(DEV_DVB_DEMUX, adapter, frontend, O_RDWR | O_NONBLOCK, true);
           if (Handle->handle < 0) {
              LOG_ERROR;
              return false;
              }
           }
        pesFilterParams.pid     = Handle->pid;
        pesFilterParams.input   = DMX_IN_FRONTEND;
        pesFilterParams.output  = DMX_OUT_TS_TAP;
        pesFilterParams.pes_type= DMX_PES_OTHER;
        pesFilterParams.flags   = DMX_IMMEDIATE_START;
        if (ioctl(Handle->handle, DMX_SET_PES_FILTER, &pesFilterParams) < 0) {
           LOG_ERROR;
           return false;
           }
        }
     else if (!Handle->used) {
        CHECK(ioctl(Handle->handle, DMX_STOP));
        if (Type <= ptTeletext) {
           pesFilterParams.pid     = 0x1FFF;
           pesFilterParams.input   = DMX_IN_FRONTEND;
           pesFilterParams.output  = DMX_OUT_DECODER;
           pesFilterParams.pes_type= DMX_PES_OTHER;
           pesFilterParams.flags   = DMX_IMMEDIATE_START;
           CHECK(ioctl(Handle->handle, DMX_SET_PES_FILTER, &pesFilterParams));
           }
        close(Handle->handle);
        Handle->handle = -1;
        }
     }
  return true;
}

#define RB_NUM_SECTIONS 8 // default: 2 sections = 8192 bytes

int cDvbDevice::OpenFilter(u_short Pid, u_char Tid, u_char Mask)
{
  cString FileName = DvbName(DEV_DVB_DEMUX, adapter, frontend);
  int f = open(FileName, O_RDWR | O_NONBLOCK);
  if (f >= 0) {
     if (Pid == EITPID) { // increase ringbuffer size for EIT
        if (ioctl(f, DMX_SET_BUFFER_SIZE, MAX_SECTION_SIZE * RB_NUM_SECTIONS) < 0)
           dsyslog("OpenFilter (pid=%d, tid=%02X): ioctl DMX_SET_BUFFER_SIZE failed", Pid, Tid);
        }
     dmx_sct_filter_params sctFilterParams;
     memset(&sctFilterParams, 0, sizeof(sctFilterParams));
     sctFilterParams.pid = Pid;
     sctFilterParams.timeout = 0;
     sctFilterParams.flags = DMX_IMMEDIATE_START;
     sctFilterParams.filter.filter[0] = Tid;
     sctFilterParams.filter.mask[0] = Mask;
     if (ioctl(f, DMX_SET_FILTER, &sctFilterParams) >= 0)
        return f;
     else {
        esyslog("ERROR: can't set filter (pid=%d, tid=%02X, mask=%02X): %m", Pid, Tid, Mask);
        close(f);
        }
     }
  else
     esyslog("ERROR: can't open filter handle on '%s'", *FileName);
  return -1;
}

void cDvbDevice::CloseFilter(int Handle)
{
  close(Handle);
}

bool cDvbDevice::ProvidesDeliverySystem(int DeliverySystem) const
{
  return dvbTuner->ProvidesDeliverySystem(DeliverySystem);
}

bool cDvbDevice::ProvidesSource(int Source) const
{
  int type = Source & cSource::st_Mask;
  return type == cSource::stNone
      || type == cSource::stAtsc  && ProvidesDeliverySystem(SYS_ATSC)
      || type == cSource::stCable && (ProvidesDeliverySystem(SYS_DVBC_ANNEX_AC) || ProvidesDeliverySystem(SYS_DVBC_ANNEX_B))
      || type == cSource::stSat   && (ProvidesDeliverySystem(SYS_DVBS) || ProvidesDeliverySystem(SYS_DVBS2))
      || type == cSource::stTerr  && (ProvidesDeliverySystem(SYS_DVBT) || ProvidesDeliverySystem(SYS_DVBT2));
}

bool cDvbDevice::ProvidesTransponder(const cChannel *Channel) const
{
  if (!ProvidesSource(Channel->Source()))
     return false; // doesn't provide source
  if (!dvbTuner->ProvidesFrontend(Channel))
     return false; // requires modulation system which frontend doesn't provide
  cDvbTransponderParameters dtp(Channel->Parameters());
  if (!cSource::IsSat(Channel->Source()) ||
     (!Setup.DiSEqC || Diseqcs.Get(DeviceNumber() + 1, Channel->Source(), Channel->Frequency(), dtp.Polarization(), NULL)))
     return DeviceHooksProvidesTransponder(Channel);
  return false;
}

bool cDvbDevice::ProvidesChannel(const cChannel *Channel, int Priority, bool *NeedsDetachReceivers) const
{
  bool result = false;
  bool hasPriority = Priority == IDLEPRIORITY || Priority > this->Priority();
  bool needsDetachReceivers = false;
  needsDetachBondedReceivers = false;

  if (ProvidesTransponder(Channel)) {
     result = hasPriority;
     if (Priority > IDLEPRIORITY) {
        if (Receiving()) {
           if (dvbTuner->IsTunedTo(Channel)) {
              if (Channel->Vpid() && !HasPid(Channel->Vpid()) || Channel->Apid(0) && !HasPid(Channel->Apid(0)) || Channel->Dpid(0) && !HasPid(Channel->Dpid(0))) {
                 if (CamSlot() && Channel->Ca() >= CA_ENCRYPTED_MIN) {
                    if (CamSlot()->CanDecrypt(Channel))
                       result = true;
                    else
                       needsDetachReceivers = true;
                    }
                 else
                    result = true;
                 }
              else
                 result = true;
              }
           else
              needsDetachReceivers = Receiving();
           }
        if (result) {
           cMutexLock MutexLock(&bondMutex);
           if (!BondingOk(Channel)) {
              // This device is bonded, so we need to check the priorities of the others:
              for (cDvbDevice *d = bondedDevice; d && d != this; d = d->bondedDevice) {
                  if (d->Priority() >= Priority) {
                     result = false;
                     break;
                     }
                  needsDetachReceivers |= d->Receiving();
                  }
              needsDetachBondedReceivers = true;
              needsDetachReceivers |= Receiving();
              }
           }
        }
     }
  if (NeedsDetachReceivers)
     *NeedsDetachReceivers = needsDetachReceivers;
  return result;
}

bool cDvbDevice::ProvidesEIT(void) const
{
  return dvbTuner != NULL && DeviceHooksProvidesEIT();
}

int cDvbDevice::NumProvidedSystems(void) const
{
  return dvbTuner ? dvbTuner->NumProvidedSystems() : 0;
}

const cPositioner *cDvbDevice::Positioner(void) const
{
  return dvbTuner ? dvbTuner->Positioner() : NULL;
}

bool cDvbDevice::SignalStats(int &Valid, double *Strength, double *Cnr, double *BerPre, double *BerPost, double *Per, int *Status) const
{
  return dvbTuner ? dvbTuner->GetSignalStats(Valid, Strength, Cnr, BerPre, BerPost, Per, Status) : false;
}

int cDvbDevice::SignalStrength(void) const
{
  return dvbTuner ? dvbTuner->GetSignalStrength() : -1;
}

int cDvbDevice::SignalQuality(void) const
{
  return dvbTuner ? dvbTuner->GetSignalQuality() : -1;
}

const cChannel *cDvbDevice::GetCurrentlyTunedTransponder(void) const
{
  return dvbTuner ? dvbTuner->GetTransponder() : NULL;
}

bool cDvbDevice::IsTunedToTransponder(const cChannel *Channel) const
{
  return dvbTuner ? dvbTuner->IsTunedTo(Channel) : false;
}

bool cDvbDevice::MaySwitchTransponder(const cChannel *Channel) const
{
  return BondingOk(Channel, true) && cDevice::MaySwitchTransponder(Channel);
}

void cDvbDevice::SetPowerSaveMode(bool On)
{
  dvbTuner->SetPowerSaveMode(On);
}

bool cDvbDevice::SetChannelDevice(const cChannel *Channel, bool LiveView)
{
  if (dvbTuner->ProvidesFrontend(Channel, true)) {
     dvbTuner->SetChannel(Channel);
     return true;
     }
  return false;
}

bool cDvbDevice::HasLock(int TimeoutMs) const
{
  return dvbTuner ? dvbTuner->Locked(TimeoutMs) : false;
}

void cDvbDevice::SetTransferModeForDolbyDigital(int Mode)
{
  setTransferModeForDolbyDigital = Mode;
}

bool cDvbDevice::OpenDvr(void)
{
  CloseDvr();
  fd_dvr = DvbOpen(DEV_DVB_DVR, adapter, frontend, O_RDONLY | O_NONBLOCK, true);
  if (fd_dvr >= 0)
     tsBuffer = new cTSBuffer(fd_dvr, TSBUFFERSIZE, DeviceNumber() + 1);
  return fd_dvr >= 0;
}

void cDvbDevice::CloseDvr(void)
{
  if (fd_dvr >= 0) {
     delete tsBuffer;
     tsBuffer = NULL;
     close(fd_dvr);
     fd_dvr = -1;
     }
}

bool cDvbDevice::GetTSPacket(uchar *&Data)
{
  if (tsBuffer) {
     if (cCamSlot *cs = CamSlot()) {
        if (cs->WantsTsData()) {
           int Available;
           Data = tsBuffer->Get(&Available, checkTsBuffer);
           if (!Data)
              Available = 0;
           Data = cs->Decrypt(Data, Available);
           tsBuffer->Skip(Available);
           checkTsBuffer = Data != NULL;
           return true;
           }
        }
     Data = tsBuffer->Get();
     return true;
     }
  return false;
}

void cDvbDevice::DetachAllReceivers(void)
{
  cMutexLock MutexLock(&bondMutex);
  cDvbDevice *d = this;
  do {
     d->cDevice::DetachAllReceivers();
     d = d->bondedDevice;
     } while (d && d != this && needsDetachBondedReceivers);
  needsDetachBondedReceivers = false;
}

// --- cDvbDeviceProbe -------------------------------------------------------

cList<cDvbDeviceProbe> DvbDeviceProbes;

cDvbDeviceProbe::cDvbDeviceProbe(void)
{
  DvbDeviceProbes.Add(this);
}

cDvbDeviceProbe::~cDvbDeviceProbe()
{
  DvbDeviceProbes.Del(this, false);
}

uint32_t cDvbDeviceProbe::GetSubsystemId(int Adapter, int Frontend)
{
  uint32_t SubsystemId = 0;
  cString FileName = cString::sprintf("/dev/dvb/adapter%d/frontend%d", Adapter, Frontend);
  struct stat st;
  if (stat(FileName, &st) == 0) {
     cReadDir d("/sys/class/dvb");
     if (d.Ok()) {
        struct dirent *e;
        while ((e = d.Next()) != NULL) {
              if (strstr(e->d_name, "frontend")) {
                 FileName = cString::sprintf("/sys/class/dvb/%s/dev", e->d_name);
                 if (FILE *f = fopen(FileName, "r")) {
                    cReadLine ReadLine;
                    char *s = ReadLine.Read(f);
                    fclose(f);
                    unsigned Major;
                    unsigned Minor;
                    if (s && 2 == sscanf(s, "%u:%u", &Major, &Minor)) {
                       if (((Major << 8) | Minor) == st.st_rdev) {
                          FileName = cString::sprintf("/sys/class/dvb/%s/device/subsystem_vendor", e->d_name);
                          if ((f = fopen(FileName, "r")) != NULL) {
                             if (char *s = ReadLine.Read(f))
                                SubsystemId = strtoul(s, NULL, 0) << 16;
                             fclose(f);
                             }
                          else {
                             FileName = cString::sprintf("/sys/class/dvb/%s/device/idVendor", e->d_name);
                             if ((f = fopen(FileName, "r")) != NULL) {
                                if (char *s = ReadLine.Read(f))
                                   SubsystemId = strtoul(s, NULL, 16) << 16;
                                fclose(f);
                                }
                             }
                          FileName = cString::sprintf("/sys/class/dvb/%s/device/subsystem_device", e->d_name);
                          if ((f = fopen(FileName, "r")) != NULL) {
                             if (char *s = ReadLine.Read(f))
                                SubsystemId |= strtoul(s, NULL, 0);
                             fclose(f);
                             }
                          else {
                             FileName = cString::sprintf("/sys/class/dvb/%s/device/idProduct", e->d_name);
                             if ((f = fopen(FileName, "r")) != NULL) {
                                if (char *s = ReadLine.Read(f))
                                   SubsystemId |= strtoul(s, NULL, 16);
                                fclose(f);
                                }
                             }
                          break;
                          }
                       }
                    }
                 }
              }
        }
     }
  return SubsystemId;
}
