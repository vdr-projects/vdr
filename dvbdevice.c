/*
 * dvbdevice.c: The DVB device interface
 *
 * See the main source file 'vdr.c' for copyright information and
 * how to reach the author.
 *
 * $Id: dvbdevice.c 1.136 2005/08/21 09:17:20 kls Exp $
 */

#include "dvbdevice.h"
#include <errno.h>
extern "C" {
#ifdef boolean
#define HAVE_BOOLEAN
#endif
#include <jpeglib.h>
#undef boolean
}
#include <limits.h>
#include <linux/videodev.h>
#include <linux/dvb/audio.h>
#include <linux/dvb/dmx.h>
#include <linux/dvb/frontend.h>
#include <linux/dvb/video.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include "channels.h"
#include "diseqc.h"
#include "dvbosd.h"
#include "eitscan.h"
#include "player.h"
#include "receiver.h"
#include "status.h"
#include "transfer.h"

#define DO_REC_AND_PLAY_ON_PRIMARY_DEVICE 1
#define DO_MULTIPLE_RECORDINGS 1

#define DEV_VIDEO         "/dev/video"
#define DEV_DVB_ADAPTER   "/dev/dvb/adapter"
#define DEV_DVB_OSD       "osd"
#define DEV_DVB_FRONTEND  "frontend"
#define DEV_DVB_DVR       "dvr"
#define DEV_DVB_DEMUX     "demux"
#define DEV_DVB_VIDEO     "video"
#define DEV_DVB_AUDIO     "audio"
#define DEV_DVB_CA        "ca"

class cDvbName {
private:
  char buffer[PATH_MAX];
public:
  cDvbName(const char *Name, int n) {
    snprintf(buffer, sizeof(buffer), "%s%d/%s%d", DEV_DVB_ADAPTER, n, Name, 0);
    }
  const char *operator*() { return buffer; }
  };

static int DvbOpen(const char *Name, int n, int Mode, bool ReportError = false)
{
  const char *FileName = *cDvbName(Name, n);
  int fd = open(FileName, Mode);
  if (fd < 0 && ReportError)
     LOG_ERROR_STR(FileName);
  return fd;
}

// --- cDvbTuner -------------------------------------------------------------

class cDvbTuner : public cThread {
private:
  enum eTunerStatus { tsIdle, tsSet, tsTuned, tsLocked, tsCam };
  int fd_frontend;
  int cardIndex;
  fe_type_t frontendType;
  cCiHandler *ciHandler;
  cChannel channel;
  const char *diseqcCommands;
  bool useCa;
  time_t startTime;
  eTunerStatus tunerStatus;
  cMutex mutex;
  cCondVar locked;
  cCondVar newSet;
  bool GetFrontendEvent(dvb_frontend_event &Event, int TimeoutMs = 0);
  bool SetFrontend(void);
  virtual void Action(void);
public:
  cDvbTuner(int Fd_Frontend, int CardIndex, fe_type_t FrontendType, cCiHandler *CiHandler);
  virtual ~cDvbTuner();
  bool IsTunedTo(const cChannel *Channel) const;
  void Set(const cChannel *Channel, bool Tune, bool UseCa);
  bool Locked(int TimeoutMs = 0);
  };

cDvbTuner::cDvbTuner(int Fd_Frontend, int CardIndex, fe_type_t FrontendType, cCiHandler *CiHandler)
{
  fd_frontend = Fd_Frontend;
  cardIndex = CardIndex;
  frontendType = FrontendType;
  ciHandler = CiHandler;
  diseqcCommands = NULL;
  useCa = false;
  tunerStatus = tsIdle;
  startTime = time(NULL);
  if (frontendType == FE_QPSK)
     CHECK(ioctl(fd_frontend, FE_SET_VOLTAGE, SEC_VOLTAGE_13)); // must explicitly turn on LNB power
  SetDescription("tuner on device %d", cardIndex + 1);
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
  return tunerStatus != tsIdle && channel.Source() == Channel->Source() && channel.Transponder() == Channel->Transponder();
}

