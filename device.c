/*
 * device.c: The basic device interface
 *
 * See the main source file 'vdr.c' for copyright information and
 * how to reach the author.
 *
 * $Id: device.c 1.106 2005/08/21 08:56:49 kls Exp $
 */

#include "device.h"
#include <errno.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include "audio.h"
#include "channels.h"
#include "i18n.h"
#include "player.h"
#include "receiver.h"
#include "status.h"
#include "transfer.h"

// --- cPesAssembler ---------------------------------------------------------

class cPesAssembler {
private:
  uchar *data;
  uint32_t tag;
  int length;
  int size;
  bool Realloc(int Size);
public:
  cPesAssembler(void);
  ~cPesAssembler();
  int ExpectedLength(void) { return PacketSize(data); }
  static int PacketSize(const uchar *data);
  int Length(void) { return length; }
  const uchar *Data(void) { return data; } // only valid if Length() >= 4
  void Reset(void);
  void Put(uchar c);
  void Put(const uchar *Data, int Length);
  bool IsPes(void);
  };

cPesAssembler::cPesAssembler(void)
{
  data = NULL;
  size = 0;
  Reset();
}

cPesAssembler::~cPesAssembler()
{
  free(data);
}

void cPesAssembler::Reset(void)
{
  tag = 0xFFFFFFFF;
  length = 0;
}

bool cPesAssembler::Realloc(int Size)
{
  if (Size > size) {
     size = max(Size, 2048);
     data = (uchar *)realloc(data, size);
     if (!data) {
        esyslog("ERROR: can't allocate memory for PES assembler");
        length = 0;
        size = 0;
        return false;
        }
     }
  return true;
}

void cPesAssembler::Put(uchar c)
{
  if (length < 4) {
     tag = (tag << 8) | c;
     if ((tag & 0xFFFFFF00) == 0x00000100) {
        if (Realloc(4)) {
           *(uint32_t *)data = htonl(tag);
           length = 4;
           }
        }
     else if (length < 3)
        length++;
     }
  else if (Realloc(length + 1))
     data[length++] = c;
}

void cPesAssembler::Put(const uchar *Data, int Length)
{
  while (length < 4 && Length > 0) {
        Put(*Data++);
        Length--;
        }
  if (Length && Realloc(length + Length)) {
     memcpy(data + length, Data, Length);
     length += Length;
     }
}

int cPesAssembler::PacketSize(const uchar *data)
{
  // we need atleast 6 bytes of data here !!!
  switch (data[3]) {
    default:
    case 0x00 ... 0xB8: // video stream start codes
    case 0xB9: // Program end
    case 0xBC: // Programm stream map
    case 0xF0 ... 0xFF: // reserved
         return 6;

    case 0xBA: // Pack header
         if ((data[4] & 0xC0) == 0x40) // MPEG2
            return 14;
         // to be absolutely correct we would have to add the stuffing bytes
         // as well, but at this point we only may have 6 bytes of data avail-
         // able. So it's up to the higher level to resync...
         //return 14 + (data[13] & 0x07); // add stuffing bytes
         else // MPEG1
            return 12;

    case 0xBB: // System header
    case 0xBD: // Private stream1
    case 0xBE: // Padding stream
    case 0xBF: // Private stream2 (navigation data)
    case 0xC0 ... 0xCF: // all the rest (the real packets)
    case 0xD0 ... 0xDF:
    case 0xE0 ... 0xEF:
         return 6 + data[4] * 256 + data[5];
    }
}

// --- cDevice ---------------------------------------------------------------

// The default priority for non-primary devices:
#define DEFAULTPRIORITY  -1
#define TUNER_LOCK_TIMEOUT 5000 // ms

int cDevice::numDevices = 0;
int cDevice::useDevice = 0;
int cDevice::nextCardIndex = 0;
int cDevice::currentChannel = 1;
cDevice *cDevice::device[MAXDEVICES] = { NULL };
cDevice *cDevice::primaryDevice = NULL;

