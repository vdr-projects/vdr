/*
 * device.c: The basic device interface
 *
 * See the main source file 'vdr.c' for copyright information and
 * how to reach the author.
 *
 * $Id: device.c 1.44 2003/05/25 10:57:59 kls Exp $
 */

#include "device.h"
#include <errno.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include "audio.h"
#include "channels.h"
#include "eit.h"
#include "i18n.h"
#include "player.h"
#include "receiver.h"
#include "status.h"
#include "transfer.h"

// --- cDevice ---------------------------------------------------------------

// The default priority for non-primary devices:
#define DEFAULTPRIORITY  -1

int cDevice::numDevices = 0;
int cDevice::useDevice = 0;
int cDevice::nextCardIndex = 0;
int cDevice::currentChannel = 0;
cDevice *cDevice::device[MAXDEVICES] = { NULL };
cDevice *cDevice::primaryDevice = NULL;

cDevice::cDevice(void)
{
  cardIndex = nextCardIndex++;

  SetVideoFormat(Setup.VideoFormat);

  active = false;

  mute = false;
  volume = Setup.CurrentVolume;

  ciHandler = NULL;
  player = NULL;

  for (int i = 0; i < MAXRECEIVERS; i++)
      receiver[i] = NULL;

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
  delete ciHandler;
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

int cDevice::DeviceNumber(void) const
{
  for (int i = 0; i < numDevices; i++) {
      if (device[i] == this)
         return i;
      }
  return -1;
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
  esyslog("ERROR: invalid primary device number: %d", n + 1);
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

cSpuDecoder *cDevice::GetSpuDecoder(void)
{
  return NULL;
}

cDevice *cDevice::ActualDevice(void)
{
  cDevice *d = cTransferControl::ReceiverDevice();
  if (!d)
     d = PrimaryDevice();
  return d;
}

cDevice *cDevice::GetDevice(int Index)
{
  return (0 <= Index && Index < numDevices) ? device[Index] : NULL;
}

cDevice *cDevice::GetDevice(const cChannel *Channel, int Priority, bool *NeedsDetachReceivers)
{
  cDevice *d = NULL;
  for (int i = 0; i < numDevices; i++) {
      bool ndr;
      if (device[i]->ProvidesChannel(Channel, Priority, &ndr) // this device is basicly able to do the job
         && (!d // we don't have a device yet, or...
            || (device[i]->Receiving() && !ndr) // ...this one is already receiving and allows additional receivers, or...
            || !d->Receiving() // ...the one we have is not receiving...
               && (device[i]->Priority() < d->Priority() // ...this one has an even lower Priority, or...
                  || device[i]->Priority() == d->Priority() // ...same Priority...
                     && device[i]->ProvidesCa(Channel->Ca()) < d->ProvidesCa(Channel->Ca()) // ...but this one provides fewer Ca values
                  )
            )
         ) {
         d = device[i];
         if (NeedsDetachReceivers)
            *NeedsDetachReceivers = ndr;
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

//#define PRINTPIDS(s) { char b[500]; char *q = b; q += sprintf(q, "%d %s ", CardIndex(), s); for (int i = 0; i < MAXPIDHANDLES; i++) q += sprintf(q, " %s%4d %d", i == ptOther ? "* " : "", pidHandles[i].pid, pidHandles[i].used); dsyslog(b); }
#define PRINTPIDS(s)

bool cDevice::HasPid(int Pid) const
{
  for (int i = 0; i < MAXPIDHANDLES; i++) {
      if (pidHandles[i].pid == Pid)
         return true;
      }
  return false;
}

bool cDevice::AddPid(int Pid, ePidType PidType)
{
  if (Pid || PidType == ptPcr) {
     int n = -1;
     int a = -1;
     if (PidType != ptPcr) { // PPID always has to be explicit
        for (int i = 0; i < MAXPIDHANDLES; i++) {
            if (i != ptPcr) {
               if (pidHandles[i].pid == Pid)
                  n = i;
               else if (a < 0 && i >= ptOther && !pidHandles[i].used)
                  a = i;
               }
            }
        }
     if (n >= 0) {
        // The Pid is already in use
        if (++pidHandles[n].used == 2 && n <= ptTeletext) {
           // It's a special PID that may have to be switched into "tap" mode
           PRINTPIDS("A");
           return SetPid(&pidHandles[n], n, true);
           }
        PRINTPIDS("a");
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
        PRINTPIDS("C");
        return SetPid(&pidHandles[n], n, true);
        }
     }
  return true;
}

void cDevice::DelPid(int Pid, ePidType PidType)
{
  if (Pid || PidType == ptPcr) {
     int n = -1;
     if (PidType == ptPcr)
        n = PidType; // PPID always has to be explicit
     else {
        for (int i = 0; i < MAXPIDHANDLES; i++) {
            if (pidHandles[i].pid == Pid) {
               n = i;
               break;
               }
            }
        }
     if (n >= 0 && pidHandles[n].used) {
        PRINTPIDS("D");
        if (--pidHandles[n].used < 2) {
           SetPid(&pidHandles[n], n, false);
           if (pidHandles[n].used == 0) {
              pidHandles[n].handle = -1;
              pidHandles[n].pid = 0;
              }
           }
        PRINTPIDS("E");
        }
     }
}

bool cDevice::SetPid(cPidHandle *Handle, int Type, bool On)
{
  return false;
}

bool cDevice::ProvidesSource(int Source) const
{
  return false;
}

bool cDevice::ProvidesChannel(const cChannel *Channel, int Priority, bool *NeedsDetachReceivers) const
{
  return false;
}

bool cDevice::SwitchChannel(const cChannel *Channel, bool LiveView)
{
  if (LiveView)
     isyslog("switching to channel %d", Channel->Number());
  for (int i = 3; i--;) {
      switch (SetChannel(Channel, LiveView)) {
        case scrOk:           return true;
        case scrNotAvailable: if (Interface)
                                 Interface->Error(tr("Channel not available!"));
                              return false;
        case scrNoTransfer:   if (Interface)
                                 Interface->Error(tr("Can't start Transfer Mode!"));
                              return false;
        case scrFailed:       break; // loop will retry
        }
      esyslog("retrying");
      }
  return false;
}

bool cDevice::SwitchChannel(int Direction)
{
  bool result = false;
  Direction = sgn(Direction);
  if (Direction) {
     int n = CurrentChannel() + Direction;
     int first = n;
     cChannel *channel;
     while ((channel = Channels.GetByNumber(n, Direction)) != NULL) {
           // try only channels which are currently available
           if (PrimaryDevice()->ProvidesChannel(channel, Setup.PrimaryLimit) || PrimaryDevice()->CanReplay() && GetDevice(channel, 0))
              break;
           n = channel->Number() + Direction;
           }
     if (channel) {
        int d = n - first;
        if (abs(d) == 1)
           dsyslog("skipped channel %d", first);
        else if (d)
           dsyslog("skipped channels %d..%d", first, n - sgn(d));
        if (PrimaryDevice()->SwitchChannel(channel, true))
           result = true;
        }
     else if (n != first && Interface)
        Interface->Error(tr("Channel not available!"));
     }
  return result;
}

eSetChannelResult cDevice::SetChannel(const cChannel *Channel, bool LiveView)
{
  if (LiveView)
     StopReplay();

  // If this card can't receive this channel, we must not actually switch
  // the channel here, because that would irritate the driver when we
  // start replaying in Transfer Mode immediately after switching the channel:
  bool NeedsTransferMode = (LiveView && IsPrimaryDevice() && !ProvidesChannel(Channel, Setup.PrimaryLimit));

  eSetChannelResult Result = scrOk;

  // If this DVB card can't receive this channel, let's see if we can
  // use the card that actually can receive it and transfer data from there:

  if (NeedsTransferMode) {
     cDevice *CaDevice = GetDevice(Channel, 0);
     if (CaDevice && CanReplay()) {
        cStatus::MsgChannelSwitch(this, 0); // only report status if we are actually going to switch the channel
        if (CaDevice->SetChannel(Channel, false) == scrOk) // calling SetChannel() directly, not SwitchChannel()!
           cControl::Launch(new cTransferControl(CaDevice, Channel->Vpid(), Channel->Apid1(), Channel->Apid2(), Channel->Dpid1(), Channel->Dpid2()));//XXX+
        else
           Result = scrNoTransfer;
        }
     else
        Result = scrNotAvailable;
     }
  else {
     cStatus::MsgChannelSwitch(this, 0); // only report status if we are actually going to switch the channel
     if (!SetChannelDevice(Channel, LiveView))
        Result = scrFailed;
     }

  if (Result == scrOk) {
     if (LiveView && IsPrimaryDevice()) {
        cSIProcessor::SetCurrentChannelID(Channel->GetChannelID());
        currentChannel = Channel->Number();
        }
     cStatus::MsgChannelSwitch(this, Channel->Number()); // only report status if channel switch successfull
     }

  return Result;
}

bool cDevice::SetChannelDevice(const cChannel *Channel, bool LiveView)
{
  return false;
}

bool cDevice::HasProgramme(void)
{
  return Replaying() || pidHandles[ptAudio].pid || pidHandles[ptVideo].pid;
}

void cDevice::SetVolumeDevice(int Volume)
{
}

int cDevice::NumAudioTracksDevice(void) const
{
  return 0;
}

const char **cDevice::GetAudioTracksDevice(int *CurrentTrack) const
{
  return NULL;
}

void cDevice::SetAudioTrackDevice(int Index)
{
}

bool cDevice::ToggleMute(void)
{
  int OldVolume = volume;
  mute = !mute;
  //XXX why is it necessary to use different sequences???
  if (mute) {
     SetVolume(0, mute);
     Audios.MuteAudio(mute); // Mute external audio after analog audio
     }
  else {
     Audios.MuteAudio(mute); // Enable external audio before analog audio
     SetVolume(0, mute);
     }
  volume = OldVolume;
  return mute;
}

void cDevice::SetVolume(int Volume, bool Absolute)
{
  volume = min(max(Absolute ? Volume : volume + Volume, 0), MAXVOLUME);
  SetVolumeDevice(volume);
  cStatus::MsgSetVolume(volume, Absolute);
  if (volume > 0) {
     mute = false;
     Audios.MuteAudio(mute);
     }
}

int cDevice::NumAudioTracks(void) const
{
  return player ? player->NumAudioTracks() : NumAudioTracksDevice();
}

const char **cDevice::GetAudioTracks(int *CurrentTrack) const
{
  return player ? player->GetAudioTracks(CurrentTrack) : GetAudioTracksDevice(CurrentTrack);
}

void cDevice::SetAudioTrack(int Index)
{
  if (player)
     player->SetAudioTrack(Index);
  else
     SetAudioTrackDevice(Index);
}

bool cDevice::CanReplay(void) const
{
  return HasDecoder();
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
  Audios.ClearAudio();
}

void cDevice::Play(void)
{
  Audios.MuteAudio(mute);
}

void cDevice::Freeze(void)
{
  Audios.MuteAudio(true);
}

void cDevice::Mute(void)
{
  Audios.MuteAudio(true);
}

void cDevice::StillPicture(const uchar *Data, int Length)
{
}

bool cDevice::Replaying(void) const
{
  return player != NULL;
}

bool cDevice::AttachPlayer(cPlayer *Player)
{
  if (CanReplay()) {
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
     Audios.ClearAudio();
     }
}

void cDevice::StopReplay(void)
{
  if (player) {
     Detach(player);
     if (IsPrimaryDevice())
        cControl::Shutdown();
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

void cDevice::PlayAudio(const uchar *Data, int Length)
{
  Audios.PlayAudio(Data, Length);
}

int cDevice::Ca(void) const
{
  int ca = 0;
  for (int i = 0; i < MAXRECEIVERS; i++) {
      if (receiver[i] && (ca = receiver[i]->ca) != 0)
         break; // all receivers have the same ca
      }
  return ca;
}

int cDevice::Priority(void) const
{
  int priority = IsPrimaryDevice() ? Setup.PrimaryLimit - 1 : DEFAULTPRIORITY;
  for (int i = 0; i < MAXRECEIVERS; i++) {
      if (receiver[i])
         priority = max(receiver[i]->priority, priority);
      }
  return priority;
}

int cDevice::CanShift(int Ca, int Priority, int UsedCards) const
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

int cDevice::ProvidesCa(int Ca) const
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

bool cDevice::Receiving(bool CheckAny) const
{
  for (int i = 0; i < MAXRECEIVERS; i++) {
      if (receiver[i] && (CheckAny || receiver[i]->priority >= 0)) // cReceiver with priority < 0 doesn't count
         return true;
      }
  return false;
}

void cDevice::Action(void)
{
  dsyslog("receiver thread started on device %d (pid=%d)", CardIndex() + 1, getpid());

  if (OpenDvr()) {
     active = true;
     for (; active;) {
         // Read data from the DVR device:
         uchar *b = NULL;
         if (GetTSPacket(b)) {
            if (b) {
               int Pid = (((uint16_t)b[1] & PID_MASK_HI) << 8) | b[2];
               // Distribute the packet to all attached receivers:
               Lock();
               for (int i = 0; i < MAXRECEIVERS; i++) {
                   if (receiver[i] && receiver[i]->WantsPid(Pid))
                      receiver[i]->Receive(b, TS_SIZE);
                   }
               Unlock();
               }
            }
         else
            break;
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

bool cDevice::GetTSPacket(uchar *&Data)
{
  return false;
}

bool cDevice::AttachReceiver(cReceiver *Receiver)
{
  if (!Receiver)
     return false;
  if (Receiver->device == this)
     return true;
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

// --- cTSBuffer -------------------------------------------------------------

cTSBuffer::cTSBuffer(int File, int Size, int CardIndex)
{
  f = File;
  size = Size / TS_SIZE * TS_SIZE;
  cardIndex = CardIndex;
  tsRead = tsWrite = 0;
  buf = (f >= 0 && size >= TS_SIZE) ? MALLOC(uchar, size + TS_SIZE) : NULL;
  // the '+ TS_SIZE' allocates some extra space for handling packets that got split by a buffer roll-over
  firstRead = true;
}

cTSBuffer::~cTSBuffer()
{
  free(buf);
}

int cTSBuffer::Read(void)
{
  if (buf) {
     cPoller Poller(f, false);
     bool repeat;
     int total = 0;
     do {
        repeat = false;
        if (firstRead || Used() > TS_SIZE || Poller.Poll(100)) { // only wait if there's not enough data in the buffer
           firstRead = false;
           if (tsRead == tsWrite)
              tsRead = tsWrite = 0; // keep the maximum buffer space available
           if (tsWrite >= size && tsRead > 0)
              tsWrite = 0;
           int free = tsRead <= tsWrite ? size - tsWrite : tsRead - tsWrite - 1;
           if (free > 0) {
              int r = read(f, buf + tsWrite, free);
              if (r > 0) {
                 total += r;
                 tsWrite += r;
                 if (tsWrite >= size && tsRead > 0) {
                    tsWrite = 0;
                    repeat = true; // read again after a boundary roll-over
                    }
                 }
              }
           }
        } while (repeat);
     return total;
     }
  return -1;
}

uchar *cTSBuffer::Get(void)
{
  if (Used() >= TS_SIZE) {
     uchar *p = buf + tsRead;
     if (*p != TS_SYNC_BYTE) {
        esyslog("ERROR: not sync'ed to TS packet on device %d", cardIndex);
        int tsMax = tsRead < tsWrite ? tsWrite : size;
        for (int i = tsRead; i < tsMax; i++) {
            if (buf[i] == TS_SYNC_BYTE) {
               esyslog("ERROR: skipped %d bytes to sync on TS packet on device %d", i - tsRead, cardIndex);
               tsRead = i;
               return NULL;
               }
            }
        if ((tsRead = tsMax) >= size)
           tsRead = 0;
        return NULL;
        }
     if (tsRead + TS_SIZE > size) {
        // the packet rolled over the buffer boundary, so let's fetch the rest from the beginning (which MUST be there, since Used() >= TS_SIZE)
        int rest = TS_SIZE - (size - tsRead);
        memcpy(buf + size, buf, rest);
        tsRead = rest;
        }
     else if ((tsRead += TS_SIZE) >= size)
        tsRead = 0;
     return p;
     }
  return NULL;
}