void cDvbTuner::Set(const cChannel *Channel, bool Tune, bool UseCa)
{
  cMutexLock MutexLock(&mutex);
  if (Tune)
     tunerStatus = tsSet;
  else if (tunerStatus == tsCam)
     tunerStatus = tsLocked;
  useCa = UseCa;
  if (Channel->Ca() && tunerStatus != tsCam)
     startTime = time(NULL);
  channel = *Channel;
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

bool cDvbTuner::GetFrontendEvent(dvb_frontend_event &Event, int TimeoutMs)
{
  if (TimeoutMs) {
     struct pollfd pfd;
     pfd.fd = fd_frontend;
     pfd.events = POLLIN | POLLPRI;
     do {
        int stat = poll(&pfd, 1, TimeoutMs);
        if (stat == 1)
           break;
        if (stat < 0) {
           if (errno == EINTR)
              continue;
           esyslog("ERROR: frontend %d poll failed: %m", cardIndex);
           }
        return false;
        } while (0);
     }
  do {
     int stat = ioctl(fd_frontend, FE_GET_EVENT, &Event);
     if (stat == 0)
        return true;
     if (stat < 0) {
        if (errno == EINTR)
           continue;
        }
     } while (0);
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
  dvb_frontend_parameters Frontend;

  memset(&Frontend, 0, sizeof(Frontend));

  switch (frontendType) {
    case FE_QPSK: { // DVB-S

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
                                memcpy(cmd.msg, codes, min(n, int(sizeof(cmd.msg))));
                                cmd.msg_len = n;
                                CHECK(ioctl(fd_frontend, FE_DISEQC_SEND_MASTER_CMD, &cmd));
                                }
                             }
                             break;
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
         Frontend.frequency = frequency * 1000UL;
         Frontend.inversion = fe_spectral_inversion_t(channel.Inversion());
         Frontend.u.qpsk.symbol_rate = channel.Srate() * 1000UL;
         Frontend.u.qpsk.fec_inner = fe_code_rate_t(channel.CoderateH());
         }
         break;
    case FE_QAM: { // DVB-C

         // Frequency and symbol rate:

         Frontend.frequency = FrequencyToHz(channel.Frequency());
         Frontend.inversion = fe_spectral_inversion_t(channel.Inversion());
         Frontend.u.qam.symbol_rate = channel.Srate() * 1000UL;
         Frontend.u.qam.fec_inner = fe_code_rate_t(channel.CoderateH());
         Frontend.u.qam.modulation = fe_modulation_t(channel.Modulation());
         }
         break;
    case FE_OFDM: { // DVB-T

         // Frequency and OFDM paramaters:

         Frontend.frequency = FrequencyToHz(channel.Frequency());
         Frontend.inversion = fe_spectral_inversion_t(channel.Inversion());
         Frontend.u.ofdm.bandwidth = fe_bandwidth_t(channel.Bandwidth());
         Frontend.u.ofdm.code_rate_HP = fe_code_rate_t(channel.CoderateH());
         Frontend.u.ofdm.code_rate_LP = fe_code_rate_t(channel.CoderateL());
         Frontend.u.ofdm.constellation = fe_modulation_t(channel.Modulation());
         Frontend.u.ofdm.transmission_mode = fe_transmit_mode_t(channel.Transmission());
         Frontend.u.ofdm.guard_interval = fe_guard_interval_t(channel.Guard());
         Frontend.u.ofdm.hierarchy_information = fe_hierarchy_t(channel.Hierarchy());
         }
         break;
    default:
         esyslog("ERROR: attempt to set channel with unknown DVB frontend type");
         return false;
    }
  if (ioctl(fd_frontend, FE_SET_FRONTEND, &Frontend) < 0) {
     esyslog("ERROR: frontend %d: %m", cardIndex);
     return false;
     }
  return true;
}

void cDvbTuner::Action(void)
{
  dvb_frontend_event event;
  while (Running()) {
        bool hasEvent = GetFrontendEvent(event, 1);

        cMutexLock MutexLock(&mutex);
        switch (tunerStatus) {
          case tsIdle:
               break;
          case tsSet:
               if (hasEvent)
                  continue;
               tunerStatus = SetFrontend() ? tsTuned : tsIdle;
               continue;
          case tsTuned:
          case tsLocked:
          case tsCam:
               if (hasEvent) {
                  if (event.status & FE_REINIT) {
                     tunerStatus = tsSet;
                     esyslog("ERROR: frontend %d was reinitialized - re-tuning", cardIndex);
                     }
                  if (event.status & FE_HAS_LOCK) {
                     tunerStatus = tsLocked;
                     locked.Broadcast();
                     }
                  continue;
                  }
          }

        if (ciHandler) {
           if (ciHandler->Process() && useCa) {
              if (tunerStatus == tsLocked) {
                 for (int Slot = 0; Slot < ciHandler->NumSlots(); Slot++) {
                     cCiCaPmt CaPmt(channel.Source(), channel.Transponder(), channel.Sid(), ciHandler->GetCaSystemIds(Slot));
                     if (CaPmt.Valid()) {
                        CaPmt.AddPid(channel.Vpid(), 2);
                        CaPmt.AddPid(channel.Apid(0), 4);
                        CaPmt.AddPid(channel.Apid(1), 4);
                        CaPmt.AddPid(channel.Dpid(0), 0);
                        if (ciHandler->SetCaPmt(CaPmt, Slot)) {
                           tunerStatus = tsCam;
                           startTime = 0;
                           }
                        }
                     }
                 }
              }
           else if (tunerStatus > tsLocked)
              tunerStatus = tsLocked;
           }
        // in the beginning we loop more often to let the CAM connection start up fast
        if (tunerStatus != tsTuned)
           newSet.TimedWait(mutex, (ciHandler && (time(NULL) - startTime < 20)) ? 100 : 1000);
        }
}

// --- cDvbDevice ------------------------------------------------------------

int cDvbDevice::devVideoOffset = -1;
bool cDvbDevice::setTransferModeForDolbyDigital = true;

