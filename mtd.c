/*
 * mtd.c: Multi Transponder Decryption
 *
 * See the main source file 'vdr.c' for copyright information and
 * how to reach the author.
 *
 * $Id: mtd.c 1.9 2017/03/27 14:26:04 kls Exp $
 */

#include "mtd.h"
#include "receiver.h"

//#define DEBUG_MTD
#ifdef DEBUG_MTD
#define DBGMTD(a...) dsyslog(a)
#else
#define DBGMTD(a...)
#endif

//#define KEEPPIDS // for testing and debugging - USE ONLY IF YOU KNOW WHAT YOU ARE DOING!

#define MAX_REAL_PIDS  MAXPID // real PIDs are 13 bit (0x0000 - 0x1FFF)
#ifdef KEEPPIDS
#define MAX_UNIQ_PIDS  MAXPID
#define UNIQ_PID_MASK  0x1FFF
#else
#define MAX_UNIQ_PIDS  256    // uniq PIDs are 8 bit (0x00 - 0xFF)
#define UNIQ_PID_MASK  0x00FF
#define UNIQ_PID_SHIFT 8
#endif // KEEPPIDS

// --- cMtdHandler -----------------------------------------------------------

cMtdHandler::cMtdHandler(void)
{
}

cMtdHandler::~cMtdHandler()
{
  for (int i = 0; i < camSlots.Size(); i++) {
      dsyslog("CAM %d/%d: deleting MTD CAM slot", camSlots[i]->MasterSlot()->SlotNumber(), i + 1);
      delete camSlots[i];
      }
}

cMtdCamSlot *cMtdHandler::GetMtdCamSlot(cCamSlot *MasterSlot)
{
  for (int i = 0; i < camSlots.Size(); i++) {
      if (!camSlots[i]->Device()) {
         dsyslog("CAM %d/%d: reusing MTD CAM slot", MasterSlot->SlotNumber(), i + 1);
         return camSlots[i];
         }
      }
  dsyslog("CAM %d/%d: creating new MTD CAM slot", MasterSlot->SlotNumber(), camSlots.Size() + 1);
  cMtdCamSlot *s = new cMtdCamSlot(MasterSlot, camSlots.Size());
  camSlots.Append(s);
  return s;
}

int cMtdHandler::Put(const uchar *Data, int Count)
{
  int Used = 0;
  while (Count >= TS_SIZE) {
        if (int Skipped = TS_SYNC(Data, Count))
           return Used + Skipped;
        int Pid = TsPid(Data);
        if (Pid != CATPID) { // the original CAT with mapped PIDs must be skipped here!
#ifdef KEEPPIDS
           int Index = 0;
#else
           int Index = (Pid >> UNIQ_PID_SHIFT) - 1;
#endif // KEEPPIDS
           if (Index >= 0 && Index < camSlots.Size()) {
              int w = camSlots[Index]->PutData(Data, TS_SIZE);
              if (w == 0)
                 break;
              else if (w != TS_SIZE)
                 esyslog("ERROR: incomplete MTD packet written (%d) in PID %d (%04X)", Index + 1, Pid, Pid);
              }
           else if (Index >= 0) // we silently ignore Index -1 (i.e. MTD number 0), since there are several hundred empty TS packets when switching to an encrypted channel for the first time since startup
              esyslog("ERROR: invalid MTD number (%d) in PID %d (%04X)", Index + 1, Pid, Pid);
           }
        Data += TS_SIZE;
        Count -= TS_SIZE;
        Used += TS_SIZE;
        }
  return Used;
}

int cMtdHandler::Priority(void)
{
  int p = IDLEPRIORITY;
  for (int i = 0; i < camSlots.Size(); i++)
      p = max(p, camSlots[i]->Priority());
  return p;
}

bool cMtdHandler::IsDecrypting(void)
{
  for (int i = 0; i < camSlots.Size(); i++) {
      if (camSlots[i]->IsDecrypting())
         return true;
      }
  return false;
}

void cMtdHandler::StartDecrypting(void)
{
  for (int i = 0; i < camSlots.Size(); i++) {
      if (camSlots[i]->Device()) {
         camSlots[i]->TriggerResendPmt();
         camSlots[i]->StartDecrypting();
         }
      }
}

