/*
 * dvbdevice.c: The DVB device interface
 *
 * See the main source file 'vdr.c' for copyright information and
 * how to reach the author.
 *
 * $Id: dvbdevice.c 1.9 2002/09/04 13:46:03 kls Exp $
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
#ifdef NEWSTRUCT
#include <linux/dvb/audio.h>
#include <linux/dvb/dmx.h>
#include <linux/dvb/frontend.h>
#include <linux/dvb/video.h>
#else
#include <ost/audio.h>
#include <ost/dmx.h>
#include <ost/sec.h>
#include <ost/video.h>
#endif
#include <sys/ioctl.h>
#include <sys/mman.h>
#include "dvbosd.h"
#include "player.h"
#include "receiver.h"
#include "status.h"
#include "transfer.h"

#define DEV_VIDEO         "/dev/video"
#ifdef NEWSTRUCT
#define DEV_DVB_ADAPTER   "/dev/dvb/adapter"
#define DEV_DVB_OSD       "osd"
#define DEV_DVB_FRONTEND  "frontend"
#define DEV_DVB_DVR       "dvr"
#define DEV_DVB_DEMUX     "demux"
#define DEV_DVB_VIDEO     "video"
#define DEV_DVB_AUDIO     "audio"
#else
#define DEV_DVB_OSD       "/dev/ost/osd"
#define DEV_DVB_FRONTEND  "/dev/ost/frontend"
#define DEV_DVB_SEC       "/dev/ost/sec"
#define DEV_DVB_DVR       "/dev/ost/dvr"
#define DEV_DVB_DEMUX     "/dev/ost/demux"
#define DEV_DVB_VIDEO     "/dev/ost/video"
#define DEV_DVB_AUDIO     "/dev/ost/audio"
#endif

static const char *DvbName(const char *Name, int n)
{
  static char buffer[PATH_MAX];
#ifdef NEWSTRUCT
  snprintf(buffer, sizeof(buffer), "%s%d/%s%d", DEV_DVB_ADAPTER, n, Name, 0);
#else
  snprintf(buffer, sizeof(buffer), "%s%d", Name, n);
#endif
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

cDvbDevice::cDvbDevice(int n)
{
  frontendType = FrontendType(-1); // don't know how else to initialize this - there is no FE_UNKNOWN
  siProcessor = NULL;
  playMode = pmNone;

  // Devices that are present on all card types:

  fd_frontend = DvbOpen(DEV_DVB_FRONTEND, n, O_RDWR | O_NONBLOCK);

  // Devices that are only present on cards with decoders:

  fd_osd      = DvbOpen(DEV_DVB_OSD,    n, O_RDWR);
  fd_video    = DvbOpen(DEV_DVB_VIDEO,  n, O_RDWR | O_NONBLOCK);
  fd_audio    = DvbOpen(DEV_DVB_AUDIO,  n, O_RDWR | O_NONBLOCK);

#ifndef NEWSTRUCT
  // Devices that are only present on DVB-S cards:

  fd_sec      = DvbOpen(DEV_DVB_SEC,      n, O_RDWR);
#endif

  // The DVR device (will be opened and closed as needed):

  fd_dvr = -1;

  // Video format:

  SetVideoFormat(Setup.VideoFormat ? VIDEO_FORMAT_16_9 : VIDEO_FORMAT_4_3);

  // We only check the devices that must be present - the others will be checked before accessing them://XXX

  if (fd_frontend >= 0) {
#ifdef NEWSTRUCT
     dvb_frontend_info feinfo;
#else
     FrontendInfo feinfo;
#endif
     siProcessor = new cSIProcessor(DvbName(DEV_DVB_DEMUX, n));
     if (ioctl(fd_frontend, FE_GET_INFO, &feinfo) >= 0)
        frontendType = feinfo.type;
     else
        LOG_ERROR;
     }
  else
     esyslog("ERROR: can't open DVB device %d", n);

  frequency = 0;
}

cDvbDevice::~cDvbDevice()
{
  delete siProcessor;
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

bool cDvbDevice::GrabImage(const char *FileName, bool Jpeg, int Quality, int SizeX, int SizeY)
{
  int videoDev = DvbOpen(DEV_VIDEO, CardIndex(), O_RDWR, true);
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

//                          ptVideo        ptAudio        ptTeletext        ptDolby        ptOther
dmxPesType_t PesTypes[] = { DMX_PES_VIDEO, DMX_PES_AUDIO, DMX_PES_TELETEXT, DMX_PES_OTHER, DMX_PES_OTHER };

bool cDvbDevice::SetPid(cPidHandle *Handle, int Type, bool On)
{
  if (Handle->pid) {
     if (On) {
        if (Handle->handle < 0) {
           Handle->handle = DvbOpen(DEV_DVB_DEMUX, CardIndex(), O_RDWR | O_NONBLOCK, true);
           if (Handle->handle < 0)
              return false;
           }
        }
     else {
        CHECK(ioctl(Handle->handle, DMX_STOP));
        if (Handle->used == 0) {
           dmxPesFilterParams pesFilterParams;
           memset(&pesFilterParams, 0, sizeof(pesFilterParams));
           pesFilterParams.pid     = 0x1FFF;
           pesFilterParams.input   = DMX_IN_FRONTEND;
           pesFilterParams.output  = DMX_OUT_DECODER;
           pesFilterParams.pesType = PesTypes[Type < ptOther ? Type : ptOther];
           pesFilterParams.flags   = DMX_IMMEDIATE_START;
           CHECK(ioctl(Handle->handle, DMX_SET_PES_FILTER, &pesFilterParams));
           close(Handle->handle);
           Handle->handle = -1;
           return true;
           }
        }

     if (Handle->pid != 0x1FFF) {
        dmxPesFilterParams pesFilterParams;
        memset(&pesFilterParams, 0, sizeof(pesFilterParams));
        pesFilterParams.pid     = Handle->pid;
        pesFilterParams.input   = DMX_IN_FRONTEND;
        pesFilterParams.output  = (Type <= ptTeletext && Handle->used <= 1) ? DMX_OUT_DECODER : DMX_OUT_TS_TAP;
        pesFilterParams.pesType = PesTypes[Type < ptOther ? Type : ptOther];
        pesFilterParams.flags   = DMX_IMMEDIATE_START;
        //XXX+ pesFilterParams.flags   = DMX_CHECK_CRC;//XXX
        if (ioctl(Handle->handle, DMX_SET_PES_FILTER, &pesFilterParams) < 0) {
           LOG_ERROR;
           return false;
           }
        //XXX+ CHECK(ioctl(Handle->handle, DMX_SET_BUFFER_SIZE, KILOBYTE(32)));//XXX
        //XXX+ CHECK(ioctl(Handle->handle, DMX_START));//XXX
        }
     }
  return true;
}

bool cDvbDevice::ProvidesChannel(const cChannel *Channel, int Priority, bool *NeedsSwitchChannel)
{
  bool result = false;
  bool hasPriority = Priority < 0 || Priority > this->Priority();
  bool needsSwitchChannel = true;

  if (ProvidesCa(Channel->ca)) {
     if (Receiving()) {
        if (frequency == Channel->frequency) {
           needsSwitchChannel = false;
           if (!HasPid(Channel->vpid)) {
              if (Channel->ca > CACONFBASE) {
                 needsSwitchChannel = true;
                 result = hasPriority;
                 }
              else if (!HasDecoder())
                 result = true; // if it has no decoder it can't be the primary device
              else {
#define DVB_DRIVER_VERSION 2002090101 //XXX+
#define MIN_DVB_DRIVER_VERSION_FOR_TIMESHIFT 2002090101
#ifdef DVB_DRIVER_VERSION
#if (DVB_DRIVER_VERSION >= MIN_DVB_DRIVER_VERSION_FOR_TIMESHIFT)
                 if (pidHandles[ptVideo].used)
                    needsSwitchChannel = true; // to have it turn off the live PIDs
                 result = !IsPrimaryDevice() || Priority >= Setup.PrimaryLimit;
#endif
#else
#warning "DVB_DRIVER_VERSION not defined - time shift with only one DVB device disabled!"
#endif
                 }
              }
           else
              result = !IsPrimaryDevice() || Priority >= Setup.PrimaryLimit;
           }
        else
           result = hasPriority;
        }
     else
        result = hasPriority;
     }
  if (NeedsSwitchChannel)
     *NeedsSwitchChannel = needsSwitchChannel;
  return result;
}

bool cDvbDevice::SetChannelDevice(const cChannel *Channel, bool LiveView)
{
#if (DVB_DRIVER_VERSION < MIN_DVB_DRIVER_VERSION_FOR_TIMESHIFT)
  if (HasDecoder())
     LiveView = true;
#endif

  // Avoid noise while switching:

  if (HasDecoder()) {
     CHECK(ioctl(fd_audio, AUDIO_SET_MUTE, true));
     CHECK(ioctl(fd_video, VIDEO_SET_BLANK, true));
     CHECK(ioctl(fd_audio, AUDIO_CLEAR_BUFFER));
     CHECK(ioctl(fd_video, VIDEO_CLEAR_BUFFER));
     }

  // Stop setting system time:

  if (siProcessor)
     siProcessor->SetCurrentTransponder(0);

  // Turn off current PIDs:

  if (HasDecoder() && (LiveView || pidHandles[ptVideo].pid == Channel->vpid)) {
     DelPid(pidHandles[ptVideo].pid);
     DelPid(pidHandles[ptAudio].pid);
     DelPid(pidHandles[ptTeletext].pid);
     DelPid(pidHandles[ptDolby].pid);
     }

  if (frequency != Channel->frequency || Channel->ca > CACONFBASE) { // CA channels can only be decrypted in "live" mode

#ifdef NEWSTRUCT
     dvb_frontend_parameters Frontend;
#else
     FrontendParameters Frontend;
#endif

     memset(&Frontend, 0, sizeof(Frontend));

     switch (frontendType) {
       case FE_QPSK: { // DVB-S

            // Frequency offsets:

            unsigned int freq = Channel->frequency;
            int tone = SEC_TONE_OFF;

            if (freq < (unsigned int)Setup.LnbSLOF) {
               freq -= Setup.LnbFrequLo;
               tone = SEC_TONE_OFF;
               }
            else {
               freq -= Setup.LnbFrequHi;
               tone = SEC_TONE_ON;
               }

#ifdef NEWSTRUCT
            Frontend.frequency = freq * 1000UL;
            Frontend.inversion = INVERSION_AUTO;
            Frontend.u.qpsk.symbol_rate = Channel->srate * 1000UL;
            Frontend.u.qpsk.fec_inner = FEC_AUTO;
#else
            Frontend.Frequency = freq * 1000UL;
            Frontend.Inversion = INVERSION_AUTO;
            Frontend.u.qpsk.SymbolRate = Channel->srate * 1000UL;
            Frontend.u.qpsk.FEC_inner = FEC_AUTO;
#endif

            int volt = (Channel->polarization == 'v' || Channel->polarization == 'V') ? SEC_VOLTAGE_13 : SEC_VOLTAGE_18;

            // DiSEqC:

#ifdef NEWSTRUCT
            struct dvb_diseqc_master_cmd cmd = { {0xE0, 0x10, 0x38, 0xF0, 0x00, 0x00}, 4};
            cmd.msg[3] = 0xF0 | (((Channel->diseqc * 4) & 0x0F) | (tone == SEC_TONE_ON ? 1 : 0) | (volt == SEC_VOLTAGE_18 ? 2 : 0));

            if (Setup.DiSEqC)
               CHECK(ioctl(fd_frontend, FE_SET_TONE, SEC_TONE_OFF));
            CHECK(ioctl(fd_frontend, FE_SET_VOLTAGE, volt));
            if (Setup.DiSEqC) {
               usleep(15 * 1000);
               CHECK(ioctl(fd_frontend, FE_DISEQC_SEND_MASTER_CMD, &cmd));
               usleep(15 * 1000);
               CHECK(ioctl(fd_frontend, FE_DISEQC_SEND_BURST, (Channel->diseqc / 4) % 2 ? SEC_MINI_B : SEC_MINI_A));
               usleep(15 * 1000);
               }
            CHECK(ioctl(fd_frontend, FE_SET_TONE, tone));
#else
            secCommand scmd;
            scmd.type = 0;
            scmd.u.diseqc.addr = 0x10;
            scmd.u.diseqc.cmd = 0x38;
            scmd.u.diseqc.numParams = 1;
            scmd.u.diseqc.params[0] = 0xF0 | ((Channel->diseqc * 4) & 0x0F) | (tone == SEC_TONE_ON ? 1 : 0) | (volt == SEC_VOLTAGE_18 ? 2 : 0);

            secCmdSequence scmds;
            scmds.voltage = volt;
            scmds.miniCommand = SEC_MINI_NONE;
            scmds.continuousTone = tone;
            scmds.numCommands = Setup.DiSEqC ? 1 : 0;
            scmds.commands = &scmd;

            CHECK(ioctl(fd_sec, SEC_SEND_SEQUENCE, &scmds));
#endif
            }
            break;
       case FE_QAM: { // DVB-C

            // Frequency and symbol rate:

#ifdef NEWSTRUCT
            Frontend.frequency = Channel->frequency * 1000000UL;
            Frontend.inversion = INVERSION_AUTO;
            Frontend.u.qam.symbol_rate = Channel->srate * 1000UL;
            Frontend.u.qam.fec_inner = FEC_AUTO;
            Frontend.u.qam.modulation = QAM_64;
#else
            Frontend.Frequency = Channel->frequency * 1000000UL;
            Frontend.Inversion = INVERSION_AUTO;
            Frontend.u.qam.SymbolRate = Channel->srate * 1000UL;
            Frontend.u.qam.FEC_inner = FEC_AUTO;
            Frontend.u.qam.QAM = QAM_64;
#endif
            }
            break;
       case FE_OFDM: { // DVB-T

            // Frequency and OFDM paramaters:

#ifdef NEWSTRUCT
            Frontend.frequency = Channel->frequency * 1000UL;
            Frontend.inversion = INVERSION_AUTO;
            Frontend.u.ofdm.bandwidth=BANDWIDTH_8_MHZ;
            Frontend.u.ofdm.code_rate_HP = FEC_2_3;
            Frontend.u.ofdm.code_rate_LP = FEC_1_2;
            Frontend.u.ofdm.constellation = QAM_64;
            Frontend.u.ofdm.transmission_mode = TRANSMISSION_MODE_2K;
            Frontend.u.ofdm.guard_interval = GUARD_INTERVAL_1_32;
            Frontend.u.ofdm.hierarchy_information = HIERARCHY_NONE;
#else
            Frontend.Frequency = Channel->frequency * 1000UL;
            Frontend.Inversion = INVERSION_AUTO;
            Frontend.u.ofdm.bandWidth=BANDWIDTH_8_MHZ;
            Frontend.u.ofdm.HP_CodeRate=FEC_2_3;
            Frontend.u.ofdm.LP_CodeRate=FEC_1_2;
            Frontend.u.ofdm.Constellation=QAM_64;
            Frontend.u.ofdm.TransmissionMode=TRANSMISSION_MODE_2K;
            Frontend.u.ofdm.guardInterval=GUARD_INTERVAL_1_32;
            Frontend.u.ofdm.HierarchyInformation=HIERARCHY_NONE;
#endif
            }
            break;
       default:
            esyslog("ERROR: attempt to set channel with unknown DVB frontend type");
            return false;
       }

#ifdef NEWSTRUCT
     // Discard stale events:

     for (;;) {
         dvb_frontend_event event;
         if (ioctl(fd_frontend, FE_GET_EVENT, &event) < 0)
            break;
         }
#endif

     // Tuning:

     CHECK(ioctl(fd_frontend, FE_SET_FRONTEND, &Frontend));

     // Wait for channel lock:

#ifdef NEWSTRUCT
     FrontendStatus status = FrontendStatus(0);
     for (int i = 0; i < 100; i++) {
         CHECK(ioctl(fd_frontend, FE_READ_STATUS, &status));
         if (status & FE_HAS_LOCK)
            break;
         usleep(10 * 1000);
         }
     if (!(status & FE_HAS_LOCK)) {
        esyslog("ERROR: channel %d not locked on DVB card %d!", Channel->number, CardIndex() + 1);
        if (IsPrimaryDevice())
           cThread::RaisePanic();
        return false;
        }
#else
     if (cFile::FileReady(fd_frontend, 5000)) {
        FrontendEvent event;
        if (ioctl(fd_frontend, FE_GET_EVENT, &event) >= 0) {
           if (event.type != FE_COMPLETION_EV) {
              esyslog("ERROR: channel %d not sync'ed on DVB card %d!", Channel->number, CardIndex() + 1);
              if (IsPrimaryDevice())
                 cThread::RaisePanic();
              return false;
              }
           }
        else
           esyslog("ERROR in frontend get event (channel %d, card %d): %m", Channel->number, CardIndex() + 1);
        }
     else
        esyslog("ERROR: timeout while tuning on DVB card %d", CardIndex() + 1);
#endif

     frequency = Channel->frequency;

     }

  // PID settings:

  if (HasDecoder() && (LiveView || Channel->ca > CACONFBASE)) { // CA channels can only be decrypted in "live" mode
     if (!HasPid(Channel->vpid)) {
        if (!(AddPid(Channel->vpid, ptVideo) && AddPid(Channel->apid1, ptAudio))) {//XXX+ dolby dpid1!!! (if audio plugins are attached)
           esyslog("ERROR: failed to set PIDs for channel %d", Channel->number);
           return false;
           }
        if (IsPrimaryDevice())
           AddPid(Channel->tpid, ptTeletext);
        CHECK(ioctl(fd_audio, AUDIO_SET_AV_SYNC, true));
        CHECK(ioctl(fd_audio, AUDIO_SET_MUTE, false));
        CHECK(ioctl(fd_video, VIDEO_SET_BLANK, false));
        }
     else
        cControl::Launch(new cTransferControl(this, Channel->vpid, Channel->apid1, 0, 0, 0));
     }

  // Start setting system time:

  if (siProcessor)
     siProcessor->SetCurrentTransponder(Channel->frequency);

  return true;
}

void cDvbDevice::SetVolumeDevice(int Volume)
{
  if (HasDecoder()) {
     audioMixer_t am;
     am.volume_left = am.volume_right = Volume;
     CHECK(ioctl(fd_audio, AUDIO_SET_MIXER, &am));
     }
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
         if (siProcessor)
            siProcessor->SetStatus(false);
         CHECK(ioctl(fd_video, VIDEO_SET_BLANK, true));
         CHECK(ioctl(fd_audio, AUDIO_SELECT_SOURCE, AUDIO_SOURCE_MEMORY));
         CHECK(ioctl(fd_audio, AUDIO_SET_AV_SYNC, true));
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
}

void cDvbDevice::Play(void)
{
  if (fd_audio >= 0)
     CHECK(ioctl(fd_audio, AUDIO_SET_AV_SYNC, true));
  if (fd_video >= 0)
     CHECK(ioctl(fd_video, VIDEO_CONTINUE));
}

void cDvbDevice::Freeze(void)
{
  if (fd_audio >= 0)
     CHECK(ioctl(fd_audio, AUDIO_SET_AV_SYNC, false));
  if (fd_video >= 0)
     CHECK(ioctl(fd_video, VIDEO_FREEZE));
}

void cDvbDevice::Mute(void)
{
  if (fd_audio >= 0) {
     CHECK(ioctl(fd_audio, AUDIO_SET_AV_SYNC, false));
     CHECK(ioctl(fd_audio, AUDIO_SET_MUTE, true));
     }
}

void cDvbDevice::StillPicture(const uchar *Data, int Length)
{
  Mute();
/* Using the VIDEO_STILLPICTURE ioctl call would be the
   correct way to display a still frame, but unfortunately this
   doesn't work with frames from VDR. So let's do pretty much the
   same here as in DVB/driver/dvb.c's play_iframe() - I have absolutely
   no idea why it works this way, but doesn't work with VIDEO_STILLPICTURE.
   If anybody ever finds out what could be changed so that VIDEO_STILLPICTURE
   could be used, please let me know!
   kls 2002-03-23
*/
//#define VIDEO_STILLPICTURE_WORKS_WITH_VDR_FRAMES
#ifdef VIDEO_STILLPICTURE_WORKS_WITH_VDR_FRAMES
  videoDisplayStillPicture sp = { (char *)Data, Length };
  CHECK(ioctl(fd_video, VIDEO_STILLPICTURE, &sp));
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
  Poller.Add(playMode == pmAudioOnly ? fd_audio : fd_video, true);
  return Poller.Poll(TimeoutMs);
}

