/*
 * dvbdevice.h: The DVB device tuner interface
 *
 * See the main source file 'vdr.c' for copyright information and
 * how to reach the author.
 *
 * $Id: dvbdevice.h 5.1 2024/07/08 09:34:33 kls Exp $
 */

#ifndef __DVBDEVICE_H
#define __DVBDEVICE_H

#include <linux/dvb/frontend.h>
#include <linux/dvb/version.h>
#include "device.h"

#define DVBAPIVERSION (DVB_API_VERSION << 8 | DVB_API_VERSION_MINOR)

#if DVBAPIVERSION < 0x0500
#error VDR requires Linux DVB driver API version 5.0 or higher!
#endif

// --- Definitions for older DVB API versions --------------------------------

#if DVBAPIVERSION < 0x0501
enum {
  FE_CAN_2G_MODULATION = 0x10000000,
  };
enum {
  TRANSMISSION_MODE_4K = TRANSMISSION_MODE_AUTO + 1,
  };
#endif

#if DVBAPIVERSION < 0x0502
enum {
  FE_CAN_TURBO_FEC = 0x8000000,
  };
#endif

#if DVBAPIVERSION < 0x0503
enum {
  TRANSMISSION_MODE_1K = TRANSMISSION_MODE_4K + 1,
  TRANSMISSION_MODE_16K,
  TRANSMISSION_MODE_32K,
  };
enum {
  GUARD_INTERVAL_1_128 = GUARD_INTERVAL_AUTO + 1,
  GUARD_INTERVAL_19_128,
  GUARD_INTERVAL_19_256,
  };
enum {
  SYS_DVBT2 = SYS_DAB + 1,
  };
#endif

#if DVBAPIVERSION < 0x0505
#define DTV_ENUM_DELSYS  44
#endif

#if DVBAPIVERSION < 0x0508
enum {
  FE_CAN_MULTISTREAM = 0x4000000,
  };
#define DTV_STREAM_ID            42
#define DTV_DVBT2_PLP_ID_LEGACY  43
#endif

// --- End of definitions for older DVB API versions -------------------------

#define DEV_VIDEO         "/dev/video"
#define DEV_DVB_BASE      "/dev/dvb"
#define DEV_DVB_ADAPTER   "adapter"
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

extern const tDvbParameterMap PilotValues[];
extern const tDvbParameterMap InversionValues[];
extern const tDvbParameterMap BandwidthValues[];
extern const tDvbParameterMap CoderateValues[];
extern const tDvbParameterMap ModulationValues[];
extern const tDvbParameterMap SystemValuesSat[];
extern const tDvbParameterMap SystemValuesTerr[];
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
  int streamId;
  int t2systemId;
  int sisoMiso;
  int pilot;
  int PrintParameter(char *p, char Name, int Value) const;
  const char *ParseParameter(const char *s, int &Value, const tDvbParameterMap *Map = NULL);
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
  int StreamId(void) const { return streamId; }
  int T2SystemId(void) const { return t2systemId; }
  int SisoMiso(void) const { return sisoMiso; }
  int Pilot(void) const { return pilot; }
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
  void SetStreamId(int StreamId) { streamId = StreamId; }
  void SetT2SystemId(int T2SystemId) { t2systemId = T2SystemId; }
  void SetSisoMiso(int SisoMiso) { sisoMiso = SisoMiso; }
  void SetPilot(int Pilot) { pilot = Pilot; }
  cString ToString(char Type) const;
  bool Parse(const char *s);
  };

class cDvbTuner;

cString DvbName(const char *Name, int Adapter, int Frontend);
int DvbOpen(const char *Name, int Adapter, int Frontend, int Mode, bool ReportError = false);

/// The cDvbDevice implements a DVB device which can be accessed through the Linux DVB driver API.