cDevice::cDevice(void)
{
  cardIndex = nextCardIndex++;

  SetDescription("receiver on device %d", CardIndex() + 1);

  SetVideoFormat(Setup.VideoFormat);

  mute = false;
  volume = Setup.CurrentVolume;

  sectionHandler = NULL;
  eitFilter = NULL;
  patFilter = NULL;
  sdtFilter = NULL;
  nitFilter = NULL;

  ciHandler = NULL;
  player = NULL;
  pesAssembler = new cPesAssembler;
  ClrAvailableTracks();
  currentAudioTrack = ttNone;
  currentAudioTrackMissingCount = 0;

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
  for (int i = 0; i < MAXRECEIVERS; i++)
      Detach(receiver[i]);
  delete ciHandler;
  delete nitFilter;
  delete sdtFilter;
  delete patFilter;
  delete eitFilter;
  delete sectionHandler;
  delete pesAssembler;
}

bool cDevice::WaitForAllDevicesReady(int Timeout)
{
  for (time_t t0 = time(NULL); time(NULL) - t0 < Timeout; ) {
      bool ready = true;
      for (int i = 0; i < numDevices; i++) {
          if (device[i] && !device[i]->Ready())
             ready = false;
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
     primaryDevice->SetVideoFormat(Setup.VideoFormat);
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

cDevice *cDevice::GetDevice(const cChannel *Channel, int Priority, bool *NeedsDetachReceivers)
{
  cDevice *d = NULL;
  int select = 8, pri;

  for (int i = 0; i < numDevices; i++) {
      bool ndr;
      if (device[i]->ProvidesChannel(Channel, Priority, &ndr)) { // this device is basicly able to do the job
         if (device[i]->Receiving() && !ndr)
            pri = 0; // receiving and allows additional receivers
         else if (d && !device[i]->Receiving() && device[i]->ProvidesCa(Channel) < d->ProvidesCa(Channel))
            pri = 1; // free and fewer Ca's
         else if (!device[i]->Receiving() && !device[i]->HasDecoder())
            pri = 2; // free and not a full featured card
         else if (!device[i]->Receiving() && !device[i]->IsPrimaryDevice())
            pri = 3; // free and not the primary device
         else if (!device[i]->Receiving())
            pri = 4; // free
         else if (d && device[i]->Priority() < d->Priority())
            pri = 5; // receiving but priority is lower
         else if (d && device[i]->Priority() == d->Priority() && device[i]->ProvidesCa(Channel) < d->ProvidesCa(Channel))
            pri = 6; // receiving with same priority but fewer Ca's
         else
            pri = 7; // all others
         if (pri < select) {
            select = pri;
            d = device[i];
            if (NeedsDetachReceivers)
               *NeedsDetachReceivers = ndr;
            }
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

void cDevice::Shutdown(void)
{
  primaryDevice = NULL;
  for (int i = 0; i < numDevices; i++) {
      delete device[i];
      device[i] = NULL;
      }
}

bool cDevice::GrabImage(const char *FileName, bool Jpeg, int Quality, int SizeX, int SizeY)
{
  return false;
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
               }
        }
     }
}

void cDevice::SetVideoFormat(bool VideoFormat16_9)
{
}

eVideoSystem cDevice::GetVideoSystem(void)
{
  return vsPAL;
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
           if (!SetPid(&pidHandles[n], n, true)) {
              esyslog("ERROR: can't set PID %d on device %d", Pid, CardIndex() + 1);
              if (PidType <= ptTeletext)
                 DetachAll(Pid);
              DelPid(Pid, PidType);
              return false;
              }
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
        esyslog("ERROR: no free slot for PID %d on device %d", Pid, CardIndex() + 1);
        return false;
        }
     if (n >= 0) {
        pidHandles[n].pid = Pid;
        pidHandles[n].used = 1;
        PRINTPIDS("C");
        if (!SetPid(&pidHandles[n], n, true)) {
           esyslog("ERROR: can't set PID %d on device %d", Pid, CardIndex() + 1);
           if (PidType <= ptTeletext)
              DetachAll(Pid);
           DelPid(Pid, PidType);
           return false;
           }
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

void cDevice::StartSectionHandler(void)
{
  if (!sectionHandler) {
     sectionHandler = new cSectionHandler(this);
     AttachFilter(eitFilter = new cEitFilter);
     AttachFilter(patFilter = new cPatFilter);
     AttachFilter(sdtFilter = new cSdtFilter(patFilter));
     AttachFilter(nitFilter = new cNitFilter);
     }
}

int cDevice::OpenFilter(u_short Pid, u_char Tid, u_char Mask)
{
  return -1;
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

bool cDevice::SwitchChannel(const cChannel *Channel, bool LiveView)
{
  if (LiveView)
     isyslog("switching to channel %d", Channel->Number());
  for (int i = 3; i--;) {
      switch (SetChannel(Channel, LiveView)) {
        case scrOk:           return true;
        case scrNotAvailable: Skins.Message(mtInfo, tr("Channel not available!"));
                              return false;
        case scrNoTransfer:   Skins.Message(mtError, tr("Can't start Transfer Mode!"));
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
     else if (n != first)
        Skins.Message(mtError, tr("Channel not available!"));
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
           cControl::Launch(new cTransferControl(CaDevice, Channel->Vpid(), Channel->Apids(), Channel->Dpids(), Channel->Spids()));
        else
           Result = scrNoTransfer;
        }
     else
        Result = scrNotAvailable;
     }
  else {
     Channels.Lock(false);
     cStatus::MsgChannelSwitch(this, 0); // only report status if we are actually going to switch the channel
     // Stop section handling:
     if (sectionHandler) {
        sectionHandler->SetStatus(false);
        sectionHandler->SetChannel(NULL);
        }
     if (SetChannelDevice(Channel, LiveView)) {
        // Start section handling:
        if (sectionHandler) {
           sectionHandler->SetChannel(Channel);
           sectionHandler->SetStatus(true);
           }
        }
     else
        Result = scrFailed;
     Channels.Unlock();
     }

  if (Result == scrOk) {
     if (LiveView && IsPrimaryDevice()) {
        currentChannel = Channel->Number();
        // Set the available audio tracks:
        ClrAvailableTracks();
        for (int i = 0; i < MAXAPIDS; i++)
            SetAvailableTrack(ttAudio, i, Channel->Apid(i), Channel->Alang(i));
        if (Setup.UseDolbyDigital) {
           for (int i = 0; i < MAXDPIDS; i++)
               SetAvailableTrack(ttDolby, i, Channel->Dpid(i), Channel->Dlang(i));
           }
        if (!NeedsTransferMode)
           EnsureAudioTrack(true);
        }
     cStatus::MsgChannelSwitch(this, Channel->Number()); // only report status if channel switch successfull
     }

  return Result;
}

bool cDevice::SetChannelDevice(const cChannel *Channel, bool LiveView)
{
  return false;
}

bool cDevice::HasLock(int TimeoutMs)
{
  return true;
}

bool cDevice::HasProgramme(void)
{
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
  volume = min(max(Absolute ? Volume : volume + Volume, 0), MAXVOLUME);
  SetVolumeDevice(volume);
  cStatus::MsgSetVolume(volume, Absolute);
  if (volume > 0) {
     mute = false;
     Audios.MuteAudio(mute);
     }
}

void cDevice::ClrAvailableTracks(bool DescriptionsOnly)
{
  if (DescriptionsOnly) {
     for (int i = ttNone; i < ttMaxTrackTypes; i++)
         *availableTracks[i].description = 0;
     }
  else {
     memset(availableTracks, 0, sizeof(availableTracks));
     pre_1_3_19_PrivateStream = false;
     SetAudioChannel(0); // fall back to stereo
     currentAudioTrackMissingCount = 0;
     currentAudioTrack = ttNone;
     }
}

bool cDevice::SetAvailableTrack(eTrackType Type, int Index, uint16_t Id, const char *Language, const char *Description)
{
  eTrackType t = eTrackType(Type + Index);
  if (Type == ttAudio && IS_AUDIO_TRACK(t) ||
      Type == ttDolby && IS_DOLBY_TRACK(t)) {
     if (Language)
        strn0cpy(availableTracks[t].language, Language, sizeof(availableTracks[t].language));
     if (Description)
        strn0cpy(availableTracks[t].description, Description, sizeof(availableTracks[t].description));
     if (Id) {
        availableTracks[t].id = Id; // setting 'id' last to avoid the need for extensive locking
        int numAudioTracks = NumAudioTracks();
        if (!availableTracks[currentAudioTrack].id && numAudioTracks && currentAudioTrackMissingCount++ > numAudioTracks * 10)
           EnsureAudioTrack();
        else if (t == currentAudioTrack)
           currentAudioTrackMissingCount = 0;
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

int cDevice::NumAudioTracks(void) const
{
  int n = 0;
  for (int i = ttAudioFirst; i <= ttDolbyLast; i++) {
      if (availableTracks[i].id)
         n++;
      }
  return n;
}

bool cDevice::SetCurrentAudioTrack(eTrackType Type)
{
  if (ttNone < Type && Type < ttDolbyLast) {
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

void cDevice::EnsureAudioTrack(bool Force)
{
  if (Force || !availableTracks[currentAudioTrack].id) {
     eTrackType PreferredTrack = ttAudioFirst;
     int LanguagePreference = -1;
     int StartCheck = Setup.CurrentDolby ? ttDolbyFirst : ttAudioFirst;
     int EndCheck = ttDolbyLast;
     for (int i = StartCheck; i <= EndCheck; i++) {
         const tTrackId *TrackId = GetTrack(eTrackType(i));
         if (TrackId && TrackId->id && I18nIsPreferredLanguage(Setup.AudioLanguages, I18nLanguageIndex(TrackId->language), LanguagePreference))
            PreferredTrack = eTrackType(i);
         if (Setup.CurrentDolby && i == ttDolbyLast) {
            i = ttAudioFirst - 1;
            EndCheck = ttAudioLast;
            }
         }
     // Make sure we're set to an available audio track:
     const tTrackId *Track = GetTrack(GetCurrentAudioTrack());
     if (Force || !Track || !Track->id || PreferredTrack != GetCurrentAudioTrack()) {
        if (!Force) // only log this for automatic changes
           dsyslog("setting audio track to %d", PreferredTrack);
        SetCurrentAudioTrack(PreferredTrack);
        }
     }
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
     if (!dynamic_cast<cTransfer *>(Player))
        ClrAvailableTracks();
     pesAssembler->Reset();
     player = Player;
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
     player->Activate(false);
     player->device = NULL;
     player = NULL;
     SetPlayMode(pmNone);
     SetVideoDisplayFormat(eVideoDisplayFormat(Setup.VideoDisplayFormat));
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

bool cDevice::Flush(int TimeoutMs)
{
  return true;
}

int cDevice::PlayVideo(const uchar *Data, int Length)
{
  return -1;
}

int cDevice::PlayAudio(const uchar *Data, int Length)
{
  return -1;
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
               w = PlayVideo(Start, d);
               break;
          case 0xC0 ... 0xDF: // audio
               SetAvailableTrack(ttAudio, c - 0xC0, c);
               if (!VideoOnly && c == availableTracks[currentAudioTrack].id) {
                  w = PlayAudio(Start, d);
                  if (FirstLoop)
                     Audios.PlayAudio(Data, Length, c);
                  }
               break;
          case 0xBD: { // private stream 1
               int PayloadOffset = Data[8] + 9;
               uchar SubStreamId = Data[PayloadOffset];
               uchar SubStreamType = SubStreamId & 0xF0;
               uchar SubStreamIndex = SubStreamId & 0x1F;
        
               // Compatibility mode for old VDR recordings, where 0xBD was only AC3:
pre_1_3_19_PrivateStreamDeteced:
               if (pre_1_3_19_PrivateStream) {
                  SubStreamId = c;
                  SubStreamType = 0x80;
                  SubStreamIndex = 0;
                  }
               switch (SubStreamType) {
                 case 0x20: // SPU
                 case 0x30: // SPU
                      break;
                 case 0x80: // AC3 & DTS
                      if (Setup.UseDolbyDigital) {
                         SetAvailableTrack(ttDolby, SubStreamIndex, SubStreamId);
                         if (!VideoOnly && SubStreamId == availableTracks[currentAudioTrack].id) {
                            w = PlayAudio(Start, d);
                            if (FirstLoop)
                               Audios.PlayAudio(Data, Length, SubStreamId);
                            }
                         }
                      break;
                 case 0xA0: // LPCM
                      SetAvailableTrack(ttAudio, SubStreamIndex, SubStreamId);
                      if (!VideoOnly && SubStreamId == availableTracks[currentAudioTrack].id) {
                         w = PlayAudio(Start, d);
                         if (FirstLoop)
                            Audios.PlayAudio(Data, Length, SubStreamId);
                         }
                      break;
                 default:
                      // Compatibility mode for old VDR recordings, where 0xBD was only AC3:
                      if (!pre_1_3_19_PrivateStream) {
                         dsyslog("switching to pre 1.3.19 Dolby Digital compatibility mode");
                         ClrAvailableTracks();
                         pre_1_3_19_PrivateStream = true;
                         goto pre_1_3_19_PrivateStreamDeteced;
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
     pesAssembler->Reset();
     return 0;
     }
  int Result = 0;
  if (pesAssembler->Length()) {
     // Make sure we have a complete PES header:
     while (pesAssembler->Length() < 6 && Length > 0) {
           pesAssembler->Put(*Data++);
           Length--;
           Result++;
           }
     if (pesAssembler->Length() < 6)
        return Result; // Still no complete PES header - wait for more
     int l = pesAssembler->ExpectedLength();
     int Rest = min(l - pesAssembler->Length(), Length);
     pesAssembler->Put(Data, Rest);
     Data += Rest;
     Length -= Rest;
     Result += Rest;
     if (pesAssembler->Length() < l)
        return Result; // Still no complete PES packet - wait for more
     // Now pesAssembler contains one complete PES packet.
     int w = PlayPesPacket(pesAssembler->Data(), pesAssembler->Length(), VideoOnly);
     if (w > 0)
        pesAssembler->Reset();
     return Result > 0 ? Result : w < 0 ? w : 0;
     }
  int i = 0;
  while (i <= Length - 6) {
        if (Data[i] == 0x00 && Data[i + 1] == 0x00 && Data[i + 2] == 0x01) {
           int l = cPesAssembler::PacketSize(&Data[i]);
           if (i + l > Length) {
              // Store incomplete PES packet for later completion:
              pesAssembler->Put(Data + i, Length - i);
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
     pesAssembler->Put(Data + i, Length - i);
  return Length;
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
  /*XXX
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
  XXX*/
}

bool cDevice::Ready(void)
{
  return true;
}

int cDevice::ProvidesCa(const cChannel *Channel) const
{
  int Ca = Channel->Ca();
  if (Ca == CardIndex() + 1)
     return 1; // exactly _this_ card was requested
  if (Ca && Ca <= MAXDEVICES)
     return 0; // a specific card was requested, but not _this_ one
  return !Ca; // by default every card can provide FTA
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
  if (Running() && OpenDvr()) {
     while (Running()) {
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
  if (!HasLock(TUNER_LOCK_TIMEOUT)) {
     esyslog("ERROR: device %d has no lock, can't attach receiver!", CardIndex() + 1);
     return false;
     }
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
         Lock();
         Receiver->device = this;
         receiver[i] = Receiver;
         Unlock();
         if (!Running())
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
  cMutexLock MutexLock(&mutexReceiver);
  for (int i = 0; i < MAXRECEIVERS; i++) {
      if (receiver[i] == Receiver) {
         Receiver->Activate(false);
         Lock();
         receiver[i] = NULL;
         Receiver->device = NULL;
         Unlock();
         for (int n = 0; n < Receiver->numPids; n++)
             DelPid(Receiver->pids[n]);
         }
      else if (receiver[i])
         receiversLeft = true;
      }
  if (!receiversLeft)
     Cancel(3);
}

void cDevice::DetachAll(int Pid)
{
  if (Pid) {
     cMutexLock MutexLock(&mutexReceiver);
     for (int i = 0; i < MAXRECEIVERS; i++) {
         cReceiver *Receiver = receiver[i];
         if (Receiver && Receiver->WantsPid(Pid))
            Detach(Receiver);
         }
     }
}

// --- cTSBuffer -------------------------------------------------------------

cTSBuffer::cTSBuffer(int File, int Size, int CardIndex)
{
  SetDescription("TS buffer on device %d", CardIndex);
  f = File;
  cardIndex = CardIndex;
  delivered = false;
  ringBuffer = new cRingBufferLinear(Size, TS_SIZE, true, "TS");
  ringBuffer->SetTimeouts(100, 100);
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
                    esyslog("ERROR: driver buffer overflow on device %d", cardIndex);
                 else {
                    LOG_ERROR;
                    break;
                    }
                 }
              }
           }
     }
}

uchar *cTSBuffer::Get(void)
{
  int Count = 0;
  if (delivered) {
     ringBuffer->Del(TS_SIZE);
     delivered = false;
     }
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
        esyslog("ERROR: skipped %d bytes to sync on TS packet on device %d", Count, cardIndex);
        return NULL;
        }
     delivered = true;
     return p;
     }
  return NULL;
}
