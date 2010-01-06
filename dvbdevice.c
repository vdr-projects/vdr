/*
 * dvbdevice.c: The DVB device tuner interface
 *
 * See the main source file 'vdr.c' for copyright information and
 * how to reach the author.
 *
 * $Id: dvbdevice.c 2.24 2010/01/04 14:06:24 kls Exp $
 */

#include "dvbdevice.h"
#include <errno.h>
#include <limits.h>
#include <linux/dvb/dmx.h>
#include <linux/dvb/frontend.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include "channels.h"
#include "diseqc.h"
#include "dvbci.h"

#define DVBS_TUNE_TIMEOUT  9000 //ms
#define DVBS_LOCK_TIMEOUT  2000 //ms
#define DVBC_TUNE_TIMEOUT  9000 //ms
#define DVBC_LOCK_TIMEOUT  2000 //ms
#define DVBT_TUNE_TIMEOUT  9000 //ms
#define DVBT_LOCK_TIMEOUT  2000 //ms

// --- cDvbTuner -------------------------------------------------------------

class cDvbTuner : public cThread {
private:
  enum eTunerStatus { tsIdle, tsSet, tsTuned, tsLocked };
  int fd_frontend;
  int adapter, frontend;
  int tuneTimeout;
  int lockTimeout;
  time_t lastTimeoutReport;
  fe_delivery_system frontendType;
  cChannel channel;
  const char *diseqcCommands;
  eTunerStatus tunerStatus;
  cMutex mutex;
  cCondVar locked;
  cCondVar newSet;
  bool GetFrontendStatus(fe_status_t &Status, int TimeoutMs = 0);
  bool SetFrontend(void);
  virtual void Action(void);
public:
  cDvbTuner(int Fd_Frontend, int Adapter, int Frontend, fe_delivery_system FrontendType);
  virtual ~cDvbTuner();
  bool IsTunedTo(const cChannel *Channel) const;
  void Set(const cChannel *Channel);
  bool Locked(int TimeoutMs = 0);
  };

cDvbTuner::cDvbTuner(int Fd_Frontend, int Adapter, int Frontend, fe_delivery_system FrontendType)
{
  fd_frontend = Fd_Frontend;
  adapter = Adapter;
  frontend = Frontend;
  frontendType = FrontendType;
  tuneTimeout = 0;
  lockTimeout = 0;
  lastTimeoutReport = 0;
  diseqcCommands = NULL;
  tunerStatus = tsIdle;
  if (frontendType == SYS_DVBS || frontendType == SYS_DVBS2)
     CHECK(ioctl(fd_frontend, FE_SET_VOLTAGE, SEC_VOLTAGE_13)); // must explicitly turn on LNB power
  SetDescription("tuner on frontend %d/%d", adapter, frontend);
  Start();
}

cDvbTuner::~cDvbTuner()
{
  tunerStatus = tsIdle;
  newSet.Broadcast();
  locked.Broadcast();
  Cancel(3);
}

bool cDvbTuner::IsTunedTo(const cChannel *Channel) const
{
  if (tunerStatus == tsIdle)
     return false; // not tuned to
  if (channel.Source() != Channel->Source() || channel.Transponder() != Channel->Transponder())
     return false; // sufficient mismatch
  char Type = **cSource::ToString(Channel->Source());
#define ST(s, p) if (strchr(s, Type)) if (channel.p() != Channel->p()) return false;
  // Polarization is already checked as part of the Transponder.
  ST("  T", Bandwidth);
  ST("CST", CoderateH);
  ST("  T", CoderateL);
  ST("  T", Guard);
  ST("CST", Inversion);
  ST("CST", Modulation);
  ST(" S ", RollOff);
  ST(" S ", System);
  ST("CS ", Srate);
  ST("  T", Transmission);
  ST("  T", Hierarchy);
  return true;
}

