/*
 * dvbdevice.h: The DVB device tuner interface
 *
 * See the main source file 'vdr.c' for copyright information and
 * how to reach the author.
 *
 * $Id: dvbdevice.h 2.14 2010/04/11 10:29:37 kls Exp $
 */

#ifndef __DVBDEVICE_H
#define __DVBDEVICE_H

#include <sys/mman.h> // FIXME: workaround for broken linux-dvb header files
#include <linux/dvb/frontend.h>
#include <linux/dvb/version.h>
#include "device.h"

#if DVB_API_VERSION < 5
#error VDR requires Linux DVB driver API version 5.0 or higher!
#endif

#define MAXDVBDEVICES  8

#define DEV_VIDEO         "/dev/video"
#define DEV_DVB_ADAPTER   "/dev/dvb/adapter"
#define DEV_DVB_OSD       "osd"
#define DEV_DVB_FRONTEND  "frontend"
#define DEV_DVB_DVR       "dvr"
#define DEV_DVB_DEMUX     "demux"
#define DEV_DVB_VIDEO     "video"
#define DEV_DVB_AUDIO     "audio"
#define DEV_DVB_CA        "ca"

struct tDvbParameterMap {
  int userValue;
  int driverValue;
  const char *userString;
  };

const char *MapToUserString(int Value, const tDvbParameterMap *Map);
int MapToUser(int Value, const tDvbParameterMap *Map, const char **String = NULL);
int MapToDriver(int Value, const tDvbParameterMap *Map);
int UserIndex(int Value, const tDvbParameterMap *Map);
int DriverIndex(int Value, const tDvbParameterMap *Map);

extern const tDvbParameterMap InversionValues[];
extern const tDvbParameterMap BandwidthValues[];
extern const tDvbParameterMap CoderateValues[];
extern const tDvbParameterMap ModulationValues[];
extern const tDvbParameterMap SystemValues[];
extern const tDvbParameterMap TransmissionValues[];
extern const tDvbParameterMap GuardValues[];
extern const tDvbParameterMap HierarchyValues[];
extern const tDvbParameterMap RollOffValues[];

class cDvbTransponderParameters {
friend class cDvbSourceParam;
private:
  char polarization;
  int inversion;
  int bandwidth;
  int coderateH;
  int coderateL;
  int modulation;
  int system;
  int transmission;
  int guard;
  int hierarchy;
  int rollOff;
  int PrintParameter(char *p, char Name, int Value) const;
  const char *ParseParameter(const char *s, int &Value, const tDvbParameterMap *Map);
public:
  cDvbTransponderParameters(const char *Parameters = NULL);
  char Polarization(void) const { return polarization; }
  int Inversion(void) const { return inversion; }
  int Bandwidth(void) const { return bandwidth; }
  int CoderateH(void) const { return coderateH; }
  int CoderateL(void) const { return coderateL; }
  int Modulation(void) const { return modulation; }
  int System(void) const { return system; }
  int Transmission(void) const { return transmission; }
  int Guard(void) const { return guard; }
  int Hierarchy(void) const { return hierarchy; }
  int RollOff(void) const { return rollOff; }
  void SetPolarization(char Polarization) { polarization = Polarization; }
  void SetInversion(int Inversion) { inversion = Inversion; }
  void SetBandwidth(int Bandwidth) { bandwidth = Bandwidth; }
  void SetCoderateH(int CoderateH) { coderateH = CoderateH; }
  void SetCoderateL(int CoderateL) { coderateL = CoderateL; }
  void SetModulation(int Modulation) { modulation = Modulation; }
  void SetSystem(int System) { system = System; }
  void SetTransmission(int Transmission) { transmission = Transmission; }
  void SetGuard(int Guard) { guard = Guard; }
  void SetHierarchy(int Hierarchy) { hierarchy = Hierarchy; }
  void SetRollOff(int RollOff) { rollOff = RollOff; }
  cString ToString(char Type) const;
  bool Parse(const char *s);
  };

class cDvbTuner;

/// The cDvbDevice implements a DVB device which can be accessed through the Linux DVB driver API.

class cDvbDevice : public cDevice {
protected:
  static cString DvbName(const char *Name, int Adapter, int Frontend);
  static int DvbOpen(const char *Name, int Adapter, int Frontend, int Mode, bool ReportError = false);
private:
  static bool Exists(int Adapter, int Frontend);
         ///< Checks whether the given adapter/frontend exists.
  static bool Probe(int Adapter, int Frontend);
         ///< Probes for existing DVB devices.
public:
  static bool Initialize(void);
         ///< Initializes the DVB devices.
         ///< Must be called before accessing any DVB functions.
         ///< \return True if any devices are available.
protected:
  int adapter, frontend;
private:
  dvb_frontend_info frontendInfo;
  int numProvidedSystems;
  fe_delivery_system frontendType;
  int fd_dvr, fd_ca;
public:
  cDvbDevice(int Adapter, int Frontend);
  virtual ~cDvbDevice();
  virtual bool Ready(void);

// Common Interface facilities:

private:
  cCiAdapter *ciAdapter;

// Channel facilities

private:
  cDvbTuner *dvbTuner;
public:
  virtual bool ProvidesSource(int Source) const;
  virtual bool ProvidesTransponder(const cChannel *Channel) const;
  virtual bool ProvidesChannel(const cChannel *Channel, int Priority = -1, bool *NeedsDetachReceivers = NULL) const;
  virtual int NumProvidedSystems(void) const;
  virtual const cChannel *GetCurrentlyTunedTransponder(void) const;
  virtual bool IsTunedToTransponder(const cChannel *Channel);
protected:
  virtual bool SetChannelDevice(const cChannel *Channel, bool LiveView);
public:
  virtual bool HasLock(int TimeoutMs = 0);

// PID handle facilities

protected:
  virtual bool SetPid(cPidHandle *Handle, int Type, bool On);

// Section filter facilities

protected:
  virtual int OpenFilter(u_short Pid, u_char Tid, u_char Mask);
  virtual void CloseFilter(int Handle);

// Common Interface facilities:

public:
  virtual bool HasCi(void);

// Audio facilities

protected:
  static int setTransferModeForDolbyDigital;
public:
  static void SetTransferModeForDolbyDigital(int Mode); // needs to be here for backwards compatibilty
         ///< Controls how the DVB device handles Transfer Mode when replaying
         ///< Dolby Digital audio.
         ///< 0 = don't set "audio bypass" in driver/firmware, don't force Transfer Mode
         ///< 1 = set "audio bypass" in driver/firmware, force Transfer Mode (default)
         ///< 2 = don't set "audio bypass" in driver/firmware, force Transfer Mode

// Receiver facilities

private:
  cTSBuffer *tsBuffer;
protected:
  virtual bool OpenDvr(void);
  virtual void CloseDvr(void);
  virtual bool GetTSPacket(uchar *&Data);
  };

// A plugin that implements a DVB device derived from cDvbDevice needs to create
// a cDvbDeviceProbe derived object on the heap in order to have its Probe()
// function called, where it can actually create the appropriate device.
// The cDvbDeviceProbe object must be created in the plugin's constructor,
// and deleted in its destructor.

class cDvbDeviceProbe : public cListObject {
public:
  cDvbDeviceProbe(void);
  virtual ~cDvbDeviceProbe();
  virtual bool Probe(int Adapter, int Frontend) = 0;
     ///< Probes for a DVB device at the given Adapter and creates the appropriate
     ///< object derived from cDvbDevice if applicable.
     ///< Returns true if a device has been created.
  };

extern cList<cDvbDeviceProbe> DvbDeviceProbes;

#endif //__DVBDEVICE_H
