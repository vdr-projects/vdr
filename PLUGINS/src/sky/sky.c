/*
 * sky.c: A plugin for the Video Disk Recorder
 *
 * See the README file for copyright information and how to reach the author.
 *
 * $Id: sky.c 1.11 2004/12/26 12:45:22 kls Exp $
 */

#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <sys/un.h>
#include <unistd.h>
#include <vdr/plugin.h>
#include <vdr/sources.h>

static const char *VERSION        = "0.3.3";
static const char *DESCRIPTION    = "Sky Digibox interface";

// --- cDigiboxDevice --------------------------------------------------------

#define DUMMYAPID  80
#define DUMMYVPID 160

class cSkyChannel : public cListObject {
public:
  tChannelID channelID;
  int digiboxChannelNumber;
  bool Parse(const char *s);
  };

bool cSkyChannel::Parse(const char *s)
{
  char *id = NULL;
  if (2 == sscanf(s, "%a[^:]:%d", &id, &digiboxChannelNumber))
     channelID = tChannelID::FromString(id);
  free(id);
  return digiboxChannelNumber && channelID.Valid();
}

class cSkyChannels : public cConfig<cSkyChannel> {
public:
  cSkyChannel *GetSkyChannel(const cChannel *Channel);
  };

cSkyChannel *cSkyChannels::GetSkyChannel(const cChannel *Channel)
{
  tChannelID ChannelID = Channel->GetChannelID();
  for (cSkyChannel *sc = First(); sc; sc = Next(sc)) {
      if (ChannelID == sc->channelID)
         return sc;
      }
  return NULL;
}

cSkyChannels SkyChannels;

class cDigiboxDevice : public cDevice {
private:
  int source;
  int digiboxChannelNumber;
  int fd_dvr;
  int apid, vpid;
  cTSBuffer *tsBuffer;
  int fd_lirc;
  void LircSend(const char *s);
  void LircSend(int n);
protected:
  virtual bool SetPid(cPidHandle *Handle, int Type, bool On);
  virtual bool OpenDvr(void);
  virtual void CloseDvr(void);
  virtual bool GetTSPacket(uchar *&Data);
public:
  cDigiboxDevice(void);
  virtual ~cDigiboxDevice();
  virtual bool ProvidesSource(int Source) const;
  virtual bool ProvidesTransponder(const cChannel *Channel) const;
  virtual bool ProvidesChannel(const cChannel *Channel, int Priority = -1, bool *NeedsSetChannel = NULL) const;
  virtual bool SetChannelDevice(const cChannel *Channel, bool LiveView);
  };

cDigiboxDevice::cDigiboxDevice(void)
{
  source = cSource::FromString("S28.2E");//XXX parameter???
  digiboxChannelNumber = 0;
  fd_dvr = -1;
  apid = vpid = 0;
  struct sockaddr_un addr;
  addr.sun_family = AF_UNIX;
  strn0cpy(addr.sun_path, "/dev/lircd", sizeof(addr.sun_path));//XXX parameter???
  fd_lirc = socket(AF_UNIX, SOCK_STREAM, 0);
  if (fd_lirc >= 0) {
     if (connect(fd_lirc, (struct sockaddr *)&addr, sizeof(addr)) < 0) {
        LOG_ERROR;
        close(fd_lirc);
        }
     }
  else
     LOG_ERROR;
}

cDigiboxDevice::~cDigiboxDevice()
{
  if (fd_lirc >= 0)
     close(fd_lirc);
}

void cDigiboxDevice::LircSend(const char *s)
{
  const char *c = "SEND_ONCE SKY %s\n";
  char buf[100];
  sprintf(buf, c, s);
  dsyslog(buf);//XXX
  if (write(fd_lirc, buf, strlen(buf)) < 0)
     LOG_ERROR;//XXX _STR
  cCondWait::SleepMs(200);
}

void cDigiboxDevice::LircSend(int n)
{
  char buf[10];
  snprintf(buf, sizeof(buf), "%d", n);
  char *p = buf;
  while (*p) {
        char q[10];
        sprintf(q, "%c", *p);
        LircSend(q);
        p++;
        }
}

bool cDigiboxDevice::SetPid(cPidHandle *Handle, int Type, bool On)
{
  //dsyslog("SetPid %d %d", Handle->pid, On);
  return true;
}

bool cDigiboxDevice::OpenDvr(void)
{
  CloseDvr();
  fd_dvr = open("/dev/video2", O_RDONLY | O_NONBLOCK);//XXX parameter???
  if (fd_dvr >= 0)
     tsBuffer = new cTSBuffer(fd_dvr, MEGABYTE(2), CardIndex() + 1);
  return fd_dvr >= 0;
}

void cDigiboxDevice::CloseDvr(void)
{
  if (fd_dvr >= 0) {
     close(fd_dvr);
     fd_dvr = -1;
     delete tsBuffer;
     tsBuffer = NULL;
     }
}

