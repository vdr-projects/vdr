/*
 * device.c: The basic device interface
 *
 * See the main source file 'vdr.c' for copyright information and
 * how to reach the author.
 *
 * $Id: device.c 1.13 2002/08/25 09:16:51 kls Exp $
 */

#include "device.h"
#include <errno.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include "eit.h"
#include "player.h"
#include "receiver.h"
#include "status.h"
#include "transfer.h"

// The default priority for non-primary devices:
#define DEFAULTPRIORITY  -2

// The maximum time we wait before assuming that a recorded video data stream
// is broken:
#define MAXBROKENTIMEOUT 30 // seconds

int cDevice::numDevices = 0;
int cDevice::useDevice = 0;
int cDevice::nextCardIndex = 0;
cDevice *cDevice::device[MAXDEVICES] = { NULL };
cDevice *cDevice::primaryDevice = NULL;

cDevice::cDevice(void)
{
  cardIndex = nextCardIndex++;

  SetVideoFormat(Setup.VideoFormat);

  active = false;

  currentChannel = 0;

  mute = false;
  volume = Setup.CurrentVolume;

  player = NULL;

  for (int i = 0; i < MAXRECEIVERS; i++)
      receiver[i] = NULL;
  ca = -1;

  if (numDevices < MAXDEVICES) {
     device[numDevices++] = this;
     SetCaCaps(cardIndex);
     }
  else
     esyslog("ERROR: too many devices!");
}

cDevice::~cDevice()
{
  Detach(player);
  for (int i = 0; i < MAXRECEIVERS; i++)
      Detach(receiver[i]);
}

void cDevice::SetUseDevice(int n)
{
  if (n < MAXDEVICES)
     useDevice |= (1 << n);
}

int cDevice::NextCardIndex(int n)
{
  if (n > 0) {
     nextCardIndex += n;
     if (nextCardIndex >= MAXDEVICES)
        esyslog("ERROR: nextCardIndex too big (%d)", nextCardIndex);
     }
  else if (n < 0)
     esyslog("ERROR: illegal value in IncCardIndex(%d)", n);
  return nextCardIndex;
}

void cDevice::MakePrimaryDevice(bool On)
{
}

bool cDevice::SetPrimaryDevice(int n)
{
  n--;
  if (0 <= n && n < numDevices && device[n]) {
     isyslog("setting primary device to %d", n + 1);
     if (primaryDevice)
        primaryDevice->MakePrimaryDevice(false);
     primaryDevice = device[n];
     primaryDevice->MakePrimaryDevice(true);
     return true;
     }
  esyslog("invalid primary device number: %d", n + 1);
  return false;
}

bool cDevice::CanBeReUsed(int Frequency, int Vpid)
{
  return false;
}

bool cDevice::HasDecoder(void) const
{
  return false;
}

cOsdBase *cDevice::NewOsd(int x, int y)
{
  return NULL;
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
         if (device[i]->CanBeReUsed(Frequency, Vpid)) {
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

void cDevice::SetCaCaps(int Index)
{
  for (int d = 0; d < numDevices; d++) {
      if (Index < 0 || Index == device[d]->CardIndex()) {
         for (int i = 0; i < MAXCACAPS; i++)
             device[d]->caCaps[i] = Setup.CaCaps[device[d]->CardIndex()][i];
         }
      }
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
  return false;
}

void cDevice::SetVideoFormat(bool VideoFormat16_9)
{
}

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
     if (n >= 0) {
        // The Pid is already in use
        if (++pidHandles[n].used == 2 && n <= ptTeletext) {
           // It's a special PID that may have to be switched into "tap" mode
           PRINTPIDS("A");//XXX+
           return SetPid(&pidHandles[n], n, true);
           }
        PRINTPIDS("a");//XXX+
        return true;
        }
     else if (PidType < ptOther) {
        // The Pid is not yet in use and it is a special one
        n = PidType;
        }
     else if (a >= 0) {
        // The Pid is not yet in use and we have a free slot
        n = a;
        }
     else
        esyslog("ERROR: no free slot for PID %d", Pid);
     if (n >= 0) {
        pidHandles[n].pid = Pid;
        pidHandles[n].used = 1;
        PRINTPIDS("C");//XXX+
        return SetPid(&pidHandles[n], n, true);
        }
     }
  return true;
}

void cDevice::DelPid(int Pid)
{
  if (Pid) {
     for (int i = 0; i < MAXPIDHANDLES; i++) {
         if (pidHandles[i].pid == Pid) {
            if (--pidHandles[i].used < 2) {
               SetPid(&pidHandles[i], i, false);
               if (pidHandles[i].used == 0) {
                   pidHandles[i].handle = -1;
                   pidHandles[i].pid = 0;
                   }
               }
            PRINTPIDS("D");//XXX+
            }
         }
     }
}

bool cDevice::SetPid(cPidHandle *Handle, int Type, bool On)
{
  return false;
}

