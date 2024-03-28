/*
 * device.c: The basic device interface
 *
 * See the main source file 'vdr.c' for copyright information and
 * how to reach the author.
 *
 * $Id: device.c 5.12 2024/03/28 13:02:42 kls Exp $
 */

#include "device.h"
#include <errno.h>
#include <math.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include "audio.h"
#include "channels.h"
#include "i18n.h"
#include "player.h"
#include "receiver.h"
#include "status.h"
#include "transfer.h"

// --- cLiveSubtitle ---------------------------------------------------------

class cLiveSubtitle : public cReceiver {
protected:
  virtual void Receive(const uchar *Data, int Length);
public:
  cLiveSubtitle(int SPid);
  virtual ~cLiveSubtitle();
  };

cLiveSubtitle::cLiveSubtitle(int SPid)
{
  AddPid(SPid);
}

cLiveSubtitle::~cLiveSubtitle()
{
  cReceiver::Detach();
}

void cLiveSubtitle::Receive(const uchar *Data, int Length)
{
  if (cDevice::PrimaryDevice())
     cDevice::PrimaryDevice()->PlayTs(Data, Length);
}

// --- cDeviceHook -----------------------------------------------------------

cDeviceHook::cDeviceHook(void)
{
  cDevice::deviceHooks.Add(this);
}

bool cDeviceHook::DeviceProvidesTransponder(const cDevice *Device, const cChannel *Channel) const
{
  return true;
}

bool cDeviceHook::DeviceProvidesEIT(const cDevice *Device) const
{
  return true;
}

// --- cDevice ---------------------------------------------------------------

// The minimum number of unknown PS1 packets to consider this a "pre 1.3.19 private stream":
#define MIN_PRE_1_3_19_PRIVATESTREAM 10

int cDevice::numDevices = 0;
int cDevice::useDevice = 0;
int cDevice::nextCardIndex = 0;
int cDevice::currentChannel = 1;
cDevice *cDevice::device[MAXDEVICES] = { NULL };
cDevice *cDevice::primaryDevice = NULL;
cList<cDeviceHook> cDevice::deviceHooks;

cDevice::cDevice(void)
:patPmtParser(true)
{
  cardIndex = nextCardIndex++;
  dsyslog("new device number %d (card index %d)", numDevices + 1, CardIndex() + 1);

  SetDescription("device %d receiver", numDevices + 1);

  mute = false;
  volume = Setup.CurrentVolume;

  sectionHandler = NULL;
  eitFilter = NULL;
  patFilter = NULL;
  sdtFilter = NULL;
  nitFilter = NULL;

  camSlot = NULL;

  occupiedTimeout = 0;

  player = NULL;
  isPlayingVideo = false;
  keepTracks = false; // used in ClrAvailableTracks()!
  ClrAvailableTracks();
  currentAudioTrack = ttNone;
  currentAudioTrackMissingCount = 0;
  currentSubtitleTrack = ttNone;
  liveSubtitle = NULL;
  dvbSubtitleConverter = NULL;
  autoSelectPreferredSubtitleLanguage = true;

  for (int i = 0; i < MAXRECEIVERS; i++)
      receiver[i] = NULL;

  if (numDevices < MAXDEVICES)
     device[numDevices++] = this;
  else
     esyslog("ERROR: too many devices!");
}

cDevice::~cDevice()
{
  Detach(player);
  DetachAllReceivers();
  delete liveSubtitle;
  delete dvbSubtitleConverter;
  if (this == primaryDevice)
     primaryDevice = NULL;
  Cancel(3);
}

bool cDevice::WaitForAllDevicesReady(int Timeout)
{
  for (time_t t0 = time(NULL); time(NULL) - t0 < Timeout; ) {
      bool ready = true;
      for (int i = 0; i < numDevices; i++) {
          if (device[i] && !device[i]->Ready()) {
             ready = false;
             cCondWait::SleepMs(100);
             }
          }
      if (ready)
         return true;
      }
  return false;
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
     esyslog("ERROR: invalid value in nextCardIndex(%d)", n);
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

cString cDevice::DeviceType(void) const
{
  return "";
}

cString cDevice::DeviceName(void) const
{
  return "";
}

void cDevice::MakePrimaryDevice(bool On)
{
  if (!On) {
     DELETENULL(liveSubtitle);
     DELETENULL(dvbSubtitleConverter);
     }
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
     primaryDevice->SetVideoFormat(Setup.VideoFormat);
     primaryDevice->SetVolumeDevice(Setup.CurrentVolume);
     Setup.PrimaryDVB = n + 1;
     return true;
     }
  esyslog("ERROR: invalid primary device number: %d", n + 1);
  return false;
}