bool cDigiboxDevice::GetTSPacket(uchar *&Data)
{
  if (tsBuffer) {
     Data = tsBuffer->Get();
     if (Data) {
        // insert the actual PIDs:
        int Pid = (((uint16_t)Data[1] & PID_MASK_HI) << 8) | Data[2];
        if (Pid == DUMMYAPID)
           Pid = apid;
        else if (Pid == DUMMYVPID)
           Pid = vpid;
        Data[1] = ((Pid >> 8) & 0xFF) | (Data[1] & ~PID_MASK_HI);
        Data[2] = Pid & 0xFF;
        }
     return true;
     }
  return false;
}

bool cDigiboxDevice::ProvidesSource(int Source) const
{
  return source == Source;
}

bool cDigiboxDevice::ProvidesTransponder(const cChannel *Channel) const
{
  return false; // can't provide any actual transponder
}

bool cDigiboxDevice::ProvidesChannel(const cChannel *Channel, int Priority, bool *NeedsDetachReceivers) const
{
  bool result = false;
  bool hasPriority = Priority < 0 || Priority > this->Priority();
  bool needsDetachReceivers = true;

  cSkyChannel *SkyChannel = SkyChannels.GetSkyChannel(Channel);
  if (SkyChannel) {
     if (Receiving(true)) {
        if (digiboxChannelNumber == SkyChannel->digiboxChannelNumber) {
           needsDetachReceivers = false;
           result = true;
           }
        else
           result = hasPriority;
        }
     else
        result = hasPriority;
     }
  if (NeedsDetachReceivers)
     *NeedsDetachReceivers = needsDetachReceivers;
  return result;
}

bool cDigiboxDevice::SetChannelDevice(const cChannel *Channel, bool LiveView)
{
  if (fd_lirc >= 0 && !Receiving(true)) { // if we are receiving the channel is already set!
     cSkyChannel *SkyChannel = SkyChannels.GetSkyChannel(Channel);
     if (SkyChannel) {
        digiboxChannelNumber = SkyChannel->digiboxChannelNumber;
        apid = Channel->Apid(0);
        vpid = Channel->Vpid();
        //XXX only when recording??? -> faster channel switching!
        LircSend("SKY"); // makes sure the Digibox is "on"
        //XXX lircprint(fd_lirc, "BACKUP");
        //XXX lircprint(fd_lirc, "BACKUP");
        //XXX lircprint(fd_lirc, "BACKUP");
        LircSend(digiboxChannelNumber);
        }
     }
  return true;
}

// --- cPluginSky ------------------------------------------------------------

class cPluginSky : public cPlugin {
private:
  // Add any member variables or functions you may need here.
public:
  cPluginSky(void);
  virtual ~cPluginSky();
  virtual const char *Version(void) { return VERSION; }
  virtual const char *Description(void) { return DESCRIPTION; }
  virtual const char *CommandLineHelp(void);
  virtual bool ProcessArgs(int argc, char *argv[]);
  virtual bool Initialize(void);
  virtual void Housekeeping(void);
  virtual cMenuSetupPage *SetupMenu(void);
  virtual bool SetupParse(const char *Name, const char *Value);
  };

cPluginSky::cPluginSky(void)
{
  // Initialize any member variables here.
  // DON'T DO ANYTHING ELSE THAT MAY HAVE SIDE EFFECTS, REQUIRE GLOBAL
  // VDR OBJECTS TO EXIST OR PRODUCE ANY OUTPUT!
}

cPluginSky::~cPluginSky()
{
  // Clean up after yourself!
}

const char *cPluginSky::CommandLineHelp(void)
{
  // Return a string that describes all known command line options.
  return NULL;
}

bool cPluginSky::ProcessArgs(int argc, char *argv[])
{
  // Implement command line argument processing here if applicable.
  return true;
}

bool cPluginSky::Initialize(void)
{
  // Initialize any background activities the plugin shall perform.
  const char *ConfigDir = ConfigDirectory(Name());
  if (ConfigDir) {
     if (SkyChannels.Load(AddDirectory(ConfigDir, "channels.conf.sky"), true)) {
        new cDigiboxDevice;
        return true;
        }
     }
  else
     esyslog("ERROR: can't get config directory");
  return false;
}

void cPluginSky::Housekeeping(void)
{
  // Perform any cleanup or other regular tasks.
}

cMenuSetupPage *cPluginSky::SetupMenu(void)
{
  // Return a setup menu in case the plugin supports one.
  return NULL;
}

bool cPluginSky::SetupParse(const char *Name, const char *Value)
{
  // Parse your own setup parameters and store their values.
  return false;
}

VDRPLUGINCREATOR(cPluginSky); // Don't touch this!