eSetChannelResult cDevice::SetChannel(const cChannel *Channel)
{
  cStatus::MsgChannelSwitch(this, 0);

  StopReplay();

  // Must set this anyway to avoid getting stuck when switching through
  // channels with 'Up' and 'Down' keys:
  currentChannel = Channel->number;

  // If this card can't receive this channel, we must not actually switch
  // the channel here, because that would irritate the driver when we
  // start replaying in Transfer Mode immediately after switching the channel:
  bool NeedsTransferMode = (IsPrimaryDevice() && !ProvidesCa(Channel->ca));

  eSetChannelResult Result = scrOk;

  // If this DVB card can't receive this channel, let's see if we can
  // use the card that actually can receive it and transfer data from there:

  if (NeedsTransferMode) {
     cDevice *CaDevice = GetDevice(Channel->ca, 0);
     if (CaDevice && !CaDevice->Receiving() && CaDevice->SetChannel(Channel) == scrOk)
        cControl::Launch(new cTransferControl(CaDevice, Channel->vpid, Channel->apid1, 0, 0, 0));//XXX+
     else
        Result = scrNoTransfer;
     }
  else if (!SetChannelDevice(Channel))
     Result = scrFailed;

  if (IsPrimaryDevice())
     cSIProcessor::SetCurrentServiceID(Channel->pnr);

  cStatus::MsgChannelSwitch(this, Channel->number);

  return Result;
}

bool cDevice::SetChannelDevice(const cChannel *Channel)
{
  return false;
}

void cDevice::SetVolumeDevice(int Volume)
{
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
  volume = min(max(Absolute ? Volume : volume + Volume, 0), MAXVOLUME);
  SetVolumeDevice(volume);
  cStatus::MsgSetVolume(volume, Absolute);
  if (volume > 0)
     mute = false;
}

bool cDevice::SetPlayMode(ePlayMode PlayMode)
{
  return false;
}

void cDevice::TrickSpeed(int Speed)
{
}

void cDevice::Clear(void)
{
}

void cDevice::Play(void)
{
}

void cDevice::Freeze(void)
{
}

void cDevice::Mute(void)
{
}

void cDevice::StillPicture(const uchar *Data, int Length)
{
}

bool cDevice::Replaying(void)
{
  return player != NULL;
}

bool cDevice::AttachPlayer(cPlayer *Player)
{
  if (Receiving()) {
     esyslog("ERROR: attempt to attach a cPlayer while receiving on device %d - ignored", CardIndex() + 1);
     return false;
     }
  if (HasDecoder()) {
     if (player)
        Detach(player);
     player = Player;
     player->device = this;
     SetPlayMode(player->playMode);
     player->Activate(true);
     return true;
     }
  return false;
}

void cDevice::Detach(cPlayer *Player)
{
  if (Player && player == Player) {
     player->Activate(false);
     player->device = NULL;
     player = NULL;
     SetPlayMode(pmNone);
     }
}

void cDevice::StopReplay(void)
{
  if (player) {
     Detach(player);
     if (IsPrimaryDevice())
        cControl::Shutdown();
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

bool cDevice::Poll(cPoller &Poller, int TimeoutMs)
{
  return false;
}

int cDevice::PlayVideo(const uchar *Data, int Length)
{
  return -1;
}

int cDevice::PlayAudio(const uchar *Data, int Length)
{
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
  // Test whether a receiver on this device can be shifted to another one
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
      if (receiver[i] && receiver[i]->priority >= 0) // cReceiver with priority < 0 doesn't count
         return true;
      }
  return false;
}

void cDevice::Action(void)
{
  dsyslog("receiver thread started on device %d (pid=%d)", CardIndex() + 1, getpid());

  if (OpenDvr()) {
     uchar b[TS_SIZE];
     time_t t = time(NULL);
     active = true;
     for (; active;) {
         // Read data from the DVR device:
         int r = GetTSPacket(b);
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
            esyslog("ERROR: got incomplete TS packet (%d bytes) on device %d", r, CardIndex() + 1);//XXX+ TODO do we have to read the rest???
         else if (r < 0)
            break;

         //XXX+ put this into the recorder??? or give the receiver a flag whether it wants this?
         if (time(NULL) - t > MAXBROKENTIMEOUT) {
            esyslog("ERROR: video data stream broken on device %d", CardIndex() + 1);
            cThread::EmergencyExit(true);
            t = time(NULL);
            }
         }
     CloseDvr();
     }

  dsyslog("receiver thread ended on device %d (pid=%d)", CardIndex() + 1, getpid());
}

bool cDevice::OpenDvr(void)
{
  return false;
}

void cDevice::CloseDvr(void)
{
}

int cDevice::GetTSPacket(uchar *Data)
{
  return -1;
}

bool cDevice::AttachReceiver(cReceiver *Receiver)
{
  //XXX+ check for same transponder???
  if (!Receiver)
     return false;
  if (Receiver->device == this)
     return true;
  StopReplay();
  for (int i = 0; i < MAXRECEIVERS; i++) {
      if (!receiver[i]) {
         for (int n = 0; n < MAXRECEIVEPIDS; n++)
             AddPid(Receiver->pids[n]);//XXX+ retval!
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
         for (int n = 0; n < MAXRECEIVEPIDS; n++)
             DelPid(Receiver->pids[n]);
         }
      else if (receiver[i])
         receiversLeft = true;
      }
  if (!receiversLeft) {
     active = false;
     Cancel(3);
     }
}