bool cDevice::HasDecoder(void) const
{
  return false;
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

static int GetClippedNumProvidedSystems(int AvailableBits, cDevice *Device)
{
  int MaxNumProvidedSystems = (1 << AvailableBits) - 1;
  int NumProvidedSystems = Device->NumProvidedSystems();
  if (NumProvidedSystems > MaxNumProvidedSystems) {
     esyslog("ERROR: device %d supports %d modulation systems but cDevice::GetDevice() currently only supports %d delivery systems which should be fixed", Device->DeviceNumber() + 1, NumProvidedSystems, MaxNumProvidedSystems);
     NumProvidedSystems = MaxNumProvidedSystems;
     }
  else if (NumProvidedSystems <= 0) {
     esyslog("ERROR: device %d reported an invalid number (%d) of supported delivery systems - assuming 1", Device->DeviceNumber() + 1, NumProvidedSystems);
     NumProvidedSystems = 1;
     }
  return NumProvidedSystems;
}

cDevice *cDevice::GetDevice(const cChannel *Channel, int Priority, bool LiveView, bool Query)
{
  // Collect the current priorities of all CAM slots that can decrypt the channel:
  int NumCamSlots = CamSlots.Count();
  int SlotPriority[NumCamSlots + 1]; // +1 to avoid a zero sized array in case there are no CAM slots
  int NumUsableSlots = 0;
  bool InternalCamNeeded = false;
  if (Channel->Ca() >= CA_ENCRYPTED_MIN) {
     for (cCamSlot *CamSlot = CamSlots.First(); CamSlot; CamSlot = CamSlots.Next(CamSlot)) {
         SlotPriority[CamSlot->Index()] = MAXPRIORITY + 1; // assumes it can't be used
         if (CamSlot->ModuleStatus() == msReady) {
            if (CamSlot->ProvidesCa(Channel->Caids())) {
               if (!ChannelCamRelations.CamChecked(Channel->GetChannelID(), CamSlot->MasterSlotNumber())) {
                  SlotPriority[CamSlot->Index()] = CamSlot->MtdActive() ? IDLEPRIORITY : CamSlot->Priority(); // we don't need to take the priority into account here for MTD CAM slots, because they can be used with several devices in parallel
                  NumUsableSlots++;
                  }
               }
            }
         }
     if (!NumUsableSlots)
        InternalCamNeeded = true; // no CAM is able to decrypt this channel
     }

  bool NeedsDetachReceivers = false;
  cDevice *d = NULL;
  cCamSlot *s = NULL;

  uint32_t Impact = 0xFFFFFFFF; // we're looking for a device with the least impact
  for (int j = 0; j < NumCamSlots || !NumUsableSlots; j++) {
      if (NumUsableSlots && SlotPriority[j] > MAXPRIORITY)
         continue; // there is no CAM available in this slot
      for (int i = 0; i < numDevices; i++) {
          if (Channel->Ca() && Channel->Ca() <= CA_DVB_MAX && Channel->Ca() != device[i]->DeviceNumber() + 1)
             continue; // a specific card was requested, but not this one
          bool HasInternalCam = device[i]->HasInternalCam();
          if (InternalCamNeeded && !HasInternalCam)
             continue; // no CAM is able to decrypt this channel and the device uses vdr handled CAMs
          if (NumUsableSlots && !HasInternalCam && !CamSlots.Get(j)->Assign(device[i], true))
             continue; // CAM slot can't be used with this device
          bool ndr = false;
          if (device[i]->ProvidesChannel(Channel, Priority, &ndr)) { // this device is basically able to do the job
             if (NumUsableSlots && !HasInternalCam) {
                if (cCamSlot *csi = device[i]->CamSlot()) {
                   cCamSlot *csj = CamSlots.Get(j);
                   if ((csj->MtdActive() ? csi->MasterSlot() : csi) != csj)
                      ndr = true; // using a different CAM slot requires detaching receivers
                   }
                }
             // Put together an integer number that reflects the "impact" using
             // this device would have on the overall system. Each condition is represented
             // by one bit in the number (or several bits, if the condition is actually
             // a numeric value). The sequence in which the conditions are listed corresponds
             // to their individual severity, where the one listed first will make the most
             // difference, because it results in the most significant bit of the result.
             uint32_t imp = 0;
             imp <<= 1; imp |= (LiveView && NumUsableSlots && !HasInternalCam) ? !ChannelCamRelations.CamDecrypt(Channel->GetChannelID(), CamSlots.Get(j)->MasterSlotNumber()) || ndr : 0; // prefer CAMs that are known to decrypt this channel for live viewing, if we don't need to detach existing receivers
             imp <<= 1; imp |= LiveView ? !device[i]->IsPrimaryDevice() || ndr : 0;                                  // prefer the primary device for live viewing if we don't need to detach existing receivers
             imp <<= 1; imp |= !device[i]->Receiving() && (device[i] != cTransferControl::ReceiverDevice() || device[i]->IsPrimaryDevice()) || ndr; // use receiving devices if we don't need to detach existing receivers, but avoid primary device in local transfer mode
             imp <<= 1; imp |= device[i]->Receiving();                                                               // avoid devices that are receiving
             imp <<= 5; imp |= GetClippedNumProvidedSystems(5, device[i]) - 1;                                       // avoid cards which support multiple delivery systems
             imp <<= 8; imp |= device[i]->Priority() - IDLEPRIORITY;                                                 // use the device with the lowest priority (- IDLEPRIORITY to assure that values -100..99 can be used)
             imp <<= 1; imp |= device[i] == cTransferControl::ReceiverDevice();                                      // avoid the Transfer Mode receiver device
             imp <<= 8; imp |= ((NumUsableSlots && !HasInternalCam) ? SlotPriority[j] : IDLEPRIORITY) - IDLEPRIORITY;// use the CAM slot with the lowest priority (- IDLEPRIORITY to assure that values -100..99 can be used)
             imp <<= 1; imp |= ndr;                                                                                  // avoid devices if we need to detach existing receivers
             imp <<= 1; imp |= (NumUsableSlots || InternalCamNeeded) ? 0 : device[i]->HasCi();                       // avoid cards with Common Interface for FTA channels
             imp <<= 1; imp |= device[i]->AvoidRecording();                                                          // avoid SD full featured cards
             imp <<= 1; imp |= (NumUsableSlots && !HasInternalCam) ? !ChannelCamRelations.CamDecrypt(Channel->GetChannelID(), CamSlots.Get(j)->MasterSlotNumber()) : 0; // prefer CAMs that are known to decrypt this channel
             imp <<= 1; imp |= device[i]->IsPrimaryDevice();                                                         // avoid the primary device
             if (imp < Impact) {
                // This device has less impact than any previous one, so we take it.
                Impact = imp;
                d = device[i];
                NeedsDetachReceivers = ndr;
                if (NumUsableSlots && !HasInternalCam)
                   s = CamSlots.Get(j);
                }
             }
          }
      if (!NumUsableSlots)
         break; // no CAM necessary, so just one loop over the devices
      }
  if (d) {
     if (!Query && NeedsDetachReceivers)
        d->DetachAllReceivers();
     if (s) {
        // Some of the following statements could probably be combined, but let's keep them
        // explicit so we can clearly see every single aspect of the decisions made here.
        if (d->CamSlot()) {
           if (s->MtdActive()) {
              if (s == d->CamSlot()->MasterSlot()) {
                 // device d already has a proper CAM slot, so nothing to do here
                 }
              else {
                 // device d has a CAM slot, but it's not the right one
                 if (!Query) {
                    d->CamSlot()->Assign(NULL);
                    s = s->MtdSpawn();
                    s->Assign(d);
                    }
                 }
              }
           else {
              if (s->Device()) {
                 if (s->Device() != d) {
                    // CAM slot s is currently assigned to a different device than d
                    if (Priority > s->Priority()) {
                       if (!Query) {
                          d->CamSlot()->Assign(NULL);
                          s->Assign(d);
                          }
                       }
                    else {
                       d = NULL;
                       s = NULL;
                       }
                    }
                 else {
                    // device d already has a proper CAM slot, so nothing to do here
                    }
                 }
              else {
                 if (s != d->CamSlot()) {
                    // device d has a CAM slot, but it's not the right one
                    if (!Query) {
                       d->CamSlot()->Assign(NULL);
                       s->Assign(d);
                       }
                    }
                 else {
                    // device d already has a proper CAM slot, so nothing to do here
                    }
                 }
              }
           }
        else {
           // device d has no CAM slot, ...
           if (s->MtdActive()) {
              // ... so we assign s with MTD support
              if (!Query) {
                 s = s->MtdSpawn();
                 s->Assign(d);
                 }
              }
           else {
              // CAM slot s has no MTD support ...
              if (s->Device()) {
                 // ... but it is assigned to a different device, so we reassign it to d
                 if (Priority > s->Priority()) {
                    if (!Query) {
                       s->Device()->DetachAllReceivers();
                       s->Assign(d);
                       }
                    }
                 else {
                    d = NULL;
                    s = NULL;
                    }
                 }
              else {
                 // ... and is not assigned to any device, so we just assign it to d
                 if (!Query)
                    s->Assign(d);
                 }
              }
           }
        }
     else if (d->CamSlot() && !d->CamSlot()->IsDecrypting())
        d->CamSlot()->Assign(NULL);
     }
  return d;
}

cDevice *cDevice::GetDeviceForTransponder(const cChannel *Channel, int Priority)
{
  cDevice *Device = NULL;
  for (int i = 0; i < cDevice::NumDevices(); i++) {
      if (cDevice *d = cDevice::GetDevice(i)) {
         if (d->IsTunedToTransponder(Channel))
            return d; // if any device is tuned to the transponder, we're done
         if (d->ProvidesTransponder(Channel)) {
            if (d->MaySwitchTransponder(Channel))
               return d; // this device may switch to the transponder without disturbing any receiver or live view
            else if (!d->Occupied() && !d->IsBonded()) { // MaySwitchTransponder() implicitly calls Occupied()
               if (d->Priority() < Priority && (!Device || d->Priority() < Device->Priority()))
                  Device = d; // use this one only if no other with less impact can be found
               }
            }
         }
      }
  return Device;
}

void cDevice::ReleaseCamSlot(void)
{
  if (camSlot && !camSlot->IsDecrypting() && !camSlot->IsActivating())
     camSlot->Assign(NULL);
}

bool cDevice::HasCi(void)
{
  return false;
}

void cDevice::SetCamSlot(cCamSlot *CamSlot)
{
  LOCK_THREAD;
  camSlot = CamSlot;
}

void cDevice::Shutdown(void)
{
  deviceHooks.Clear();
  for (int i = 0; i < numDevices; i++) {
      delete device[i];
      device[i] = NULL;
      }
}

uchar *cDevice::GrabImage(int &Size, bool Jpeg, int Quality, int SizeX, int SizeY)
{
  return NULL;
}

bool cDevice::GrabImageFile(const char *FileName, bool Jpeg, int Quality, int SizeX, int SizeY)
{
  int result = 0;
  int fd = open(FileName, O_WRONLY | O_CREAT | O_NOFOLLOW | O_TRUNC, DEFFILEMODE);
  if (fd >= 0) {
     int ImageSize;
     uchar *Image = GrabImage(ImageSize, Jpeg, Quality, SizeX, SizeY);
     if (Image) {
        if (safe_write(fd, Image, ImageSize) == ImageSize)
           isyslog("grabbed image to %s", FileName);
        else {
           LOG_ERROR_STR(FileName);
           result |= 1;
           }
        free(Image);
        }
     else
        result |= 1;
     close(fd);
     }
  else {
     LOG_ERROR_STR(FileName);
     result |= 1;
     }
  return result == 0;
}

void cDevice::SetVideoDisplayFormat(eVideoDisplayFormat VideoDisplayFormat)
{
  cSpuDecoder *spuDecoder = GetSpuDecoder();
  if (spuDecoder) {
     if (Setup.VideoFormat)
        spuDecoder->setScaleMode(cSpuDecoder::eSpuNormal);
     else {
        switch (VideoDisplayFormat) {
               case vdfPanAndScan:
                    spuDecoder->setScaleMode(cSpuDecoder::eSpuPanAndScan);
                    break;
               case vdfLetterBox:
                    spuDecoder->setScaleMode(cSpuDecoder::eSpuLetterBox);
                    break;
               case vdfCenterCutOut:
                    spuDecoder->setScaleMode(cSpuDecoder::eSpuNormal);
                    break;
               default: esyslog("ERROR: invalid value for VideoDisplayFormat '%d'", VideoDisplayFormat);
               }
        }
     }
}

void cDevice::SetVideoFormat(bool VideoFormat16_9)
{
}

void cDevice::GetVideoSize(int &Width, int &Height, double &VideoAspect)
{
  Width = 0;
  Height = 0;
  VideoAspect = 1.0;
}

void cDevice::GetOsdSize(int &Width, int &Height, double &PixelAspect)
{
  Width = 720;
  Height = 480;
  PixelAspect = 1.0;
}

//#define PRINTPIDS(s) { char b[500]; char *q = b; q += sprintf(q, "%d %s ", DeviceNumber() + 1, s); for (int i = 0; i < MAXPIDHANDLES; i++) q += sprintf(q, " %s%4d %d", i == ptOther ? "* " : "", pidHandles[i].pid, pidHandles[i].used); dsyslog("%s", b); }
#define PRINTPIDS(s)

bool cDevice::HasPid(int Pid) const
{
  cMutexLock MutexLock(&mutexPids);
  for (int i = 0; i < MAXPIDHANDLES; i++) {
      if (pidHandles[i].pid == Pid)
         return true;
      }
  return false;
}

bool cDevice::AddPid(int Pid, ePidType PidType, int StreamType)
{
  cMutexLock MutexLock(&mutexPids);
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
           if (!SetPid(&pidHandles[n], n, true)) {
              esyslog("ERROR: can't set PID %d on device %d", Pid, DeviceNumber() + 1);
              if (PidType <= ptTeletext)
                 DetachAll(Pid);
              DelPid(Pid, PidType);
              return false;
              }
           if (camSlot)
              camSlot->SetPid(Pid, true);
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
     else {
        esyslog("ERROR: no free slot for PID %d on device %d", Pid, DeviceNumber() + 1);
        return false;
        }
     if (n >= 0) {
        pidHandles[n].pid = Pid;
        pidHandles[n].streamType = StreamType;
        pidHandles[n].used = 1;
        PRINTPIDS("C");
        if (!SetPid(&pidHandles[n], n, true)) {
           esyslog("ERROR: can't set PID %d on device %d", Pid, DeviceNumber() + 1);
           if (PidType <= ptTeletext)
              DetachAll(Pid);
           DelPid(Pid, PidType);
           return false;
           }
        if (camSlot)
           camSlot->SetPid(Pid, true);
        }
     }
  return true;
}

