/*
 * ci.h: Common Interface
 *
 * See the main source file 'vdr.c' for copyright information and
 * how to reach the author.
 *
 * $Id: ci.h 5.1 2025/03/02 11:03:35 kls Exp $
 */

#ifndef __CI_H
#define __CI_H

#include <stdint.h>
#include <stdio.h>
#include "channels.h"
#include "ringbuffer.h"
#include "thread.h"
#include "tools.h"

#define MAX_CAM_SLOTS_PER_ADAPTER    16 // maximum possible value is 255 (same value as MAXDEVICES!)
#define MAX_CONNECTIONS_PER_CAM_SLOT  8 // maximum possible value is 254
#define CAM_READ_TIMEOUT  50 // ms

class cCiTransportConnection;
class cCamSlot;

// VDR's Common Interface functions implement only the features that are absolutely
// necessary to control a CAM. If a plugin wants to implement additional functionality
// (i.e. "resources"), it can do so by deriving from cCiResourceHandler, cCiSession
// and (if necessary) from cCiApplicationInformation.

class cCiSession {
private:
  uint16_t sessionId;
  uint32_t resourceId;
  cCiTransportConnection *tc;
protected:
  void SetTsPostProcessor(void);
       ///< If this cCiSession implements the TsPostProcess() function, it shall call
       ///< SetTsPostProcessor() to register itself as the TS post processor.
  void SetResourceId(uint32_t Id);
       ///< If this is a class that has been derived from an existing cCiSession class,
       ///< but implements a different resource id, it shall call SetResourceId() with
       ///< that Id.
  int GetTag(int &Length, const uint8_t **Data);
  const uint8_t *GetData(const uint8_t *Data, int &Length);
  void SendData(int Tag, int Length = 0, const uint8_t *Data = NULL);
  cCiTransportConnection *Tc(void) { return tc; }
public:
  cCiSession(uint16_t SessionId, uint32_t ResourceId, cCiTransportConnection *Tc);
  virtual ~cCiSession();
  uint16_t SessionId(void) { return sessionId; }
  uint32_t ResourceId(void) { return resourceId; }
  cCamSlot *CamSlot(void);
  virtual bool HasUserIO(void) { return false; }
  virtual void Process(int Length = 0, const uint8_t *Data = NULL);
  virtual bool TsPostProcess(uint8_t *TsPacket) { return false; }
       ///< If this cCiSession needs to do additional processing on TS packets (after
       ///< the CAM has done the decryption), it shall implement TsPostProcess() and
       ///< do whatever operations are necessary on the given TsPacket. This function
       ///< is called once for each TS packet, and any and all operations must be
       ///< finished upon return.
       ///< A derived cCiSession that implements this function must call
       ///< SetTsPostProcessor() to make it actually get called.
       ///< Returns true if the TsPacket was in any way modified.
  };

class cCiApplicationInformation : public cCiSession {
protected:
  int state;
  uint8_t applicationType;
  uint16_t applicationManufacturer;
  uint16_t manufacturerCode;
  char *menuString;
public:
  cCiApplicationInformation(uint16_t SessionId, cCiTransportConnection *Tc);
  virtual ~cCiApplicationInformation() override;
  virtual void Process(int Length = 0, const uint8_t *Data = NULL) override;
  bool EnterMenu(void);
  const char *GetMenuString(void) { return menuString; }
  };

class cCiResourceHandler : public cListObject {
public:
  cCiResourceHandler(void);
       ///< Creates a new resource handler, through which the available resources
       ///< can be provides. A resource handler shall be allocated on the heap and
       ///< registered with the global CiResourceHandlers, as in
       ///< CiResourceHandlers.Register(new cMyResourceHandler);
       ///< It will be automatically deleted at the end of the program.
  virtual ~cCiResourceHandler() override;
  virtual const uint32_t *ResourceIds(void) const = 0;
       ///< Returns a pointer to an array of resource identifiers, where the
       ///< last value is zero.
  virtual cCiSession *GetNewCiSession(uint32_t ResourceId, uint16_t SessionId, cCiTransportConnection *Tc) = 0;
       ///< Returns a new cCiSession, according to the given ResourceId.
  };

