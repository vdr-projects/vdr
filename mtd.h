/*
 * mtd.h: Multi Transponder Decryption
 *
 * See the main source file 'vdr.c' for copyright information and
 * how to reach the author.
 *
 * $Id: mtd.h 5.1 2025/03/02 11:03:35 kls Exp $
 */

#ifndef __MTD_H
#define __MTD_H

/*
Multiple Transponder Decryption (MTD) is the method of sending TS packets
from channels on different transponders to one single CAM for decryption.
While decrypting several channels from the same transponder ("Multi Channel
Decryption") is straightforward, because the PIDs are unique within one
transponder, channels on different transponders might use the same PIDs
for different streams.

Here's a summary of how MTD is implemented in VDR:

Identifying the relevant source code
------------------------------------

The actual code that implements the MTD handling is located in the files
mtd.h and mtd.c. There are also a few places in ci.[hc], device.c and
menu.c where things need to be handled differently for MTD. All functions
and variables that have to do with MTD have the three letters "mtd" (upper-
and/or lowercase) in their name, so that these code lines can be easily
identified if necessary.

What a plugin implementing a cCiAdapter/cCamSlot needs to do
------------------------------------------------------------

If an implementation of cCiAdapter/cCamSlot supports MTD, it needs to
fulfill the following requirements:
- The cCiAdapter's Assign() function needs to return true for any given
  device.
- The cCamSlot's constructor needs to call MtdEnable().
- The cCamSlot's Decrypt() function shall accept the given TS packet,
  but shall *not* return a decrypted packet. Decypted packets shall be
  delivered through a call to MtdPutData(), one at a time.
- The cCamSlot's Decrypt() function needs to be thread safe, because
  it will be called from several cMtdCamSlot objects.

Physical vs. virtual CAMs
-------------------------

MTD is done by having one physical CAM (accessed through a plugin's
implementation of cCiAdapter/cCamSlot) and several "virtual" CAMs,
implemented through cMtdCamSlot objects ("MTD CAMs"). For each device
that requires the physical CAM, one instance of a cMtdCamSlot is created
on the fly at runtime, and that MTD CAM is assigned to the device.
The MTD CAM takes care of mapping the PIDs, and a cMtdHandler in the
physical CAM object distributes the decrypted TS packets to the proper
devices.

Mapping the PIDs
----------------

The main problem with MTD is that the TS packets from different devices
(and thus different transponders with possibly overlapping PIDs) need to
be combined into one stream, sent to the physical CAM, and finally be sorted
apart again and returned to the devices they came from. Both aspects are
solved in VDR by mapping the "real" PIDs into "unique" PIDs. Real PIDs
are in the range 0x0000-0x1FFF (13 bit). Unique PIDs use the upper 5 bit
to indicate the number of the MTD CAM a TS packet came from, and the lower
8 bit as individual PID values. Mapping is done with a single array lookup
and is thus very fast. The cMtdHandler class takes care of distributing
the TS packets to the individual cMtdCamSlot objects, while mapping the
PIDs (in both directions) is done by the cMtdMapper class.

Mapping the SIDs
----------------

Besides the PIDs there are also the "service ids" (SIDs, a.k.a. "programme
numbers" or PNRs) that need to be taken care of. SIDs only appear in the
CA-PMTs sent to the CAM, so they only need to be mapped from real to unique
(not the other way) and since the are only mapped when switching channels,
mapping doesn't need to be very fast. Mapping SIDs is also done by the
cMtdMapper class.

Handling the CAT
----------------

Each transponder carries a CAT ("Conditional Access Table") with the fixed PID 1.
The CAT contains a list of EMM PIDs, which are necessary to convey entitlement
messages to the smart card. Since the CAM only recognizes the CAT if it has
its fixed PID of 1, this PID cannot be mapped and has to be sent to the CAM
as is. However, the cCaPidReceiver also needs to see the CAM in order to
request from the device the TS packets with the EMM PIDs. Since any receivers
only get the TS packets after they have been sent through the CAM, we need
to send the CAT in both ways, with mapped PID but unmapped EMM PIDs for the
cCaPidReceiver, and with unmapped PID but mapped EMM PIDs for the CAM itself.
Since the PID 0x0001 can always be distinguished from any mapped PID (which
always have a non-zero upper byte), the CAT can be easily channeled in both
ways.

Handling the CA-PMTs
--------------------

The CA-PMTs that are sent to the CAM contain both SIDs and PIDs, which are
mapped in cCiCaPmt::MtdMapPids().
*/

