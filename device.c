/*
 * device.c: The basic device interface
 *
 * See the main source file 'vdr.c' for copyright information and
 * how to reach the author.
 *
 * $Id: device.c 1.2 2002/06/16 13:23:31 kls Exp $
 */

#include "device.h"
#include <errno.h>
extern "C" {
#define HAVE_BOOLEAN
#include <jpeglib.h>
}
#include <linux/videodev.h>
#include <ost/sec.h>
#include <poll.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include "player.h"
#include "receiver.h"
#include "status.h"

#define DEV_VIDEO         "/dev/video"
#define DEV_OST_OSD       "/dev/ost/osd"
#define DEV_OST_FRONTEND  "/dev/ost/frontend"
#define DEV_OST_SEC       "/dev/ost/sec"
#define DEV_OST_DVR       "/dev/ost/dvr"
#define DEV_OST_DEMUX     "/dev/ost/demux"
#define DEV_OST_VIDEO     "/dev/ost/video"
#define DEV_OST_AUDIO     "/dev/ost/audio"

// The default priority for non-primary DVB cards:
#define DEFAULTPRIORITY  -2

#define TS_SIZE          188
#define TS_SYNC_BYTE     0x47
#define PID_MASK_HI      0x1F

// The maximum time we wait before assuming that a recorded video data stream
// is broken:
#define MAXBROKENTIMEOUT 30 // seconds

static const char *OstName(const char *Name, int n)
{
  static char buffer[_POSIX_PATH_MAX];
  snprintf(buffer, sizeof(buffer), "%s%d", Name, n);
  return buffer;
}

static int OstOpen(const char *Name, int n, int Mode, bool ReportError = false)
{
  const char *FileName = OstName(Name, n);
  int fd = open(FileName, Mode);
  if (fd < 0 && ReportError)
     LOG_ERROR_STR(FileName);
  return fd;
}

int cDevice::numDevices = 0;
int cDevice::useDevice = 0;
cDevice *cDevice::device[MAXDEVICES] = { NULL };
cDevice *cDevice::primaryDevice = NULL;

cDevice::cDevice(int n)
{
  frontendType = FrontendType(-1); // don't know how else to initialize this - there is no FE_UNKNOWN
  siProcessor = NULL;
  cardIndex = n;

  // Devices that are present on all card types:

  fd_frontend = OstOpen(DEV_OST_FRONTEND, n, O_RDWR);

  // Devices that are only present on DVB-S cards:

  fd_sec      = OstOpen(DEV_OST_SEC,      n, O_RDWR);

  // Devices that are only present on cards with decoders:

  fd_osd      = OstOpen(DEV_OST_OSD,    n, O_RDWR);
  fd_video    = OstOpen(DEV_OST_VIDEO,  n, O_RDWR | O_NONBLOCK);
  fd_audio    = OstOpen(DEV_OST_AUDIO,  n, O_RDWR | O_NONBLOCK);

  // Video format:

  SetVideoFormat(Setup.VideoFormat ? VIDEO_FORMAT_16_9 : VIDEO_FORMAT_4_3);

  // We only check the devices that must be present - the others will be checked before accessing them://XXX

  if (fd_frontend >= 0) {
     siProcessor = new cSIProcessor(OstName(DEV_OST_DEMUX, n));
     FrontendInfo feinfo;
     if (ioctl(fd_frontend, FE_GET_INFO, &feinfo) >= 0)
        frontendType = feinfo.type;
     else
        LOG_ERROR;
     }
  else
     esyslog("ERROR: can't open video device %d", n);

  dvrFileName = strdup(OstName(DEV_OST_DVR, CardIndex()));
  active = false;

  currentChannel = 0;
  frequency = 0;

  mute = false;
  volume = Setup.CurrentVolume;

  player = NULL;

  for (int i = 0; i < MAXRECEIVERS; i++)
      receiver[i] = NULL;
  ca = -1;
}

cDevice::~cDevice()
{
  delete dvrFileName;
  delete siProcessor;
  Detach(player);
  for (int i = 0; i < MAXRECEIVERS; i++)
      Detach(receiver[i]);
  // We're not explicitly closing any device files here, since this sometimes
  // caused segfaults. Besides, the program is about to terminate anyway...
}

void cDevice::SetUseDevice(int n)
{
  if (n < MAXDEVICES)
     useDevice |= (1 << n);
}