class cCiResourceHandlers : public cList<cCiResourceHandler> {
private:
  cVector<uint32_t> resourceIds;
public:
  cCiResourceHandlers(void);
       ///< Creates the default list of resourceIds.
  void Register(cCiResourceHandler *ResourceHandler);
       ///< Adds the given ResourceHandler to the list of resource handlers and
       ///< appends its ResourceIds to the global resourceIds.
       ///< A plugin that implements additional CAM capabilities must call
       ///< this function to register its resources.
  const uint32_t *Ids(void) { return &resourceIds[0]; }
  int NumIds(void) { return resourceIds.Size(); }
  cCiSession *GetNewCiSession(uint32_t ResourceId, uint16_t SessionId, cCiTransportConnection *Tc);
  };

extern cCiResourceHandlers CiResourceHandlers;

class cCiMMI;

class cCiMenu {
  friend class cCamSlot;
  friend class cCiMMI;
private:
  enum { MAX_CIMENU_ENTRIES = 64 }; ///< XXX is there a specified maximum?
  cCiMMI *mmi;
  cMutex *mutex;
  bool selectable;
  char *titleText;
  char *subTitleText;
  char *bottomText;
  char *entries[MAX_CIMENU_ENTRIES];
  int numEntries;
  bool AddEntry(char *s);
  cCiMenu(cCiMMI *MMI, bool Selectable);
public:
  ~cCiMenu();
  const char *TitleText(void) { return titleText; }
  const char *SubTitleText(void) { return subTitleText; }
  const char *BottomText(void) { return bottomText; }
  const char *Entry(int n) { return n < numEntries ? entries[n] : NULL; }
  int NumEntries(void) { return numEntries; }
  bool Selectable(void) { return selectable; }
  void Select(int Index);
  void Cancel(void);
  void Abort(void);
  bool HasUpdate(void);
  };

class cCiEnquiry {
  friend class cCamSlot;
  friend class cCiMMI;
private:
  cCiMMI *mmi;
  cMutex *mutex;
  char *text;
  bool blind;
  int expectedLength;
  cCiEnquiry(cCiMMI *MMI);
public:
  ~cCiEnquiry();
  const char *Text(void) { return text; }
  bool Blind(void) { return blind; }
  int ExpectedLength(void) { return expectedLength; }
  void Reply(const char *s);
  void Cancel(void);
  void Abort(void);
  };

class cDevice;

enum eModuleStatus { msNone, msReset, msPresent, msReady };

class cCiAdapter : public cThread {
  friend class cCamSlot;
private:
  cCamSlot *camSlots[MAX_CAM_SLOTS_PER_ADAPTER];
  void AddCamSlot(cCamSlot *CamSlot);
       ///< Adds the given CamSlot to this CI adapter.
protected:
  cCamSlot *ItCamSlot(int &Iter);
       ///< Iterates over all added CAM slots of this adapter. Iter has to be
       ///< initialized to 0 and is required to store the iteration state.
       ///< Returns NULL if no further CAM slot is found.
  virtual void Action(void) override;
       ///< Handles the attached CAM slots in a separate thread.
       ///< The derived class must call the Start() function to
       ///< actually start CAM handling.
  virtual int Read(uint8_t *Buffer, int MaxLength) { return 0; }
       ///< Reads one chunk of data into the given Buffer, up to MaxLength bytes.
       ///< If no data is available immediately, wait for up to CAM_READ_TIMEOUT.
       ///< Returns the number of bytes read (in case of an error it will also
       ///< return 0).
  virtual void Write(const uint8_t *Buffer, int Length) {}
       ///< Writes Length bytes of the given Buffer.
  virtual bool Reset(int Slot) { return false; }
       ///< Resets the CAM in the given Slot.
       ///< Returns true if the operation was successful.
  virtual eModuleStatus ModuleStatus(int Slot) { return msNone; }
       ///< Returns the status of the CAM in the given Slot.
  virtual bool Assign(cDevice *Device, bool Query = false) { return false; }
       ///< Assigns this adapter to the given Device, if this is possible.
       ///< If Query is 'true', the adapter only checks whether it can be
       ///< assigned to the Device, but doesn't actually assign itself to it.
       ///< Returns true if the adapter can be assigned to the Device.
       ///< If Device is NULL, the adapter will be unassigned from any
       ///< device it was previously assigned to. The value of Query
       ///< is ignored in that case, and this function always returns
       ///< 'true'.
public:
  cCiAdapter(void);
  virtual ~cCiAdapter() override;
       ///< The derived class must call Cancel(3) in its destructor.
  };