cDvbDevice::cDvbDevice(int n)
{
  dvbTuner = NULL;
  frontendType = fe_type_t(-1); // don't know how else to initialize this - there is no FE_UNKNOWN
  spuDecoder = NULL;
  digitalAudio = false;
  playMode = pmNone;

  // Devices that are present on all card types:

  int fd_frontend = DvbOpen(DEV_DVB_FRONTEND, n, O_RDWR | O_NONBLOCK);

  // Devices that are only present on cards with decoders:

  fd_osd      = DvbOpen(DEV_DVB_OSD,    n, O_RDWR);
  fd_video    = DvbOpen(DEV_DVB_VIDEO,  n, O_RDWR | O_NONBLOCK);
  fd_audio    = DvbOpen(DEV_DVB_AUDIO,  n, O_RDWR | O_NONBLOCK);
  fd_stc      = DvbOpen(DEV_DVB_DEMUX,  n, O_RDWR);

  // The DVR device (will be opened and closed as needed):

  fd_dvr = -1;

  // The offset of the /dev/video devices:

  if (devVideoOffset < 0) { // the first one checks this
     FILE *f = NULL;
     char buffer[PATH_MAX];
     for (int ofs = 0; ofs < 100; ofs++) {
         snprintf(buffer, sizeof(buffer), "/proc/video/dev/video%d", ofs);
         if ((f = fopen(buffer, "r")) != NULL) {
            if (fgets(buffer, sizeof(buffer), f)) {
               if (strstr(buffer, "DVB Board")) { // found the _first_ DVB card
                  devVideoOffset = ofs;
                  dsyslog("video device offset is %d", devVideoOffset);
                  break;
                  }
               }
            else
               break;
            fclose(f);
            }
         else
            break;
         }
     if (devVideoOffset < 0)
        devVideoOffset = 0;
     if (f)
        fclose(f);
     }
  devVideoIndex = (devVideoOffset >= 0 && HasDecoder()) ? devVideoOffset++ : -1;

  // Video format:

  SetVideoFormat(Setup.VideoFormat);

  // We only check the devices that must be present - the others will be checked before accessing them://XXX

  if (fd_frontend >= 0) {
     dvb_frontend_info feinfo;
     if (ioctl(fd_frontend, FE_GET_INFO, &feinfo) >= 0) {
        frontendType = feinfo.type;
        ciHandler = cCiHandler::CreateCiHandler(*cDvbName(DEV_DVB_CA, n));
        dvbTuner = new cDvbTuner(fd_frontend, CardIndex(), frontendType, ciHandler);
        }
     else
        LOG_ERROR;
     }
  else
     esyslog("ERROR: can't open DVB device %d", n);

  StartSectionHandler();
}

cDvbDevice::~cDvbDevice()
{
  delete spuDecoder;
  delete dvbTuner;
  // We're not explicitly closing any device files here, since this sometimes
  // caused segfaults. Besides, the program is about to terminate anyway...
}

bool cDvbDevice::Probe(const char *FileName)
{
  if (access(FileName, F_OK) == 0) {
     dsyslog("probing %s", FileName);
     int f = open(FileName, O_RDONLY);
     if (f >= 0) {
        close(f);
        return true;
        }
     else if (errno != ENODEV && errno != EINVAL)
        LOG_ERROR_STR(FileName);
     }
  else if (errno != ENOENT)
     LOG_ERROR_STR(FileName);
  return false;
}

bool cDvbDevice::Initialize(void)
{
  int found = 0;
  int i;
  for (i = 0; i < MAXDVBDEVICES; i++) {
      if (UseDevice(NextCardIndex())) {
         if (Probe(*cDvbName(DEV_DVB_FRONTEND, i))) {
            new cDvbDevice(i);
            found++;
            }
         else
            break;
         }
      else
         NextCardIndex(1); // skips this one
      }
  NextCardIndex(MAXDVBDEVICES - i); // skips the rest
  if (found > 0)
     isyslog("found %d video device%s", found, found > 1 ? "s" : "");
  else
     isyslog("no DVB device found");
  return found > 0;
}

void cDvbDevice::MakePrimaryDevice(bool On)
{
  if (HasDecoder())
     new cDvbOsdProvider(fd_osd);
}

bool cDvbDevice::HasDecoder(void) const
{
  return fd_video >= 0 && fd_audio >= 0;
}

bool cDvbDevice::Ready(void)
{
  if (ciHandler) {
     ciHandler->Process();
     return ciHandler->Ready();
     }
  return true;
}

int cDvbDevice::ProvidesCa(const cChannel *Channel) const
{
  if (Channel->Ca() >= 0x0100 && ciHandler) {
     unsigned short ids[MAXCAIDS + 1];
     for (int i = 0; i <= MAXCAIDS; i++) // '<=' copies the terminating 0!
         ids[i] = Channel->Ca(i);
     return ciHandler->ProvidesCa(ids);
     }
  return cDevice::ProvidesCa(Channel);
}

cSpuDecoder *cDvbDevice::GetSpuDecoder(void)
{
  if (!spuDecoder && IsPrimaryDevice())
     spuDecoder = new cDvbSpuDecoder();
  return spuDecoder;
}