bool cDevice::SetPrimaryDevice(int n)
{
  n--;
  if (0 <= n && n < numDevices && device[n]) {
     isyslog("setting primary device to %d", n + 1);
     primaryDevice = device[n];
     return true;
     }
  esyslog("invalid devive number: %d", n + 1);
  return false;
}

cDevice *cDevice::GetDevice(int Ca, int Priority, int Frequency, int Vpid, bool *ReUse)
{
  if (ReUse)
     *ReUse = false;
  cDevice *d = NULL;
  int Provides[MAXDEVICES];
  // Check which devices provide Ca:
  for (int i = 0; i < numDevices; i++) {
      if ((Provides[i] = device[i]->ProvidesCa(Ca)) != 0) { // this device is basicly able to do the job
         //XXX+ dsyslog("GetDevice: %d %d %d %5d %5d", i, device[i]->HasDecoder(), device[i]->Receiving(), Frequency, device[i]->frequency);//XXX
         if (  (!device[i]->HasDecoder() // it's a "budget card" which can receive multiple channels...
               && device[i]->frequency == Frequency // ...and it is tuned to the requested frequency...
               && device[i]->Receiving() // ...and is already receiving
               // need not check priority - if a budget card is already receiving on the requested
               // frequency, we can attach another receiver regardless of priority
               )
            || (device[i]->HasDecoder() // it's a "full featured card" which can receive only one channel...
               && device[i]->frequency == Frequency // ...and it is tuned to the requested frequency...
               && device[i]->pidHandles[ptVideo].pid == Vpid // ...and the requested video PID...
               && device[i]->Receiving() // ...and is already receiving
               // need not check priority - if a full featured card is already receiving the requested
               // frequency and video PID, we can attach another receiver regardless of priority
               )
            ) {
            d = device[i];
            if (ReUse)
               *ReUse = true;
            break;
            }
         if (Priority > device[i]->Priority() // Priority is high enough to use this device
            && (!d // we don't have a device yet, or...
               || device[i]->Priority() < d->Priority() // ...this one has an even lower Priority
               || (device[i]->Priority() == d->Priority() // ...same Priority...
                  && Provides[i] < Provides[d->CardIndex()] // ...but this one provides fewer Ca values
                  )
               )
            )
            d = device[i];
         }
      }
  /*XXX+ too complex with multiple recordings per device
  if (!d && Ca > MAXDEVICES) {
     // We didn't find one the easy way, so now we have to try harder:
     int ShiftLevel = -1;
     for (int i = 0; i < numDevices; i++) {
         if (Provides[i]) { // this device is basicly able to do the job, but for some reason we didn't get it above
            int sl = device[i]->CanShift(Ca, Priority); // asks this device to shift its job to another device
            if (sl >= 0 && (ShiftLevel < 0 || sl < ShiftLevel)) {
               d = device[i]; // found one that can be shifted with the fewest number of subsequent shifts
               ShiftLevel = sl;
               }
            }
         }
     }
  XXX*/
  return d;
}

void cDevice::SetCaCaps(void)
{
  for (int d = 0; d < numDevices; d++) {
      for (int i = 0; i < MAXCACAPS; i++)
          device[d]->caCaps[i] = Setup.CaCaps[device[d]->CardIndex()][i];
      }
}

bool cDevice::Probe(const char *FileName)
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

bool cDevice::Initialize(void)
{
  numDevices = 0;
  for (int i = 0; i < MAXDEVICES; i++) {
      if (useDevice == 0 || (useDevice & (1 << i)) != 0) {
         if (Probe(OstName(DEV_OST_FRONTEND, i)))
            device[numDevices++] = new cDevice(i);
         else
            break;
         }
      }
  primaryDevice = device[0];
  if (numDevices > 0) {
     isyslog("found %d video device%s", numDevices, numDevices > 1 ? "s" : "");
     SetCaCaps();
     }
  else
     esyslog("ERROR: no video device found, giving up!");
  return numDevices > 0;
}

void cDevice::Shutdown(void)
{
  for (int i = 0; i < numDevices; i++) {
      delete device[i];
      device[i] = NULL;
      }
  primaryDevice = NULL;
}