void cDevice::DelPid(int Pid, ePidType PidType)
{
  cMutexLock MutexLock(&mutexPids);
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
              if (camSlot)
                 camSlot->SetPid(Pid, false);
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

void cDevice::DelLivePids(void)
{
  cMutexLock MutexLock(&mutexPids);
  for (int i = ptAudio; i < ptOther; i++) {
      if (pidHandles[i].pid)
         DelPid(pidHandles[i].pid, ePidType(i));
      }
}

void cDevice::StartSectionHandler(void)
{
  if (!sectionHandler) {
     sectionHandler = new cSectionHandler(this);
     AttachFilter(eitFilter = new cEitFilter);
     AttachFilter(patFilter = new cPatFilter);
     AttachFilter(sdtFilter = new cSdtFilter(patFilter));
     AttachFilter(nitFilter = new cNitFilter(sdtFilter));
     }
}

void cDevice::StopSectionHandler(void)
{
  if (sectionHandler) {
     delete sectionHandler; // automatically detaches filters
     delete nitFilter;
     delete sdtFilter;
     delete patFilter;
     delete eitFilter;
     nitFilter = NULL;
     sdtFilter = NULL;
     patFilter = NULL;
     eitFilter = NULL;
     sectionHandler = NULL;
     }
}

int cDevice::OpenFilter(u_short Pid, u_char Tid, u_char Mask)
{
  return -1;
}

int cDevice::ReadFilter(int Handle, void *Buffer, size_t Length)
{
  return safe_read(Handle, Buffer, Length);
}

void cDevice::CloseFilter(int Handle)
{
  close(Handle);
}

void cDevice::AttachFilter(cFilter *Filter)
{
  if (sectionHandler)
     sectionHandler->Attach(Filter);
}

void cDevice::Detach(cFilter *Filter)
{
  if (sectionHandler)
     sectionHandler->Detach(Filter);
}

bool cDevice::ProvidesSource(int Source) const
{
  return false;
}

bool cDevice::DeviceHooksProvidesTransponder(const cChannel *Channel) const
{
  cDeviceHook *Hook = deviceHooks.First();
  while (Hook) {
        if (!Hook->DeviceProvidesTransponder(this, Channel))
           return false;
        Hook = deviceHooks.Next(Hook);
        }
  return true;
}

bool cDevice::DeviceHooksProvidesEIT(void) const
{
  cDeviceHook *Hook = deviceHooks.First();
  while (Hook) {
        if (!Hook->DeviceProvidesEIT(this))
           return false;
        Hook = deviceHooks.Next(Hook);
        }
  return true;
}

bool cDevice::ProvidesTransponder(const cChannel *Channel) const
{
  return false;
}

bool cDevice::ProvidesTransponderExclusively(const cChannel *Channel) const
{
  for (int i = 0; i < numDevices; i++) {
      if (device[i] && device[i] != this && device[i]->ProvidesTransponder(Channel))
         return false;
      }
  return true;
}

bool cDevice::ProvidesChannel(const cChannel *Channel, int Priority, bool *NeedsDetachReceivers) const
{
  return false;
}

bool cDevice::ProvidesEIT(void) const
{
  return false;
}

int cDevice::NumProvidedSystems(void) const
{
  return 0;
}

const cPositioner *cDevice::Positioner(void) const
{
  return NULL;
}

bool cDevice::SignalStats(int &Valid, double *Strength, double *Cnr, double *BerPre, double *BerPost, double *Per, int *Status) const
{
  return false;
}

int cDevice::SignalStrength(void) const
{
  return -1;
}

int cDevice::SignalQuality(void) const
{
  return -1;
}

const cChannel *cDevice::GetCurrentlyTunedTransponder(void) const
{
  return NULL;
}

bool cDevice::IsTunedToTransponder(const cChannel *Channel) const
{
  return false;
}

bool cDevice::MaySwitchTransponder(const cChannel *Channel) const
{
  return time(NULL) > occupiedTimeout && !Receiving() && !(pidHandles[ptAudio].pid || pidHandles[ptVideo].pid || pidHandles[ptDolby].pid);
}

bool cDevice::SwitchChannel(const cChannel *Channel, bool LiveView)
{
  if (LiveView) {
     isyslog("switching to channel %d %s (%s)", Channel->Number(), *Channel->GetChannelID().ToString(), Channel->Name());
     cControl::Shutdown(); // prevents old channel from being shown too long if GetDevice() takes longer
                           // and, if decrypted, this removes the now superfluous PIDs from the CAM, too
     }
  for (int i = 3; i--;) {
      switch (SetChannel(Channel, LiveView)) {
        case scrOk:           return true;
        case scrNotAvailable: Skins.QueueMessage(mtInfo, tr("Channel not available!"));
                              return false;
        case scrNoTransfer:   Skins.QueueMessage(mtError, tr("Can't start Transfer Mode!"));
                              return false;
        case scrFailed:       break; // loop will retry
        default:              esyslog("ERROR: invalid return value from SetChannel");
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
     cControl::Shutdown(); // prevents old channel from being shown too long if GetDevice() takes longer
                           // and, if decrypted, this removes the now superfluous PIDs from the CAM, too
     int n = CurrentChannel() + Direction;
     int first = n;
     LOCK_CHANNELS_READ;
     const cChannel *Channel;
     while ((Channel = Channels->GetByNumber(n, Direction)) != NULL) {
           // try only channels which are currently available
           if (GetDevice(Channel, LIVEPRIORITY, true, true))
              break;
           n = Channel->Number() + Direction;
           }
     if (Channel) {
        int d = n - first;
        if (abs(d) == 1)
           dsyslog("skipped channel %d", first);
        else if (d)
           dsyslog("skipped channels %d..%d", first, n - sgn(d));
        if (PrimaryDevice()->SwitchChannel(Channel, true))
           result = true;
        }
     else if (n != first)
        Skins.QueueMessage(mtError, tr("Channel not available!"));
     }
  return result;
}

eSetChannelResult cDevice::SetChannel(const cChannel *Channel, bool LiveView)
{
  cMutexLock MutexLock(&mutexChannel); // to avoid a race between SVDRP CHAN and HasProgramme()
  cStatus::MsgChannelSwitch(this, 0, LiveView);

  if (LiveView) {
     if (IsPrimaryDevice() && !Replaying() && !Transferring()) { // this is only for FF DVB cards!
        LOCK_CHANNELS_READ;
        if (const cChannel *ch = Channels->GetByNumber(currentChannel)) {
           if (patFilter)
              patFilter->Release(ch->Sid());
           }
        }
     StopReplay();
     DELETENULL(liveSubtitle);
     DELETENULL(dvbSubtitleConverter);
     }

  cDevice *Device = (LiveView && IsPrimaryDevice(false)) ? GetDevice(Channel, LIVEPRIORITY, true) : this;

  bool NeedsTransferMode = LiveView && Device != PrimaryDevice();
  // If the CAM slot wants the TS data, we need to switch to Transfer Mode:
  if (!NeedsTransferMode && LiveView && IsPrimaryDevice() && CamSlot() && CamSlot()->WantsTsData())
     NeedsTransferMode = true;

  eSetChannelResult Result = scrOk;

  // If this DVB card can't receive this channel, let's see if we can
  // use the card that actually can receive it and transfer data from there:

  if (NeedsTransferMode) {
     if (Device && PrimaryDevice()->CanReplay()) {
        if (Device->SetChannel(Channel, false) == scrOk) // calling SetChannel() directly, not SwitchChannel()!
           cControl::Launch(new cTransferControl(Device, Channel));
        else
           Result = scrNoTransfer;
        }
     else
        Result = scrNotAvailable;
     }
  else {
     // Stop section handling:
     if (sectionHandler) {
        sectionHandler->SetStatus(false);
        sectionHandler->SetChannel(NULL);
        }
     // Tell the camSlot about the channel switch and add all PIDs of this
     // channel to it, for possible later decryption:
     if (camSlot)
        camSlot->AddChannel(Channel);
     if (SetChannelDevice(Channel, LiveView)) {
        // Start section handling:
        if (sectionHandler) {
           sectionHandler->SetChannel(Channel);
           sectionHandler->SetStatus(true);
           }
        // Start decrypting any PIDs that might have been set in SetChannelDevice():
        if (camSlot)
           camSlot->StartDecrypting();
        }
     else
        Result = scrFailed;
     }

  if (Result == scrOk) {
     if (LiveView) {
        if (IsPrimaryDevice(false))
           currentChannel = Channel->Number();
        if (IsPrimaryDevice()) {
           if (patFilter) // this is only for FF DVB cards!
              patFilter->Request(Channel->Sid());
           // Set the available audio tracks:
           ClrAvailableTracks();
           for (int i = 0; i < MAXAPIDS; i++)
               SetAvailableTrack(ttAudio, i, Channel->Apid(i), Channel->Alang(i));
           if (Setup.UseDolbyDigital) {
              for (int i = 0; i < MAXDPIDS; i++)
                  SetAvailableTrack(ttDolby, i, Channel->Dpid(i), Channel->Dlang(i));
              }
           for (int i = 0; i < MAXSPIDS; i++)
               SetAvailableTrack(ttSubtitle, i, Channel->Spid(i), Channel->Slang(i));
           if (!NeedsTransferMode)
              EnsureAudioTrack(true);
           EnsureSubtitleTrack();
           }
        }
     cStatus::MsgChannelSwitch(this, Channel->Number(), LiveView); // only report status if channel switch successful
     }

  return Result;
}

void cDevice::ForceTransferMode(void)
{
  if (!cTransferControl::ReceiverDevice()) {
     LOCK_CHANNELS_READ;
     if (const cChannel *Channel = Channels->GetByNumber(CurrentChannel()))
        SetChannelDevice(Channel, false); // this implicitly starts Transfer Mode
     }
}

int cDevice::Occupied(void) const
{
  int Seconds = occupiedTimeout - time(NULL);
  return Seconds > 0 ? Seconds : 0;
}

void cDevice::SetOccupied(int Seconds)
{
  if (Seconds >= 0)
     occupiedTimeout = time(NULL) + min(Seconds, MAXOCCUPIEDTIMEOUT);
}

bool cDevice::SetChannelDevice(const cChannel *Channel, bool LiveView)
{
  return false;
}

bool cDevice::HasLock(int TimeoutMs) const
{
  return true;
}

bool cDevice::HasProgramme(void) const
{
  cMutexLock MutexLock(&mutexChannel); // to avoid a race between SVDRP CHAN and HasProgramme()
  return Replaying() || pidHandles[ptAudio].pid || pidHandles[ptVideo].pid;
}

int cDevice::GetAudioChannelDevice(void)
{
  return 0;
}

void cDevice::SetAudioChannelDevice(int AudioChannel)
{
}

void cDevice::SetVolumeDevice(int Volume)
{
}

void cDevice::SetDigitalAudioDevice(bool On)
{
}

void cDevice::SetAudioTrackDevice(eTrackType Type)
{
}

void cDevice::SetSubtitleTrackDevice(eTrackType Type)
{
}

bool cDevice::ToggleMute(void)
{
  int OldVolume = volume;
  mute = !mute;
  //XXX why is it necessary to use different sequences???
  if (mute) {
     SetVolume(0, true);
     Audios.MuteAudio(mute); // Mute external audio after analog audio
     }
  else {
     Audios.MuteAudio(mute); // Enable external audio before analog audio
     SetVolume(OldVolume, true);
     }
  volume = OldVolume;
  return mute;
}

int cDevice::GetAudioChannel(void)
{
  int c = GetAudioChannelDevice();
  return (0 <= c && c <= 2) ? c : 0;
}

void cDevice::SetAudioChannel(int AudioChannel)
{
  if (0 <= AudioChannel && AudioChannel <= 2)
     SetAudioChannelDevice(AudioChannel);
}

void cDevice::SetVolume(int Volume, bool Absolute)
{
  int OldVolume = volume;
  double VolumeDelta = double(MAXVOLUME) / Setup.VolumeSteps;
  double VolumeLinearize = (Setup.VolumeLinearize >= 0) ? (Setup.VolumeLinearize / 10.0 + 1.0) : (1.0 / ((-Setup.VolumeLinearize / 10.0) + 1.0));
  volume = constrain(int(floor((Absolute ? Volume : volume + Volume) / VolumeDelta + 0.5) * VolumeDelta), 0, MAXVOLUME);
  SetVolumeDevice(MAXVOLUME - int(pow(1.0 - pow(double(volume) / MAXVOLUME, VolumeLinearize), 1.0 / VolumeLinearize) * MAXVOLUME));
  Absolute |= mute;
  cStatus::MsgSetVolume(Absolute ? volume : volume - OldVolume, Absolute);
  if (volume > 0) {
     mute = false;
     Audios.MuteAudio(mute);
     }
}

void cDevice::ClrAvailableTracks(bool DescriptionsOnly, bool IdsOnly)
{
  if (keepTracks)
     return;
  if (DescriptionsOnly) {
     for (int i = ttNone; i < ttMaxTrackTypes; i++)
         *availableTracks[i].description = 0;
     }
  else {
     if (IdsOnly) {
        for (int i = ttNone; i < ttMaxTrackTypes; i++)
            availableTracks[i].id = 0;
        }
     else
        memset(availableTracks, 0, sizeof(availableTracks));
     pre_1_3_19_PrivateStream = 0;
     SetAudioChannel(0); // fall back to stereo
     currentAudioTrackMissingCount = 0;
     currentAudioTrack = ttNone;
     currentSubtitleTrack = ttNone;
     }
}

bool cDevice::SetAvailableTrack(eTrackType Type, int Index, uint16_t Id, const char *Language, const char *Description)
{
  eTrackType t = eTrackType(Type + Index);
  if (Type == ttAudio && IS_AUDIO_TRACK(t) ||
      Type == ttDolby && IS_DOLBY_TRACK(t) ||
      Type == ttSubtitle && IS_SUBTITLE_TRACK(t)) {
     if (Language)
        strn0cpy(availableTracks[t].language, Language, sizeof(availableTracks[t].language));
     if (Description)
        Utf8Strn0Cpy(availableTracks[t].description, Description, sizeof(availableTracks[t].description));
     if (Id) {
        availableTracks[t].id = Id; // setting 'id' last to avoid the need for extensive locking
        if (Type == ttAudio || Type == ttDolby) {
           int numAudioTracks = NumAudioTracks();
           if (!availableTracks[currentAudioTrack].id && numAudioTracks && currentAudioTrackMissingCount++ > numAudioTracks * 10)
              EnsureAudioTrack();
           else if (t == currentAudioTrack)
              currentAudioTrackMissingCount = 0;
           }
        else if (Type == ttSubtitle && autoSelectPreferredSubtitleLanguage)
           EnsureSubtitleTrack();
        }
     return true;
     }
  else
     esyslog("ERROR: SetAvailableTrack called with invalid Type/Index (%d/%d)", Type, Index);
  return false;
}

const tTrackId *cDevice::GetTrack(eTrackType Type)
{
  return (ttNone < Type && Type < ttMaxTrackTypes) ? &availableTracks[Type] : NULL;
}

int cDevice::NumTracks(eTrackType FirstTrack, eTrackType LastTrack) const
{
  int n = 0;
  for (int i = FirstTrack; i <= LastTrack; i++) {
      if (availableTracks[i].id)
         n++;
      }
  return n;
}

int cDevice::NumAudioTracks(void) const
{
  return NumTracks(ttAudioFirst, ttDolbyLast);
}

int cDevice::NumSubtitleTracks(void) const
{
  return NumTracks(ttSubtitleFirst, ttSubtitleLast);
}

bool cDevice::SetCurrentAudioTrack(eTrackType Type)
{
  if (ttNone < Type && Type <= ttDolbyLast) {
     cMutexLock MutexLock(&mutexCurrentAudioTrack);
     if (IS_DOLBY_TRACK(Type))
        SetDigitalAudioDevice(true);
     currentAudioTrack = Type;
     if (player)
        player->SetAudioTrack(currentAudioTrack, GetTrack(currentAudioTrack));
     else
        SetAudioTrackDevice(currentAudioTrack);
     if (IS_AUDIO_TRACK(Type))
        SetDigitalAudioDevice(false);
     return true;
     }
  return false;
}

bool cDevice::SetCurrentSubtitleTrack(eTrackType Type, bool Manual)
{
  if (Type == ttNone || IS_SUBTITLE_TRACK(Type)) {
     currentSubtitleTrack = Type;
     autoSelectPreferredSubtitleLanguage = !Manual;
     if (dvbSubtitleConverter)
        dvbSubtitleConverter->Reset();
     if (Type == ttNone && dvbSubtitleConverter) {
        cMutexLock MutexLock(&mutexCurrentSubtitleTrack);
        DELETENULL(dvbSubtitleConverter);
        }
     DELETENULL(liveSubtitle);
     if (player)
        player->SetSubtitleTrack(currentSubtitleTrack, GetTrack(currentSubtitleTrack));
     else
        SetSubtitleTrackDevice(currentSubtitleTrack);
     if (currentSubtitleTrack != ttNone && !Replaying() && !Transferring()) {
        const tTrackId *TrackId = GetTrack(currentSubtitleTrack);
        if (TrackId && TrackId->id) {
           liveSubtitle = new cLiveSubtitle(TrackId->id);
           AttachReceiver(liveSubtitle);
           }
        }
     return true;
     }
  return false;
}

void cDevice::EnsureAudioTrack(bool Force)
{
  if (keepTracks)
     return;
  if (Force || !availableTracks[currentAudioTrack].id) {
     eTrackType PreferredTrack = ttAudioFirst;
     int PreferredAudioChannel = 0;
     int LanguagePreference = -1;
     int StartCheck = Setup.CurrentDolby ? ttDolbyFirst : ttAudioFirst;
     int EndCheck = ttDolbyLast;
     for (int i = StartCheck; i <= EndCheck; i++) {
         const tTrackId *TrackId = GetTrack(eTrackType(i));
         int pos = 0;
         if (TrackId && TrackId->id && I18nIsPreferredLanguage(Setup.AudioLanguages, TrackId->language, LanguagePreference, &pos)) {
            PreferredTrack = eTrackType(i);
            PreferredAudioChannel = pos;
            }
         if (Setup.CurrentDolby && i == ttDolbyLast) {
            i = ttAudioFirst - 1;
            EndCheck = ttAudioLast;
            }
         }
     // Make sure we're set to an available audio track:
     const tTrackId *Track = GetTrack(GetCurrentAudioTrack());
     if (Force || !Track || !Track->id || PreferredTrack != GetCurrentAudioTrack()) {
        if (!Force) // only log this for automatic changes
           dsyslog("setting audio track to %d (%d)", PreferredTrack, PreferredAudioChannel);
        SetCurrentAudioTrack(PreferredTrack);
        SetAudioChannel(PreferredAudioChannel);
        }
     }
}

void cDevice::EnsureSubtitleTrack(void)
{
  if (keepTracks)
     return;
  if (Setup.DisplaySubtitles) {
     eTrackType PreferredTrack = ttNone;
     int LanguagePreference = INT_MAX; // higher than the maximum possible value
     for (int i = ttSubtitleFirst; i <= ttSubtitleLast; i++) {
         const tTrackId *TrackId = GetTrack(eTrackType(i));
         if (TrackId && TrackId->id && (I18nIsPreferredLanguage(Setup.SubtitleLanguages, TrackId->language, LanguagePreference) ||
            (i == ttSubtitleFirst + 8 && !*TrackId->language && LanguagePreference == INT_MAX))) // compatibility mode for old subtitles plugin
            PreferredTrack = eTrackType(i);
         }
     // Make sure we're set to an available subtitle track:
     const tTrackId *Track = GetTrack(GetCurrentSubtitleTrack());
     if (!Track || !Track->id || PreferredTrack != GetCurrentSubtitleTrack())
        SetCurrentSubtitleTrack(PreferredTrack);
     }
  else
     SetCurrentSubtitleTrack(ttNone);
}

bool cDevice::CanReplay(void) const
{
  return HasDecoder();
}

bool cDevice::SetPlayMode(ePlayMode PlayMode)
{
  return false;
}

int64_t cDevice::GetSTC(void)
{
  return -1;
}

void cDevice::TrickSpeed(int Speed, bool Forward)
{
}

void cDevice::Clear(void)
{
  Audios.ClearAudio();
  if (dvbSubtitleConverter)
     dvbSubtitleConverter->Reset();
}

void cDevice::Play(void)
{
  Audios.MuteAudio(mute);
  if (dvbSubtitleConverter)
     dvbSubtitleConverter->Freeze(false);
}

void cDevice::Freeze(void)
{
  Audios.MuteAudio(true);
  if (dvbSubtitleConverter)
     dvbSubtitleConverter->Freeze(true);
}

void cDevice::Mute(void)
{
  Audios.MuteAudio(true);
}

void cDevice::StillPicture(const uchar *Data, int Length)
{
  if (Data[0] == 0x47) {
     // TS data
     cTsToPes TsToPes;
     uchar *buf = NULL;
     int Size = 0;
     while (Length >= TS_SIZE) {
           int Pid = TsPid(Data);
           if (Pid == PATPID)
              patPmtParser.ParsePat(Data, TS_SIZE);
           else if (patPmtParser.IsPmtPid(Pid))
              patPmtParser.ParsePmt(Data, TS_SIZE);
           else if (Pid == patPmtParser.Vpid()) {
              if (TsPayloadStart(Data)) {
                 int l;
                 while (const uchar *p = TsToPes.GetPes(l)) {
                       int Offset = Size;
                       int NewSize = Size + l;
                       if (uchar *NewBuffer = (uchar *)realloc(buf, NewSize)) {
                          Size = NewSize;
                          buf = NewBuffer;
                          memcpy(buf + Offset, p, l);
                          }
                       else {
                          LOG_ERROR_STR("out of memory");
                          free(buf);
                          return;
                          }
                       }
                 TsToPes.Reset();
                 }
              TsToPes.PutTs(Data, TS_SIZE);
              }
           Length -= TS_SIZE;
           Data += TS_SIZE;
           }
     int l;
     while (const uchar *p = TsToPes.GetPes(l)) {
           int Offset = Size;
           int NewSize = Size + l;
           if (uchar *NewBuffer = (uchar *)realloc(buf, NewSize)) {
              Size = NewSize;
              buf = NewBuffer;
              memcpy(buf + Offset, p, l);
              }
           else {
              esyslog("ERROR: out of memory");
              free(buf);
              return;
              }
           }
     if (buf) {
        StillPicture(buf, Size);
        free(buf);
        }
     }
}

bool cDevice::Replaying(void) const
{
  return player != NULL;
}

bool cDevice::Transferring(void) const
{
  return cTransferControl::ReceiverDevice() != NULL;
}

bool cDevice::AttachPlayer(cPlayer *Player)
{
  if (CanReplay()) {
     if (player)
        Detach(player);
     DELETENULL(liveSubtitle);
     DELETENULL(dvbSubtitleConverter);
     patPmtParser.Reset();
     player = Player;
     if (!Transferring())
        ClrAvailableTracks(false, true);
     SetPlayMode(player->playMode);
     player->device = this;
     player->Activate(true);
     return true;
     }
  return false;
}

void cDevice::Detach(cPlayer *Player)
{
  if (Player && player == Player) {
     cPlayer *p = player;
     player = NULL; // avoids recursive calls to Detach()
     p->Activate(false);
     p->device = NULL;
     cMutexLock MutexLock(&mutexCurrentSubtitleTrack);
     delete dvbSubtitleConverter;
     dvbSubtitleConverter = NULL;
     SetPlayMode(pmNone);
     SetVideoDisplayFormat(eVideoDisplayFormat(Setup.VideoDisplayFormat));
     PlayTs(NULL, 0);
     patPmtParser.Reset();
     Audios.ClearAudio();
     isPlayingVideo = false;
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

bool cDevice::Flush(int TimeoutMs)
{
  return true;
}

int cDevice::PlayVideo(const uchar *Data, int Length)
{
  return -1;
}

int cDevice::PlayAudio(const uchar *Data, int Length, uchar Id)
{
  return -1;
}

int cDevice::PlaySubtitle(const uchar *Data, int Length)
{
  if (!dvbSubtitleConverter)
     dvbSubtitleConverter = new cDvbSubtitleConverter;
  return dvbSubtitleConverter->ConvertFragments(Data, Length);
}

int cDevice::PlayPesPacket(const uchar *Data, int Length, bool VideoOnly)
{
  bool FirstLoop = true;
  uchar c = Data[3];
  const uchar *Start = Data;
  const uchar *End = Start + Length;
  while (Start < End) {
        int d = End - Start;
        int w = d;
        switch (c) {
          case 0xBE:          // padding stream, needed for MPEG1
          case 0xE0 ... 0xEF: // video
               isPlayingVideo = true;
               w = PlayVideo(Start, d);
               break;
          case 0xC0 ... 0xDF: // audio
               SetAvailableTrack(ttAudio, c - 0xC0, c);
               if ((!VideoOnly || HasIBPTrickSpeed()) && c == availableTracks[currentAudioTrack].id) {
                  w = PlayAudio(Start, d, c);
                  if (FirstLoop)
                     Audios.PlayAudio(Data, Length, c);
                  }
               break;
          case 0xBD: { // private stream 1
               int PayloadOffset = Data[8] + 9;

               // Compatibility mode for old subtitles plugin:
               if ((Data[7] & 0x01) && (Data[PayloadOffset - 3] & 0x81) == 0x01 && Data[PayloadOffset - 2] == 0x81)
                  PayloadOffset--;

               uchar SubStreamId = Data[PayloadOffset];
               uchar SubStreamType = SubStreamId & 0xF0;
               uchar SubStreamIndex = SubStreamId & 0x1F;

               // Compatibility mode for old VDR recordings, where 0xBD was only AC3:
pre_1_3_19_PrivateStreamDetected:
               if (pre_1_3_19_PrivateStream > MIN_PRE_1_3_19_PRIVATESTREAM) {
                  SubStreamId = c;
                  SubStreamType = 0x80;
                  SubStreamIndex = 0;
                  }
               else if (pre_1_3_19_PrivateStream)
                  pre_1_3_19_PrivateStream--; // every known PS1 packet counts down towards 0 to recover from glitches...
               switch (SubStreamType) {
                 case 0x20: // SPU
                 case 0x30: // SPU
                      SetAvailableTrack(ttSubtitle, SubStreamIndex, SubStreamId);
                      if ((!VideoOnly || HasIBPTrickSpeed()) && currentSubtitleTrack != ttNone && SubStreamId == availableTracks[currentSubtitleTrack].id)
                         w = PlaySubtitle(Start, d);
                      break;
                 case 0x80: // AC3 & DTS
                      if (Setup.UseDolbyDigital) {
                         SetAvailableTrack(ttDolby, SubStreamIndex, SubStreamId);
                         if ((!VideoOnly || HasIBPTrickSpeed()) && SubStreamId == availableTracks[currentAudioTrack].id) {
                            w = PlayAudio(Start, d, SubStreamId);
                            if (FirstLoop)
                               Audios.PlayAudio(Data, Length, SubStreamId);
                            }
                         }
                      break;
                 case 0xA0: // LPCM
                      SetAvailableTrack(ttAudio, SubStreamIndex, SubStreamId);
                      if ((!VideoOnly || HasIBPTrickSpeed()) && SubStreamId == availableTracks[currentAudioTrack].id) {
                         w = PlayAudio(Start, d, SubStreamId);
                         if (FirstLoop)
                            Audios.PlayAudio(Data, Length, SubStreamId);
                         }
                      break;
                 default:
                      // Compatibility mode for old VDR recordings, where 0xBD was only AC3:
                      if (pre_1_3_19_PrivateStream <= MIN_PRE_1_3_19_PRIVATESTREAM) {
                         dsyslog("unknown PS1 packet, substream id = %02X (counter is at %d)", SubStreamId, pre_1_3_19_PrivateStream);
                         pre_1_3_19_PrivateStream += 2; // ...and every unknown PS1 packet counts up (the very first one counts twice, but that's ok)
                         if (pre_1_3_19_PrivateStream > MIN_PRE_1_3_19_PRIVATESTREAM) {
                            dsyslog("switching to pre 1.3.19 Dolby Digital compatibility mode - substream id = %02X", SubStreamId);
                            ClrAvailableTracks();
                            pre_1_3_19_PrivateStream = MIN_PRE_1_3_19_PRIVATESTREAM + 1;
                            goto pre_1_3_19_PrivateStreamDetected;
                            }
                         }
                 }
               }
               break;
          default:
               ;//esyslog("ERROR: unexpected packet id %02X", c);
          }
        if (w > 0)
           Start += w;
        else {
           if (Start != Data)
              esyslog("ERROR: incomplete PES packet write!");
           return Start == Data ? w : Start - Data;
           }
        FirstLoop = false;
        }
  return Length;
}

int cDevice::PlayPes(const uchar *Data, int Length, bool VideoOnly)
{
  if (!Data) {
     if (dvbSubtitleConverter)
        dvbSubtitleConverter->Reset();
     return 0;
     }
  int i = 0;
  while (i <= Length - 6) {
        if (Data[i] == 0x00 && Data[i + 1] == 0x00 && Data[i + 2] == 0x01) {
           int l = PesLength(Data + i);
           if (i + l > Length) {
              esyslog("ERROR: incomplete PES packet!");
              return Length;
              }
           int w = PlayPesPacket(Data + i, l, VideoOnly);
           if (w > 0)
              i += l;
           else
              return i == 0 ? w : i;
           }
        else
           i++;
        }
  if (i < Length)
     esyslog("ERROR: leftover PES data!");
  return Length;
}

int cDevice::PlayTsVideo(const uchar *Data, int Length)
{
  // Video PES has no explicit length, so we can only determine the end of
  // a PES packet when the next TS packet that starts a payload comes in:
  if (TsPayloadStart(Data)) {
     int l;
     while (const uchar *p = tsToPesVideo.GetPes(l)) {
           int w = PlayVideo(p, l);
           if (w <= 0) {
              tsToPesVideo.SetRepeatLast();
              return w;
              }
           }
     tsToPesVideo.Reset();
     }
  tsToPesVideo.PutTs(Data, Length);
  return Length;
}

int cDevice::PlayTsAudio(const uchar *Data, int Length)
{
  // Audio PES always has an explicit length and consists of single packets:
  int l;
  if (const uchar *p = tsToPesAudio.GetPes(l)) {
     int w = PlayAudio(p, l, p[3]);
     if (w <= 0) {
        tsToPesAudio.SetRepeatLast();
        return w;
        }
     tsToPesAudio.Reset();
     }
  tsToPesAudio.PutTs(Data, Length);
  return Length;
}

int cDevice::PlayTsSubtitle(const uchar *Data, int Length)
{
  if (!dvbSubtitleConverter)
     dvbSubtitleConverter = new cDvbSubtitleConverter;
  tsToPesSubtitle.PutTs(Data, Length);
  int l;
  if (const uchar *p = tsToPesSubtitle.GetPes(l)) {
     dvbSubtitleConverter->Convert(p, l);
     tsToPesSubtitle.Reset();
     }
  return Length;
}

int cDevice::PlayTs(const uchar *Data, int Length, bool VideoOnly)
{
  int Played = 0;
  if (!Data) {
     tsToPesVideo.Reset();
     tsToPesAudio.Reset();
     tsToPesSubtitle.Reset();
     }
  else if (Length < TS_SIZE) {
     esyslog("ERROR: skipped %d bytes of TS fragment", Length);
     return Length;
     }
  else {
     while (Length >= TS_SIZE) {
           if (int Skipped = TS_SYNC(Data, Length))
              return Played + Skipped;
           int Pid = TsPid(Data);
           if (TsHasPayload(Data)) { // silently ignore TS packets w/o payload
              int PayloadOffset = TsPayloadOffset(Data);
              if (PayloadOffset < TS_SIZE) {
                 if (Pid == PATPID)
                    patPmtParser.ParsePat(Data, TS_SIZE);
                 else if (patPmtParser.IsPmtPid(Pid))
                    patPmtParser.ParsePmt(Data, TS_SIZE);
                 else if (Pid == patPmtParser.Vpid()) {
                    isPlayingVideo = true;
                    int w = PlayTsVideo(Data, TS_SIZE);
                    if (w < 0)
                       return Played ? Played : w;
                    if (w == 0)
                       break;
                    }
                 else if (Pid == availableTracks[currentAudioTrack].id) {
                    if (!VideoOnly || HasIBPTrickSpeed()) {
                       int w = PlayTsAudio(Data, TS_SIZE);
                       if (w < 0)
                          return Played ? Played : w;
                       if (w == 0)
                          break;
                       Audios.PlayTsAudio(Data, TS_SIZE);
                       }
                    }
                 else if (Pid == availableTracks[currentSubtitleTrack].id) {
                    if (!VideoOnly || HasIBPTrickSpeed())
                       PlayTsSubtitle(Data, TS_SIZE);
                    }
                 }
              }
           else if (Pid == patPmtParser.Ppid()) {
              int w = PlayTsVideo(Data, TS_SIZE);
              if (w < 0)
                 return Played ? Played : w;
              if (w == 0)
                 break;
              }
           Played += TS_SIZE;
           Length -= TS_SIZE;
           Data += TS_SIZE;
           }
     }
  return Played;
}

int cDevice::Priority(void) const
{
  int priority = IDLEPRIORITY;
  if (IsPrimaryDevice() && !Replaying() && HasProgramme())
     priority = TRANSFERPRIORITY; // we use the same value here, no matter whether it's actual Transfer Mode or real live viewing
  cMutexLock MutexLock(&mutexReceiver);
  for (int i = 0; i < MAXRECEIVERS; i++) {
      if (receiver[i])
         priority = max(receiver[i]->priority, priority);
      }
  return priority;
}

bool cDevice::Ready(void)
{
  return true;
}

bool cDevice::Receiving(bool Dummy) const
{
  cMutexLock MutexLock(&mutexReceiver);
  for (int i = 0; i < MAXRECEIVERS; i++) {
      if (receiver[i])
         return true;
      }
  return false;
}

#define TS_SCRAMBLING_TIMEOUT     3 // seconds to wait until a TS becomes unscrambled
#define TS_SCRAMBLING_TIME_OK     3 // seconds before a Channel/CAM combination is marked as known to decrypt
#define EIT_INJECTION_TIME       10 // seconds for which to inject EIT event

void cDevice::Action(void)
{
  if (Running() && OpenDvr()) {
     while (Running()) {
           // Read data from the DVR device:
           uchar *b = NULL;
           if (GetTSPacket(b)) {
              if (b) {
                 // Distribute the packet to all attached receivers:
                 Lock();
                 cCamSlot *cs = CamSlot();
                 if (cs)
                    cs->TsPostProcess(b);
                 int Pid = TsPid(b);
                 bool IsScrambled = TsIsScrambled(b);
                 for (int i = 0; i < MAXRECEIVERS; i++) {
                     cMutexLock MutexLock(&mutexReceiver);
                     cReceiver *Receiver = receiver[i];
                     if (Receiver && Receiver->WantsPid(Pid)) {
                        Receiver->Receive(b, TS_SIZE);
                        // Check whether the TS packet is scrambled:
                        if (Receiver->startScrambleDetection) {
                           if (cs) {
                              int CamSlotNumber = cs->MasterSlotNumber();
                              if (Receiver->lastScrambledPacket < Receiver->startScrambleDetection)
                                 Receiver->lastScrambledPacket = Receiver->startScrambleDetection;
                              time_t Now = time(NULL);
                              if (IsScrambled) {
                                 Receiver->lastScrambledPacket = Now;
                                 if (Now - Receiver->startScrambleDetection > Receiver->scramblingTimeout) {
                                    if (!cs->IsActivating() || Receiver->Priority() >= LIVEPRIORITY) {
                                       if (Receiver->ChannelID().Valid()) {
                                          dsyslog("CAM %d: won't decrypt channel %s, detaching receiver", CamSlotNumber, *Receiver->ChannelID().ToString());
                                          ChannelCamRelations.SetChecked(Receiver->ChannelID(), CamSlotNumber);
                                          }
                                       Detach(Receiver);
                                       }
                                    }
                                 }
                              else if (Now - Receiver->lastScrambledPacket > TS_SCRAMBLING_TIME_OK) {
                                 if (Receiver->ChannelID().Valid()) {
                                    dsyslog("CAM %d: decrypts channel %s", CamSlotNumber, *Receiver->ChannelID().ToString());
                                    ChannelCamRelations.SetDecrypt(Receiver->ChannelID(), CamSlotNumber);
                                    }
                                 Receiver->startScrambleDetection = 0;
                                 }
                              }
                           }
                        // Inject EIT event to avoid the CAMs parental rating prompt:
                        if (Receiver->startEitInjection) {
                           time_t Now = time(NULL);
                           if (cCamSlot *cs = CamSlot()) {
                              if (Now != Receiver->lastEitInjection) { // once per second
                                 cs->InjectEit(Receiver->ChannelID().Sid());
                                 Receiver->lastEitInjection = Now;
                                 }
                              }
                           if (Now - Receiver->startEitInjection > EIT_INJECTION_TIME)
                              Receiver->startEitInjection = 0;
                           }
                        }
                     }
                 Unlock();
                 }
              }
           else
              break;
           }
     CloseDvr();
     }
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
// activate the following line if you need it - actually the driver should be fixed!
//#define WAIT_FOR_TUNER_LOCK
#ifdef WAIT_FOR_TUNER_LOCK
#define TUNER_LOCK_TIMEOUT 5000 // ms
  if (!HasLock(TUNER_LOCK_TIMEOUT)) {
     esyslog("ERROR: device %d has no lock, can't attach receiver!", DeviceNumber() + 1);
     return false;
     }
#endif
  cMutexLock MutexLock(&mutexReceiver);
  for (int i = 0; i < MAXRECEIVERS; i++) {
      if (!receiver[i]) {
         for (int n = 0; n < Receiver->numPids; n++) {
             if (!AddPid(Receiver->pids[n])) {
                for ( ; n-- > 0; )
                    DelPid(Receiver->pids[n]);
                return false;
                }
             }
         Receiver->Activate(true);
         Receiver->device = this;
         receiver[i] = Receiver;
         if (camSlot && Receiver->priority > MINPRIORITY) { // priority check to avoid an infinite loop with the CAM slot's caPidReceiver
            camSlot->StartDecrypting();
            if (camSlot->WantsTsData()) {
               Receiver->lastEitInjection = 0;
               Receiver->startEitInjection = time(NULL);
               }
            if (CamSlots.NumReadyMasterSlots() > 1) { // don't try different CAMs if there is only one
               Receiver->startScrambleDetection = time(NULL);
               Receiver->scramblingTimeout = TS_SCRAMBLING_TIMEOUT;
               bool KnownToDecrypt = ChannelCamRelations.CamDecrypt(Receiver->ChannelID(), camSlot->MasterSlotNumber());
               if (KnownToDecrypt)
                  Receiver->scramblingTimeout *= 9; // give it time to receive ECM/EMM (must be less than MAXBROKENTIMEOUT in recorder.c!)
               if (Receiver->ChannelID().Valid())
                  dsyslog("CAM %d: %sknown to decrypt channel %s (scramblingTimeout = %ds)", camSlot->MasterSlotNumber(), KnownToDecrypt ? "" : "not ", *Receiver->ChannelID().ToString(), Receiver->scramblingTimeout);
               }
            }
         if (patFilter && Receiver->ChannelID().Valid())
            patFilter->Request(Receiver->ChannelID().Sid());
         Start();
         return true;
         }
      }
  esyslog("ERROR: no free receiver slot!");
  return false;
}

void cDevice::Detach(cReceiver *Receiver, bool ReleaseCam)
{
  if (!Receiver || Receiver->device != this)
     return;
  bool receiversLeft = false;
  mutexReceiver.Lock();
  for (int i = 0; i < MAXRECEIVERS; i++) {
      if (receiver[i] == Receiver)
         receiver[i] = NULL;
      else if (receiver[i])
         receiversLeft = true;
      }
  if (patFilter && Receiver->ChannelID().Valid())
     patFilter->Release(Receiver->ChannelID().Sid());
  mutexReceiver.Unlock();
  Receiver->device = NULL;
  Receiver->Activate(false);
  for (int n = 0; n < Receiver->numPids; n++)
      DelPid(Receiver->pids[n]);
  if (camSlot) {
     if (Receiver->priority > MINPRIORITY) { // priority check to avoid an infinite loop with the CAM slot's caPidReceiver
        camSlot->StartDecrypting();
        if (ReleaseCam)
           ReleaseCamSlot();
        }
     }
  if (!receiversLeft)
     Cancel(-1);
}

void cDevice::DetachAll(int Pid)
{
  if (Pid) {
     cMutexLock MutexLock(&mutexReceiver);
     for (int i = 0; i < MAXRECEIVERS; i++) {
         cReceiver *Receiver = receiver[i];
         if (Receiver && Receiver->WantsPid(Pid))
            Detach(Receiver, false);
         }
     ReleaseCamSlot();
     }
}

void cDevice::DetachAllReceivers(void)
{
  cMutexLock MutexLock(&mutexReceiver);
  for (int i = 0; i < MAXRECEIVERS; i++)
      Detach(receiver[i], false);
  ReleaseCamSlot();
}

// --- cTSBuffer -------------------------------------------------------------

cTSBuffer::cTSBuffer(int File, int Size, int DeviceNumber)
{
  SetDescription("device %d TS buffer", DeviceNumber);
  f = File;
  deviceNumber = DeviceNumber;
  delivered = 0;
  ringBuffer = new cRingBufferLinear(Size, TS_SIZE, true, "TS");
  ringBuffer->SetTimeouts(100, 100);
  ringBuffer->SetIoThrottle();
  Start();
}

cTSBuffer::~cTSBuffer()
{
  Cancel(3);
  delete ringBuffer;
}

void cTSBuffer::Action(void)
{
  if (ringBuffer) {
     bool firstRead = true;
     cPoller Poller(f);
     while (Running()) {
           if (firstRead || Poller.Poll(100)) {
              firstRead = false;
              int r = ringBuffer->Read(f);
              if (r < 0 && FATALERRNO) {
                 if (errno == EOVERFLOW)
                    esyslog("ERROR: driver buffer overflow on device %d", deviceNumber);
                 else {
                    LOG_ERROR;
                    break;
                    }
                 }
              cCondWait::SleepMs(10); // avoids small chunks of data, which cause high CPU usage, esp. on ARM CPUs
              }
           }
     }
}

uchar *cTSBuffer::Get(int *Available, bool CheckAvailable)
{
  int Count = 0;
  if (delivered) {
     ringBuffer->Del(delivered);
     delivered = 0;
     }
  if (CheckAvailable && ringBuffer->Available() < TS_SIZE)
     return NULL;
  uchar *p = ringBuffer->Get(Count);
  if (p && Count >= TS_SIZE) {
     if (*p != TS_SYNC_BYTE) {
        for (int i = 1; i < Count; i++) {
            if (p[i] == TS_SYNC_BYTE) {
               Count = i;
               break;
               }
            }
        ringBuffer->Del(Count);
        esyslog("ERROR: skipped %d bytes to sync on TS packet on device %d", Count, deviceNumber);
        return NULL;
        }
     delivered = TS_SIZE;
     if (Available)
        *Available = Count;
     return p;
     }
  return NULL;
}

void cTSBuffer::Skip(int Count)
{
  delivered = Count;
}