void cMtdHandler::CancelActivation(void)
{
  for (int i = 0; i < camSlots.Size(); i++)
      camSlots[i]->CancelActivation();
}

bool cMtdHandler::IsActivating(void)
{
  for (int i = 0; i < camSlots.Size(); i++) {
      if (camSlots[i]->IsActivating())
         return true;
      }
  return false;
}

bool cMtdHandler::Devices(cVector<int> &CardIndexes)
{
  for (int i = 0; i < camSlots.Size(); i++)
      camSlots[i]->Devices(CardIndexes);
  return CardIndexes.Size() > 0;
}

// --- cMtdMapper ------------------------------------------------------------

#define MTD_INVALID_PID 0xFFFF

class cMtdMapper {
private:
  int number;
  int masterCamSlotNumber;
  uint16_t uniqPids[MAX_REAL_PIDS]; // maps a real PID to a unique PID
  uint16_t realPids[MAX_UNIQ_PIDS]; // maps a unique PID to a real PID
  cVector<uint16_t> uniqSids;
  uint16_t MakeUniqPid(uint16_t RealPid);
public:
  cMtdMapper(int Number, int MasterCamSlotNumber);
  ~cMtdMapper();
  uint16_t RealToUniqPid(uint16_t RealPid) { if (uniqPids[RealPid]) return uniqPids[RealPid]; return MakeUniqPid(RealPid); }
  uint16_t UniqToRealPid(uint16_t UniqPid) { return realPids[UniqPid & UNIQ_PID_MASK]; }
  uint16_t RealToUniqSid(uint16_t RealSid);
  void Clear(void);
  };

cMtdMapper::cMtdMapper(int Number, int MasterCamSlotNumber)
{
  number = Number;
  masterCamSlotNumber = MasterCamSlotNumber;
  Clear();
}

cMtdMapper::~cMtdMapper()
{
}

uint16_t cMtdMapper::MakeUniqPid(uint16_t RealPid)
{
#ifdef KEEPPIDS
  uniqPids[RealPid] = realPids[RealPid] = RealPid;
  DBGMTD("CAM %d/%d: mapped PID %d (%04X) to %d (%04X)", masterCamSlotNumber, number, RealPid, RealPid, uniqPids[RealPid], uniqPids[RealPid]);
  return uniqPids[RealPid];
#else
  for (int i = 0; i < MAX_UNIQ_PIDS; i++) {
      if (realPids[i] == MTD_INVALID_PID) { // 0x0000 is a valid PID (PAT)!
         realPids[i] = RealPid;
         uniqPids[RealPid] = (number << UNIQ_PID_SHIFT) | i;
         DBGMTD("CAM %d/%d: mapped PID %d (%04X) to %d (%04X)", masterCamSlotNumber, number, RealPid, RealPid, uniqPids[RealPid], uniqPids[RealPid]);
         return uniqPids[RealPid];
         }
      }
#endif // KEEPPIDS
  esyslog("ERROR: MTD %d: mapper ran out of unique PIDs", number);
  return 0;
}

uint16_t cMtdMapper::RealToUniqSid(uint16_t RealSid)
{
#ifdef KEEPPIDS
  return RealSid;
#endif // KEEPPIDS
  int UniqSid = uniqSids.IndexOf(RealSid);
  if (UniqSid < 0) {
     UniqSid = uniqSids.Size();
     uniqSids.Append(RealSid);
     DBGMTD("CAM %d/%d: mapped SID %d (%04X) to %d (%04X)", masterCamSlotNumber, number, RealSid, RealSid, UniqSid | (number << UNIQ_PID_SHIFT), UniqSid | (number << UNIQ_PID_SHIFT));
     }
  UniqSid |= number << UNIQ_PID_SHIFT;
  return UniqSid;
}

void cMtdMapper::Clear(void)
{
  DBGMTD("CAM %d/%d: MTD mapper cleared", masterCamSlotNumber, number);
  memset(uniqPids, 0, sizeof(uniqPids));
  memset(realPids, MTD_INVALID_PID, sizeof(realPids));
  uniqSids.Clear();
}