class cTPDU;
class cCiTransportConnection;
class cCiSession;
class cCiCaProgramData;
class cCaPidReceiver;
class cCaActivationReceiver;
class cMtdHandler;
class cMtdMapper;
class cMtdCamSlot;
class cCiCaPmt;

struct cCiCaPmtList {
  cVector<cCiCaPmt *> caPmts;
  ~cCiCaPmtList();
  cCiCaPmt *Add(uint8_t CmdId, int Source, int Transponder, int ProgramNumber, const int *CaSystemIds);
  void Del(cCiCaPmt *CaPmt);
  };

class cCamSlot : public cListObject {
  friend class cCiAdapter;
  friend class cCiTransportConnection;
  friend class cCiConditionalAccessSupport;
  friend class cMtdCamSlot;
private:
  cMutex mutex;
  cCondVar processed;
  cCiAdapter *ciAdapter;
  cCamSlot *masterSlot;
  cDevice *assignedDevice;
  cCaPidReceiver *caPidReceiver;
  cCaActivationReceiver *caActivationReceiver;
  int slotIndex;
  int slotNumber;
  cCiTransportConnection *tc[MAX_CONNECTIONS_PER_CAM_SLOT + 1];  // connection numbering starts with 1
  eModuleStatus lastModuleStatus;
  time_t resetTime;
  cTimeMs moduleCheckTimer;
  bool resendPmt;
  int source;
  int transponder;
  cList<cCiCaProgramData> caProgramList;
  bool mtdAvailable;
  cMtdHandler *mtdHandler;
  void KeepSharedCaPids(int ProgramNumber, const int *CaSystemIds, int *CaPids);
  void NewConnection(void);
  void DeleteAllConnections(void);
  void Process(cTPDU *TPDU = NULL);
  void Write(cTPDU *TPDU);
  cCiSession *GetSessionByResourceId(uint32_t ResourceId);
  void MtdActivate(bool On);
       ///< Activates (On == true) or deactivates (On == false) MTD.
protected:
  virtual const int *GetCaSystemIds(void);
  virtual void SendCaPmt(uint8_t CmdId);
  virtual bool RepliesToQuery(void);
       ///< Returns true if the CAM in this slot replies to queries and thus
       ///< supports MCD ("Multi Channel Decryption").
  void BuildCaPmts(uint8_t CmdId, cCiCaPmtList &CaPmtList, cMtdMapper *MtdMapper = NULL);
       ///< Generates all CA_PMTs with the given CmdId and stores them in the given CaPmtList.
       ///< If MtdMapper is given, all SIDs and PIDs will be mapped accordingly.
  void SendCaPmts(cCiCaPmtList &CaPmtList);
       ///< Sends the given list of CA_PMTs to the CAM.
  void MtdEnable(void);
       ///< Enables MTD support for this CAM. Note that actual MTD operation also
       ///< requires a CAM that supports MCD ("Multi Channel Decryption").
  int MtdPutData(uchar *Data, int Count);
       ///< Sends at most Count bytes of the given Data to the individual MTD CAM slots
       ///< that are using this CAM. Data must point to the beginning of a TS packet.
       ///< Returns the number of bytes actually processed.
public:
  bool McdAvailable(void) { return RepliesToQuery(); }
       ///< Returns true if this CAM supports MCD ("Multi Channel Decyption").
  bool MtdAvailable(void) { return mtdAvailable; }
       ///< Returns true if this CAM supports MTD ("Multi Transponder Decryption").
  bool MtdActive(void) { return mtdHandler != NULL; }
       ///< Returns true if MTD is currently active.
public:
  cCamSlot(cCiAdapter *CiAdapter, bool WantsTsData = false, cCamSlot *MasterSlot = NULL);
       ///< Creates a new CAM slot for the given CiAdapter.
       ///< The CiAdapter will take care of deleting the CAM slot,
       ///< so the caller must not delete it!
       ///< If WantsTsData is true, the device this CAM slot is assigned to will
       ///< call the Decrypt() function of this CAM slot, presenting it the complete
       ///< TS data stream of the encrypted programme, including the CA pids.
       ///< If this CAM slot is basically the same as an other one, MasterSlot can
       ///< be given to indicate this. This can be used for instance for CAM slots
       ///< that can do MTD ("Multi Transponder Decryption"), where the first cCamSlot
       ///< is created without giving a MasterSlot, and all others are given the first
       ///< one as their MasterSlot. This can speed up the search for a suitable CAM
       ///< when tuning to an encrypted channel, and it also makes the Setup/CAM menu
       ///< clearer because only the master CAM slots will be shown there.
  virtual ~cCamSlot() override;
  bool IsMasterSlot(void) { return !masterSlot; }
       ///< Returns true if this CAM slot itself is a master slot (which means that
       ///< it doesn't have a pointer to another CAM slot that's its master).
  cCamSlot *MasterSlot(void) { return masterSlot ? masterSlot : this; }
       ///< Returns this CAM slot's master slot, or a pointer to itself if it is a
       ///< master slot.
  cCamSlot *MtdSpawn(void);
       ///< If this CAM slot can do MTD ("Multi Transponder Decryption"),
       ///< a call to this function returns a cMtdCamSlot with this CAM slot
       ///< as its master. Otherwise a pointer to this object is returned, which
       ///< means that MTD is not supported.
  void TriggerResendPmt(void) { resendPmt = true; }
       ///< Tells this CAM slot to resend the list of CA_PMTs to the CAM.
  virtual bool Assign(cDevice *Device, bool Query = false);
       ///< Assigns this CAM slot to the given Device, if this is possible.
       ///< If Query is 'true', the CI adapter of this slot only checks whether
       ///< it can be assigned to the Device, but doesn't actually assign itself to it.
       ///< Returns true if this slot can be assigned to the Device.
       ///< If Device is NULL, the slot will be unassigned from any
       ///< device it was previously assigned to. The value of Query
       ///< is ignored in that case, and this function always returns
       ///< 'true'.
       ///< If a derived class reimplements this function, it can return 'false'
       ///< if this CAM can't be assigned to the given Device. If the CAM can be
       ///< assigned to the Device, or if Device is NULL, it must call the base
       ///< class function.
  cDevice *Device(void) { return assignedDevice; }
       ///< Returns the device this CAM slot is currently assigned to.
  bool Devices(cVector<int> &DeviceNumbers);
       ///< Adds the numbers of any devices that currently use this CAM to
       ///< the given DeviceNumbers. This can be more than one in case of MTD.
       ///< Returns true if the array is not empty.
  bool WantsTsData(void) const { return caPidReceiver != NULL; }
       ///< Returns true if this CAM slot wants to receive the TS data through
       ///< its Decrypt() function.
  int SlotIndex(void) { return slotIndex; }
       ///< Returns the index of this CAM slot within its CI adapter.
       ///< The first slot has an index of 0.
  int SlotNumber(void) { return slotNumber; }
       ///< Returns the number of this CAM slot within the whole system.
       ///< The first slot has the number 1.
  int MasterSlotNumber(void) { return masterSlot ? masterSlot->SlotNumber() : slotNumber; }
       ///< Returns the number of this CAM's master slot within the whole system.
       ///< The first slot has the number 1.
  virtual bool Reset(void);
       ///< Resets the CAM in this slot.
       ///< Returns true if the operation was successful.
  virtual bool CanActivate(void);
       ///< Returns true if there is a CAM in this slot that can be put into
       ///< activation mode.
  virtual void StartActivation(void);
       ///< Puts the CAM in this slot into a mode where an inserted smart card
       ///< can be activated. The default action is to make IsActivating() return
       ///< true, which causes the device this CAM slot is attached to to never
       ///< automatically detach any receivers with negative priority if the
       ///< PIDs they want to receive are not decrypted by the CAM.
       ///< StartActivation() must be called *after* the CAM slot has been assigned
       ///< to a device. The CAM slot will stay in activation mode until the CAM
       ///< begins to decrypt, a call to CancelActivation() is made, or the device
       ///< is needed for a recording.
  virtual void CancelActivation(void);
       ///< Cancels a previously started activation (if any).
  virtual bool IsActivating(void);
       ///< Returns true if this CAM slot is currently activating a smart card.
  virtual eModuleStatus ModuleStatus(void);
       ///< Returns the status of the CAM in this slot.
  virtual const char *GetCamName(void);
       ///< Returns the name of the CAM in this slot, or NULL if there is
       ///< no ready CAM in this slot.
  virtual bool Ready(void);
       ///< Returns 'true' if the CAM in this slot is ready to decrypt.
  virtual bool HasMMI(void);
       ///< Returns 'true' if the CAM in this slot has an active MMI.
  virtual bool HasUserIO(void);
       ///< Returns true if there is a pending user interaction, which shall
       ///< be retrieved via GetMenu() or GetEnquiry().
  virtual bool EnterMenu(void);
       ///< Requests the CAM in this slot to start its menu.
  virtual cCiMenu *GetMenu(void);
       ///< Gets a pending menu, or NULL if there is no menu.
  virtual cCiEnquiry *GetEnquiry(void);
       ///< Gets a pending enquiry, or NULL if there is no enquiry.
  int Priority(void);
       ///< Returns the priority of the device this slot is currently assigned
       ///< to, or IDLEPRIORITY if it is not assigned to any device.
  virtual bool ProvidesCa(const int *CaSystemIds);
       ///< Returns true if the CAM in this slot provides one of the given
       ///< CaSystemIds. This doesn't necessarily mean that it will be
       ///< possible to actually decrypt such a programme, since CAMs
       ///< usually advertise several CA system ids, while the actual
       ///< decryption is controlled by the smart card inserted into
       ///< the CAM.
  virtual void AddPid(int ProgramNumber, int Pid, int StreamType);
       ///< Adds the given PID information to the list of PIDs. A later call
       ///< to SetPid() will (de)activate one of these entries.
  virtual void SetPid(int Pid, bool Active);
       ///< Sets the given Pid (which has previously been added through a
       ///< call to AddPid()) to Active. A later call to StartDecrypting() will
       ///< send the full list of currently active CA_PMT entries to the CAM.
  virtual void AddChannel(const cChannel *Channel);
       ///< Adds all PIDs of the given Channel to the current list of PIDs.
       ///< If the source or transponder of the channel are different than
       ///< what was given in a previous call to AddChannel(), any previously
       ///< added PIDs will be cleared.
  virtual bool CanDecrypt(const cChannel *Channel, cMtdMapper *MtdMapper = NULL);
       ///< Returns true if there is a CAM in this slot that is able to decrypt
       ///< the given Channel (or at least claims to be able to do so).
       ///< Since the QUERY/REPLY mechanism for CAMs is pretty unreliable (some
       ///< CAMs don't reply to queries at all), we always return true if the
       ///< CAM is currently not decrypting anything. If there is already a
       ///< channel being decrypted, a call to CanDecrypt() checks whether the
       ///< CAM can also decrypt the given channel. Only CAMs that have replied
       ///< to the initial QUERY will perform this check at all. CAMs that never
       ///< replied to the initial QUERY are assumed not to be able to handle
       ///< more than one channel at a time.
       ///< If MtdMapper is given, all SIDs and PIDs will be mapped accordingly.
  virtual void StartDecrypting(void);
       ///< Sends all CA_PMT entries to the CAM that have been modified since the
       ///< last call to this function. This includes CA_PMTs that have been
       ///< added or activated, as well as ones that have been deactivated.
       ///< StartDecrypting() will be called whenever a PID is activated or
       ///< deactivated.
  virtual void StopDecrypting(void);
       ///< Clears the list of CA_PMT entries and tells the CAM to stop decrypting.
       ///< Note that this function is only called when there are no more PIDs for
       ///< the CAM to decrypt. There is no symmetry between StartDecrypting() and
       ///< StopDecrypting().
  virtual bool IsDecrypting(void);
       ///< Returns true if the CAM in this slot is currently used for decrypting.
  virtual uchar *Decrypt(uchar *Data, int &Count);
       ///< If this is a CAM slot that can be freely assigned to any device,
       ///< but will not be directly inserted into the full TS data stream
       ///< in hardware, it can implement this function to be given access
       ///< to the data in the device's TS buffer. Data points to a buffer
       ///< of Count bytes of TS data. The first byte in Data is guaranteed
       ///< to be a TS_SYNC_BYTE, and Count is at least TS_SIZE.
       ///< Note that Decrypt() may be called with Data == NULL! This is necessary
       ///< to allow CAMs that copy the incoming data into a separate buffer to
       ///< return previously received and decrypted TS packets. If Data is NULL,
       ///< Count is 0 and must not be modified, and the return value shall point to the
       ///< next available decrypted TS packet (if any).
       ///< There are three possible ways a CAM can handle decryption:
       ///< 1. If the full TS data is physically routed through the CAM in hardware,
       ///< there is no need to reimplement this function.
       ///< The default implementation simply sets Count to TS_SIZE and returns Data.
       ///< 2. If the CAM works directly on Data and decrypts the TS "in place" it
       ///< shall decrypt at least the very first TS packet in Data, set Count to
       ///< TS_SIZE and return Data. It may decrypt as many TS packets in Data as it
       ///< wants, but it must decrypt at least the very first TS packet (if at all
       ///< possible - if, for whatever reasons, it can't decrypt the very first
       ///< packet, it must return it regardless). Only this very first TS packet will
       ///< be further processed after the call to this function. The next call will
       ///< be done with Data pointing to the TS packet immediately following the
       ///< previous one.
       ///< 3. If the CAM needs to copy the data into a buffer of its own, and/or send
       ///< the data to some file handle for processing and later retrieval, it shall
       ///< set Count to the number of bytes it has read from Data and return a pointer
       ///< to the next available decrypted TS packet (which will *not* be in the
       ///< memory area pointed to by Data, but rather in some buffer that is under
       ///< the CAM's control). If no decrypted TS packet is currently available, NULL
       ///< shall be returned. If no data from Data can currently be processed, Count
       ///< shall be set to 0 and the same Data pointer will be offered in the next
       ///< call to Decrypt(). See mtd.h for further requirements if this CAM can
       ///< do MTD ("Multi Transponder Decryption").
       ///< A derived class that implements this function will also need
       ///< to set the WantsTsData parameter in the call to the base class
       ///< constructor to true in order to receive the TS data.
  virtual bool TsPostProcess(uchar *Data);
       ///< If there is a cCiSession that needs to do additional processing on TS packets
       ///< (after the CAM has done the decryption), this function will call its
       ///< TsPostProcess() function to have it do whatever operations are necessary on
       ///< the given TsPacket.
       ///< Returns true if the TsPacket was in any way modified.
  virtual bool Inject(uchar *Data, int Count);
       ///< Sends all Count bytes of the given Data to the CAM, and returns true
       ///< if this was possible. If the data can't be sent to the CAM completely,
       ///< nothing shall be sent and the return value shall be false.
       ///< No decrypted packet is returned by this function.
       ///< Data is guaranteed to point to one or more complete TS packets.
  virtual void InjectEit(int Sid);
       ///< Injects a generated EIT with a "present event" for the given Sid into
       ///< the TS data stream sent to the CAM. This only applies to CAM slots that
       ///< have WantsTsData set to true in their constructor.
       ///< The default implementation sends an EIT with the minimum event
       ///< necessary to disable the CAMs parental rating prompt.
  };