class cDvbDevice : public cDevice {
private:
  static bool Exists(int Adapter, int Frontend);
         ///< Checks whether the given adapter/frontend exists.
  static bool Probe(int Adapter, int Frontend);
         ///< Probes for existing DVB devices.
public:
  static bool useDvbDevices;
  static bool Initialize(void);
         ///< Initializes the DVB devices.
         ///< Must be called before accessing any DVB functions.
         ///< Returns true if any devices are available.
protected:
  int adapter, frontend;
  virtual bool IsBonded(void) const { return bondedDevice; }
private:
  int fd_dvr, fd_ca;
  bool checkTsBuffer;
  static cMutex bondMutex;
  cDvbDevice *bondedDevice;
  mutable bool needsDetachBondedReceivers;
public:
  cDvbDevice(int Adapter, int Frontend);
  virtual ~cDvbDevice();
  int Adapter(void) const { return adapter; }
  int Frontend(void) const;
  virtual cString DeviceType(void) const;
  virtual cString DeviceName(void) const;
  static bool BondDevices(const char *Bondings);
       ///< Bonds the devices as defined in the given Bondings string.
       ///< A bonding is a sequence of device numbers (starting at 1),
       ///< separated by '+' characters. Several bondings are separated by
       ///< commas, as in "1+2,3+4+5".
       ///< Returns false if an error occurred.
  static void UnBondDevices(void);
       ///< Unbonds all devices.
  bool Bond(cDvbDevice *Device);
       ///< Bonds this device with the given Device, making both of them use
       ///< the same satellite cable and LNB. Only DVB-S(2) devices can be
       ///< bonded. When this function is called, the calling device must
       ///< not be bonded to any other device. The given Device, however,
       ///< may already be bonded to an other device. That way several devices
       ///< can be bonded together.
       ///< Returns true if the bonding was successful.
  void UnBond(void);
       ///< Removes this device from any bonding it might have with other
       ///< devices. If this device is not bonded with any other device,
       ///< nothing happens.
  bool BondingOk(const cChannel *Channel, bool ConsiderOccupied = false) const;
       ///< Returns true if this device is either not bonded to any other
       ///< device, or the given Channel is on the same satellite, polarization
       ///< and band as those the bonded devices are tuned to (if any).
       ///< If ConsiderOccupied is true, any bonded devices that are currently
       ///< occupied but not otherwise receiving will cause this function to
       ///< return false.

// Common Interface facilities:

private:
  cCiAdapter *ciAdapter;

// Channel facilities

private:
  cDvbTuner *dvbTuner;
public:
  virtual bool ProvidesDeliverySystem(int DeliverySystem) const;
  virtual bool ProvidesSource(int Source) const;
  virtual bool ProvidesTransponder(const cChannel *Channel) const;
  virtual bool ProvidesChannel(const cChannel *Channel, int Priority = IDLEPRIORITY, bool *NeedsDetachReceivers = NULL) const;
  virtual bool ProvidesEIT(void) const;
  virtual int NumProvidedSystems(void) const;
  virtual const cPositioner *Positioner(void) const;
  virtual bool SignalStats(int &Valid, double *Strength = NULL, double *Cnr = NULL, double *BerPre = NULL, double *BerPost = NULL, double *Per = NULL, int *Status = NULL) const;
  virtual int SignalStrength(void) const;
  virtual int SignalQuality(void) const;
  virtual const cChannel *GetCurrentlyTunedTransponder(void) const;
  virtual bool IsTunedToTransponder(const cChannel *Channel) const;
  virtual bool MaySwitchTransponder(const cChannel *Channel) const;
  virtual void SetPowerSaveMode(bool On);
protected:
  virtual bool SetChannelDevice(const cChannel *Channel, bool LiveView);
public:
  virtual bool HasLock(int TimeoutMs = 0) const;

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
  static void SetTransferModeForDolbyDigital(int Mode); // needs to be here for backwards compatibility
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
  virtual void DetachAllReceivers(void);
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
  static uint32_t GetSubsystemId(int Adapter, int Frontend);
  virtual bool Probe(int Adapter, int Frontend) = 0;
     ///< Probes for a DVB device at the given Adapter and creates the appropriate
     ///< object derived from cDvbDevice if applicable.
     ///< Returns true if a device has been created.
  };

extern cList<cDvbDeviceProbe> DvbDeviceProbes;

#endif //__DVBDEVICE_H