#include "ci.h"
#include "remux.h"
#include "ringbuffer.h"

class cMtdHandler {
private:
  cVector<cMtdCamSlot *> camSlots;
public:
  cMtdHandler(void);
      ///< Creates a new MTD handler that distributes TS data received through
      ///< calls to the Put() function to the individual CAM slots that have been
      ///< created via GetMtdCamSlot(). It also distributes several function
      ///< calls from the physical master CAM slot to the individual MTD CAM slots.
  ~cMtdHandler();
  cMtdCamSlot *GetMtdCamSlot(cCamSlot *MasterSlot);
      ///< Creates a new MTD CAM slot, or reuses an existing one that is currently
      ///< unused.
  int Put(const uchar *Data, int Count);
      ///< Puts at most Count bytes of Data into the CAM slot which's index is
      ///< derived from the PID of the TS packets.
      ///< Data must point to the beginning of a TS packet.
      ///< Returns the number of bytes actually stored.
  int Priority(void);
      ///< Returns the maximum priority of any of the active MTD CAM slots.
  bool IsDecrypting(void);
      ///< Returns true if any of the active MTD CAM slots is currently decrypting.
  void StartDecrypting(void);
      ///< Tells all active MTD CAM slots to start decrypting.
  void StopDecrypting(void);
      ///< Tells all active MTD CAM slots to stop decrypting.
  void CancelActivation(void);
      ///< Tells all active MTD CAM slots to cancel activation.
  bool IsActivating(void);
      ///< Returns true if any of the active MTD CAM slots is currently activating.
  bool Devices(cVector<int> &DeviceNumbers);
      ///< Adds the numbers of the devices of any active MTD CAM slots to
      ///< the given DeviceNumbers.
      ///< Returns true if the array is not empty.
  void UnAssignAll(void);
      ///< Unassigns all MTD CAM slots from their devices.
  };

#define MTD_DONT_CALL(v) dsyslog("PROGRAMMING ERROR (%s,%d): DON'T CALL %s", __FILE__, __LINE__, __FUNCTION__); return v;

class cMtdMapper;

void MtdMapSid(uchar *p, cMtdMapper *MtdMapper);
void MtdMapPid(uchar *p, cMtdMapper *MtdMapper);

class cMtdCamSlot : public cCamSlot {
private:
  cMutex clearMutex;
  cMtdMapper *mtdMapper;
  cRingBufferLinear *mtdBuffer;
  bool delivered;
protected:
  virtual const int *GetCaSystemIds(void) override;
  virtual void SendCaPmt(uint8_t CmdId) override;
public:
  cMtdCamSlot(cCamSlot *MasterSlot, int Index);
       ///< Creates a new "Multi Transponder Decryption" CAM slot, connected to the
       ///< given physical MasterSlot, using the given Index for mapping PIDs.
  virtual ~cMtdCamSlot() override;
  cMtdMapper *MtdMapper(void) { return mtdMapper; }
  virtual bool RepliesToQuery(void) override;
  virtual bool ProvidesCa(const int *CaSystemIds) override;
  virtual bool CanDecrypt(const cChannel *Channel, cMtdMapper *MtdMapper = NULL) override;
  virtual void StartDecrypting(void) override;
  virtual void StopDecrypting(void) override;
  virtual uchar *Decrypt(uchar *Data, int &Count) override;
  virtual bool TsPostProcess(uchar *Data) override;
  virtual void InjectEit(int Sid) override;
  int PutData(const uchar *Data, int Count);
  int PutCat(const uchar *Data, int Count);
  // The following functions shall not be called for a cMtdCamSlot:
  virtual cCamSlot *Spawn(void) { MTD_DONT_CALL(NULL); }
  virtual bool Reset(void) override { MTD_DONT_CALL(false); }
  virtual eModuleStatus ModuleStatus(void) override { MTD_DONT_CALL(msNone); }
  virtual const char *GetCamName(void) override { MTD_DONT_CALL(NULL); }
  virtual bool Ready(void) override { MTD_DONT_CALL(false); }
  virtual bool HasMMI(void) override { MTD_DONT_CALL(false); }
  virtual bool HasUserIO(void) override { MTD_DONT_CALL(false); }
  virtual bool EnterMenu(void) override { MTD_DONT_CALL(false); }
  virtual cCiMenu *GetMenu(void) override { MTD_DONT_CALL(NULL); }
  virtual cCiEnquiry *GetEnquiry(void) override { MTD_DONT_CALL(NULL); }
  };

#endif //__MTD_H