class cCamSlots : public cList<cCamSlot> {
public:
  int NumReadyMasterSlots(void);
       ///< Returns the number of master CAM slots in the system that are ready
       ///< to decrypt.
  bool WaitForAllCamSlotsReady(int Timeout = 0);
       ///< Waits until all CAM slots have become ready, or the given Timeout
       ///< (seconds) has expired. While waiting, the Ready() function of each
       ///< CAM slot is called in turn, until they all return true.
       ///< Returns true if all CAM slots have become ready within the given
       ///< timeout.
  };

extern cCamSlots CamSlots;

class cChannelCamRelation;

class cChannelCamRelations : public cList<cChannelCamRelation> {
private:
  cMutex mutex;
  cString fileName;
  cChannelCamRelation *GetEntry(tChannelID ChannelID);
  cChannelCamRelation *AddEntry(tChannelID ChannelID);
  time_t lastCleanup;
  void Cleanup(void);
public:
  cChannelCamRelations(void);
  void Reset(int CamSlotNumber);
  bool CamChecked(tChannelID ChannelID, int CamSlotNumber);
  bool CamDecrypt(tChannelID ChannelID, int CamSlotNumber);
  void SetChecked(tChannelID ChannelID, int CamSlotNumber);
  void SetDecrypt(tChannelID ChannelID, int CamSlotNumber);
  void ClrChecked(tChannelID ChannelID, int CamSlotNumber);
  void ClrDecrypt(tChannelID ChannelID, int CamSlotNumber);
  void Load(const char *FileName);
  void Save(void);
  };

extern cChannelCamRelations ChannelCamRelations;

bool CamResponsesLoad(const char *FileName, bool AllowComments = false, bool MustExist = false);

#endif //__CI_H