bool cDvbDevice::GrabImage(const char *FileName, bool Jpeg, int Quality, int SizeX, int SizeY)
{
  if (devVideoIndex < 0)
     return false;
  char buffer[PATH_MAX];
  snprintf(buffer, sizeof(buffer), "%s%d", DEV_VIDEO, devVideoIndex);
  int videoDev = open(buffer, O_RDWR);
  if (videoDev < 0)
     LOG_ERROR_STR(buffer);
  if (videoDev >= 0) {
     int result = 0;
     struct video_mbuf mbuf;
     result |= ioctl(videoDev, VIDIOCGMBUF, &mbuf);
     if (result == 0) {
        int msize = mbuf.size;
        unsigned char *mem = (unsigned char *)mmap(0, msize, PROT_READ | PROT_WRITE, MAP_SHARED, videoDev, 0);
        if (mem && mem != (unsigned char *)-1) {
           // set up the size and RGB
           struct video_capability vc;
           result |= ioctl(videoDev, VIDIOCGCAP, &vc);
           struct video_mmap vm;
           vm.frame = 0;
           if ((SizeX > 0) && (SizeX <= vc.maxwidth) &&
               (SizeY > 0) && (SizeY <= vc.maxheight)) {
              vm.width = SizeX;
              vm.height = SizeY;
              }
           else {
              vm.width = vc.maxwidth;
              vm.height = vc.maxheight;
              }
           vm.format = VIDEO_PALETTE_RGB24;
           result |= ioctl(videoDev, VIDIOCMCAPTURE, &vm);
           result |= ioctl(videoDev, VIDIOCSYNC, &vm.frame);
           // make RGB out of BGR:
           int memsize = vm.width * vm.height;
           unsigned char *mem1 = mem;
           for (int i = 0; i < memsize; i++) {
               unsigned char tmp = mem1[2];
               mem1[2] = mem1[0];
               mem1[0] = tmp;
               mem1 += 3;
               }

           if (Quality < 0)
              Quality = 100;

           isyslog("grabbing to %s (%s %d %d %d)", FileName, Jpeg ? "JPEG" : "PNM", Quality, vm.width, vm.height);
           FILE *f = fopen(FileName, "wb");
           if (f) {
              if (Jpeg) {
                 // write JPEG file:
                 struct jpeg_compress_struct cinfo;
                 struct jpeg_error_mgr jerr;
                 cinfo.err = jpeg_std_error(&jerr);
                 jpeg_create_compress(&cinfo);
                 jpeg_stdio_dest(&cinfo, f);
                 cinfo.image_width = vm.width;
                 cinfo.image_height = vm.height;
                 cinfo.input_components = 3;
                 cinfo.in_color_space = JCS_RGB;

                 jpeg_set_defaults(&cinfo);
                 jpeg_set_quality(&cinfo, Quality, true);
                 jpeg_start_compress(&cinfo, true);

                 int rs = vm.width * 3;
                 JSAMPROW rp[vm.height];
                 for (int k = 0; k < vm.height; k++)
                     rp[k] = &mem[rs * k];
                 jpeg_write_scanlines(&cinfo, rp, vm.height);
                 jpeg_finish_compress(&cinfo);
                 jpeg_destroy_compress(&cinfo);
                 }
              else {
                 // write PNM file:
                 if (fprintf(f, "P6\n%d\n%d\n255\n", vm.width, vm.height) < 0 ||
                     fwrite(mem, vm.width * vm.height * 3, 1, f) != 1) {
                    LOG_ERROR_STR(FileName);
                    result |= 1;
                    }
                 }
              fclose(f);
              }
           else {
              LOG_ERROR_STR(FileName);
              result |= 1;
              }
           munmap(mem, msize);
           }
        else
           result |= 1;
        }
     close(videoDev);
     return result == 0;
     }
  return false;
}

void cDvbDevice::SetVideoDisplayFormat(eVideoDisplayFormat VideoDisplayFormat)
{
  cDevice::SetVideoDisplayFormat(VideoDisplayFormat);
  if (HasDecoder()) {
     if (Setup.VideoFormat) {
        CHECK(ioctl(fd_video, VIDEO_SET_DISPLAY_FORMAT, VIDEO_LETTER_BOX));
        }
     else {
        switch (VideoDisplayFormat) {
          case vdfPanAndScan:
               CHECK(ioctl(fd_video, VIDEO_SET_DISPLAY_FORMAT, VIDEO_PAN_SCAN));
               break;
          case vdfLetterBox:
               CHECK(ioctl(fd_video, VIDEO_SET_DISPLAY_FORMAT, VIDEO_LETTER_BOX));
               break;
          case vdfCenterCutOut:
               CHECK(ioctl(fd_video, VIDEO_SET_DISPLAY_FORMAT, VIDEO_CENTER_CUT_OUT));
               break;
          }
        }
     }
}

void cDvbDevice::SetVideoFormat(bool VideoFormat16_9)
{
  if (HasDecoder()) {
     CHECK(ioctl(fd_video, VIDEO_SET_FORMAT, VideoFormat16_9 ? VIDEO_FORMAT_16_9 : VIDEO_FORMAT_4_3));
     SetVideoDisplayFormat(eVideoDisplayFormat(Setup.VideoDisplayFormat));
     }
}

eVideoSystem cDvbDevice::GetVideoSystem(void)
{
  eVideoSystem VideoSystem = vsPAL;
  video_size_t vs;
  if (ioctl(fd_video, VIDEO_GET_SIZE, &vs) == 0) {
     if (vs.h == 480 || vs.h == 240)
        VideoSystem = vsNTSC;
     }
  else
     LOG_ERROR;
  return VideoSystem;
}

