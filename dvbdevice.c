/*
 * dvbdevice.c: The DVB device interface
 *
 * See the main source file 'vdr.c' for copyright information and
 * how to reach the author.
 *
 * $Id: dvbdevice.c 1.64 2003/09/06 13:19:33 kls Exp $
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

static const char *DvbName(const char *Name, int n)
{
  static char buffer[PATH_MAX];
  snprintf(buffer, sizeof(buffer), "%s%d/%s%d", DEV_DVB_ADAPTER, n, Name, 0);
  return buffer;
}

static int DvbOpen(const char *Name, int n, int Mode, bool ReportError = false)
{
  const char *FileName = DvbName(Name, n);
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
  bool active;
  bool useCa;
  time_t startTime;
  eTunerStatus tunerStatus;
  cMutex mutex;
  cCondVar newSet;
  bool SetFrontend(void);
  virtual void Action(void);
public:
  cDvbTuner(int Fd_Frontend, int CardIndex, fe_type_t FrontendType, cCiHandler *CiHandler);
  virtual ~cDvbTuner();
  bool IsTunedTo(const cChannel *Channel) const;
  void Set(const cChannel *Channel, bool Tune, bool UseCa);
  bool Locked(void) { return tunerStatus == tsLocked; }
  };

cDvbTuner::cDvbTuner(int Fd_Frontend, int CardIndex, fe_type_t FrontendType, cCiHandler *CiHandler)
{
  fd_frontend = Fd_Frontend;
  cardIndex = CardIndex;
  frontendType = FrontendType;
  ciHandler = CiHandler;
  diseqcCommands = NULL;
  active = false;
  useCa = false;
  tunerStatus = tsIdle;
  startTime = time(NULL);
  Start();
}

cDvbTuner::~cDvbTuner()
{
  active = false;
  tunerStatus = tsIdle;
  newSet.Broadcast();
  Cancel(3);
}

bool cDvbTuner::IsTunedTo(const cChannel *Channel) const
{
  return tunerStatus != tsIdle && channel.Source() == Channel->Source() && channel.Frequency() == Channel->Frequency();
}

void cDvbTuner::Set(const cChannel *Channel, bool Tune, bool UseCa)
{
  cMutexLock MutexLock(&mutex);
  bool CaChange = !(Channel->GetChannelID() == channel.GetChannelID());
  if (Tune)
     tunerStatus = tsSet;
  else if (tunerStatus == tsCam && CaChange)
     tunerStatus = tsTuned;
  useCa = UseCa;
  if (Channel->Ca() && CaChange)
     startTime = time(NULL);
  channel = *Channel;
  newSet.Broadcast();
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
            int volt = (channel.Polarization() == 'v' || channel.Polarization() == 'V') ? SEC_VOLTAGE_13 : SEC_VOLTAGE_18;
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
  dsyslog("tuner thread started on device %d (pid=%d)", cardIndex + 1, getpid());
  active = true;
  while (active) {
        cMutexLock MutexLock(&mutex);
        if (tunerStatus == tsSet)
           tunerStatus = SetFrontend() ? tsTuned : tsIdle;
        if (tunerStatus == tsTuned) {
           fe_status_t status = fe_status_t(0);
           CHECK(ioctl(fd_frontend, FE_READ_STATUS, &status));
           if (status & FE_HAS_LOCK)
              tunerStatus = tsLocked;
           }
        if (tunerStatus != tsIdle) {
           dvb_frontend_event event;
           if (ioctl(fd_frontend, FE_GET_EVENT, &event) == 0) {
              if (event.status & FE_REINIT) {
                 tunerStatus = tsSet;
                 esyslog("ERROR: frontend %d was reinitialized - re-tuning", cardIndex);
                 continue;
                 }
              }
           if (tunerStatus >= tsLocked) {
              if (ciHandler) {
                 if (ciHandler->Process() && useCa) {
                    if (tunerStatus != tsCam) {//XXX TODO update in case the CA descriptors have changed
                       for (int Slot = 0; Slot < ciHandler->NumSlots(); Slot++) {
                           uchar buffer[2048];
                           int length = cSIProcessor::GetCaDescriptors(channel.Source(), channel.Frequency(), channel.Sid(), ciHandler->GetCaSystemIds(Slot), sizeof(buffer), buffer);
                           if (length > 0) {
                              cCiCaPmt CaPmt(channel.Sid());
                              CaPmt.AddCaDescriptor(length, buffer);
                              if (channel.Vpid())
                                 CaPmt.AddPid(channel.Vpid());
                              if (channel.Apid1())
                                 CaPmt.AddPid(channel.Apid1());
                              if (channel.Apid2())
                                 CaPmt.AddPid(channel.Apid2());
                              if (channel.Dpid1())
                                 CaPmt.AddPid(channel.Dpid1());
                              if (ciHandler->SetCaPmt(CaPmt, Slot)) {
                                 tunerStatus = tsCam;
                                 startTime = 0;
                                 }
                              }
                           }
                       }
                    }
                 else
                    tunerStatus = tsLocked;
                 }
              }
           }
        // in the beginning we loop more often to let the CAM connection start up fast
        newSet.TimedWait(mutex, (ciHandler && (time(NULL) - startTime < 20)) ? 100 : 1000);
        }
  dsyslog("tuner thread ended on device %d (pid=%d)", cardIndex + 1, getpid());
}

// --- cDvbDevice ------------------------------------------------------------

cDvbDevice::cDvbDevice(int n)
{
  dvbTuner = NULL;
  frontendType = fe_type_t(-1); // don't know how else to initialize this - there is no FE_UNKNOWN
  siProcessor = NULL;
  spuDecoder = NULL;
  playMode = pmNone;

  // Devices that are present on all card types:

  int fd_frontend = DvbOpen(DEV_DVB_FRONTEND, n, O_RDWR | O_NONBLOCK);

  // Devices that are only present on cards with decoders:

  fd_osd      = DvbOpen(DEV_DVB_OSD,    n, O_RDWR);
  fd_video    = DvbOpen(DEV_DVB_VIDEO,  n, O_RDWR | O_NONBLOCK);
  fd_audio    = DvbOpen(DEV_DVB_AUDIO,  n, O_RDWR | O_NONBLOCK);

  // The DVR device (will be opened and closed as needed):

  fd_dvr = -1;

  // Video format:

  SetVideoFormat(Setup.VideoFormat ? VIDEO_FORMAT_16_9 : VIDEO_FORMAT_4_3);

  // We only check the devices that must be present - the others will be checked before accessing them://XXX

  if (fd_frontend >= 0) {
     dvb_frontend_info feinfo;
     siProcessor = new cSIProcessor(DvbName(DEV_DVB_DEMUX, n));
     if (ioctl(fd_frontend, FE_GET_INFO, &feinfo) >= 0) {
        frontendType = feinfo.type;
        ciHandler = cCiHandler::CreateCiHandler(DvbName(DEV_DVB_CA, n));
        dvbTuner = new cDvbTuner(fd_frontend, CardIndex(), frontendType, ciHandler);
        }
     else
        LOG_ERROR;
     }
  else
     esyslog("ERROR: can't open DVB device %d", n);

  aPid1 = aPid2 = 0;
}

cDvbDevice::~cDvbDevice()
{
  delete spuDecoder;
  delete siProcessor;
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
         if (Probe(DvbName(DEV_DVB_FRONTEND, i))) {
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
     cDvbOsd::SetDvbDevice(On ? this : NULL);
}

bool cDvbDevice::HasDecoder(void) const
{
  return fd_video >= 0 && fd_audio >= 0;
}

cOsdBase *cDvbDevice::NewOsd(int x, int y)
{
  return new cDvbOsd(x, y);
}

cSpuDecoder *cDvbDevice::GetSpuDecoder(void)
{
  if (!spuDecoder && IsPrimaryDevice())
     spuDecoder = new cDvbSpuDecoder();
  return spuDecoder;
}

bool cDvbDevice::GrabImage(const char *FileName, bool Jpeg, int Quality, int SizeX, int SizeY)
{
  char buffer[PATH_MAX];
  snprintf(buffer, sizeof(buffer), "%s%d", DEV_VIDEO, CardIndex());
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
              Quality = 255; //XXX is this 'best'???

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
                     fwrite(mem, vm.width * vm.height * 3, 1, f) < 0) {
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

void cDvbDevice::SetVideoFormat(bool VideoFormat16_9)
{
  if (HasDecoder())
     CHECK(ioctl(fd_video, VIDEO_SET_FORMAT, VideoFormat16_9 ? VIDEO_FORMAT_16_9 : VIDEO_FORMAT_4_3));
}

eVideoSystem cDvbDevice::GetVideoSystem(void)
{
  eVideoSystem VideoSytem = vsPAL;
  video_size_t vs;
  if (ioctl(fd_video, VIDEO_GET_SIZE, &vs) == 0) {
     if (vs.h == 480 || vs.h == 240)
        VideoSytem = vsNTSC;
     }
  else
     LOG_ERROR;
  return VideoSytem;
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
           if (Handle->handle < 0)
              return false;
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

void cDvbDevice::TurnOffLiveMode(void)
{
  // Avoid noise while switching:

  CHECK(ioctl(fd_audio, AUDIO_SET_MUTE, true));
  CHECK(ioctl(fd_video, VIDEO_SET_BLANK, true));
  CHECK(ioctl(fd_audio, AUDIO_CLEAR_BUFFER));
  CHECK(ioctl(fd_video, VIDEO_CLEAR_BUFFER));

  // Turn off live PIDs:

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
  return true;
}

bool cDvbDevice::ProvidesChannel(const cChannel *Channel, int Priority, bool *NeedsDetachReceivers) const
{
  bool result = false;
  bool hasPriority = Priority < 0 || Priority > this->Priority();
  bool needsDetachReceivers = false;

  if (ProvidesSource(Channel->Source()) && ProvidesCa(Channel->Ca())) {
     result = hasPriority;
     if (Receiving()) {
        if (dvbTuner->IsTunedTo(Channel)) {
           if (!HasPid(Channel->Vpid())) {
#ifdef DO_MULTIPLE_RECORDINGS
              if (Channel->Ca() > CACONFBASE)
                 needsDetachReceivers = !ciHandler // only LL-firmware can do non-live CA channels
                                        || Ca() != Channel->Ca();
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
  bool IsEncrypted = Channel->Ca() > CACONFBASE && !ciHandler; // only LL-firmware can do non-live CA channels

  bool DoTune = !dvbTuner->IsTunedTo(Channel);

  bool TurnOffLivePIDs = HasDecoder()
                         && (DoTune
                            || IsEncrypted && pidHandles[ptVideo].pid != Channel->Vpid() // CA channels can only be decrypted in "live" mode
                            || !IsPrimaryDevice()
                            || LiveView // for a new live view the old PIDs need to be turned off
                            || pidHandles[ptVideo].pid == Channel->Vpid() // for recording the PIDs must be shifted from DMX_PES_AUDIO/VIDEO to DMX_PES_OTHER
                            );

  bool StartTransferMode = IsPrimaryDevice() && !IsEncrypted && !DoTune
                           && (LiveView && HasPid(Channel->Vpid()) && pidHandles[ptVideo].pid != Channel->Vpid() // the PID is already set as DMX_PES_OTHER
                              || !LiveView && pidHandles[ptVideo].pid == Channel->Vpid() // a recording is going to shift the PIDs from DMX_PES_AUDIO/VIDEO to DMX_PES_OTHER
                              );

  bool TurnOnLivePIDs = HasDecoder() && !StartTransferMode
                        && (IsEncrypted // CA channels can only be decrypted in "live" mode
                           || LiveView
                           );

#ifndef DO_MULTIPLE_RECORDINGS
  TurnOffLivePIDs = TurnOnLivePIDs = true;
  StartTransferMode = false;
#endif

  // XXX 1.3: use the same mechanism as below (!EITScanner.UsesDevice(this))
  if (EITScanner.Active()) {
     StartTransferMode = false;
     TurnOnLivePIDs = false;
     }

  // Stop SI filtering:

  if (siProcessor) {
     siProcessor->SetCurrentTransponder(0, 0);
     siProcessor->SetStatus(false);
     }

  // Turn off live PIDs if necessary:

  if (TurnOffLivePIDs)
     TurnOffLiveMode();

  dvbTuner->Set(Channel, DoTune, !EITScanner.UsesDevice(this)); //XXX 1.3: this is an ugly hack - find a cleaner solution

  // PID settings:

  if (TurnOnLivePIDs) {
     aPid1 = Channel->Apid1();
     aPid2 = Channel->Apid2();
     int pPid = Channel->Ppid() ? Channel->Ppid() : Channel->Vpid();
     if (!(AddPid(pPid, ptPcr) && AddPid(Channel->Apid1(), ptAudio) && AddPid(Channel->Vpid(), ptVideo))) {//XXX+ dolby dpid1!!! (if audio plugins are attached)
        esyslog("ERROR: failed to set PIDs for channel %d on device %d", Channel->Number(), CardIndex() + 1);
        return false;
        }
     if (IsPrimaryDevice())
        AddPid(Channel->Tpid(), ptTeletext);
     CHECK(ioctl(fd_audio, AUDIO_SET_AV_SYNC, true));
     }
  else if (StartTransferMode)
     cControl::Launch(new cTransferControl(this, Channel->Vpid(), Channel->Apid1(), Channel->Apid2(), Channel->Dpid1(), Channel->Dpid2()));

  // Start SI filtering:

  if (siProcessor) {
     siProcessor->SetCurrentTransponder(Channel->Source(), Channel->Frequency());
     siProcessor->SetStatus(true);
     }

  return true;
}

void cDvbDevice::SetVolumeDevice(int Volume)
{
  if (HasDecoder()) {
     audio_mixer_t am;
     am.volume_left = am.volume_right = Volume;
     CHECK(ioctl(fd_audio, AUDIO_SET_MIXER, &am));
     }
}

int cDvbDevice::NumAudioTracksDevice(void) const
{
  int n = 0;
  if (aPid1)
     n++;
  if (Ca() <= MAXDEVICES && aPid2 && aPid1 != aPid2) // a CA recording session blocks switching live audio tracks
     n++;
  return n;
}

const char **cDvbDevice::GetAudioTracksDevice(int *CurrentTrack) const
{
  if (NumAudioTracksDevice()) {
     if (CurrentTrack)
        *CurrentTrack = (pidHandles[ptAudio].pid == aPid1) ? 0 : 1;
     static const char *audioTracks1[] = { "Audio 1", NULL };
     static const char *audioTracks2[] = { "Audio 1", "Audio 2", NULL };
     return NumAudioTracksDevice() > 1 ? audioTracks2 : audioTracks1;
     }
  return NULL;
}

void cDvbDevice::SetAudioTrackDevice(int Index)
{
  if (0 <= Index && Index < NumAudioTracksDevice()) {
     int Pid = Index ? aPid2 : aPid1;
     pidHandles[ptAudio].pid = Pid;
     SetPid(&pidHandles[ptAudio], ptAudio, true);
     }
}

bool cDvbDevice::CanReplay(void) const
{
#ifndef DO_REC_AND_PLAY_ON_PRIMARY_DEVICE
  if (Receiving())
     return false;
#endif
  return cDevice::CanReplay() && (Ca() <= MAXDEVICES || ciHandler); // with non-LL-firmware we can only replay if there is no CA recording going on
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
         if (siProcessor)
            siProcessor->SetStatus(true);
         break;
    case pmAudioVideo:
         if (playMode == pmNone)
            TurnOffLiveMode();
         // continue with next...
    case pmAudioOnlyBlack:
         if (siProcessor)
            siProcessor->SetStatus(false);
         CHECK(ioctl(fd_video, VIDEO_SET_BLANK, true));
         CHECK(ioctl(fd_audio, AUDIO_SELECT_SOURCE, AUDIO_SOURCE_MEMORY));
         CHECK(ioctl(fd_audio, AUDIO_SET_AV_SYNC, PlayMode == pmAudioVideo));
         CHECK(ioctl(fd_audio, AUDIO_PLAY));
         CHECK(ioctl(fd_video, VIDEO_SELECT_SOURCE, VIDEO_SOURCE_MEMORY));
         CHECK(ioctl(fd_video, VIDEO_PLAY));
         break;
    case pmAudioOnly:
         if (siProcessor)
            siProcessor->SetStatus(false);
         CHECK(ioctl(fd_video, VIDEO_SET_BLANK, true));
         CHECK(ioctl(fd_audio, AUDIO_STOP, true));
         CHECK(ioctl(fd_audio, AUDIO_CLEAR_BUFFER));
         CHECK(ioctl(fd_audio, AUDIO_SELECT_SOURCE, AUDIO_SOURCE_MEMORY));
         CHECK(ioctl(fd_audio, AUDIO_SET_AV_SYNC, false));
         CHECK(ioctl(fd_audio, AUDIO_PLAY));
         CHECK(ioctl(fd_video, VIDEO_SET_BLANK, false));
         break;
    case pmExtern_THIS_SHOULD_BE_AVOIDED:
         if (siProcessor)
            siProcessor->SetStatus(false);
         close(fd_video);
         close(fd_audio);
         fd_video = fd_audio = -1;
         break;
    }
  playMode = PlayMode;
  return true;
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
/* Using the VIDEO_STILLPICTURE ioctl call would be the
   correct way to display a still frame, but unfortunately this
   doesn't work with frames from VDR. So let's do pretty much the
   same here as in DVB/driver/dvb.c's play_iframe() - I have absolutely
   no idea why it works this way, but doesn't work with VIDEO_STILLPICTURE.
   If anybody ever finds out what could be changed so that VIDEO_STILLPICTURE
   could be used, please let me know!
   kls 2002-03-23
   2003-08-30: apparently the driver can't handle PES data, so Oliver Endriss
               <o.endriss@gmx.de> has changed this to strip all PES headers
               and send pure ES data to the driver. Seems to work just fine!
               Let's drop the VIDEO_STILLPICTURE_WORKS_WITH_VDR_FRAMES stuff
               once this has proven to work in all cases.
*/
#define VIDEO_STILLPICTURE_WORKS_WITH_VDR_FRAMES
#ifdef VIDEO_STILLPICTURE_WORKS_WITH_VDR_FRAMES
  if (Data[0] == 0x00 && Data[1] == 0x00 && Data[2] == 0x01 && (Data[3] & 0xF0) == 0xE0) {
     // PES data
     char *buf = MALLOC(char, Length);
     if (!buf)
        return;
     int i = 0;
     int blen = 0;
     while (i < Length - 4) {
           if (Data[i] == 0x00 && Data[i + 1] == 0x00 && Data[i + 2] == 0x01 && (Data[i + 3] & 0xF0) == 0xE0) {
              // skip PES header
              int offs = i + 6;
              int len = Data[i + 4] * 256 + Data[i + 5];
              // skip header extension
              if ((Data[i + 6] & 0xC0) == 0x80) {
                 offs += 3;
                 offs += Data[i + 8];
                 len -= 3;
                 len -= Data[i + 8];
                 }
              memcpy(&buf[blen], &Data[offs], len);
              i = offs + len;
              blen += len;
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
#else
#define MIN_IFRAME 400000
  for (int i = MIN_IFRAME / Length + 1; i > 0; i--) {
      safe_write(fd_video, Data, Length);
      usleep(1); // allows the buffer to be displayed in case the progress display is active
      }
#endif
}

bool cDvbDevice::Poll(cPoller &Poller, int TimeoutMs)
{
  Poller.Add((playMode == pmAudioOnly || playMode == pmAudioOnlyBlack) ? fd_audio : fd_video, true);
  return Poller.Poll(TimeoutMs);
}

int cDvbDevice::PlayVideo(const uchar *Data, int Length)
{
  int fd = (playMode == pmAudioOnly || playMode == pmAudioOnlyBlack) ? fd_audio : fd_video;
  if (fd >= 0)
     return write(fd, Data, Length);
  return -1;
}

void cDvbDevice::PlayAudio(const uchar *Data, int Length)
{
  //XXX actually this function will only be needed to implement replaying AC3 over the DVB card's S/PDIF
  cDevice::PlayAudio(Data, Length);
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
     close(fd_dvr);
     fd_dvr = -1;
     delete tsBuffer;
     tsBuffer = NULL;
     }
}

bool cDvbDevice::GetTSPacket(uchar *&Data)
{
  if (tsBuffer) {
     int r = tsBuffer->Read();
     if (r >= 0) {
        Data = tsBuffer->Get();
        return true;
        }
     else if (FATALERRNO) {
        if (errno == EOVERFLOW)
           esyslog("ERROR: DVB driver buffer overflow on device %d", CardIndex() + 1);
        else {
           LOG_ERROR;
           return false;
           }
        }
     return true;
     }
  return false;
}