int cDvbDevice::PlayVideo(const uchar *Data, int Length)
{
  int fd = playMode == pmAudioOnly ? fd_audio : fd_video;
  if (fd >= 0)
     return write(fd, Data, Length);
  return -1;
}

int cDvbDevice::PlayAudio(const uchar *Data, int Length)
{
  //XXX+
  return -1;
}

bool cDvbDevice::OpenDvr(void)
{
  CloseDvr();
  fd_dvr = DvbOpen(DEV_DVB_DVR, CardIndex(), O_RDONLY | O_NONBLOCK, true);
  return fd_dvr >= 0;
}

void cDvbDevice::CloseDvr(void)
{
  if (fd_dvr >= 0) {
     close(fd_dvr);
     fd_dvr = -1;
     }
}

int cDvbDevice::GetTSPacket(uchar *Data)
{
  if (fd_dvr >= 0) {
     cPoller Poller(fd_dvr, false);
     if (Poller.Poll(100)) {
        int r = read(fd_dvr, Data, TS_SIZE);
        if (r >= 0)
           return r;
        else if (FATALERRNO) {
           if (errno == EBUFFEROVERFLOW) // this error code is not defined in the library
              esyslog("ERROR: DVB driver buffer overflow on device %d", CardIndex() + 1);
           else {
              LOG_ERROR;
              return -1;
              }
           }
        }
     return 0;
     }
  else
     return -1;
}