void MtdMapSid(uchar *p, cMtdMapper *MtdMapper)
{
  Poke13(p, MtdMapper->RealToUniqSid(Peek13(p)));
}

void MtdMapPid(uchar *p, cMtdMapper *MtdMapper)
{
  Poke13(p, MtdMapper->RealToUniqPid(Peek13(p)));
}

// --- cMtdCamSlot -----------------------------------------------------------

#define MTD_BUFFER_SIZE MEGABYTE(1)

cMtdCamSlot::cMtdCamSlot(cCamSlot *MasterSlot, int Index)
:cCamSlot(NULL, true, MasterSlot)
{
  mtdBuffer = new cRingBufferLinear(MTD_BUFFER_SIZE, TS_SIZE, true, "MTD buffer");
  mtdMapper = new cMtdMapper(Index + 1, MasterSlot->SlotNumber());
  delivered = false;
  ciAdapter = MasterSlot->ciAdapter; // we don't pass the CI adapter in the constructor, to prevent this one from being inserted into CamSlots
}

cMtdCamSlot::~cMtdCamSlot()
{
  Assign(NULL);
  delete mtdMapper;
  delete mtdBuffer;
}

const int *cMtdCamSlot::GetCaSystemIds(void)
{
  return MasterSlot()->GetCaSystemIds();
}

void cMtdCamSlot::SendCaPmt(uint8_t CmdId)
{
  cMutexLock MutexLock(&mutex);
  cCiCaPmtList CaPmtList;
  BuildCaPmts(CmdId, CaPmtList, mtdMapper);
  MasterSlot()->SendCaPmts(CaPmtList);
}

bool cMtdCamSlot::RepliesToQuery(void)
{
  return MasterSlot()->RepliesToQuery();
}

bool cMtdCamSlot::ProvidesCa(const int *CaSystemIds)
{
  return MasterSlot()->ProvidesCa(CaSystemIds);
}

bool cMtdCamSlot::CanDecrypt(const cChannel *Channel, cMtdMapper *MtdMapper)
{
  return MasterSlot()->CanDecrypt(Channel, mtdMapper);
}

void cMtdCamSlot::StartDecrypting(void)
{
  MasterSlot()->StartDecrypting();
  cCamSlot::StartDecrypting();
}

void cMtdCamSlot::StopDecrypting(void)
{
  cCamSlot::StopDecrypting();
  if (!MasterSlot()->IsDecrypting())
     MasterSlot()->StopDecrypting();
  cMutexLock MutexLock(&clearMutex);
  mtdMapper->Clear();
  mtdBuffer->Clear();
  delivered = false;
}

uchar *cMtdCamSlot::Decrypt(uchar *Data, int &Count)
{
  // Send data to CAM:
  if (Count >= TS_SIZE) {
     Count = TS_SIZE;
     int Pid = TsPid(Data);
     TsSetPid(Data, mtdMapper->RealToUniqPid(Pid));
     MasterSlot()->Decrypt(Data, Count);
     if (Count == 0)
        TsSetPid(Data, Pid); // must restore PID for later retry
     }
  else
     Count = 0;
  // Drop delivered data from previous call:
  cMutexLock MutexLock(&clearMutex);
  if (delivered) {
     mtdBuffer->Del(TS_SIZE);
     delivered = false;
     }
  // Receive data from buffer:
  int c = 0;
  uchar *d = mtdBuffer->Get(c);
  if (d) {
     if (int Skipped = TS_SYNC(d, c)) {
        mtdBuffer->Del(Skipped);
        return NULL;
        }
     if (c >= TS_SIZE) {
        TsSetPid(d, mtdMapper->UniqToRealPid(TsPid(d)));
        delivered = true;
        }
     else
        d = NULL;
     }
  return d;
}

int cMtdCamSlot::PutData(const uchar *Data, int Count)
{
  int Free = mtdBuffer->Free();
  Free -= Free % TS_SIZE;
  if (Free < TS_SIZE)
     return 0;
  if (Free < Count)
     Count = Free;
  return mtdBuffer->Put(Data, Count);
}

int cMtdCamSlot::PutCat(const uchar *Data, int Count)
{
  MasterSlot()->Decrypt(const_cast<uchar *>(Data), Count);
  return Count;
}
