/*
 * ci.h: Common Interface
 *
 * See the main source file 'vdr.c' for copyright information and
 * how to reach the author.
 *
 * $Id: ci.h 4.0 2015/01/31 14:36:41 kls Exp $
 */

#ifndef __CI_H
#define __CI_H

#include <stdint.h>
#include <stdio.h>
#include "channels.h"
#include "thread.h"
#include "tools.h"

#define MAX_CAM_SLOTS_PER_ADAPTER     8 // maximum possible value is 255
#define MAX_CONNECTIONS_PER_CAM_SLOT  8 // maximum possible value is 254
#define CAM_READ_TIMEOUT  50 // ms

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
class cCamSlot;

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
  virtual void Action(void);
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
  virtual ~cCiAdapter();
       ///< The derived class must call Cancel(3) in its destructor.
  };

class cTPDU;
class cCiTransportConnection;
class cCiSession;
class cCiCaProgramData;
class cCaPidReceiver;
class cCaActivationReceiver;

class cCamSlot : public cListObject {
  friend class cCiAdapter;
  friend class cCiTransportConnection;
private:
  cMutex mutex;
  cCondVar processed;
  cCiAdapter *ciAdapter;
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
  const int *GetCaSystemIds(void);
  void SendCaPmt(uint8_t CmdId);
  void NewConnection(void);
  void DeleteAllConnections(void);
  void Process(cTPDU *TPDU = NULL);
  void Write(cTPDU *TPDU);
  cCiSession *GetSessionByResourceId(uint32_t ResourceId);
public:
  cCamSlot(cCiAdapter *CiAdapter, bool WantsTsData = false);
       ///< Creates a new CAM slot for the given CiAdapter.
       ///< The CiAdapter will take care of deleting the CAM slot,
       ///< so the caller must not delete it!
       ///< If WantsTsData is true, the device this CAM slot is assigned to will
       ///< call the Decrypt() function of this CAM slot, presenting it the complete
       ///< TS data stream of the encrypted programme, including the CA pids.
  virtual ~cCamSlot();
  bool Assign(cDevice *Device, bool Query = false);
       ///< Assigns this CAM slot to the given Device, if this is possible.
       ///< If Query is 'true', the CI adapter of this slot only checks whether
       ///< it can be assigned to the Device, but doesn't actually assign itself to it.
       ///< Returns true if this slot can be assigned to the Device.
       ///< If Device is NULL, the slot will be unassigned from any
       ///< device it was previously assigned to. The value of Query
       ///< is ignored in that case, and this function always returns
       ///< 'true'.
  cDevice *Device(void) { return assignedDevice; }
       ///< Returns the device this CAM slot is currently assigned to.
  bool WantsTsData(void) const { return caPidReceiver != NULL; }
       ///< Returns true if this CAM slot wants to receive the TS data through
       ///< its Decrypt() function.
  int SlotIndex(void) { return slotIndex; }
       ///< Returns the index of this CAM slot within its CI adapter.
       ///< The first slot has an index of 0.
  int SlotNumber(void) { return slotNumber; }
       ///< Returns the number of this CAM slot within the whole system.
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
       ///< Adds all PIDs if the given Channel to the current list of PIDs.
       ///< If the source or transponder of the channel are different than
       ///< what was given in a previous call to AddChannel(), any previously
       ///< added PIDs will be cleared.
  virtual bool CanDecrypt(const cChannel *Channel);
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
  virtual void StartDecrypting(void);
       ///< Triggers sending all currently active CA_PMT entries to the CAM,
       ///< so that it will start decrypting.
  virtual void StopDecrypting(void);
       ///< Clears the list of CA_PMT entries and tells the CAM to stop decrypting.
  virtual bool IsDecrypting(void);
       ///< Returns true if the CAM in this slot is currently used for decrypting.
  virtual uchar *Decrypt(uchar *Data, int &Count);
       ///< If this is a CAM slot that can be freely assigned to any device,
       ///< but will not be directly inserted into the full TS data stream
       ///< in hardware, it can implement this function to be given access
       ///< to the data in the device's TS buffer. Data points to a buffer
       ///< of Count bytes of TS data. The first byte in Data is guaranteed
       ///< to be a TS_SYNC_BYTE.
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
       ///< call to Decrypt().
       ///< A derived class that implements this function will also need
       ///< to set the WantsTsData parameter in the call to the base class
       ///< constructor to true in order to receive the TS data.
  };

class cCamSlots : public cList<cCamSlot> {
public:
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
  };

extern cChannelCamRelations ChannelCamRelations;

#endif //__CI_H