//                            ptAudio        ptVideo        ptPcr        ptTeletext        ptDolby        ptOther
dmx_pes_type_t PesTypes[] = { DMX_PES_AUDIO, DMX_PES_VIDEO, DMX_PES_PCR, DMX_PES_TELETEXT, DMX_PES_OTHER, DMX_PES_OTHER };

bool cDvbDevice::SetPid(cPidHandle *Handle, int Type, bool On)
{
  if (Handle->pid) {
     dmx_pes_filter_params pesFilterParams;
     memset(&pesFilterParams, 0, sizeof(pesFilterParams));
     if (On) {
        if (Handle->handle < 0) {
           Handle->handle = DvbOpen(DEV_DVB_DEMUX, CardIndex(), O_RDWR | O_NONBLOCK, true);
           if (Handle->handle < 0) {
              LOG_ERROR;
              return false;
              }
           }
        pesFilterParams.pid     = Handle->pid;
        pesFilterParams.input   = DMX_IN_FRONTEND;
        pesFilterParams.output  = (Type <= ptTeletext && Handle->used <= 1) ? DMX_OUT_DECODER : DMX_OUT_TS_TAP;
        pesFilterParams.pes_type= PesTypes[Type < ptOther ? Type : ptOther];
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
           pesFilterParams.pes_type= PesTypes[Type];
           pesFilterParams.flags   = DMX_IMMEDIATE_START;
           CHECK(ioctl(Handle->handle, DMX_SET_PES_FILTER, &pesFilterParams));
           if (PesTypes[Type] == DMX_PES_VIDEO) // let's only do this once
              SetPlayMode(pmNone); // necessary to switch a PID from DMX_PES_VIDEO/AUDIO to DMX_PES_OTHER
           }
        close(Handle->handle);
        Handle->handle = -1;
        }
     }
  return true;
}

int cDvbDevice::OpenFilter(u_short Pid, u_char Tid, u_char Mask)
{
  const char *FileName = *cDvbName(DEV_DVB_DEMUX, CardIndex());
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
     esyslog("ERROR: can't open filter handle on '%s'", FileName);
  return -1;
}

void cDvbDevice::TurnOffLiveMode(bool LiveView)
{
  if (LiveView) {
     // Avoid noise while switching:
     CHECK(ioctl(fd_audio, AUDIO_SET_MUTE, true));
     CHECK(ioctl(fd_video, VIDEO_SET_BLANK, true));
     CHECK(ioctl(fd_audio, AUDIO_CLEAR_BUFFER));
     CHECK(ioctl(fd_video, VIDEO_CLEAR_BUFFER));
     }

  // Turn off live PIDs:

  DetachAll(pidHandles[ptAudio].pid);
  DetachAll(pidHandles[ptVideo].pid);
  DetachAll(pidHandles[ptPcr].pid);
  DetachAll(pidHandles[ptTeletext].pid);
  DelPid(pidHandles[ptAudio].pid);
  DelPid(pidHandles[ptVideo].pid);
  DelPid(pidHandles[ptPcr].pid, ptPcr);
  DelPid(pidHandles[ptTeletext].pid);
  DelPid(pidHandles[ptDolby].pid);
}

bool cDvbDevice::ProvidesSource(int Source) const
{
  int type = Source & cSource::st_Mask;
  return type == cSource::stNone
      || type == cSource::stCable && frontendType == FE_QAM
      || type == cSource::stSat   && frontendType == FE_QPSK
      || type == cSource::stTerr  && frontendType == FE_OFDM;
}

bool cDvbDevice::ProvidesTransponder(const cChannel *Channel) const
{
  return ProvidesSource(Channel->Source()) && (!cSource::IsSat(Channel->Source()) || !Setup.DiSEqC || Diseqcs.Get(Channel->Source(), Channel->Frequency(), Channel->Polarization()));
}