void cDvbTuner::Set(const cChannel *Channel)
{
  cMutexLock MutexLock(&mutex);
  if (!IsTunedTo(Channel))
     tunerStatus = tsSet;
  channel = *Channel;
  lastTimeoutReport = 0;
  newSet.Broadcast();
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

bool cDvbTuner::GetFrontendStatus(fe_status_t &Status, int TimeoutMs)
{
  if (TimeoutMs) {
     cPoller Poller(fd_frontend);
     if (Poller.Poll(TimeoutMs)) {
        dvb_frontend_event Event;
        while (ioctl(fd_frontend, FE_GET_EVENT, &Event) == 0)
              ; // just to clear the event queue - we'll read the actual status below
        }
     }
  while (1) {
        if (ioctl(fd_frontend, FE_READ_STATUS, &Status) != -1)
           return true;
        if (errno != EINTR)
           break;
        }
  return false;
}

static unsigned int FrequencyToHz(unsigned int f)
{
  while (f && f < 1000000)
        f *= 1000;
  return f;
}

bool cDvbTuner::SetFrontend(void)
{
#define MAXFRONTENDCMDS 16
#define SETCMD(c, d) { Frontend[CmdSeq.num].cmd = (c);\
                       Frontend[CmdSeq.num].u.data = (d);\
                       if (CmdSeq.num++ > MAXFRONTENDCMDS) {\
                          esyslog("ERROR: too many tuning commands on frontend %d/%d", adapter, frontend);\
                          return false;\
                          }\
                     }
  dtv_property Frontend[MAXFRONTENDCMDS];
  memset(&Frontend, 0, sizeof(Frontend));
  dtv_properties CmdSeq;
  memset(&CmdSeq, 0, sizeof(CmdSeq));
  CmdSeq.props = Frontend;
  SETCMD(DTV_CLEAR, 0);
  if (ioctl(fd_frontend, FE_SET_PROPERTY, &CmdSeq) < 0) {
     esyslog("ERROR: frontend %d/%d: %m", adapter, frontend);
     return false;
     }
  CmdSeq.num = 0;

  if (frontendType == SYS_DVBS || frontendType == SYS_DVBS2) {
     unsigned int frequency = channel.Frequency();
     if (Setup.DiSEqC) {
        cDiseqc *diseqc = Diseqcs.Get(channel.Source(), channel.Frequency(), channel.Polarization());
        if (diseqc) {
           if (diseqc->Commands() && (!diseqcCommands || strcmp(diseqcCommands, diseqc->Commands()) != 0)) {
              cDiseqc::eDiseqcActions da;
              for (char *CurrentAction = NULL; (da = diseqc->Execute(&CurrentAction)) != cDiseqc::daNone; ) {
                  switch (da) {
                    case cDiseqc::daNone:      break;
                    case cDiseqc::daToneOff:   CHECK(ioctl(fd_frontend, FE_SET_TONE, SEC_TONE_OFF)); break;
                    case cDiseqc::daToneOn:    CHECK(ioctl(fd_frontend, FE_SET_TONE, SEC_TONE_ON)); break;
                    case cDiseqc::daVoltage13: CHECK(ioctl(fd_frontend, FE_SET_VOLTAGE, SEC_VOLTAGE_13)); break;
                    case cDiseqc::daVoltage18: CHECK(ioctl(fd_frontend, FE_SET_VOLTAGE, SEC_VOLTAGE_18)); break;
                    case cDiseqc::daMiniA:     CHECK(ioctl(fd_frontend, FE_DISEQC_SEND_BURST, SEC_MINI_A)); break;
                    case cDiseqc::daMiniB:     CHECK(ioctl(fd_frontend, FE_DISEQC_SEND_BURST, SEC_MINI_B)); break;
                    case cDiseqc::daCodes: {
                         int n = 0;
                         uchar *codes = diseqc->Codes(n);
                         if (codes) {
                            struct dvb_diseqc_master_cmd cmd;
                            cmd.msg_len = min(n, int(sizeof(cmd.msg)));
                            memcpy(cmd.msg, codes, cmd.msg_len);
                            CHECK(ioctl(fd_frontend, FE_DISEQC_SEND_MASTER_CMD, &cmd));
                            }
                         }
                         break;
                    default: esyslog("ERROR: unknown diseqc command %d", da);
                    }
                  }
              diseqcCommands = diseqc->Commands();
              }
           frequency -= diseqc->Lof();
           }
        else {
           esyslog("ERROR: no DiSEqC parameters found for channel %d", channel.Number());
           return false;
           }
        }
     else {
        int tone = SEC_TONE_OFF;
        if (frequency < (unsigned int)Setup.LnbSLOF) {
           frequency -= Setup.LnbFrequLo;
           tone = SEC_TONE_OFF;
           }
        else {
           frequency -= Setup.LnbFrequHi;
           tone = SEC_TONE_ON;
           }
        int volt = (channel.Polarization() == 'v' || channel.Polarization() == 'V' || channel.Polarization() == 'r' || channel.Polarization() == 'R') ? SEC_VOLTAGE_13 : SEC_VOLTAGE_18;
        CHECK(ioctl(fd_frontend, FE_SET_VOLTAGE, volt));
        CHECK(ioctl(fd_frontend, FE_SET_TONE, tone));
        }
     frequency = abs(frequency); // Allow for C-band, where the frequency is less than the LOF

     // DVB-S/DVB-S2 (common parts)
     SETCMD(DTV_DELIVERY_SYSTEM, channel.System());
     SETCMD(DTV_FREQUENCY, frequency * 1000UL);
     SETCMD(DTV_MODULATION, channel.Modulation());
     SETCMD(DTV_SYMBOL_RATE, channel.Srate() * 1000UL);
     SETCMD(DTV_INNER_FEC, channel.CoderateH());
     SETCMD(DTV_INVERSION, channel.Inversion());
     if (channel.System() == SYS_DVBS2) {
        if (frontendType == SYS_DVBS2) {
           // DVB-S2
           SETCMD(DTV_PILOT, PILOT_AUTO);
           SETCMD(DTV_ROLLOFF, channel.RollOff());
           }
        else {
           esyslog("ERROR: frontend %d/%d doesn't provide DVB-S2", adapter, frontend);
           return false;
           }
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
     SETCMD(DTV_DELIVERY_SYSTEM, frontendType);
     SETCMD(DTV_FREQUENCY, FrequencyToHz(channel.Frequency()));
     SETCMD(DTV_INVERSION, channel.Inversion());
     SETCMD(DTV_SYMBOL_RATE, channel.Srate() * 1000UL);
     SETCMD(DTV_INNER_FEC, channel.CoderateH());
     SETCMD(DTV_MODULATION, channel.Modulation());

     tuneTimeout = DVBC_TUNE_TIMEOUT;
     lockTimeout = DVBC_LOCK_TIMEOUT;
     }
  else if (frontendType == SYS_DVBT) {
     // DVB-T
     SETCMD(DTV_DELIVERY_SYSTEM, frontendType);
     SETCMD(DTV_FREQUENCY, FrequencyToHz(channel.Frequency()));
     SETCMD(DTV_INVERSION, channel.Inversion());
     SETCMD(DTV_BANDWIDTH_HZ, channel.Bandwidth());
     SETCMD(DTV_CODE_RATE_HP, channel.CoderateH());
     SETCMD(DTV_CODE_RATE_LP, channel.CoderateL());
     SETCMD(DTV_MODULATION, channel.Modulation());
     SETCMD(DTV_TRANSMISSION_MODE, channel.Transmission());
     SETCMD(DTV_GUARD_INTERVAL, channel.Guard());
     SETCMD(DTV_HIERARCHY, channel.Hierarchy());

     tuneTimeout = DVBT_TUNE_TIMEOUT;
     lockTimeout = DVBT_LOCK_TIMEOUT;
     }
  else {
     esyslog("ERROR: attempt to set channel with unknown DVB frontend type");
     return false;
     }
  SETCMD(DTV_TUNE, 0);
  if (ioctl(fd_frontend, FE_SET_PROPERTY, &CmdSeq) < 0) {
     esyslog("ERROR: frontend %d/%d: %m", adapter, frontend);
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
        fe_status_t NewStatus;
        if (GetFrontendStatus(NewStatus, 10))
           Status = NewStatus;
        cMutexLock MutexLock(&mutex);
        switch (tunerStatus) {
          case tsIdle:
               break;
          case tsSet:
               tunerStatus = SetFrontend() ? tsTuned : tsIdle;
               Timer.Set(tuneTimeout);
               continue;
          case tsTuned:
               if (Timer.TimedOut()) {
                  tunerStatus = tsSet;
                  diseqcCommands = NULL;
                  if (time(NULL) - lastTimeoutReport > 60) { // let's not get too many of these
                     isyslog("frontend %d/%d timed out while tuning to channel %d, tp %d", adapter, frontend, channel.Number(), channel.Transponder());
                     lastTimeoutReport = time(NULL);
                     }
                  continue;
                  }
          case tsLocked:
               if (Status & FE_REINIT) {
                  tunerStatus = tsSet;
                  diseqcCommands = NULL;
                  isyslog("frontend %d/%d was reinitialized", adapter, frontend);
                  lastTimeoutReport = 0;
                  continue;
                  }
               else if (Status & FE_HAS_LOCK) {
                  if (LostLock) {
                     isyslog("frontend %d/%d regained lock on channel %d, tp %d", adapter, frontend, channel.Number(), channel.Transponder());
                     LostLock = false;
                     }
                  tunerStatus = tsLocked;
                  locked.Broadcast();
                  lastTimeoutReport = 0;
                  }
               else if (tunerStatus == tsLocked) {
                  LostLock = true;
                  isyslog("frontend %d/%d lost lock on channel %d, tp %d", adapter, frontend, channel.Number(), channel.Transponder());
                  tunerStatus = tsTuned;
                  Timer.Set(lockTimeout);
                  lastTimeoutReport = 0;
                  continue;
                  }
               break;
          default: esyslog("ERROR: unknown tuner status %d", tunerStatus);
          }

        if (tunerStatus != tsTuned)
           newSet.TimedWait(mutex, 1000);
        }
}

// --- cDvbDevice ------------------------------------------------------------

int cDvbDevice::setTransferModeForDolbyDigital = 1;

const char *DeliverySystems[] = {
  "UNDEFINED",
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
  "DMBTH",
  "CMMB",
  "DAB",
  NULL
  };

cDvbDevice::cDvbDevice(int Adapter, int Frontend)
{
  adapter = Adapter;
  frontend = Frontend;
  ciAdapter = NULL;
  dvbTuner = NULL;
  frontendType = SYS_UNDEFINED;
  numProvidedSystems = 0;

  // Devices that are present on all card types:

  int fd_frontend = DvbOpen(DEV_DVB_FRONTEND, adapter, frontend, O_RDWR | O_NONBLOCK);

  // Common Interface:

  fd_ca = DvbOpen(DEV_DVB_CA, adapter, frontend, O_RDWR);
  if (fd_ca >= 0)
     ciAdapter = cDvbCiAdapter::CreateCiAdapter(this, fd_ca);

  // The DVR device (will be opened and closed as needed):

  fd_dvr = -1;

  // We only check the devices that must be present - the others will be checked before accessing them://XXX

  if (fd_frontend >= 0) {
     if (ioctl(fd_frontend, FE_GET_INFO, &frontendInfo) >= 0) {
        switch (frontendInfo.type) {
          case FE_QPSK: frontendType = (frontendInfo.caps & FE_CAN_2G_MODULATION) ? SYS_DVBS2 : SYS_DVBS; break;
          case FE_OFDM: frontendType = SYS_DVBT; break;
          case FE_QAM:  frontendType = SYS_DVBC_ANNEX_AC; break;
          case FE_ATSC: frontendType = SYS_ATSC; break;
          default: esyslog("ERROR: unknown frontend type %d on frontend %d/%d", frontendInfo.type, adapter, frontend);
          }
        }
     else
        LOG_ERROR;
     if (frontendType != SYS_UNDEFINED) {
        numProvidedSystems++;
        if (frontendType == SYS_DVBS2)
           numProvidedSystems++;
        isyslog("frontend %d/%d provides %s (\"%s\")", adapter, frontend, DeliverySystems[frontendType], frontendInfo.name);
        dvbTuner = new cDvbTuner(fd_frontend, adapter, frontend, frontendType);
        }
     }
  else
     esyslog("ERROR: can't open DVB device %d/%d", adapter, frontend);

  StartSectionHandler();
}

cDvbDevice::~cDvbDevice()
{
  StopSectionHandler();
  delete dvbTuner;
  delete ciAdapter;
  // We're not explicitly closing any device files here, since this sometimes
  // caused segfaults. Besides, the program is about to terminate anyway...
}

cString cDvbDevice::DvbName(const char *Name, int Adapter, int Frontend)
{
  return cString::sprintf("%s%d/%s%d", DEV_DVB_ADAPTER, Adapter, Name, Frontend);
}

int cDvbDevice::DvbOpen(const char *Name, int Adapter, int Frontend, int Mode, bool ReportError)
{
  cString FileName = DvbName(Name, Adapter, Frontend);
  int fd = open(FileName, Mode);
  if (fd < 0 && ReportError)
     LOG_ERROR_STR(*FileName);
  return fd;
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

bool cDvbDevice::Initialize(void)
{
  int Checked = 0;
  int Found = 0;
  for (int Adapter = 0; ; Adapter++) {
      for (int Frontend = 0; ; Frontend++) {
          if (Exists(Adapter, Frontend)) {
             if (Checked++ < MAXDVBDEVICES) {
                if (UseDevice(NextCardIndex())) {
                   if (Probe(Adapter, Frontend))
                      Found++;
                   }
                else
                   NextCardIndex(1); // skips this one
                }
             }
          else if (Frontend == 0)
             goto LastAdapter;
          else
             goto NextAdapter;
          }
      NextAdapter: ;
      }
LastAdapter:
  NextCardIndex(MAXDVBDEVICES - Checked); // skips the rest
  if (Found > 0)
     isyslog("found %d DVB device%s", Found, Found > 1 ? "s" : "");
  else
     isyslog("no DVB device found");
  return Found > 0;
}

bool cDvbDevice::Ready(void)
{
  if (ciAdapter)
     return ciAdapter->Ready();
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

int cDvbDevice::OpenFilter(u_short Pid, u_char Tid, u_char Mask)
{
  cString FileName = DvbName(DEV_DVB_DEMUX, adapter, frontend);
  int f = open(FileName, O_RDWR | O_NONBLOCK);
  if (f >= 0) {
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

bool cDvbDevice::ProvidesSource(int Source) const
{
  int type = Source & cSource::st_Mask;
  return type == cSource::stNone
      || type == cSource::stCable && (frontendType == SYS_DVBC_ANNEX_AC || frontendType == SYS_DVBC_ANNEX_B)
      || type == cSource::stSat   && (frontendType == SYS_DVBS || frontendType == SYS_DVBS2)
      || type == cSource::stTerr  && (frontendType == SYS_DVBT);
}

bool cDvbDevice::ProvidesTransponder(const cChannel *Channel) const
{
  if (!ProvidesSource(Channel->Source()))
     return false; // doesn't provide source
  if (!cSource::IsSat(Channel->Source()))
     return true; // source is sufficient for non sat
  if (frontendType == SYS_DVBS && Channel->System() == SYS_DVBS2)
     return false; // requires modulation system which frontend doesn't provide
  return !Setup.DiSEqC || Diseqcs.Get(Channel->Source(), Channel->Frequency(), Channel->Polarization());
}

bool cDvbDevice::ProvidesChannel(const cChannel *Channel, int Priority, bool *NeedsDetachReceivers) const
{
  bool result = false;
  bool hasPriority = Priority < 0 || Priority > this->Priority();
  bool needsDetachReceivers = false;

  if (ProvidesTransponder(Channel)) {
     result = hasPriority;
     if (Priority >= 0 && Receiving(true)) {
        if (dvbTuner->IsTunedTo(Channel)) {
           if (Channel->Vpid() && !HasPid(Channel->Vpid()) || Channel->Apid(0) && !HasPid(Channel->Apid(0))) {
              if (CamSlot() && Channel->Ca() >= CA_ENCRYPTED_MIN) {
                 if (CamSlot()->CanDecrypt(Channel))
                    result = true;
                 else
                    needsDetachReceivers = true;
                 }
              else if (!IsPrimaryDevice())
                 result = true;
              else
                 result = Priority >= Setup.PrimaryLimit;
              }
           else
              result = !IsPrimaryDevice() || Priority >= Setup.PrimaryLimit;
           }
        else
           needsDetachReceivers = true;
        }
     }
  if (NeedsDetachReceivers)
     *NeedsDetachReceivers = needsDetachReceivers;
  return result;
}

int cDvbDevice::NumProvidedSystems(void) const
{
  return numProvidedSystems;
}

bool cDvbDevice::IsTunedToTransponder(const cChannel *Channel)
{
  return dvbTuner->IsTunedTo(Channel);
}

bool cDvbDevice::SetChannelDevice(const cChannel *Channel, bool LiveView)
{
  dvbTuner->Set(Channel);
  return true;
}

bool cDvbDevice::HasLock(int TimeoutMs)
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
     tsBuffer = new cTSBuffer(fd_dvr, MEGABYTE(2), CardIndex() + 1);
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
     Data = tsBuffer->Get();
     return true;
     }
  return false;
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