bool cDevice::GrabImage(const char *FileName, bool Jpeg, int Quality, int SizeX, int SizeY)
{
  int videoDev = OstOpen(DEV_VIDEO, CardIndex(), O_RDWR, true);
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

void cDevice::SetVideoFormat(videoFormat_t Format)
{
  if (HasDecoder())
     CHECK(ioctl(fd_video, VIDEO_SET_FORMAT, Format));
}

//                          ptVideo        ptAudio        ptTeletext        ptDolby        ptOther
dmxPesType_t PesTypes[] = { DMX_PES_VIDEO, DMX_PES_AUDIO, DMX_PES_TELETEXT, DMX_PES_OTHER, DMX_PES_OTHER };

//#define PRINTPIDS(s) { char b[500]; char *q = b; q += sprintf(q, "%d %s ", CardIndex(), s); for (int i = 0; i < MAXPIDHANDLES; i++) q += sprintf(q, " %s%4d %d", i == ptOther ? "* " : "", pidHandles[i].pid, pidHandles[i].used); dsyslog(b); } //XXX+
#define PRINTPIDS(s)

bool cDevice::AddPid(int Pid, ePidType PidType)
{
  if (Pid) {
     int n = -1;
     int a = -1;
     for (int i = 0; i < MAXPIDHANDLES; i++) {
         if (pidHandles[i].pid == Pid)
            n = i;
         else if (a < 0 && i >= ptOther && !pidHandles[i].used)
            a = i;
         }
     dmxPesType_t PesType = PesTypes[ptOther];
     if (n >= 0) {
        // The Pid is already in use
        if (++pidHandles[n].used == 2 && n <= ptTeletext) {
           // It's a special PID that has to be switched into "tap" mode
           PRINTPIDS("A");//XXX+
           return SetPid(pidHandles[n].fd, PesTypes[n], Pid, DMX_OUT_TS_TAP);
           }
        PRINTPIDS("a");//XXX+
        return true;
        }
     else if (PidType < ptOther) {
        // The Pid is not yet in use and it is a special one
        n = PidType;
        PesType = PesTypes[PidType];
        PRINTPIDS("B");//XXX+
        }
     else if (a >= 0) {
        // The Pid is not yet in use and we have a free slot
        n = a;
        }
     else
        esyslog("ERROR: no free slot for PID %d", Pid);
     if (n >= 0) {
        pidHandles[n].pid = Pid;
        pidHandles[n].fd = OstOpen(DEV_OST_DEMUX, CardIndex(), O_RDWR | O_NONBLOCK, true);
        pidHandles[n].used = 1;
        PRINTPIDS("C");//XXX+
        return SetPid(pidHandles[n].fd, PesType, Pid, PidType <= ptTeletext ? DMX_OUT_DECODER : DMX_OUT_TS_TAP);
        }
     }
  return true;
}

bool cDevice::DelPid(int Pid)
{
  if (Pid) {
     for (int i = 0; i < MAXPIDHANDLES; i++) {
         if (pidHandles[i].pid == Pid) {
            switch (--pidHandles[i].used) {
              case 0: CHECK(ioctl(pidHandles[i].fd, DMX_STOP));//XXX+ is this necessary???
                      close(pidHandles[i].fd);
                      pidHandles[i].fd = -1;
                      pidHandles[i].pid = 0;
                      break;
              case 1: if (i <= ptTeletext)
                         SetPid(pidHandles[i].fd, PesTypes[i], Pid, DMX_OUT_DECODER);
                      break;
              }
            PRINTPIDS("D");//XXX+
            return pidHandles[i].used;
            }
         }
     }
  return false;
}

bool cDevice::SetPid(int fd, dmxPesType_t PesType, int Pid, dmxOutput_t Output)
{
  if (Pid) {
     CHECK(ioctl(fd, DMX_STOP));
     if (Pid != 0x1FFF) {
        dmxPesFilterParams pesFilterParams;
        pesFilterParams.pid     = Pid;
        pesFilterParams.input   = DMX_IN_FRONTEND;
        pesFilterParams.output  = Output;
        pesFilterParams.pesType = PesType;
        pesFilterParams.flags   = DMX_IMMEDIATE_START;
        //XXX+ pesFilterParams.flags   = DMX_CHECK_CRC;//XXX
        if (ioctl(fd, DMX_SET_PES_FILTER, &pesFilterParams) < 0) {
           LOG_ERROR;
           return false;
           }
        //XXX+ CHECK(ioctl(fd, DMX_SET_BUFFER_SIZE, KILOBYTE(32)));//XXX
        //XXX+ CHECK(ioctl(fd, DMX_START));//XXX
        }
     }
  return true;
}

eSetChannelResult cDevice::SetChannel(int ChannelNumber, int Frequency, char Polarization, int Diseqc, int Srate, int Vpid, int Apid, int Tpid, int Ca, int Pnr)
{
  //XXX+StopTransfer();
  //XXX+StopReplay();

  cStatus::MsgChannelSwitch(this, 0);

  // Must set this anyway to avoid getting stuck when switching through
  // channels with 'Up' and 'Down' keys:
  currentChannel = ChannelNumber;

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

  // If this card can't receive this channel, we must not actually switch
  // the channel here, because that would irritate the driver when we
  // start replaying in Transfer Mode immediately after switching the channel:
  bool NeedsTransferMode = (IsPrimaryDevice() && !ProvidesCa(Ca));

  if (!NeedsTransferMode) {

     // Turn off current PIDs:

     if (HasDecoder()) {
        DelPid(pidHandles[ptVideo].pid);
        DelPid(pidHandles[ptAudio].pid);
        DelPid(pidHandles[ptTeletext].pid);
        DelPid(pidHandles[ptDolby].pid);
        }

     FrontendParameters Frontend;

     switch (frontendType) {
       case FE_QPSK: { // DVB-S

            // Frequency offsets:

            unsigned int freq = Frequency;
            int tone = SEC_TONE_OFF;

            if (freq < (unsigned int)Setup.LnbSLOF) {
               freq -= Setup.LnbFrequLo;
               tone = SEC_TONE_OFF;
               }
            else {
               freq -= Setup.LnbFrequHi;
               tone = SEC_TONE_ON;
               }

            Frontend.Frequency = freq * 1000UL;
            Frontend.Inversion = INVERSION_AUTO;
            Frontend.u.qpsk.SymbolRate = Srate * 1000UL;
            Frontend.u.qpsk.FEC_inner = FEC_AUTO;

            int volt = (Polarization == 'v' || Polarization == 'V') ? SEC_VOLTAGE_13 : SEC_VOLTAGE_18;

            // DiseqC:

            secCommand scmd;
            scmd.type = 0;
            scmd.u.diseqc.addr = 0x10;
            scmd.u.diseqc.cmd = 0x38;
            scmd.u.diseqc.numParams = 1;
            scmd.u.diseqc.params[0] = 0xF0 | ((Diseqc * 4) & 0x0F) | (tone == SEC_TONE_ON ? 1 : 0) | (volt == SEC_VOLTAGE_18 ? 2 : 0);

            secCmdSequence scmds;
            scmds.voltage = volt;
            scmds.miniCommand = SEC_MINI_NONE;
            scmds.continuousTone = tone;
            scmds.numCommands = Setup.DiSEqC ? 1 : 0;
            scmds.commands = &scmd;

            CHECK(ioctl(fd_sec, SEC_SEND_SEQUENCE, &scmds));
            }
            break;
       case FE_QAM: { // DVB-C

            // Frequency and symbol rate:

            Frontend.Frequency = Frequency * 1000000UL;
            Frontend.Inversion = INVERSION_AUTO;
            Frontend.u.qam.SymbolRate = Srate * 1000UL;
            Frontend.u.qam.FEC_inner = FEC_AUTO;
            Frontend.u.qam.QAM = QAM_64;
            }
            break;
       case FE_OFDM: { // DVB-T

            // Frequency and OFDM paramaters:

            Frontend.Frequency = Frequency * 1000UL;
            Frontend.Inversion = INVERSION_AUTO;
            Frontend.u.ofdm.bandWidth=BANDWIDTH_8_MHZ;
            Frontend.u.ofdm.HP_CodeRate=FEC_2_3;
            Frontend.u.ofdm.LP_CodeRate=FEC_1_2;
            Frontend.u.ofdm.Constellation=QAM_64;
            Frontend.u.ofdm.TransmissionMode=TRANSMISSION_MODE_2K;
            Frontend.u.ofdm.guardInterval=GUARD_INTERVAL_1_32;
            Frontend.u.ofdm.HierarchyInformation=HIERARCHY_NONE;
            }
            break;
       default:
            esyslog("ERROR: attempt to set channel with unknown DVB frontend type");
            return scrFailed;
       }

     // Tuning:

     CHECK(ioctl(fd_frontend, FE_SET_FRONTEND, &Frontend));

     // Wait for channel sync:

     if (cFile::FileReady(fd_frontend, 5000)) {
        FrontendEvent event;
        int res = ioctl(fd_frontend, FE_GET_EVENT, &event);
        if (res >= 0) {
           if (event.type != FE_COMPLETION_EV) {
              esyslog("ERROR: channel %d not sync'ed on DVB card %d!", ChannelNumber, CardIndex() + 1);
              if (IsPrimaryDevice())
                 cThread::RaisePanic();
              return scrFailed;
              }
           }
        else
           esyslog("ERROR %d in frontend get event (channel %d, card %d)", res, ChannelNumber, CardIndex() + 1);
        }
     else
        esyslog("ERROR: timeout while tuning");

     frequency = Frequency;

     // PID settings:

     if (HasDecoder()) {
        if (!(AddPid(Vpid, ptVideo) && AddPid(Apid, ptAudio))) {//XXX+ dolby Dpid1!!! (if audio plugins are attached)
           esyslog("ERROR: failed to set PIDs for channel %d", ChannelNumber);
           return scrFailed;
           }
        if (IsPrimaryDevice())
           AddPid(Tpid, ptTeletext);
        CHECK(ioctl(fd_audio, AUDIO_SET_AV_SYNC, true));
        }
     }

  if (IsPrimaryDevice() && siProcessor)
     siProcessor->SetCurrentServiceID(Pnr);

  eSetChannelResult Result = scrOk;

  // If this DVB card can't receive this channel, let's see if we can
  // use the card that actually can receive it and transfer data from there:

  if (NeedsTransferMode) {
     cDevice *CaDevice = GetDevice(Ca, 0);
     if (CaDevice && !CaDevice->Receiving()) {
        if ((Result = CaDevice->SetChannel(ChannelNumber, Frequency, Polarization, Diseqc, Srate, Vpid, Apid, Tpid, Ca, Pnr)) == scrOk) {
           //XXX+SetModeReplay();
           //XXX+transferringFromDevice = CaDevice->StartTransfer(fd_video);
           }
        }
     else
        Result = scrNoTransfer;
     }

  if (HasDecoder()) {
     CHECK(ioctl(fd_audio, AUDIO_SET_MUTE, false));
     CHECK(ioctl(fd_video, VIDEO_SET_BLANK, false));
     }

  // Start setting system time:

  if (Result == scrOk && siProcessor)
     siProcessor->SetCurrentTransponder(Frequency);

  cStatus::MsgChannelSwitch(this, ChannelNumber);

  return Result;
}

bool cDevice::ToggleMute(void)
{
  int OldVolume = volume;
  mute = !mute;
  SetVolume(0, mute);
  volume = OldVolume;
  return mute;
}

void cDevice::SetVolume(int Volume, bool Absolute)
{
  if (HasDecoder()) {
     volume = min(max(Absolute ? Volume : volume + Volume, 0), MAXVOLUME);
     audioMixer_t am;
     am.volume_left = am.volume_right = volume;
     CHECK(ioctl(fd_audio, AUDIO_SET_MIXER, &am));
     cStatus::MsgSetVolume(volume, Absolute);
     if (volume > 0)
        mute = false;
     }
}

void cDevice::TrickSpeed(int Speed)
{
  if (fd_video >= 0)
     CHECK(ioctl(fd_video, VIDEO_SLOWMOTION, Speed));
}

void cDevice::Clear(void)
{
  if (fd_video >= 0)
     CHECK(ioctl(fd_video, VIDEO_CLEAR_BUFFER));
  if (fd_audio >= 0)
     CHECK(ioctl(fd_audio, AUDIO_CLEAR_BUFFER));
}

void cDevice::Play(void)
{
  if (fd_audio >= 0)
     CHECK(ioctl(fd_audio, AUDIO_SET_AV_SYNC, true));
  if (fd_video >= 0)
     CHECK(ioctl(fd_video, VIDEO_CONTINUE));
}

void cDevice::Freeze(void)
{
  if (fd_audio >= 0)
     CHECK(ioctl(fd_audio, AUDIO_SET_AV_SYNC, false));
  if (fd_video >= 0)
     CHECK(ioctl(fd_video, VIDEO_FREEZE));
}

void cDevice::Mute(void)
{
  if (fd_audio >= 0) {
     CHECK(ioctl(fd_audio, AUDIO_SET_AV_SYNC, false));
     CHECK(ioctl(fd_audio, AUDIO_SET_MUTE, true));
     }
}

void cDevice::StillPicture(const uchar *Data, int Length)
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

bool cDevice::Replaying(void)
{
  /*XXX+
  if (replayBuffer && !replayBuffer->Active())
     StopReplay();
  return replayBuffer != NULL;
  XXX*/
  return player != NULL;
}

bool cDevice::Attach(cPlayer *Player)
{
  if (Receiving()) {
     esyslog("ERROR: attempt to attach a cPlayer while receiving on device %d - ignored", CardIndex() + 1);
     return false;
     }
  if (HasDecoder()) {
     if (player)
        Detach(player);

     if (siProcessor)
        siProcessor->SetStatus(false);
     CHECK(ioctl(fd_video, VIDEO_SET_BLANK, true));
     CHECK(ioctl(fd_audio, AUDIO_SELECT_SOURCE, AUDIO_SOURCE_MEMORY));
     CHECK(ioctl(fd_audio, AUDIO_SET_AV_SYNC, true));
     CHECK(ioctl(fd_audio, AUDIO_PLAY));
     CHECK(ioctl(fd_video, VIDEO_SELECT_SOURCE, VIDEO_SOURCE_MEMORY));
     CHECK(ioctl(fd_video, VIDEO_PLAY));

     player = Player;
     player->device = this;
     player->deviceFileHandle = fd_video;
     player->Activate(true);
     return true;
     }
  return false;
}

void cDevice::Detach(cPlayer *Player)
{
  if (Player && player == Player) {
     player->Activate(false);
     player->deviceFileHandle = -1;
     player->device = NULL;
     player = NULL;

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
     }
}

void cDevice::StopReplay(void)
{
  if (player) {
     Detach(player);
     /*XXX+
     if (IsPrimaryDevice()) {
        // let's explicitly switch the channel back in case it was in Transfer Mode:
        cChannel *Channel = Channels.GetByNumber(currentChannel);
        if (Channel) {
           Channel->Switch(this, false);
           usleep(100000); // allow driver to sync in case a new replay will start immediately
           }
        }
        XXX*/
     }
}

int cDevice::PlayVideo(const uchar *Data, int Length)
{
  if (fd_video >= 0)
     return write(fd_video, Data, Length);
  return -1;
}

int cDevice::PlayAudio(const uchar *Data, int Length)
{
  //XXX+
  return -1;
}

int cDevice::Priority(void)
{
  if (IsPrimaryDevice() && !Receiving())
     return Setup.PrimaryLimit - 1;
  int priority = DEFAULTPRIORITY;
  for (int i = 0; i < MAXRECEIVERS; i++) {
      if (receiver[i])
         priority = max(receiver[i]->priority, priority);
      }
  return priority;
}

int cDevice::CanShift(int Ca, int Priority, int UsedCards)
{
  return -1;//XXX+ too complex with multiple recordings per device
  // Test whether a receiving on this DVB device can be shifted to another one
  // in order to perform a new receiving with the given Ca and Priority on this device:
  int ShiftLevel = -1; // default means this device can't be shifted
  if (UsedCards & (1 << CardIndex()) != 0)
     return ShiftLevel; // otherwise we would get into a loop
  if (Receiving()) {
     if (ProvidesCa(Ca) // this device provides the requested Ca
        && (Ca != this->Ca() // the requested Ca is different from the one currently used...
           || Priority > this->Priority())) { // ...or the request comes from a higher priority
        cDevice *d = NULL;
        int Provides[MAXDEVICES];
        UsedCards |= (1 << CardIndex());
        for (int i = 0; i < numDevices; i++) {
            if ((Provides[i] = device[i]->ProvidesCa(this->Ca())) != 0) { // this device is basicly able to do the job
               if (device[i] != this) { // it is not _this_ device
                  int sl = device[i]->CanShift(this->Ca(), Priority, UsedCards); // this is the original Priority!
                  if (sl >= 0 && (ShiftLevel < 0 || sl < ShiftLevel)) {
                     d = device[i];
                     ShiftLevel = sl;
                     }
                  }
               }
            }
        if (ShiftLevel >= 0)
           ShiftLevel++; // adds the device's own shift
        }
     }
  else if (Priority > this->Priority())
     ShiftLevel = 0; // no shifting necessary, this device can do the job
  return ShiftLevel;
}

int cDevice::ProvidesCa(int Ca)
{
  if (Ca == CardIndex() + 1)
     return 1; // exactly _this_ card was requested
  if (Ca && Ca <= MAXDEVICES)
     return 0; // a specific card was requested, but not _this_ one
  int result = Ca ? 0 : 1; // by default every card can provide FTA
  int others = Ca ? 1 : 0;
  for (int i = 0; i < MAXCACAPS; i++) {
      if (caCaps[i]) {
         if (caCaps[i] == Ca)
            result = 1;
         else
            others++;
         }
      }
  return result ? result + others : 0;
}

bool cDevice::Receiving(void)
{
  for (int i = 0; i < MAXRECEIVERS; i++) {
      if (receiver[i])
         return true;
      }
  return false;
}

void cDevice::Action(void)
{
  dsyslog("receiver thread started on device %d (pid=%d)", CardIndex() + 1, getpid());

  int fd_dvr = open(dvrFileName, O_RDONLY | O_NONBLOCK);
  if (fd_dvr >= 0) {
     pollfd pfd;
     pfd.fd = fd_dvr;
     pfd.events = pfd.revents = POLLIN;
     uchar b[TS_SIZE];
     time_t t = time(NULL);
     active = true;
     for (; active;) {

         // Read data from the DVR device:

         if (pfd.revents & POLLIN != 0) {
            int r = read(fd_dvr, b, sizeof(b));
            if (r == TS_SIZE) {
               if (*b == TS_SYNC_BYTE) {
                  // We're locked on to a TS packet
                  int Pid = (((uint16_t)b[1] & PID_MASK_HI) << 8) | b[2];
                  // Distribute the packet to all attached receivers:
                  Lock();
                  for (int i = 0; i < MAXRECEIVERS; i++) {
                      if (receiver[i] && receiver[i]->WantsPid(Pid))
                         receiver[i]->Receive(b, TS_SIZE);
                      }
                  Unlock();
                  }
               t = time(NULL);
               }
            else if (r > 0)
               esyslog("ERROR: got incomplete TS packet (%d bytes)", r);//XXX+ TODO do we have to read the rest???
            else if (r < 0) {
               if (FATALERRNO) {
                  if (errno == EBUFFEROVERFLOW) // this error code is not defined in the library
                     esyslog("ERROR: DVB driver buffer overflow on device %d", CardIndex() + 1);
                  else {
                     LOG_ERROR;
                     break;
                     }
                  }
               }
            }

         // Wait for more data to become available:

         poll(&pfd, 1, 100);

         //XXX+ put this into the recorder??? or give the receiver a flag whether it wants this?
         if (time(NULL) - t > MAXBROKENTIMEOUT) {
            esyslog("ERROR: video data stream broken on device %d", CardIndex() + 1);
            cThread::EmergencyExit(true);
            t = time(NULL);
            }
         }
     close(fd_dvr);
     }
  else
     LOG_ERROR_STR(dvrFileName);

  dsyslog("receiver thread ended on device %d (pid=%d)", CardIndex() + 1, getpid());
}

bool cDevice::Attach(cReceiver *Receiver)
{
  //XXX+ check for same transponder???
  if (!Receiver)
     return false;
  if (Receiver->device == this)
     return true;
  StopReplay();
  for (int i = 0; i < MAXRECEIVERS; i++) {
      if (!receiver[i]) {
         //siProcessor->SetStatus(false);//XXX+
         for (int n = 0; n < MAXRECEIVEPIDS; n++) {
             if (Receiver->pids[n])
                AddPid(Receiver->pids[n]);//XXX+ retval!
             else
                break;
             }
         Receiver->Activate(true);
         Lock();
         Receiver->device = this;
         receiver[i] = Receiver;
         Unlock();
         Start();
         return true;
         }
      }
  esyslog("ERROR: no free receiver slot!");
  return false;
}

void cDevice::Detach(cReceiver *Receiver)
{
  if (!Receiver || Receiver->device != this)
     return;
  bool receiversLeft = false;
  for (int i = 0; i < MAXRECEIVERS; i++) {
      if (receiver[i] == Receiver) {
         Receiver->Activate(false);
         Lock();
         receiver[i] = NULL;
         Receiver->device = NULL;
         Unlock();
         for (int n = 0; n < MAXRECEIVEPIDS; n++) {
             if (Receiver->pids[n])
                DelPid(Receiver->pids[n]);
             else
                break;
             }
         }
      else if (receiver[i])
         receiversLeft = true;
      }
  if (!receiversLeft) {
     active = false;
     Cancel(3);
     }
}