bool cDvbDevice::ProvidesChannel(const cChannel *Channel, int Priority, bool *NeedsDetachReceivers) const
{
  bool result = false;
  bool hasPriority = Priority < 0 || Priority > this->Priority();
  bool needsDetachReceivers = false;

  if (ProvidesSource(Channel->Source()) && ProvidesCa(Channel)) {
     result = hasPriority;
     if (Priority >= 0 && Receiving(true)) {
        if (dvbTuner->IsTunedTo(Channel)) {
           if (Channel->Vpid() && !HasPid(Channel->Vpid()) || Channel->Apid(0) && !HasPid(Channel->Apid(0))) {
#ifdef DO_MULTIPLE_RECORDINGS
              if (Ca() > CACONFBASE || Channel->Ca() > CACONFBASE)
                 needsDetachReceivers = Ca() != Channel->Ca();
              else if (!IsPrimaryDevice())
                 result = true;
#ifdef DO_REC_AND_PLAY_ON_PRIMARY_DEVICE
              else
                 result = Priority >= Setup.PrimaryLimit;
#endif
#endif
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

bool cDvbDevice::SetChannelDevice(const cChannel *Channel, bool LiveView)
{
  bool DoTune = !dvbTuner->IsTunedTo(Channel);

  bool TurnOffLivePIDs = HasDecoder()
                         && (DoTune
                            || !IsPrimaryDevice()
                            || LiveView // for a new live view the old PIDs need to be turned off
                            || pidHandles[ptVideo].pid == Channel->Vpid() // for recording the PIDs must be shifted from DMX_PES_AUDIO/VIDEO to DMX_PES_OTHER
                            );

  bool StartTransferMode = IsPrimaryDevice() && !DoTune
                           && (LiveView && HasPid(Channel->Vpid() ? Channel->Vpid() : Channel->Apid(0)) && (pidHandles[ptVideo].pid != Channel->Vpid() || pidHandles[ptAudio].pid != Channel->Apid(0))// the PID is already set as DMX_PES_OTHER
                              || !LiveView && (pidHandles[ptVideo].pid == Channel->Vpid() || pidHandles[ptAudio].pid == Channel->Apid(0)) // a recording is going to shift the PIDs from DMX_PES_AUDIO/VIDEO to DMX_PES_OTHER
                              );

  bool TurnOnLivePIDs = HasDecoder() && !StartTransferMode && LiveView;

#ifndef DO_MULTIPLE_RECORDINGS
  TurnOffLivePIDs = TurnOnLivePIDs = true;
  StartTransferMode = false;
#endif

  // Turn off live PIDs if necessary:

  if (TurnOffLivePIDs)
     TurnOffLiveMode(LiveView);

  // Set the tuner:

  dvbTuner->Set(Channel, DoTune, !EITScanner.UsesDevice(this)); //XXX 1.3: this is an ugly hack - find a cleaner solution//XXX

  // If this channel switch was requested by the EITScanner we don't wait for
  // a lock and don't set any live PIDs (the EITScanner will wait for the lock
  // by itself before setting any filters):

  if (EITScanner.UsesDevice(this))
     return true;

  // PID settings:

  if (TurnOnLivePIDs) {
     if (!(AddPid(Channel->Ppid(), ptPcr) && AddPid(Channel->Vpid(), ptVideo) && AddPid(Channel->Apid(0), ptAudio))) {
        esyslog("ERROR: failed to set PIDs for channel %d on device %d", Channel->Number(), CardIndex() + 1);
        return false;
        }
     if (IsPrimaryDevice())
        AddPid(Channel->Tpid(), ptTeletext);
     CHECK(ioctl(fd_audio, AUDIO_SET_MUTE, true)); // actually one would expect 'false' here, but according to Marco Schlüßler <marco@lordzodiac.de> this works
                                                   // to avoid missing audio after replaying a DVD; with 'false' there is an audio disturbance when switching
                                                   // between two channels on the same transponder on DVB-S
     CHECK(ioctl(fd_audio, AUDIO_SET_AV_SYNC, true));
     }
  else if (StartTransferMode)
     cControl::Launch(new cTransferControl(this, Channel->Vpid(), Channel->Apids(), Channel->Dpids(), Channel->Spids()));

  return true;
}

bool cDvbDevice::HasLock(int TimeoutMs)
{
  return dvbTuner ? dvbTuner->Locked(TimeoutMs) : false;
}

int cDvbDevice::GetAudioChannelDevice(void)
{
  if (HasDecoder()) {
     audio_status_t as;
     CHECK(ioctl(fd_audio, AUDIO_GET_STATUS, &as));
     return as.channel_select;
     }
  return 0;
}

void cDvbDevice::SetAudioChannelDevice(int AudioChannel)
{
  if (HasDecoder())
     CHECK(ioctl(fd_audio, AUDIO_CHANNEL_SELECT, AudioChannel));
}

void cDvbDevice::SetVolumeDevice(int Volume)
{
  if (HasDecoder()) {
     if (digitalAudio)
        Volume = 0;
     audio_mixer_t am;
     // conversion for linear volume response:
     am.volume_left = am.volume_right = 2 * Volume - Volume * Volume / 255;
     CHECK(ioctl(fd_audio, AUDIO_SET_MIXER, &am));
     }
}

void cDvbDevice::SetDigitalAudioDevice(bool On)
{
  if (digitalAudio != On) {
     if (digitalAudio)
        cCondWait::SleepMs(1000); // Wait until any leftover digital data has been flushed
     digitalAudio = On;
     SetVolumeDevice(On || IsMute() ? 0 : CurrentVolume());
     }
}

void cDvbDevice::SetTransferModeForDolbyDigital(bool On)
{
  setTransferModeForDolbyDigital = On;
}

void cDvbDevice::SetAudioTrackDevice(eTrackType Type)
{
  const tTrackId *TrackId = GetTrack(Type);
  if (TrackId && TrackId->id) {
     if (IS_AUDIO_TRACK(Type)) {
        if (pidHandles[ptAudio].pid && pidHandles[ptAudio].pid != TrackId->id) {
           DetachAll(pidHandles[ptAudio].pid);
           pidHandles[ptAudio].pid = TrackId->id;
           SetPid(&pidHandles[ptAudio], ptAudio, true);
           }
        }
     else if (IS_DOLBY_TRACK(Type)) {
        if (!setTransferModeForDolbyDigital)
           return;
        // Currently this works only in Transfer Mode
        cChannel *Channel = Channels.GetByNumber(CurrentChannel());
        if (Channel)
           SetChannelDevice(Channel, false); // this implicitly starts Transfer Mode
        }
     }
}

bool cDvbDevice::CanReplay(void) const
{
#ifndef DO_REC_AND_PLAY_ON_PRIMARY_DEVICE
  if (Receiving())
     return false;
#endif
  return cDevice::CanReplay();
}

bool cDvbDevice::SetPlayMode(ePlayMode PlayMode)
{
  if (PlayMode != pmExtern_THIS_SHOULD_BE_AVOIDED && fd_video < 0 && fd_audio < 0) {
     // reopen the devices
     fd_video = DvbOpen(DEV_DVB_VIDEO,  CardIndex(), O_RDWR | O_NONBLOCK);
     fd_audio = DvbOpen(DEV_DVB_AUDIO,  CardIndex(), O_RDWR | O_NONBLOCK);
     SetVideoFormat(Setup.VideoFormat);
     }

  switch (PlayMode) {
    case pmNone:
         // special handling to return from PCM replay:
         CHECK(ioctl(fd_video, VIDEO_SET_BLANK, true));
         CHECK(ioctl(fd_video, VIDEO_SELECT_SOURCE, VIDEO_SOURCE_MEMORY));
         CHECK(ioctl(fd_video, VIDEO_PLAY));

         CHECK(ioctl(fd_video, VIDEO_STOP, true));
         CHECK(ioctl(fd_audio, AUDIO_STOP, true));
         CHECK(ioctl(fd_video, VIDEO_CLEAR_BUFFER));
         CHECK(ioctl(fd_audio, AUDIO_CLEAR_BUFFER));
         CHECK(ioctl(fd_video, VIDEO_SELECT_SOURCE, VIDEO_SOURCE_DEMUX));
         CHECK(ioctl(fd_audio, AUDIO_SELECT_SOURCE, AUDIO_SOURCE_DEMUX));
         CHECK(ioctl(fd_audio, AUDIO_SET_AV_SYNC, true));
         CHECK(ioctl(fd_audio, AUDIO_SET_MUTE, false));
         break;
    case pmAudioVideo:
    case pmAudioOnlyBlack:
         if (playMode == pmNone)
            TurnOffLiveMode(true);
         CHECK(ioctl(fd_video, VIDEO_SET_BLANK, true));
         CHECK(ioctl(fd_audio, AUDIO_SELECT_SOURCE, AUDIO_SOURCE_MEMORY));
         CHECK(ioctl(fd_audio, AUDIO_SET_AV_SYNC, PlayMode == pmAudioVideo));
         CHECK(ioctl(fd_audio, AUDIO_PLAY));
         CHECK(ioctl(fd_video, VIDEO_SELECT_SOURCE, VIDEO_SOURCE_MEMORY));
         CHECK(ioctl(fd_video, VIDEO_PLAY));
         break;
    case pmAudioOnly:
         CHECK(ioctl(fd_video, VIDEO_SET_BLANK, true));
         CHECK(ioctl(fd_audio, AUDIO_STOP, true));
         CHECK(ioctl(fd_audio, AUDIO_CLEAR_BUFFER));
         CHECK(ioctl(fd_audio, AUDIO_SELECT_SOURCE, AUDIO_SOURCE_MEMORY));
         CHECK(ioctl(fd_audio, AUDIO_SET_AV_SYNC, false));
         CHECK(ioctl(fd_audio, AUDIO_PLAY));
         CHECK(ioctl(fd_video, VIDEO_SET_BLANK, false));
         break;
    case pmVideoOnly:
         CHECK(ioctl(fd_video, VIDEO_SET_BLANK, true));
         CHECK(ioctl(fd_video, VIDEO_STOP, true));
         CHECK(ioctl(fd_audio, AUDIO_SELECT_SOURCE, AUDIO_SOURCE_DEMUX));
         CHECK(ioctl(fd_audio, AUDIO_SET_AV_SYNC, false));
         CHECK(ioctl(fd_audio, AUDIO_PLAY));
         CHECK(ioctl(fd_video, VIDEO_CLEAR_BUFFER));
         CHECK(ioctl(fd_video, VIDEO_SELECT_SOURCE, VIDEO_SOURCE_MEMORY));
         CHECK(ioctl(fd_video, VIDEO_PLAY));
         break;
    case pmExtern_THIS_SHOULD_BE_AVOIDED:
         close(fd_video);
         close(fd_audio);
         fd_video = fd_audio = -1;
         break;
    }
  playMode = PlayMode;
  return true;
}

int64_t cDvbDevice::GetSTC(void)
{
  if (fd_stc >= 0) {
     struct dmx_stc stc;
     stc.num = 0;
     if (ioctl(fd_stc, DMX_GET_STC, &stc) == -1) {
        esyslog("ERROR: stc %d: %m", CardIndex() + 1);
        return -1;
        }
     return stc.stc / stc.base;
     }
  return -1;
}

void cDvbDevice::TrickSpeed(int Speed)
{
  if (fd_video >= 0)
     CHECK(ioctl(fd_video, VIDEO_SLOWMOTION, Speed));
}

void cDvbDevice::Clear(void)
{
  if (fd_video >= 0)
     CHECK(ioctl(fd_video, VIDEO_CLEAR_BUFFER));
  if (fd_audio >= 0)
     CHECK(ioctl(fd_audio, AUDIO_CLEAR_BUFFER));
  cDevice::Clear();
}

void cDvbDevice::Play(void)
{
  if (playMode == pmAudioOnly || playMode == pmAudioOnlyBlack) {
     if (fd_audio >= 0)
        CHECK(ioctl(fd_audio, AUDIO_CONTINUE));
     }
  else {
     if (fd_audio >= 0)
        CHECK(ioctl(fd_audio, AUDIO_SET_AV_SYNC, true));
     if (fd_video >= 0)
        CHECK(ioctl(fd_video, VIDEO_CONTINUE));
     }
  cDevice::Play();
}

void cDvbDevice::Freeze(void)
{
  if (playMode == pmAudioOnly || playMode == pmAudioOnlyBlack) {
     if (fd_audio >= 0)
        CHECK(ioctl(fd_audio, AUDIO_PAUSE));
     }
  else {
     if (fd_audio >= 0)
        CHECK(ioctl(fd_audio, AUDIO_SET_AV_SYNC, false));
     if (fd_video >= 0)
        CHECK(ioctl(fd_video, VIDEO_FREEZE));
     }
  cDevice::Freeze();
}

void cDvbDevice::Mute(void)
{
  if (fd_audio >= 0) {
     CHECK(ioctl(fd_audio, AUDIO_SET_AV_SYNC, false));
     CHECK(ioctl(fd_audio, AUDIO_SET_MUTE, true));
     }
  cDevice::Mute();
}

void cDvbDevice::StillPicture(const uchar *Data, int Length)
{
  if (Data[0] == 0x00 && Data[1] == 0x00 && Data[2] == 0x01 && (Data[3] & 0xF0) == 0xE0) {
     // PES data
     char *buf = MALLOC(char, Length);
     if (!buf)
        return;
     int i = 0;
     int blen = 0;
     while (i < Length - 6) {
           if (Data[i] == 0x00 && Data[i + 1] == 0x00 && Data[i + 2] == 0x01) {
              int len = Data[i + 4] * 256 + Data[i + 5];
              if ((Data[i + 3] & 0xF0) == 0xE0) { // video packet
                 // skip PES header
                 int offs = i + 6;
                 // skip header extension
                 if ((Data[i + 6] & 0xC0) == 0x80) {
                    // MPEG-2 PES header
                    if (Data[i + 8] >= Length)
                       break;
                    offs += 3;
                    offs += Data[i + 8];
                    len -= 3;
                    len -= Data[i + 8];
                    if (len < 0 || offs + len > Length)
                       break;
                    }
                 else {
                    // MPEG-1 PES header
                    while (offs < Length && len > 0 && Data[offs] == 0xFF) {
                          offs++;
                          len--;
                          }
                    if (offs <= Length - 2 && len >= 2 && (Data[offs] & 0xC0) == 0x40) {
                       offs += 2;
                       len -= 2;
                       }
                    if (offs <= Length - 5 && len >= 5 && (Data[offs] & 0xF0) == 0x20) {
                       offs += 5;
                       len -= 5;
                       }
                    else if (offs <= Length - 10 && len >= 10 && (Data[offs] & 0xF0) == 0x30) {
                       offs += 10;
                       len -= 10;
                       }
                    else if (offs < Length && len > 0) {
                       offs++;
                       len--;
                       }
                    }
                 if (blen + len > Length) // invalid PES length field
                    break;
                 memcpy(&buf[blen], &Data[offs], len);
                 i = offs + len;
                 blen += len;
                 }
              else if (Data[i + 3] >= 0xBD && Data[i + 3] <= 0xDF) // other PES packets
                 i += len + 6;
              else
                 i++;
              }
           else
              i++;
           }
     video_still_picture sp = { buf, blen };
     CHECK(ioctl(fd_video, VIDEO_STILLPICTURE, &sp));
     free(buf);
     }
  else {
     // non-PES data
     video_still_picture sp = { (char *)Data, Length };
     CHECK(ioctl(fd_video, VIDEO_STILLPICTURE, &sp));
     }
}

bool cDvbDevice::Poll(cPoller &Poller, int TimeoutMs)
{
  Poller.Add((playMode == pmAudioOnly || playMode == pmAudioOnlyBlack) ? fd_audio : fd_video, true);
  return Poller.Poll(TimeoutMs);
}

bool cDvbDevice::Flush(int TimeoutMs)
{
  //TODO actually this function should wait until all buffered data has been processed by the card, but how?
  return true;
}

int cDvbDevice::PlayVideo(const uchar *Data, int Length)
{
  return WriteAllOrNothing(fd_video, Data, Length, 1000, 10);
}

int cDvbDevice::PlayAudio(const uchar *Data, int Length)
{
  return WriteAllOrNothing(fd_audio, Data, Length, 1000, 10);
}

bool cDvbDevice::OpenDvr(void)
{
  CloseDvr();
  fd_dvr = DvbOpen(DEV_DVB_DVR, CardIndex(), O_RDONLY | O_NONBLOCK, true);
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
