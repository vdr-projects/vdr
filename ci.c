/*
 * ci.c: Common Interface
 *
 * See the main source file 'vdr.c' for copyright information and
 * how to reach the author.
 *
 * $Id: ci.c 4.32 2020/08/17 10:26:18 kls Exp $
 */

#include "ci.h"
#include <ctype.h>
#include <linux/dvb/ca.h>
#include <malloc.h>
#include <netinet/in.h>
#include <poll.h>
#include <stdio.h>
#include <string.h>
#include <sys/ioctl.h>
#include <time.h>
#include <unistd.h>
#include "device.h"
#include "mtd.h"
#include "pat.h"
#include "receiver.h"
#include "remux.h"
#include "libsi/si.h"
#include "skins.h"
#include "tools.h"

// Set these to 'true' for debug output:
static bool DumpTPDUDataTransfer = false;
static bool DebugProtocol = false;
static bool DumpPolls = false;
static bool DumpDateTime = false;

#define dbgprotocol(a...) if (DebugProtocol) fprintf(stderr, a)

// --- Helper functions ------------------------------------------------------

#define SIZE_INDICATOR 0x80

static const uint8_t *GetLength(const uint8_t *Data, int &Length)
///< Gets the length field from the beginning of Data.
///< Returns a pointer to the first byte after the length and
///< stores the length value in Length.
{
  Length = *Data++;
  if ((Length & SIZE_INDICATOR) != 0) {
     int l = Length & ~SIZE_INDICATOR;
     Length = 0;
     for (int i = 0; i < l; i++)
         Length = (Length << 8) | *Data++;
     }
  return Data;
}

static uint8_t *SetLength(uint8_t *Data, int Length)
///< Sets the length field at the beginning of Data.
///< Returns a pointer to the first byte after the length.
{
  uint8_t *p = Data;
  if (Length < 128)
     *p++ = Length;
  else {
     int n = sizeof(Length);
     for (int i = n - 1; i >= 0; i--) {
         int b = (Length >> (8 * i)) & 0xFF;
         if (p != Data || b)
            *++p = b;
         }
     *Data = (p - Data) | SIZE_INDICATOR;
     p++;
     }
  return p;
}

static char *CopyString(int Length, const uint8_t *Data)
///< Copies the string at Data.
///< Returns a pointer to a newly allocated string.
{
  char *s = MALLOC(char, Length + 1);
  char *p = s;
  while (Length > 0) {
        int c = *Data;
        if (isprint(c)) // some CAMs send funny characters in their strings, let's just skip them
           *p++ = c;
        else if (c == 0x8A) // the character 0x8A is used as newline, so let's put a real '\n' in there
           *p++ = '\n';
        Length--;
        Data++;
        }
  *p = 0;
  return s;
}

static char *GetString(int &Length, const uint8_t **Data)
///< Gets the string at Data.
///< Returns a pointer to a newly allocated string, or NULL in case of error.
///< Upon return Length and Data represent the remaining data after the string has been skipped.
{
  if (Length > 0 && Data && *Data) {
     int l = 0;
     const uint8_t *d = GetLength(*Data, l);
     char *s = CopyString(l, d);
     Length -= d - *Data + l;
     *Data = d + l;
     return s;
     }
  return NULL;
}

// --- cCaPidReceiver --------------------------------------------------------

// A receiver that is used to make the device receive the ECM pids, as well as the
// CAT and the EMM pids.

class cCaPidReceiver : public cReceiver {
private:
  int catVersion;
  cVector<int> emmPids;
  uchar buffer[1024]; // CAT table length: 10 bit -> max. 1021 + 3 bytes
  uchar *bufp;
  #define CAT_MAXPACKETS  6 // 6 * 184 = 1104 bytes for CAT table
  uchar mtdCatBuffer[CAT_MAXPACKETS][TS_SIZE]; // TODO: handle multi table CATs!
  int mtdNumCatPackets;
  int length;
  cMutex mutex;
  bool handlingPid;
  void AddEmmPid(int Pid);
  void DelEmmPids(void);
public:
  cCaPidReceiver(void);
  virtual ~cCaPidReceiver() { Detach(); }
  virtual void Receive(const uchar *Data, int Length);
  bool HasCaPids(void) const { return NumPids() - emmPids.Size() - 1 > 0; }
  void Reset(void) { DelEmmPids(); catVersion = -1; }
  bool HandlingPid(void);
       ///< The cCaPidReceiver adds/deletes PIDs to/from the base class cReceiver,
       ///< which in turn does the same on the cDevice it is attached to. The cDevice
       ///< then sets the PIDs on the assigned cCamSlot, which can cause a deadlock on the
       ///< cCamSlot's mutex if a cReceiver is detached from the device at the same time.
       ///< Since these PIDs, however, are none that have to be decrypted,
       ///< it is not necessary to set them in the CAM. Therefore this function is
       ///< used in cCamSlot::SetPid() to detect this situation, and thus avoid the
       ///< deadlock.
  };

cCaPidReceiver::cCaPidReceiver(void)
{
  catVersion = -1;
  bufp = NULL;
  mtdNumCatPackets = 0;
  length = 0;
  handlingPid = false;
  cMutexLock MutexLock(&mutex);
  handlingPid = true;
  AddPid(CATPID);
  handlingPid = false;
}

void cCaPidReceiver::AddEmmPid(int Pid)
{
  for (int i = 0; i < emmPids.Size(); i++) {
      if (emmPids[i] == Pid)
         return;
      }
  emmPids.Append(Pid);
  cMutexLock MutexLock(&mutex);
  handlingPid = true;
  AddPid(Pid);
  handlingPid = false;
}

void cCaPidReceiver::DelEmmPids(void)
{
  cMutexLock MutexLock(&mutex);
  handlingPid = true;
  for (int i = 0; i < emmPids.Size(); i++)
      DelPid(emmPids[i]);
  emmPids.Clear();
  handlingPid = false;
}

void cCaPidReceiver::Receive(const uchar *Data, int Length)
{
  if (TsPid(Data) == CATPID) {
     cMtdCamSlot *MtdCamSlot = dynamic_cast<cMtdCamSlot *>(Device()->CamSlot());
     const uchar *p = NULL;
     if (TsPayloadStart(Data)) {
        if (Data[5] == SI::TableIdCAT) {
           length = (int(Data[6] & 0x0F) << 8) | Data[7]; // section length (12 bit field)
           if (length > 5) {
              int v = (Data[10] & 0x3E) >> 1; // version number
              if (v != catVersion) {
                 if (Data[11] == 0 && Data[12] == 0) { // section number, last section number
                    length += 3; // with TableIdCAT -> Data[5]
                    if (length > TS_SIZE - 5) {
                       int n = TS_SIZE - 5;
                       memcpy(buffer, Data + 5, n);
                       bufp = buffer + n;
                       length -= n;
                       }
                    else {
                       p = Data + 5; // no need to copy the data
                       }
                    if (MtdCamSlot) {
                       mtdNumCatPackets = 0;
                       memcpy(mtdCatBuffer[mtdNumCatPackets++], Data, TS_SIZE);
                       }
                    }
                 else
                    dsyslog("multi table CAT section - unhandled!");
                 catVersion = v;
                 }
              else if (MtdCamSlot) {
                 for (int i = 0; i < mtdNumCatPackets; i++)
                     MtdCamSlot->PutCat(mtdCatBuffer[i], TS_SIZE);
                 }
              }
           }
        }
     else if (bufp && length > 0) {
        int n = min(length, TS_SIZE - 4);
        if (bufp + n - buffer <= int(sizeof(buffer))) {
           memcpy(bufp, Data + 4, n);
           bufp += n;
           length -= n;
           if (length <= 0) {
              p = buffer;
              length = bufp - buffer;
              }
           if (MtdCamSlot)
              memcpy(mtdCatBuffer[mtdNumCatPackets++], Data, TS_SIZE);
           }
        else {
           esyslog("ERROR: buffer overflow in cCaPidReceiver::Receive()");
           bufp = NULL;
           length = 0;
           }
        }
     if (p) {
        if (!SI::CRC32::crc32((const char *)p, length, 0xFFFFFFFF)) { // <TableIdCAT,....,crc32>
           DelEmmPids();
           for (int i = 8; i < length - 4; i++) { // -4 = checksum
               if (p[i] == 0x09) {
                  int CaId = int(p[i + 2] << 8) | p[i + 3];
                  int EmmPid = Peek13(p + i + 4);
                  AddEmmPid(EmmPid);
                  if (MtdCamSlot)
                     MtdMapPid(const_cast<uchar *>(p + i + 4), MtdCamSlot->MtdMapper());
                  switch (CaId >> 8) {
                    case 0x01: for (int j = i + 7; j < i + p[i + 1] + 2; j += 4) {
                                   EmmPid = Peek13(p + j);
                                   AddEmmPid(EmmPid);
                                   if (MtdCamSlot)
                                      MtdMapPid(const_cast<uchar *>(p + j), MtdCamSlot->MtdMapper());
                                   }
                               break;
                    }
                  i += p[i + 1] + 2 - 1; // -1 to compensate for the loop increment
                  }
               }
           if (MtdCamSlot) {
              // update crc32
              uint32_t crc = SI::CRC32::crc32((const char *)p, length - 4, 0xFFFFFFFF); // <TableIdCAT....>[crc32]
              uchar *c = const_cast<uchar *>(p + length - 4);
              *c++ = crc >> 24;
              *c++ = crc >> 16;
              *c++ = crc >> 8;
              *c++ = crc;
              // modify CAT packets
              const uchar *t = p;
              for (int i = 0, j = 5; i < mtdNumCatPackets; i++, j = 4) {
                  int n = min(length, TS_SIZE - j);
                  memcpy(mtdCatBuffer[i] + j, t, n);
                  t += n;
                  length -= n;
                  MtdCamSlot->PutCat(mtdCatBuffer[i], TS_SIZE);
                  }
              }
           }
        else {
           esyslog("ERROR: wrong checksum in CAT");
           catVersion = -1;
           }
        p = NULL;
        bufp = NULL;
        length = 0;
        }
     }
}

bool cCaPidReceiver::HandlingPid(void)
{
  cMutexLock MutexLock(&mutex);
  return handlingPid;
}

// --- cCaActivationReceiver -------------------------------------------------

// A receiver that is used to make the device stay on a given channel and
// keep the CAM slot assigned.

#define UNSCRAMBLE_TIME     5 // seconds of receiving purely unscrambled data before considering the smart card "activated"
#define TS_PACKET_FACTOR 1024 // only process every TS_PACKET_FACTORth packet to keep the load down

class cCaActivationReceiver : public cReceiver {
private:
  cCamSlot *camSlot;
  time_t lastScrambledTime;
  int numTsPackets;
protected:
  virtual void Receive(const uchar *Data, int Length);
public:
  cCaActivationReceiver(const cChannel *Channel, cCamSlot *CamSlot);
  virtual ~cCaActivationReceiver();
  };

cCaActivationReceiver::cCaActivationReceiver(const cChannel *Channel, cCamSlot *CamSlot)
:cReceiver(Channel, MINPRIORITY + 1)
{
  camSlot = CamSlot;
  lastScrambledTime = time(NULL);
  numTsPackets = 0;
}

cCaActivationReceiver::~cCaActivationReceiver()
{
  Detach();
}

void cCaActivationReceiver::Receive(const uchar *Data, int Length)
{
  if (numTsPackets++ % TS_PACKET_FACTOR == 0) {
     time_t Now = time(NULL);
     if (TsIsScrambled(Data))
        lastScrambledTime = Now;
     else if (Now - lastScrambledTime > UNSCRAMBLE_TIME) {
        dsyslog("CAM %d: activated!", camSlot->MasterSlotNumber());
        Skins.QueueMessage(mtInfo, tr("CAM activated!"));
        cDevice *d = Device();
        Detach();
        if (d) {
           if (cCamSlot *s = d->CamSlot())
              s->CancelActivation(); // this will delete *this* object, so no more code referencing *this* after this call!
           }
        }
     }
}

// --- cCamResponse ----------------------------------------------------------

// CAM Response Actions:

#define CRA_NONE      0
#define CRA_DISCARD  -1
#define CRA_CONFIRM  -2
#define CRA_SELECT   -3

class cCamResponse : public cListObject {
private:
  int camNumber;
  char *text;
  int action;
public:
  cCamResponse(void);
  ~cCamResponse();
  bool Parse(const char *s);
  int Matches(int CamNumber, const char *Text) const;
  };

cCamResponse::cCamResponse(void)
{
  camNumber = -1;
  text = NULL;
  action = CRA_NONE;
}

cCamResponse::~cCamResponse()
{
  free(text);
}

bool cCamResponse::Parse(const char *s)
{
  // Number:
  s = skipspace(s);
  if (*s == '*') {
     camNumber = 0; // all CAMs
     s++;
     }
  else {
     char *e;
     camNumber = strtol(s, &e, 10);
     if (e == s || camNumber <= 0)
        return false;
     s = e;
     }
  // Text:
  s = skipspace(s);
  char *t = const_cast<char *>(s); // might have to modify it
  char *q = NULL; // holds a copy in case of backslashes
  bool InQuotes = false;
  while (*t) {
        if (*t == '"') {
           if (t == s) { // opening quotes
              InQuotes = true;
              s++;
              }
           else if (InQuotes) // closing quotes
              break;
           }
        else if (*t == '\\') {
           if (!q) { // need to make a copy in order to strip backslashes
              q = strdup(s);
              t = q + (t - s);
              s = q;
              }
           memmove(t, t + 1, strlen(t));
           }
        else if (*t == ' ') {
           if (!InQuotes)
              break;
           }
        t++;
        }
  free(text); // just for safety
  text = NULL;
  if (t != s) {
     text = strndup(s, t - s);
     s = t + 1;
     }
  free(q);
  if (!text)
     return false;
  // Action:
  s = skipspace(s);
  if      (strcasecmp(s, "DISCARD") == 0) action = CRA_DISCARD;
  else if (strcasecmp(s, "CONFIRM") == 0) action = CRA_CONFIRM;
  else if (strcasecmp(s, "SELECT")  == 0) action = CRA_SELECT;
  else if (isnumber(s))                   action = atoi(s);
  else
     return false;
  return true;
}

int cCamResponse::Matches(int CamNumber, const char *Text) const
{
  if (!camNumber || camNumber == CamNumber) {
     if (strcmp(text, Text) == 0)
        return action;
     }
  return CRA_NONE;
}

// --- cCamResponses  --------------------------------------------------------

class cCamResponses : public cConfig<cCamResponse> {
public:
  int GetMatch(int CamNumber, const char *Text) const;
  };

int cCamResponses::GetMatch(int CamNumber, const char *Text) const
{
  for (const cCamResponse *cr = First(); cr; cr = Next(cr)) {
      int Action = cr->Matches(CamNumber, Text);
      if (Action != CRA_NONE) {
         dsyslog("CAM %d: auto response %4d to '%s'\n", CamNumber, Action, Text);
         return Action;
         }
      }
  return CRA_NONE;
}

cCamResponses CamResponses;

bool CamResponsesLoad(const char *FileName, bool AllowComments, bool MustExist)
{
  return CamResponses.Load(FileName, AllowComments, MustExist);
}

// --- cTPDU -----------------------------------------------------------------

#define MAX_TPDU_SIZE  4096
#define MAX_TPDU_DATA  (MAX_TPDU_SIZE - 4)

#define DATA_INDICATOR 0x80

#define T_SB           0x80
#define T_RCV          0x81
#define T_CREATE_TC    0x82
#define T_CTC_REPLY    0x83
#define T_DELETE_TC    0x84
#define T_DTC_REPLY    0x85
#define T_REQUEST_TC   0x86
#define T_NEW_TC       0x87
#define T_TC_ERROR     0x88
#define T_DATA_LAST    0xA0
#define T_DATA_MORE    0xA1

class cTPDU {
private:
  int size;
  uint8_t buffer[MAX_TPDU_SIZE];
  const uint8_t *GetData(const uint8_t *Data, int &Length);
public:
  cTPDU(void) { size = 0; }
  cTPDU(uint8_t Slot, uint8_t Tcid, uint8_t Tag, int Length = 0, const uint8_t *Data = NULL);
  uint8_t Slot(void) { return buffer[0]; }
  uint8_t Tcid(void) { return buffer[1]; }
  uint8_t Tag(void)  { return buffer[2]; }
  const uint8_t *Data(int &Length) { return GetData(buffer + 3, Length); }
  uint8_t Status(void);
  uint8_t *Buffer(void) { return buffer; }
  int Size(void) { return size; }
  void SetSize(int Size) { size = Size; }
  int MaxSize(void) { return sizeof(buffer); }
  void Dump(int SlotNumber, bool Outgoing);
  };

cTPDU::cTPDU(uint8_t Slot, uint8_t Tcid, uint8_t Tag, int Length, const uint8_t *Data)
{
  size = 0;
  buffer[0] = Slot;
  buffer[1] = Tcid;
  buffer[2] = Tag;
  switch (Tag) {
    case T_RCV:
    case T_CREATE_TC:
    case T_CTC_REPLY:
    case T_DELETE_TC:
    case T_DTC_REPLY:
    case T_REQUEST_TC:
         buffer[3] = 1; // length
         buffer[4] = Tcid;
         size = 5;
         break;
    case T_NEW_TC:
    case T_TC_ERROR:
         if (Length == 1) {
            buffer[3] = 2; // length
            buffer[4] = Tcid;
            buffer[5] = Data[0];
            size = 6;
            }
         else
            esyslog("ERROR: invalid data length for TPDU tag 0x%02X: %d (%d/%d)", Tag, Length, Slot, Tcid);
         break;
    case T_DATA_LAST:
    case T_DATA_MORE:
         if (Length <= MAX_TPDU_DATA) {
            uint8_t *p = buffer + 3;
            p = SetLength(p, Length + 1);
            *p++ = Tcid;
            if (Length)
               memcpy(p, Data, Length);
            size = Length + (p - buffer);
            }
         else
            esyslog("ERROR: invalid data length for TPDU tag 0x%02X: %d (%d/%d)", Tag, Length, Slot, Tcid);
         break;
    default:
         esyslog("ERROR: unknown TPDU tag: 0x%02X (%d/%d)", Tag, Slot, Tcid);
    }
 }

void cTPDU::Dump(int SlotNumber, bool Outgoing)
{
  if (DumpTPDUDataTransfer && (DumpPolls || Tag() != T_SB)) {
#define MAX_DUMP 256
     fprintf(stderr, "     %d: %s ", SlotNumber, Outgoing ? "-->" : "<--");
     for (int i = 0; i < size && i < MAX_DUMP; i++)
         fprintf(stderr, "%02X ", buffer[i]);
     fprintf(stderr, "%s\n", size >= MAX_DUMP ? "..." : "");
     if (!Outgoing) {
        fprintf(stderr, "           ");
        for (int i = 0; i < size && i < MAX_DUMP; i++)
            fprintf(stderr, "%2c ", isprint(buffer[i]) ? buffer[i] : '.');
        fprintf(stderr, "%s\n", size >= MAX_DUMP ? "..." : "");
        }
     }
}

const uint8_t *cTPDU::GetData(const uint8_t *Data, int &Length)
{
  if (size) {
     Data = GetLength(Data, Length);
     if (Length) {
        Length--; // the first byte is always the tcid
        return Data + 1;
        }
     }
  return NULL;
}

uint8_t cTPDU::Status(void)
{
  if (size >= 4 && buffer[size - 4] == T_SB && buffer[size - 3] == 2)
     return buffer[size - 1];
  return 0;
}

// --- cCiTransportConnection ------------------------------------------------

#define MAX_SESSIONS_PER_TC  16

class cCiTransportConnection {
private:
  enum eState { stIDLE, stCREATION, stACTIVE, stDELETION };
  cMutex mutex;
  cCamSlot *camSlot;
  uint8_t tcid;
  eState state;
  bool createConnectionRequested;
  bool deleteConnectionRequested;
  bool hasUserIO;
  cTimeMs alive;
  cTimeMs timer;
  cCiSession *sessions[MAX_SESSIONS_PER_TC + 1]; // session numbering starts with 1
  cCiSession *tsPostProcessor;
  void SendTPDU(uint8_t Tag, int Length = 0, const uint8_t *Data = NULL);
  void SendTag(uint8_t Tag, uint16_t SessionId, uint32_t ResourceId = 0, int Status = -1);
  void Poll(void);
  uint32_t ResourceIdToInt(const uint8_t *Data);
  cCiSession *GetSessionBySessionId(uint16_t SessionId);
  void OpenSession(int Length, const uint8_t *Data);
  void CloseSession(uint16_t SessionId);
  void HandleSessions(cTPDU *TPDU);
public:
  cCiTransportConnection(cCamSlot *CamSlot, uint8_t Tcid);
  virtual ~cCiTransportConnection();
  void SetTsPostProcessor(cCiSession *CiSession);
  bool TsPostProcess(uint8_t *TsPacket);
  cCamSlot *CamSlot(void) { return camSlot; }
  uint8_t Tcid(void) const { return tcid; }
  void CreateConnection(void) { createConnectionRequested = true; }
  void DeleteConnection(void) { deleteConnectionRequested = true; }
  const char *GetCamName(void);
  bool Ready(void);
  bool HasUserIO(void) { return hasUserIO; }
  void SendData(int Length, const uint8_t *Data);
  bool Process(cTPDU *TPDU = NULL);
  cCiSession *GetSessionByResourceId(uint32_t ResourceId);
  };

// --- cCiSession ------------------------------------------------------------

// Session Tags:

#define ST_SESSION_NUMBER           0x90
#define ST_OPEN_SESSION_REQUEST     0x91
#define ST_OPEN_SESSION_RESPONSE    0x92
#define ST_CREATE_SESSION           0x93
#define ST_CREATE_SESSION_RESPONSE  0x94
#define ST_CLOSE_SESSION_REQUEST    0x95
#define ST_CLOSE_SESSION_RESPONSE   0x96

// Session Status:

#define SS_OK             0x00
#define SS_NOT_ALLOCATED  0xF0

// Resource Identifiers:

#define RI_RESOURCE_MANAGER            0x00010041
#define RI_APPLICATION_INFORMATION     0x00020041
#define RI_CONDITIONAL_ACCESS_SUPPORT  0x00030041
#define RI_HOST_CONTROL                0x00200041
#define RI_DATE_TIME                   0x00240041
#define RI_MMI                         0x00400041

// Application Object Tags:

#define AOT_NONE                    0x000000
#define AOT_PROFILE_ENQ             0x9F8010
#define AOT_PROFILE                 0x9F8011
#define AOT_PROFILE_CHANGE          0x9F8012
#define AOT_APPLICATION_INFO_ENQ    0x9F8020
#define AOT_APPLICATION_INFO        0x9F8021
#define AOT_ENTER_MENU              0x9F8022
#define AOT_CA_INFO_ENQ             0x9F8030
#define AOT_CA_INFO                 0x9F8031
#define AOT_CA_PMT                  0x9F8032
#define AOT_CA_PMT_REPLY            0x9F8033
#define AOT_TUNE                    0x9F8400
#define AOT_REPLACE                 0x9F8401
#define AOT_CLEAR_REPLACE           0x9F8402
#define AOT_ASK_RELEASE             0x9F8403
#define AOT_DATE_TIME_ENQ           0x9F8440
#define AOT_DATE_TIME               0x9F8441
#define AOT_CLOSE_MMI               0x9F8800
#define AOT_DISPLAY_CONTROL         0x9F8801
#define AOT_DISPLAY_REPLY           0x9F8802
#define AOT_TEXT_LAST               0x9F8803
#define AOT_TEXT_MORE               0x9F8804
#define AOT_KEYPAD_CONTROL          0x9F8805
#define AOT_KEYPRESS                0x9F8806
#define AOT_ENQ                     0x9F8807
#define AOT_ANSW                    0x9F8808
#define AOT_MENU_LAST               0x9F8809
#define AOT_MENU_MORE               0x9F880A
#define AOT_MENU_ANSW               0x9F880B
#define AOT_LIST_LAST               0x9F880C
#define AOT_LIST_MORE               0x9F880D
#define AOT_SUBTITLE_SEGMENT_LAST   0x9F880E
#define AOT_SUBTITLE_SEGMENT_MORE   0x9F880F
#define AOT_DISPLAY_MESSAGE         0x9F8810
#define AOT_SCENE_END_MARK          0x9F8811
#define AOT_SCENE_DONE              0x9F8812
#define AOT_SCENE_CONTROL           0x9F8813
#define AOT_SUBTITLE_DOWNLOAD_LAST  0x9F8814
#define AOT_SUBTITLE_DOWNLOAD_MORE  0x9F8815
#define AOT_FLUSH_DOWNLOAD          0x9F8816
#define AOT_DOWNLOAD_REPLY          0x9F8817
#define AOT_COMMS_CMD               0x9F8C00
#define AOT_CONNECTION_DESCRIPTOR   0x9F8C01
#define AOT_COMMS_REPLY             0x9F8C02
#define AOT_COMMS_SEND_LAST         0x9F8C03
#define AOT_COMMS_SEND_MORE         0x9F8C04
#define AOT_COMMS_RCV_LAST          0x9F8C05
#define AOT_COMMS_RCV_MORE          0x9F8C06

#define RESOURCE_CLASS_MASK         0xFFFF0000

cCiSession::cCiSession(uint16_t SessionId, uint32_t ResourceId, cCiTransportConnection *Tc)
{
  sessionId = SessionId;
  resourceId = ResourceId;
  tc = Tc;
}

cCiSession::~cCiSession()
{
}

void cCiSession::SetResourceId(uint32_t Id)
{
  resourceId = Id;
}

void cCiSession::SetTsPostProcessor(void)
{
  tc->SetTsPostProcessor(this);
}

int cCiSession::GetTag(int &Length, const uint8_t **Data)
///< Gets the tag at Data.
///< Returns the actual tag, or AOT_NONE in case of error.
///< Upon return Length and Data represent the remaining data after the tag has been skipped.
{
  if (Length >= 3 && Data && *Data) {
     int t = 0;
     for (int i = 0; i < 3; i++)
         t = (t << 8) | *(*Data)++;
     Length -= 3;
     return t;
     }
  return AOT_NONE;
}

const uint8_t *cCiSession::GetData(const uint8_t *Data, int &Length)
{
  Data = GetLength(Data, Length);
  return Length ? Data : NULL;
}

void cCiSession::SendData(int Tag, int Length, const uint8_t *Data)
{
  uint8_t buffer[MAX_TPDU_SIZE];
  uint8_t *p = buffer;
  *p++ = ST_SESSION_NUMBER;
  *p++ = 0x02;
  *p++ = (sessionId >> 8) & 0xFF;
  *p++ =  sessionId       & 0xFF;
  *p++ = (Tag >> 16) & 0xFF;
  *p++ = (Tag >>  8) & 0xFF;
  *p++ =  Tag        & 0xFF;
  p = SetLength(p, Length);
  if (p - buffer + Length < int(sizeof(buffer))) {
     if (Data)
        memcpy(p, Data, Length);
     p += Length;
     tc->SendData(p - buffer, buffer);
     }
  else
     esyslog("ERROR: CAM %d: data length (%d) exceeds buffer size", CamSlot()->SlotNumber(), Length);
}

cCamSlot *cCiSession::CamSlot(void)
{
  return Tc()->CamSlot();
}

void cCiSession::Process(int Length, const uint8_t *Data)
{
}

// --- cCiResourceManager ----------------------------------------------------

class cCiResourceManager : public cCiSession {
private:
  int state;
public:
  cCiResourceManager(uint16_t SessionId, cCiTransportConnection *Tc);
  virtual void Process(int Length = 0, const uint8_t *Data = NULL);
  };

cCiResourceManager::cCiResourceManager(uint16_t SessionId, cCiTransportConnection *Tc)
:cCiSession(SessionId, RI_RESOURCE_MANAGER, Tc)
{
  dbgprotocol("Slot %d: new Resource Manager (session id %d)\n", CamSlot()->SlotNumber(), SessionId);
  state = 0;
}

void cCiResourceManager::Process(int Length, const uint8_t *Data)
{
  if (Data) {
     int Tag = GetTag(Length, &Data);
     switch (Tag) {
       case AOT_PROFILE_ENQ: {
            dbgprotocol("Slot %d: <== Profile Enquiry (%d)\n", CamSlot()->SlotNumber(), SessionId());
            dbgprotocol("Slot %d: ==> Profile (%d)\n", CamSlot()->SlotNumber(), SessionId());
            SendData(AOT_PROFILE, CiResourceHandlers.NumIds() * sizeof(uint32_t), (uint8_t*)CiResourceHandlers.Ids());
            state = 3;
            }
            break;
       case AOT_PROFILE: {
            dbgprotocol("Slot %d: <== Profile (%d)\n", CamSlot()->SlotNumber(), SessionId());
            if (state == 1) {
               int l = 0;
               const uint8_t *d = GetData(Data, l);
               if (l > 0 && d)
                  esyslog("ERROR: CAM %d: resource manager: unexpected data", CamSlot()->SlotNumber());
               dbgprotocol("Slot %d: ==> Profile Change (%d)\n", CamSlot()->SlotNumber(), SessionId());
               SendData(AOT_PROFILE_CHANGE);
               state = 2;
               }
            else {
               esyslog("ERROR: CAM %d: resource manager: unexpected tag %06X in state %d", CamSlot()->SlotNumber(), Tag, state);
               }
            }
            break;
       default: esyslog("ERROR: CAM %d: resource manager: unknown tag %06X", CamSlot()->SlotNumber(), Tag);
       }
     }
  else if (state == 0) {
     dbgprotocol("Slot %d: ==> Profile Enq (%d)\n", CamSlot()->SlotNumber(), SessionId());
     SendData(AOT_PROFILE_ENQ);
     state = 1;
     }
}

// --- cCiApplicationInformation ---------------------------------------------

cCiApplicationInformation::cCiApplicationInformation(uint16_t SessionId, cCiTransportConnection *Tc)
:cCiSession(SessionId, RI_APPLICATION_INFORMATION, Tc)
{
  dbgprotocol("Slot %d: new Application Information (session id %d)\n", CamSlot()->SlotNumber(), SessionId);
  state = 0;
  menuString = NULL;
}

cCiApplicationInformation::~cCiApplicationInformation()
{
  free(menuString);
}

void cCiApplicationInformation::Process(int Length, const uint8_t *Data)
{
  if (Data) {
     int Tag = GetTag(Length, &Data);
     switch (Tag) {
       case AOT_APPLICATION_INFO: {
            dbgprotocol("Slot %d: <== Application Info (%d)\n", CamSlot()->SlotNumber(), SessionId());
            int l = 0;
            const uint8_t *d = GetData(Data, l);
            if ((l -= 1) < 0) break;
            applicationType = *d++;
            if ((l -= 2) < 0) break;
            applicationManufacturer = ntohs(get_unaligned((uint16_t *)d));
            d += 2;
            if ((l -= 2) < 0) break;
            manufacturerCode = ntohs(get_unaligned((uint16_t *)d));
            d += 2;
            free(menuString);
            menuString = GetString(l, &d);
            isyslog("CAM %d: %s, %02X, %04X, %04X", CamSlot()->SlotNumber(), menuString, applicationType, applicationManufacturer, manufacturerCode);
            state = 2;
            }
            break;
       default: esyslog("ERROR: CAM %d: application information: unknown tag %06X", CamSlot()->SlotNumber(), Tag);
       }
     }
  else if (state == 0) {
     dbgprotocol("Slot %d: ==> Application Info Enq (%d)\n", CamSlot()->SlotNumber(), SessionId());
     SendData(AOT_APPLICATION_INFO_ENQ);
     state = 1;
     }
}

bool cCiApplicationInformation::EnterMenu(void)
{
  if (state == 2) {
     dbgprotocol("Slot %d: ==> Enter Menu (%d)\n", CamSlot()->SlotNumber(), SessionId());
     SendData(AOT_ENTER_MENU);
     return true;
     }
  return false;
}

// --- cCiCaPmt --------------------------------------------------------------

#define MAXCASYSTEMIDS 64

// Ca Pmt List Management:

#define CPLM_MORE    0x00
#define CPLM_FIRST   0x01
#define CPLM_LAST    0x02
#define CPLM_ONLY    0x03
#define CPLM_ADD     0x04
#define CPLM_UPDATE  0x05

// Ca Pmt Cmd Ids:

#define CPCI_OK_DESCRAMBLING  0x01
#define CPCI_OK_MMI           0x02
#define CPCI_QUERY            0x03
#define CPCI_NOT_SELECTED     0x04

class cCiCaPmt {
  friend class cCiConditionalAccessSupport;
private:
  uint8_t cmdId;
  int esInfoLengthPos;
  cDynamicBuffer caDescriptors;
  cDynamicBuffer capmt;
  int source;
  int transponder;
  int programNumber;
  int caSystemIds[MAXCASYSTEMIDS + 1]; // list is zero terminated!
  void AddCaDescriptors(int Length, const uint8_t *Data);
public:
  cCiCaPmt(uint8_t CmdId, int Source, int Transponder, int ProgramNumber, const int *CaSystemIds);
  uint8_t CmdId(void) { return cmdId; }
  void SetListManagement(uint8_t ListManagement);
  uint8_t ListManagement(void) { return capmt.Get(0); }
  void AddPid(int Pid, uint8_t StreamType);
  void MtdMapPids(cMtdMapper *MtdMapper);
  };

cCiCaPmt::cCiCaPmt(uint8_t CmdId, int Source, int Transponder, int ProgramNumber, const int *CaSystemIds)
{
  cmdId = CmdId;
  source = Source;
  transponder = Transponder;
  programNumber = ProgramNumber;
  int i = 0;
  if (CaSystemIds) {
     for (; CaSystemIds[i]; i++)
         caSystemIds[i] = CaSystemIds[i];
     }
  caSystemIds[i] = 0;
  GetCaDescriptors(source, transponder, programNumber, caSystemIds, caDescriptors, 0);
  capmt.Append(CPLM_ONLY);
  capmt.Append((ProgramNumber >> 8) & 0xFF);
  capmt.Append( ProgramNumber       & 0xFF);
  capmt.Append(0x01); // version_number, current_next_indicator - apparently vn doesn't matter, but cni must be 1
  esInfoLengthPos = capmt.Length();
  capmt.Append(0x00); // program_info_length H (at program level)
  capmt.Append(0x00); // program_info_length L
  AddCaDescriptors(caDescriptors.Length(), caDescriptors.Data());
}

void cCiCaPmt::SetListManagement(uint8_t ListManagement)
{
  capmt.Set(0, ListManagement);
}

void cCiCaPmt::AddPid(int Pid, uint8_t StreamType)
{
  if (Pid) {
     GetCaDescriptors(source, transponder, programNumber, caSystemIds, caDescriptors, Pid);
     capmt.Append(StreamType);
     capmt.Append((Pid >> 8) & 0xFF);
     capmt.Append( Pid       & 0xFF);
     esInfoLengthPos = capmt.Length();
     capmt.Append(0x00); // ES_info_length H (at ES level)
     capmt.Append(0x00); // ES_info_length L
     AddCaDescriptors(caDescriptors.Length(), caDescriptors.Data());
     }
}

void cCiCaPmt::AddCaDescriptors(int Length, const uint8_t *Data)
{
  if (esInfoLengthPos) {
     if (Length || cmdId == CPCI_QUERY) {
        capmt.Append(cmdId);
        capmt.Append(Data, Length);
        int l = capmt.Length() - esInfoLengthPos - 2;
        capmt.Set(esInfoLengthPos,     (l >> 8) & 0xFF);
        capmt.Set(esInfoLengthPos + 1,  l       & 0xFF);
        }
     esInfoLengthPos = 0;
     }
  else
     esyslog("ERROR: adding CA descriptor without Pid!");
}

static int MtdMapCaDescriptor(uchar *p, cMtdMapper *MtdMapper)
{
  // See pat.c: cCaDescriptor::cCaDescriptor() for the layout of the data!
  if (*p == SI::CaDescriptorTag) {
     int l = *++p;
     if (l >= 4) {
        MtdMapPid(p + 3, MtdMapper);
        return l + 2;
        }
     else
        esyslog("ERROR: wrong length (%d) in MtdMapCaDescriptor()", l);
     }
  else
     esyslog("ERROR: wrong tag (%d) in MtdMapCaDescriptor()", *p);
  return -1;
}

static int MtdMapCaDescriptors(uchar *p, cMtdMapper *MtdMapper)
{
  int Length = p[0] * 256 + p[1];
  if (Length >= 3) {
     p += 3;
     int m = Length - 1;
     while (m > 0) {
           int l = MtdMapCaDescriptor(p, MtdMapper);
           if (l > 0) {
              p += l;
              m -= l;
              }
           }
     }
  return Length + 2;
}

static int MtdMapStream(uchar *p, cMtdMapper *MtdMapper)
{
  // See ci.c: cCiCaPmt::AddPid() for the layout of the data!
  MtdMapPid(p + 1, MtdMapper);
  int l = MtdMapCaDescriptors(p + 3, MtdMapper);
  if (l > 0)
     return l + 3;
  return -1;
}

static int MtdMapStreams(uchar *p, cMtdMapper *MtdMapper, int Length)
{
  int m = Length;
  while (m >= 5) {
        int l = MtdMapStream(p, MtdMapper);
        if (l > 0) {
           p += l;
           m -= l;
           }
        else
           break;
        }
  return Length;
}

void cCiCaPmt::MtdMapPids(cMtdMapper *MtdMapper)
{
  uchar *p = capmt.Data();
  int m = capmt.Length();
  if (m >= 3) {
     MtdMapSid(p + 1, MtdMapper);
     p += 4;
     m -= 4;
     if (m >= 2) {
        int l = MtdMapCaDescriptors(p, MtdMapper);
        if (l >= 0) {
           p += l;
           m -= l;
           MtdMapStreams(p, MtdMapper, m);
           }
        }
     }
}

// --- cCiConditionalAccessSupport -------------------------------------------

// CA Enable Ids:

#define CAEI_POSSIBLE                  0x01
#define CAEI_POSSIBLE_COND_PURCHASE    0x02
#define CAEI_POSSIBLE_COND_TECHNICAL   0x03
#define CAEI_NOT_POSSIBLE_ENTITLEMENT  0x71
#define CAEI_NOT_POSSIBLE_TECHNICAL    0x73

#define CA_ENABLE_FLAG                 0x80

#define CA_ENABLE(x) (((x) & CA_ENABLE_FLAG) ? (x) & ~CA_ENABLE_FLAG : 0)

#define QUERY_WAIT_TIME       500 // ms to wait before sending a query
#define QUERY_REPLY_TIMEOUT  2000 // ms to wait for a reply to a query
#define QUERY_RETRIES           6 // max. number of retries to check if there is a reply to a query

class cCiConditionalAccessSupport : public cCiSession {
private:
  int state;
  int numCaSystemIds;
  int caSystemIds[MAXCASYSTEMIDS + 1]; // list is zero terminated!
  bool repliesToQuery;
  cTimeMs timer;
  int numRetries;
public:
  cCiConditionalAccessSupport(uint16_t SessionId, cCiTransportConnection *Tc);
  virtual void Process(int Length = 0, const uint8_t *Data = NULL);
  const int *GetCaSystemIds(void) { return caSystemIds; }
  void SendPMT(cCiCaPmt *CaPmt);
  bool RepliesToQuery(void) { return repliesToQuery; }
  bool Ready(void) { return state >= 4; }
  bool ReceivedReply(void) { return state >= 5; }
  bool CanDecrypt(void) { return state == 6; }
  };

cCiConditionalAccessSupport::cCiConditionalAccessSupport(uint16_t SessionId, cCiTransportConnection *Tc)
:cCiSession(SessionId, RI_CONDITIONAL_ACCESS_SUPPORT, Tc)
{
  dbgprotocol("Slot %d: new Conditional Access Support (session id %d)\n", CamSlot()->SlotNumber(), SessionId);
  state = 0; // inactive
  caSystemIds[numCaSystemIds = 0] = 0;
  repliesToQuery = false;
  numRetries = 0;
}

void cCiConditionalAccessSupport::Process(int Length, const uint8_t *Data)
{
  if (Data) {
     int Tag = GetTag(Length, &Data);
     switch (Tag) {
       case AOT_CA_INFO: {
            dbgprotocol("Slot %d: <== Ca Info (%d)", CamSlot()->SlotNumber(), SessionId());
            cString Ids;
            numCaSystemIds = 0;
            int l = 0;
            const uint8_t *d = GetData(Data, l);
            while (l > 1) {
                  uint16_t id = ((uint16_t)(*d) << 8) | *(d + 1);
                  Ids = cString::sprintf("%s %04X", *Ids ? *Ids : "", id);
                  dbgprotocol(" %04X", id);
                  d += 2;
                  l -= 2;
                  if (numCaSystemIds < MAXCASYSTEMIDS)
                     caSystemIds[numCaSystemIds++] = id;
                  else {
                     esyslog("ERROR: CAM %d: too many CA system IDs!", CamSlot()->SlotNumber());
                     break;
                     }
                  }
            caSystemIds[numCaSystemIds] = 0;
            dbgprotocol("\n");
            if (state == 1) {
               timer.Set(0);
               numRetries = QUERY_RETRIES;
               state = 2; // got ca info
               }
            dsyslog("CAM %d: system ids:%s", CamSlot()->SlotNumber(), *Ids ? *Ids : " none");
            }
            break;
       case AOT_CA_PMT_REPLY: {
            dbgprotocol("Slot %d: <== Ca Pmt Reply (%d)", CamSlot()->SlotNumber(), SessionId());
            if (!repliesToQuery) {
               if (CamSlot()->IsMasterSlot())
                  dsyslog("CAM %d: replies to QUERY - multi channel decryption (MCD) possible", CamSlot()->SlotNumber());
               repliesToQuery = true;
               if (CamSlot()->MtdAvailable()) {
                  if (CamSlot()->IsMasterSlot())
                     dsyslog("CAM %d: supports multi transponder decryption (MTD)", CamSlot()->SlotNumber());
                  CamSlot()->MtdActivate(true);
                  }
               }
            state = 5; // got ca pmt reply
            int l = 0;
            const uint8_t *d = GetData(Data, l);
            if (l > 1) {
               uint16_t pnr = ((uint16_t)(*d) << 8) | *(d + 1);
               dbgprotocol(" %d", pnr);
               d += 2;
               l -= 2;
               if (l > 0) {
                  dbgprotocol(" %02X", *d);
                  d += 1;
                  l -= 1;
                  if (l > 0) {
                     if (l % 3 == 0 && l > 1) {
                        // The EN50221 standard defines that the next byte is supposed
                        // to be the CA_enable value at programme level. However, there are
                        // CAMs (for instance the AlphaCrypt with firmware <= 3.05) that
                        // insert a two byte length field here.
                        // This is a workaround to skip this length field:
                        uint16_t len = ((uint16_t)(*d) << 8) | *(d + 1);
                        if (len == l - 2) {
                           d += 2;
                           l -= 2;
                           }
                        }
                     unsigned char caepl = *d;
                     dbgprotocol(" %02X", caepl);
                     d += 1;
                     l -= 1;
                     bool ok = true;
                     if (l <= 2)
                        ok = CA_ENABLE(caepl) == CAEI_POSSIBLE;
                     while (l > 2) {
                           uint16_t pid = ((uint16_t)(*d) << 8) | *(d + 1);
                           unsigned char caees = *(d + 2);
                           dbgprotocol(" %d=%02X", pid, caees);
                           d += 3;
                           l -= 3;
                           if (CA_ENABLE(caees) != CAEI_POSSIBLE)
                              ok = false;
                           }
                     if (ok)
                        state = 6; // descrambling possible
                     }
                  }
               }
            dbgprotocol("\n");
            }
            break;
       default: esyslog("ERROR: CAM %d: conditional access support: unknown tag %06X", CamSlot()->SlotNumber(), Tag);
       }
     }
  else if (state == 0) {
     dbgprotocol("Slot %d: ==> Ca Info Enq (%d)\n", CamSlot()->SlotNumber(), SessionId());
     SendData(AOT_CA_INFO_ENQ);
     state = 1; // enquired ca info
     }
  else if ((state == 2 || state == 3) && timer.TimedOut()) {
     if (numRetries-- > 0) {
        cCiCaPmt CaPmt(CPCI_QUERY, 0, 0, 0, NULL);
        SendPMT(&CaPmt);
        timer.Set(QUERY_WAIT_TIME);
        state = 3; // waiting for reply
        }
     else {
        dsyslog("CAM %d: doesn't reply to QUERY - only a single channel can be decrypted", CamSlot()->SlotNumber());
        CamSlot()->MtdActivate(false);
        state = 4; // normal operation
        }
     }
}

void cCiConditionalAccessSupport::SendPMT(cCiCaPmt *CaPmt)
{
  if (CaPmt && state >= 2) {
     dbgprotocol("Slot %d: ==> Ca Pmt (%d) %d %d\n", CamSlot()->SlotNumber(), SessionId(), CaPmt->ListManagement(), CaPmt->CmdId());
     SendData(AOT_CA_PMT, CaPmt->capmt.Length(), CaPmt->capmt.Data());
     state = 4; // sent ca pmt
     }
}

// --- cCiHostControl --------------------------------------------------------

class cCiHostControl : public cCiSession {
public:
  cCiHostControl(uint16_t SessionId, cCiTransportConnection *Tc);
  virtual void Process(int Length = 0, const uint8_t *Data = NULL);
  };

cCiHostControl::cCiHostControl(uint16_t SessionId, cCiTransportConnection* Tc)
:cCiSession(SessionId, RI_HOST_CONTROL, Tc)
{
  dbgprotocol("Slot %d: new Host Control (session id %d)\n", CamSlot()->SlotNumber(), SessionId);
}

void cCiHostControl::Process(int Length, const uint8_t* Data)
{
  if (Data) {
     int Tag = GetTag(Length, &Data);
     switch (Tag) {
       case AOT_TUNE:
            dbgprotocol("Slot %d: <== Host Control Tune (%d)\n", CamSlot()->SlotNumber(), SessionId());
            break;
       case AOT_REPLACE:
            dbgprotocol("Slot %d: <== Host Control Replace (%d)\n", CamSlot()->SlotNumber(), SessionId());
            break;
       case AOT_CLEAR_REPLACE:
            dbgprotocol("Slot %d: <== Host Control Clear Replace (%d)\n", CamSlot()->SlotNumber(), SessionId());
            break;
       default: esyslog("ERROR: CAM %d: Host Control: unknown tag %06X", CamSlot()->SlotNumber(), Tag);
       }
     }
}

// --- cCiDateTime -----------------------------------------------------------

class cCiDateTime : public cCiSession {
private:
  int interval;
  time_t lastTime;
  void SendDateTime(void);
public:
  cCiDateTime(uint16_t SessionId, cCiTransportConnection *Tc);
  virtual void Process(int Length = 0, const uint8_t *Data = NULL);
  };

cCiDateTime::cCiDateTime(uint16_t SessionId, cCiTransportConnection *Tc)
:cCiSession(SessionId, RI_DATE_TIME, Tc)
{
  interval = 0;
  lastTime = 0;
  dbgprotocol("Slot %d: new Date Time (session id %d)\n", CamSlot()->SlotNumber(), SessionId);
}

void cCiDateTime::SendDateTime(void)
{
  time_t t = time(NULL);
  struct tm tm_gmt;
  struct tm tm_loc;
  if (gmtime_r(&t, &tm_gmt) && localtime_r(&t, &tm_loc)) {
     int Y = tm_gmt.tm_year;
     int M = tm_gmt.tm_mon + 1;
     int D = tm_gmt.tm_mday;
     int L = (M == 1 || M == 2) ? 1 : 0;
     int MJD = 14956 + D + int((Y - L) * 365.25) + int((M + 1 + L * 12) * 30.6001);
#define DEC2BCD(d) uint8_t(((d / 10) << 4) + (d % 10))
#pragma pack(1)
     struct tTime { uint16_t mjd; uint8_t h, m, s; short offset; };
#pragma pack()
     tTime T = { mjd : htons(MJD), h : DEC2BCD(tm_gmt.tm_hour), m : DEC2BCD(tm_gmt.tm_min), s : DEC2BCD(tm_gmt.tm_sec), offset : short(htons(tm_loc.tm_gmtoff / 60)) };
     bool OldDumpTPDUDataTransfer = DumpTPDUDataTransfer;
     DumpTPDUDataTransfer &= DumpDateTime;
     if (DumpDateTime)
        dbgprotocol("Slot %d: ==> Date Time (%d)\n", CamSlot()->SlotNumber(), SessionId());
     SendData(AOT_DATE_TIME, 7, (uint8_t*)&T);
     DumpTPDUDataTransfer = OldDumpTPDUDataTransfer;
     }
}

void cCiDateTime::Process(int Length, const uint8_t *Data)
{
  if (Data) {
     int Tag = GetTag(Length, &Data);
     switch (Tag) {
       case AOT_DATE_TIME_ENQ: {
            interval = 0;
            int l = 0;
            const uint8_t *d = GetData(Data, l);
            if (l > 0)
               interval = *d;
            dbgprotocol("Slot %d: <== Date Time Enq (%d), interval = %d\n", CamSlot()->SlotNumber(), SessionId(), interval);
            lastTime = time(NULL);
            SendDateTime();
            }
            break;
       default: esyslog("ERROR: CAM %d: date time: unknown tag %06X", CamSlot()->SlotNumber(), Tag);
       }
     }
  else if (interval && time(NULL) - lastTime > interval) {
     lastTime = time(NULL);
     SendDateTime();
     }
}

// --- cCiMMI ----------------------------------------------------------------

// Display Control Commands:

#define DCC_SET_MMI_MODE                          0x01
#define DCC_DISPLAY_CHARACTER_TABLE_LIST          0x02
#define DCC_INPUT_CHARACTER_TABLE_LIST            0x03
#define DCC_OVERLAY_GRAPHICS_CHARACTERISTICS      0x04
#define DCC_FULL_SCREEN_GRAPHICS_CHARACTERISTICS  0x05

// MMI Modes:

#define MM_HIGH_LEVEL                      0x01
#define MM_LOW_LEVEL_OVERLAY_GRAPHICS      0x02
#define MM_LOW_LEVEL_FULL_SCREEN_GRAPHICS  0x03

// Display Reply IDs:

#define DRI_MMI_MODE_ACK                              0x01
#define DRI_LIST_DISPLAY_CHARACTER_TABLES             0x02
#define DRI_LIST_INPUT_CHARACTER_TABLES               0x03
#define DRI_LIST_GRAPHIC_OVERLAY_CHARACTERISTICS      0x04
#define DRI_LIST_FULL_SCREEN_GRAPHIC_CHARACTERISTICS  0x05
#define DRI_UNKNOWN_DISPLAY_CONTROL_CMD               0xF0
#define DRI_UNKNOWN_MMI_MODE                          0xF1
#define DRI_UNKNOWN_CHARACTER_TABLE                   0xF2

// Enquiry Flags:

#define EF_BLIND  0x01

// Answer IDs:

#define AI_CANCEL  0x00
#define AI_ANSWER  0x01

class cCiMMI : public cCiSession {
private:
  char *GetText(int &Length, const uint8_t **Data);
  cCiMenu *menu, *fetchedMenu;
  cCiEnquiry *enquiry, *fetchedEnquiry;
public:
  cCiMMI(uint16_t SessionId, cCiTransportConnection *Tc);
  virtual ~cCiMMI();
  virtual void Process(int Length = 0, const uint8_t *Data = NULL);
  virtual bool HasUserIO(void) { return menu || enquiry; }
  cCiMenu *Menu(bool Clear = false);
  cCiEnquiry *Enquiry(bool Clear = false);
  void SendMenuAnswer(uint8_t Selection);
  bool SendAnswer(const char *Text);
  bool SendCloseMMI(void);
  };

cCiMMI::cCiMMI(uint16_t SessionId, cCiTransportConnection *Tc)
:cCiSession(SessionId, RI_MMI, Tc)
{
  dbgprotocol("Slot %d: new MMI (session id %d)\n", CamSlot()->SlotNumber(), SessionId);
  menu = fetchedMenu = NULL;
  enquiry = fetchedEnquiry = NULL;
}

cCiMMI::~cCiMMI()
{
  if (fetchedMenu) {
     cMutexLock MutexLock(fetchedMenu->mutex);
     fetchedMenu->mmi = NULL;
     }
  delete menu;
  if (fetchedEnquiry) {
     cMutexLock MutexLock(fetchedEnquiry->mutex);
     fetchedEnquiry->mmi = NULL;
     }
  delete enquiry;
}

char *cCiMMI::GetText(int &Length, const uint8_t **Data)
///< Gets the text at Data.
///< Returns a pointer to a newly allocated string, or NULL in case of error.
///< Upon return Length and Data represent the remaining data after the text has been skipped.
{
  int Tag = GetTag(Length, Data);
  if (Tag == AOT_TEXT_LAST) {
     char *s = GetString(Length, Data);
     dbgprotocol("Slot %d: <== Text Last (%d) '%s'\n", CamSlot()->SlotNumber(), SessionId(), s);
     return s;
     }
  else
     esyslog("ERROR: CAM %d: MMI: unexpected text tag: %06X", CamSlot()->SlotNumber(), Tag);
  return NULL;
}

void cCiMMI::Process(int Length, const uint8_t *Data)
{
  if (Data) {
     int Tag = GetTag(Length, &Data);
     switch (Tag) {
       case AOT_DISPLAY_CONTROL: {
            dbgprotocol("Slot %d: <== Display Control (%d)\n", CamSlot()->SlotNumber(), SessionId());
            int l = 0;
            const uint8_t *d = GetData(Data, l);
            if (l > 0) {
               switch (*d) {
                 case DCC_SET_MMI_MODE:
                      if (l == 2 && *++d == MM_HIGH_LEVEL) {
                         struct tDisplayReply { uint8_t id; uint8_t mode; };
                         tDisplayReply dr = { id : DRI_MMI_MODE_ACK, mode : MM_HIGH_LEVEL };
                         dbgprotocol("Slot %d: ==> Display Reply (%d)\n", CamSlot()->SlotNumber(), SessionId());
                         SendData(AOT_DISPLAY_REPLY, 2, (uint8_t *)&dr);
                         }
                      break;
                 default: esyslog("ERROR: CAM %d: MMI: unsupported display control command %02X", CamSlot()->SlotNumber(), *d);
                 }
               }
            }
            break;
       case AOT_LIST_LAST:
       case AOT_MENU_LAST: {
            dbgprotocol("Slot %d: <== Menu Last (%d)\n", CamSlot()->SlotNumber(), SessionId());
            delete menu;
            menu = new cCiMenu(this, Tag == AOT_MENU_LAST);
            int l = 0;
            const uint8_t *d = GetData(Data, l);
            if (l > 0) {
               // since the specification allows choiceNb to be undefined it is useless, so let's just skip it:
               d++;
               l--;
               if (l > 0) menu->titleText = GetText(l, &d);
               if (l > 0) menu->subTitleText = GetText(l, &d);
               if (l > 0) menu->bottomText = GetText(l, &d);
               int Action = CRA_NONE;
               int Select = -1;
               int Item = 0;
               while (l > 0) {
                     char *s = GetText(l, &d);
                     if (s) {
                        if (!menu->AddEntry(s))
                           free(s);
                        else if (Action == CRA_NONE) {
                           Action = CamResponses.GetMatch(CamSlot()->SlotNumber(), s);
                           if (Action == CRA_SELECT)
                              Select = Item;
                           }
                        }
                     else
                        break;
                     Item++;
                     }
               if (Action != CRA_NONE) {
                  delete menu;
                  menu = NULL;
                  cCondWait::SleepMs(100);
                  if (Action == CRA_DISCARD) {
                     SendCloseMMI();
                     dsyslog("CAM %d: DISCARD", CamSlot()->SlotNumber());
                     }
                  else if (Action == CRA_CONFIRM) {
                     SendMenuAnswer(1);
                     dsyslog("CAM %d: CONFIRM", CamSlot()->SlotNumber());
                     }
                  else if (Action == CRA_SELECT) {
                     SendMenuAnswer(Select + 1);
                     dsyslog("CAM %d: SELECT %d", CamSlot()->SlotNumber(), Select + 1);
                     }
                  }
               }
            }
            break;
       case AOT_ENQ: {
            dbgprotocol("Slot %d: <== Enq (%d)\n", CamSlot()->SlotNumber(), SessionId());
            delete enquiry;
            enquiry = new cCiEnquiry(this);
            int l = 0;
            const uint8_t *d = GetData(Data, l);
            if (l > 0) {
               uint8_t blind = *d++;
               //XXX GetByte()???
               l--;
               enquiry->blind = blind & EF_BLIND;
               enquiry->expectedLength = *d++;
               l--;
               // I really wonder why there is no text length field here...
               enquiry->text = CopyString(l, d);
               int Action = CamResponses.GetMatch(CamSlot()->SlotNumber(), enquiry->text);
               if (Action > CRA_NONE) {
                  char s[enquiry->expectedLength * 2];
                  snprintf(s, sizeof(s), "%d", Action);
                  if (int(strlen(s)) == enquiry->expectedLength) {
                     delete enquiry;
                     enquiry = NULL;
                     SendAnswer(s);
                     dsyslog("CAM %d: PIN", CamSlot()->SlotNumber());
                     }
                  else
                     esyslog("CAM %d: ERROR: unexpected PIN length %d, expected %d", CamSlot()->SlotNumber(), int(strlen(s)), enquiry->expectedLength);
                  }
               }
            }
            break;
       case AOT_CLOSE_MMI: {
            int id = -1;
            int delay = -1;
            int l = 0;
            const uint8_t *d = GetData(Data, l);
            if (l > 0) {
               id = *d++;
               if (l > 1)
                  delay = *d;
               }
            dbgprotocol("Slot %d: <== Close MMI (%d)  id = %02X  delay = %d\n", CamSlot()->SlotNumber(), SessionId(), id, delay);
            }
            break;
       default: esyslog("ERROR: CAM %d: MMI: unknown tag %06X", CamSlot()->SlotNumber(), Tag);
       }
     }
}

cCiMenu *cCiMMI::Menu(bool Clear)
{
  if (Clear)
     fetchedMenu = NULL;
  else if (menu) {
     fetchedMenu = menu;
     menu = NULL;
     }
  return fetchedMenu;
}

cCiEnquiry *cCiMMI::Enquiry(bool Clear)
{
  if (Clear)
     fetchedEnquiry = NULL;
  else if (enquiry) {
     fetchedEnquiry = enquiry;
     enquiry = NULL;
     }
  return fetchedEnquiry;
}

void cCiMMI::SendMenuAnswer(uint8_t Selection)
{
  dbgprotocol("Slot %d: ==> Menu Answ (%d)\n", CamSlot()->SlotNumber(), SessionId());
  SendData(AOT_MENU_ANSW, 1, &Selection);
}

bool cCiMMI::SendAnswer(const char *Text)
{
  dbgprotocol("Slot %d: ==> Answ (%d)\n", CamSlot()->SlotNumber(), SessionId());
  struct tAnswer { uint8_t id; char text[256]; };//XXX
  tAnswer answer;
  answer.id = Text ? AI_ANSWER : AI_CANCEL;
  int len = 0;
  if (Text) {
     len = min(sizeof(answer.text), strlen(Text));
     memcpy(answer.text, Text, len);
     }
  SendData(AOT_ANSW, len + 1, (uint8_t *)&answer);
  return true;
}

bool cCiMMI::SendCloseMMI(void)
{
  dbgprotocol("Slot %d: ==> Close MMI (%d)\n", CamSlot()->SlotNumber(), SessionId());
  SendData(AOT_CLOSE_MMI, 0);
  return true;
}

// --- cCiMenu ---------------------------------------------------------------

cCiMenu::cCiMenu(cCiMMI *MMI, bool Selectable)
{
  mmi = MMI;
  mutex = NULL;
  selectable = Selectable;
  titleText = subTitleText = bottomText = NULL;
  numEntries = 0;
}

cCiMenu::~cCiMenu()
{
  cMutexLock MutexLock(mutex);
  if (mmi)
     mmi->Menu(true);
  free(titleText);
  free(subTitleText);
  free(bottomText);
  for (int i = 0; i < numEntries; i++)
      free(entries[i]);
}

bool cCiMenu::AddEntry(char *s)
{
  if (numEntries < MAX_CIMENU_ENTRIES) {
     entries[numEntries++] = s;
     return true;
     }
  return false;
}

bool cCiMenu::HasUpdate(void)
{
  // If the mmi is gone, the menu shall be closed, which also qualifies as 'update'.
  return !mmi || mmi->HasUserIO();
}

void cCiMenu::Select(int Index)
{
  cMutexLock MutexLock(mutex);
  if (mmi && -1 <= Index && Index < numEntries)
     mmi->SendMenuAnswer(Index + 1);
}

void cCiMenu::Cancel(void)
{
  Select(-1);
}

void cCiMenu::Abort(void)
{
  cMutexLock MutexLock(mutex);
  if (mmi)
     mmi->SendCloseMMI();
}

// --- cCiEnquiry ------------------------------------------------------------

cCiEnquiry::cCiEnquiry(cCiMMI *MMI)
{
  mmi = MMI;
  mutex = NULL;
  text = NULL;
  blind = false;
  expectedLength = 0;
}

cCiEnquiry::~cCiEnquiry()
{
  cMutexLock MutexLock(mutex);
  if (mmi)
     mmi->Enquiry(true);
  free(text);
}

void cCiEnquiry::Reply(const char *s)
{
  cMutexLock MutexLock(mutex);
  if (mmi)
     mmi->SendAnswer(s);
}

void cCiEnquiry::Cancel(void)
{
  Reply(NULL);
}

void cCiEnquiry::Abort(void)
{
  cMutexLock MutexLock(mutex);
  if (mmi)
     mmi->SendCloseMMI();
}

// --- cCiResourceHandler ----------------------------------------------------

cCiResourceHandler::cCiResourceHandler(void)
{
}

cCiResourceHandler::~cCiResourceHandler()
{
}

// --- cCiDefaultResourceHandler ---------------------------------------------

class cCiDefaultResourceHandler : public cCiResourceHandler {
public:
  virtual const uint32_t *ResourceIds(void) const;
  virtual cCiSession *GetNewCiSession(uint32_t ResourceId, uint16_t SessionId, cCiTransportConnection *Tc);
  };

const uint32_t *cCiDefaultResourceHandler::ResourceIds(void) const
{
  static uint32_t Ids[] = {
    RI_RESOURCE_MANAGER,
    RI_APPLICATION_INFORMATION,
    RI_CONDITIONAL_ACCESS_SUPPORT,
    RI_HOST_CONTROL,
    RI_DATE_TIME,
    RI_MMI,
    0
    };
  return Ids;
}

cCiSession *cCiDefaultResourceHandler::GetNewCiSession(uint32_t ResourceId, uint16_t SessionId, cCiTransportConnection *Tc)
{
  switch (ResourceId) {
    case RI_RESOURCE_MANAGER:           return new cCiResourceManager(SessionId, Tc); break;
    case RI_APPLICATION_INFORMATION:    return new cCiApplicationInformation(SessionId, Tc); break;
    case RI_CONDITIONAL_ACCESS_SUPPORT: return new cCiConditionalAccessSupport(SessionId, Tc); break;
    case RI_HOST_CONTROL:               return new cCiHostControl(SessionId, Tc); break;
    case RI_DATE_TIME:                  return new cCiDateTime(SessionId, Tc); break;
    case RI_MMI:                        return new cCiMMI(SessionId, Tc); break;
    default:                            return NULL;
    }
}

// --- cCiResourceHandlers ---------------------------------------------------

cCiResourceHandlers CiResourceHandlers;

cCiResourceHandlers::cCiResourceHandlers(void)
{
  Register(new cCiDefaultResourceHandler);
}

void cCiResourceHandlers::Register(cCiResourceHandler *ResourceHandler)
{
  if (ResourceHandler) {
     Add(ResourceHandler);
     if (const uint32_t *r = ResourceHandler->ResourceIds()) {
        while (*r) {
              resourceIds.Append(htonl(*r));
              r++;
              }
        }
     }
}

cCiSession *cCiResourceHandlers::GetNewCiSession(uint32_t ResourceId, uint16_t SessionId, cCiTransportConnection *Tc)
{
  for (cCiResourceHandler *r = Last(); r; r = Prev(r)) {
      if (cCiSession *CiSession = r->GetNewCiSession(ResourceId, SessionId, Tc))
         return CiSession;
      }
  return NULL;
}

// --- cCiTransportConnection (cont'd) ---------------------------------------

#define TC_POLL_TIMEOUT   300 // ms WORKAROUND: TC_POLL_TIMEOUT < 300ms doesn't work with DragonCAM
#define TC_ALIVE_TIMEOUT 2000 // ms after which a transport connection is assumed dead

cCiTransportConnection::cCiTransportConnection(cCamSlot *CamSlot, uint8_t Tcid)
{
  dbgprotocol("Slot %d: creating connection %d/%d\n", CamSlot->SlotNumber(), CamSlot->SlotIndex(), Tcid);
  camSlot = CamSlot;
  tcid = Tcid;
  state = stIDLE;
  createConnectionRequested = false;
  deleteConnectionRequested = false;
  hasUserIO = false;
  alive.Set(TC_ALIVE_TIMEOUT);
  for (int i = 0; i <= MAX_SESSIONS_PER_TC; i++) // sessions[0] is not used, but initialized anyway
      sessions[i] = NULL;
  tsPostProcessor = NULL;
}

cCiTransportConnection::~cCiTransportConnection()
{
  for (int i = 1; i <= MAX_SESSIONS_PER_TC; i++)
      delete sessions[i];
}

void cCiTransportConnection::SetTsPostProcessor(cCiSession *CiSession)
{
  tsPostProcessor = CiSession;
}

bool cCiTransportConnection::TsPostProcess(uint8_t *TsPacket)
{
  cMutexLock MutexLock(&mutex);
  if (tsPostProcessor)
     return tsPostProcessor->TsPostProcess(TsPacket);
  return false;
}

bool cCiTransportConnection::Ready(void)
{
  cCiConditionalAccessSupport *cas = (cCiConditionalAccessSupport *)GetSessionByResourceId(RI_CONDITIONAL_ACCESS_SUPPORT);
  return cas && cas->Ready();
}

const char *cCiTransportConnection::GetCamName(void)
{
  cCiApplicationInformation *ai = (cCiApplicationInformation *)GetSessionByResourceId(RI_APPLICATION_INFORMATION);
  return ai ? ai->GetMenuString() : NULL;
}

void cCiTransportConnection::SendTPDU(uint8_t Tag, int Length, const uint8_t *Data)
{
  cTPDU TPDU(camSlot->SlotIndex(), tcid, Tag, Length, Data);
  camSlot->Write(&TPDU);
  timer.Set(TC_POLL_TIMEOUT);
}

void cCiTransportConnection::SendData(int Length, const uint8_t *Data)
{
  // if Length ever exceeds MAX_TPDU_DATA this needs to be handled differently
  if (state == stACTIVE && Length > 0)
     SendTPDU(T_DATA_LAST, Length, Data);
}

void cCiTransportConnection::SendTag(uint8_t Tag, uint16_t SessionId, uint32_t ResourceId, int Status)
{
  uint8_t buffer[16];
  uint8_t *p = buffer;
  *p++ = Tag;
  *p++ = 0x00; // will contain length
  if (Status >= 0)
     *p++ = Status;
  if (ResourceId) {
     put_unaligned(htonl(ResourceId), (uint32_t *)p);
     p += 4;
     }
  put_unaligned(htons(SessionId), (uint16_t *)p);
  p += 2;
  buffer[1] = p - buffer - 2; // length
  SendData(p - buffer, buffer);
}

void cCiTransportConnection::Poll(void)
{
  bool OldDumpTPDUDataTransfer = DumpTPDUDataTransfer;
  DumpTPDUDataTransfer &= DumpPolls;
  if (DumpPolls)
     dbgprotocol("Slot %d: ==> Poll\n", camSlot->SlotNumber());
  SendTPDU(T_DATA_LAST);
  DumpTPDUDataTransfer = OldDumpTPDUDataTransfer;
}

uint32_t cCiTransportConnection::ResourceIdToInt(const uint8_t *Data)
{
  return (ntohl(get_unaligned((uint32_t *)Data)));
}

cCiSession *cCiTransportConnection::GetSessionBySessionId(uint16_t SessionId)
{
  return (SessionId <= MAX_SESSIONS_PER_TC) ? sessions[SessionId] : NULL;
}

cCiSession *cCiTransportConnection::GetSessionByResourceId(uint32_t ResourceId)
{
  cCiSession *CiSession = NULL;
  for (int i = 1; i <= MAX_SESSIONS_PER_TC; i++) {
      if (cCiSession *s = sessions[i]) {
         if (s->ResourceId() == ResourceId)
            return s; // prefer exact match
         if ((s->ResourceId() & RESOURCE_CLASS_MASK) == (ResourceId & RESOURCE_CLASS_MASK))
            CiSession = s;
         }
      }
  return CiSession;
}

void cCiTransportConnection::OpenSession(int Length, const uint8_t *Data)
{
  if (Length == 6 && *(Data + 1) == 0x04) {
     uint32_t ResourceId = ResourceIdToInt(Data + 2);
     dbgprotocol("Slot %d: open session %08X\n", camSlot->SlotNumber(), ResourceId);
     if (!GetSessionByResourceId(ResourceId)) {
        for (int i = 1; i <= MAX_SESSIONS_PER_TC; i++) {
            if (!sessions[i]) {
               sessions[i] = CiResourceHandlers.GetNewCiSession(ResourceId, i, this);
               if (sessions[i])
                  SendTag(ST_OPEN_SESSION_RESPONSE, sessions[i]->SessionId(), sessions[i]->ResourceId(), SS_OK);
               else
                  esyslog("ERROR: CAM %d: unknown resource identifier: %08X (%d/%d)", camSlot->SlotNumber(), ResourceId, camSlot->SlotIndex(), tcid);
               return;
               }
            }
        esyslog("ERROR: CAM %d: no free session slot for resource identifier %08X (%d/%d)", camSlot->SlotNumber(), ResourceId, camSlot->SlotIndex(), tcid);
        }
     else
        esyslog("ERROR: CAM %d: session for resource identifier %08X already exists (%d/%d)", camSlot->SlotNumber(), ResourceId, camSlot->SlotIndex(), tcid);
     }
}

void cCiTransportConnection::CloseSession(uint16_t SessionId)
{
  dbgprotocol("Slot %d: close session %d\n", camSlot->SlotNumber(), SessionId);
  cCiSession *Session = GetSessionBySessionId(SessionId);
  if (Session && sessions[SessionId] == Session) {
     delete Session;
     sessions[SessionId] = NULL;
     SendTag(ST_CLOSE_SESSION_RESPONSE, SessionId, 0, SS_OK);
     }
  else {
     esyslog("ERROR: CAM %d: unknown session id: %d (%d/%d)", camSlot->SlotNumber(), SessionId, camSlot->SlotIndex(), tcid);
     SendTag(ST_CLOSE_SESSION_RESPONSE, SessionId, 0, SS_NOT_ALLOCATED);
     }
}

void cCiTransportConnection::HandleSessions(cTPDU *TPDU)
{
  int Length;
  const uint8_t *Data = TPDU->Data(Length);
  if (Data && Length > 1) {
     switch (*Data) {
       case ST_SESSION_NUMBER:          if (Length > 4) {
                                           uint16_t SessionId = ntohs(get_unaligned((uint16_t *)&Data[2]));
                                           cCiSession *Session = GetSessionBySessionId(SessionId);
                                           if (Session)
                                              Session->Process(Length - 4, Data + 4);
                                           else
                                              esyslog("ERROR: CAM %d: unknown session id: %d (%d/%d)", camSlot->SlotNumber(), SessionId, camSlot->SlotIndex(), tcid);
                                           }
                                        break;
       case ST_OPEN_SESSION_REQUEST:    OpenSession(Length, Data);
                                        break;
       case ST_CLOSE_SESSION_REQUEST:   if (Length == 4)
                                           CloseSession(ntohs(get_unaligned((uint16_t *)&Data[2])));
                                        break;
       case ST_CREATE_SESSION_RESPONSE: // not implemented
       case ST_CLOSE_SESSION_RESPONSE:  // not implemented
       default: esyslog("ERROR: CAM %d: unknown session tag: %02X (%d/%d)", camSlot->SlotNumber(), *Data, camSlot->SlotIndex(), tcid);
       }
     }
}

bool cCiTransportConnection::Process(cTPDU *TPDU)
{
  if (TPDU)
     alive.Set(TC_ALIVE_TIMEOUT);
  else if (alive.TimedOut())
     return false;
  switch (state) {
    case stIDLE:
         if (createConnectionRequested) {
            dbgprotocol("Slot %d: create connection %d/%d\n", camSlot->SlotNumber(), camSlot->SlotIndex(), tcid);
            createConnectionRequested = false;
            SendTPDU(T_CREATE_TC);
            state = stCREATION;
            }
         return true;
    case stCREATION:
         if (TPDU && TPDU->Tag() == T_CTC_REPLY) {
            dbgprotocol("Slot %d: connection created %d/%d\n", camSlot->SlotNumber(), camSlot->SlotIndex(), tcid);
            Poll();
            state = stACTIVE;
            }
         else if (timer.TimedOut()) {
            dbgprotocol("Slot %d: timeout while creating connection %d/%d\n", camSlot->SlotNumber(), camSlot->SlotIndex(), tcid);
            state = stIDLE;
            }
         return true;
    case stACTIVE:
         if (deleteConnectionRequested) {
            dbgprotocol("Slot %d: delete connection requested %d/%d\n", camSlot->SlotNumber(), camSlot->SlotIndex(), tcid);
            deleteConnectionRequested = false;
            SendTPDU(T_DELETE_TC);
            state = stDELETION;
            return true;
            }
         if (TPDU) {
            switch (TPDU->Tag()) {
              case T_REQUEST_TC:
                   esyslog("ERROR: CAM %d: T_REQUEST_TC not implemented (%d/%d)", camSlot->SlotNumber(), camSlot->SlotIndex(), tcid);
                   break;
              case T_DATA_MORE:
              case T_DATA_LAST:
                   HandleSessions(TPDU);
                   // continue with T_SB
              case T_SB:
                   if ((TPDU->Status() & DATA_INDICATOR) != 0) {
                      dbgprotocol("Slot %d: receive data %d/%d\n", camSlot->SlotNumber(), camSlot->SlotIndex(), tcid);
                      SendTPDU(T_RCV);
                      }
                   break;
              case T_DELETE_TC:
                   dbgprotocol("Slot %d: delete connection %d/%d\n", camSlot->SlotNumber(), camSlot->SlotIndex(), tcid);
                   SendTPDU(T_DTC_REPLY);
                   state = stIDLE;
                   return true;
              case T_RCV:
              case T_CREATE_TC:
              case T_CTC_REPLY:
              case T_DTC_REPLY:
              case T_NEW_TC:
              case T_TC_ERROR:
                   break;
              default:
                   esyslog("ERROR: unknown TPDU tag: 0x%02X (%s)", TPDU->Tag(), __FUNCTION__);
              }
            }
         else if (timer.TimedOut())
            Poll();
         hasUserIO = false;
         for (int i = 1; i <= MAX_SESSIONS_PER_TC; i++) {
             if (sessions[i]) {
                sessions[i]->Process();
                if (sessions[i]->HasUserIO())
                   hasUserIO = true;
                }
             }
         break;
    case stDELETION:
         if (TPDU && TPDU->Tag() == T_DTC_REPLY || timer.TimedOut()) {
            dbgprotocol("Slot %d: connection deleted %d/%d\n", camSlot->SlotNumber(), camSlot->SlotIndex(), tcid);
            state = stIDLE;
            }
         return true;
    default:
         esyslog("ERROR: unknown state: %d (%s)", state, __FUNCTION__);
    }
  return true;
}

// --- cCiCaPidData ----------------------------------------------------------

class cCiCaPidData : public cListObject {
public:
  bool active;
  int pid;
  int streamType;
  cCiCaPidData(int Pid, int StreamType)
  {
    active = false;
    pid = Pid;
    streamType = StreamType;
  }
  };

// --- cCiCaProgramData ------------------------------------------------------

class cCiCaProgramData : public cListObject {
public:
  int programNumber;
  bool modified;
  cList<cCiCaPidData> pidList;
  cCiCaProgramData(int ProgramNumber)
  {
    programNumber = ProgramNumber;
    modified = false;
  }
  bool Active(void)
  {
    for (cCiCaPidData *p = pidList.First(); p; p = pidList.Next(p)) {
        if (p->active)
           return true;
        }
    return false;
  }
  };

// --- cCiAdapter ------------------------------------------------------------

cCiAdapter::cCiAdapter(void)
:cThread("CI adapter")
{
  for (int i = 0; i < MAX_CAM_SLOTS_PER_ADAPTER; i++)
      camSlots[i] = NULL;
}

cCiAdapter::~cCiAdapter()
{
  Cancel(3);
  for (int i = 0; i < MAX_CAM_SLOTS_PER_ADAPTER; i++)
      delete camSlots[i];
}

void cCiAdapter::AddCamSlot(cCamSlot *CamSlot)
{
  if (CamSlot) {
     for (int i = 0; i < MAX_CAM_SLOTS_PER_ADAPTER; i++) {
         if (!camSlots[i]) {
            CamSlot->slotIndex = i;
            camSlots[i] = CamSlot;
            return;
            }
        }
     esyslog("ERROR: no free CAM slot in CI adapter");
     }
}

cCamSlot *cCiAdapter::ItCamSlot(int &Iter)
{
  if (Iter >= 0) {
     for (; Iter < MAX_CAM_SLOTS_PER_ADAPTER; ) {
         if (cCamSlot *Found = camSlots[Iter++])
            return Found;
         }
     }
  return NULL;
}

void cCiAdapter::Action(void)
{
  cTPDU TPDU;
  while (Running()) {
        int n = Read(TPDU.Buffer(), TPDU.MaxSize());
        if (n > 0 && TPDU.Slot() < MAX_CAM_SLOTS_PER_ADAPTER) {
           TPDU.SetSize(n);
           cCamSlot *cs = camSlots[TPDU.Slot()];
           TPDU.Dump(cs ? cs->SlotNumber() : 0, false);
           if (cs)
              cs->Process(&TPDU);
           }
        for (int i = 0; i < MAX_CAM_SLOTS_PER_ADAPTER; i++) {
            if (camSlots[i])
               camSlots[i]->Process();
            }
        }
}

// --- cCamSlot --------------------------------------------------------------

#define MODULE_CHECK_INTERVAL 500 // ms
#define MODULE_RESET_TIMEOUT    2 // s

cCamSlot::cCamSlot(cCiAdapter *CiAdapter, bool WantsTsData, cCamSlot *MasterSlot)
{
  ciAdapter = CiAdapter;
  masterSlot = MasterSlot;
  assignedDevice = NULL;
  caPidReceiver = WantsTsData ? new cCaPidReceiver : NULL;
  caActivationReceiver = NULL;
  slotIndex = -1;
  mtdAvailable = false;
  mtdHandler = NULL;
  lastModuleStatus = msReset; // avoids initial reset log message
  resetTime = 0;
  resendPmt = false;
  for (int i = 0; i <= MAX_CONNECTIONS_PER_CAM_SLOT; i++) // tc[0] is not used, but initialized anyway
      tc[i] = NULL;
  if (MasterSlot)
     slotNumber = MasterSlot->SlotNumber();
  if (ciAdapter) {
     CamSlots.Add(this);
     slotNumber = Index() + 1;
     ciAdapter->AddCamSlot(this);
     Reset();
     }
}

cCamSlot::~cCamSlot()
{
  Assign(NULL);
  delete caPidReceiver;
  delete caActivationReceiver;
  CamSlots.Del(this, false);
  DeleteAllConnections();
  delete mtdHandler;
}

cCamSlot *cCamSlot::MtdSpawn(void)
{
  cMutexLock MutexLock(&mutex);
  if (mtdHandler)
     return mtdHandler->GetMtdCamSlot(this);
  return this;
}

bool cCamSlot::Assign(cDevice *Device, bool Query)
{
  cMutexLock MutexLock(&mutex);
  if (Device == assignedDevice)
     return true;
  if (ciAdapter) {
     int OldDeviceNumber = 0;
     if (assignedDevice && !Query) {
        OldDeviceNumber = assignedDevice->DeviceNumber() + 1;
        if (caPidReceiver)
           assignedDevice->Detach(caPidReceiver);
        assignedDevice->SetCamSlot(NULL);
        assignedDevice = NULL;
        }
     if (ciAdapter->Assign(Device, true)) {
        if (!Query) {
           StopDecrypting();
           if (ciAdapter->Assign(Device)) {
              if (Device) {
                 Device->SetCamSlot(this);
                 assignedDevice = Device;
                 if (caPidReceiver) {
                    caPidReceiver->Reset();
                    Device->AttachReceiver(caPidReceiver);
                    }
                 dsyslog("CAM %d: assigned to device %d", MasterSlotNumber(), Device->DeviceNumber() + 1);
                 }
              else {
                 CancelActivation();
                 dsyslog("CAM %d: unassigned from device %d", MasterSlotNumber(), OldDeviceNumber);
                 }
              }
           else
              return false;
           }
        return true;
        }
     }
  return false;
}

bool cCamSlot::Devices(cVector<int> &DeviceNumbers)
{
  cMutexLock MutexLock(&mutex);
  if (mtdHandler)
     return mtdHandler->Devices(DeviceNumbers);
  if (assignedDevice)
     DeviceNumbers.Append(assignedDevice->DeviceNumber());
  return DeviceNumbers.Size() > 0;
}

void cCamSlot::NewConnection(void)
{
  cMutexLock MutexLock(&mutex);
  for (int i = 1; i <= MAX_CONNECTIONS_PER_CAM_SLOT; i++) {
      if (!tc[i]) {
         tc[i] = new cCiTransportConnection(this, i);
         tc[i]->CreateConnection();
         return;
         }
      }
  esyslog("ERROR: CAM %d: can't create new transport connection!", slotNumber);
}

void cCamSlot::DeleteAllConnections(void)
{
  cMutexLock MutexLock(&mutex);
  for (int i = 1; i <= MAX_CONNECTIONS_PER_CAM_SLOT; i++) {
      delete tc[i];
      tc[i] = NULL;
      }
}

void cCamSlot::Process(cTPDU *TPDU)
{
  cMutexLock MutexLock(&mutex);
  if (TPDU) {
     int n = TPDU->Tcid();
     if (1 <= n && n <= MAX_CONNECTIONS_PER_CAM_SLOT) {
        if (tc[n])
           tc[n]->Process(TPDU);
        }
     }
  for (int i = 1; i <= MAX_CONNECTIONS_PER_CAM_SLOT; i++) {
      if (tc[i]) {
         if (!tc[i]->Process()) {
            Reset();
            return;
            }
         }
      }
  if (moduleCheckTimer.TimedOut()) {
     eModuleStatus ms = ModuleStatus();
     if (ms != lastModuleStatus) {
        switch (ms) {
          case msNone:
               dbgprotocol("Slot %d: no module present\n", slotNumber);
               isyslog("CAM %d: no module present", slotNumber);
               StopDecrypting();
               DeleteAllConnections();
               CancelActivation();
               if (mtdHandler)
                  mtdHandler->UnAssignAll();
               else
                  Assign(NULL);
               break;
          case msReset:
               dbgprotocol("Slot %d: module reset\n", slotNumber);
               isyslog("CAM %d: module reset", slotNumber);
               DeleteAllConnections();
               break;
          case msPresent:
               dbgprotocol("Slot %d: module present\n", slotNumber);
               isyslog("CAM %d: module present", slotNumber);
               break;
          case msReady:
               dbgprotocol("Slot %d: module ready\n", slotNumber);
               isyslog("CAM %d: module ready", slotNumber);
               NewConnection();
               resendPmt = true;
               break;
          default:
               esyslog("ERROR: unknown module status %d (%s)", ms, __FUNCTION__);
          }
        lastModuleStatus = ms;
        }
     moduleCheckTimer.Set(MODULE_CHECK_INTERVAL);
     }
  if (resendPmt && Ready()) {
     if (mtdHandler) {
        mtdHandler->StartDecrypting();
        resendPmt = false;
        }
     else if (caProgramList.Count())
        StartDecrypting();
     }
  processed.Broadcast();
}

cCiSession *cCamSlot::GetSessionByResourceId(uint32_t ResourceId)
{
  cMutexLock MutexLock(&mutex);
  return tc[1] ? tc[1]->GetSessionByResourceId(ResourceId) : NULL;
}

void cCamSlot::Write(cTPDU *TPDU)
{
  cMutexLock MutexLock(&mutex);
  if (ciAdapter && TPDU->Size()) {
     TPDU->Dump(SlotNumber(), true);
     ciAdapter->Write(TPDU->Buffer(), TPDU->Size());
     }
}

bool cCamSlot::Reset(void)
{
  cMutexLock MutexLock(&mutex);
  ChannelCamRelations.Reset(slotNumber);
  DeleteAllConnections();
  if (ciAdapter) {
     dbgprotocol("Slot %d: reset...", slotNumber);
     if (ciAdapter->Reset(slotIndex)) {
        resetTime = time(NULL);
        dbgprotocol("ok.\n");
        lastModuleStatus = msReset;
        return true;
        }
     dbgprotocol("failed!\n");
     }
  return false;
}

bool cCamSlot::CanActivate(void)
{
  return ModuleStatus() == msReady;
}

void cCamSlot::StartActivation(void)
{
  cMutexLock MutexLock(&mutex);
  if (!caActivationReceiver) {
     if (cDevice *d = Device()) {
        LOCK_CHANNELS_READ;
        if (const cChannel *Channel = Channels->GetByNumber(cDevice::CurrentChannel())) {
           caActivationReceiver = new cCaActivationReceiver(Channel, this);
           d->AttachReceiver(caActivationReceiver);
           dsyslog("CAM %d: activating on device %d with channel %d (%s)", SlotNumber(), d->DeviceNumber() + 1, Channel->Number(), Channel->Name());
           }
        }
     }
}

void cCamSlot::CancelActivation(void)
{
  cMutexLock MutexLock(&mutex);
  if (mtdHandler)
     mtdHandler->CancelActivation();
  else {
     delete caActivationReceiver;
     caActivationReceiver = NULL;
     }
}

bool cCamSlot::IsActivating(void)
{
  if (mtdHandler)
     return mtdHandler->IsActivating();
  return caActivationReceiver;
}

eModuleStatus cCamSlot::ModuleStatus(void)
{
  cMutexLock MutexLock(&mutex);
  eModuleStatus ms = ciAdapter ? ciAdapter->ModuleStatus(slotIndex) : msNone;
  if (resetTime) {
     if (ms <= msReset) {
        if (time(NULL) - resetTime < MODULE_RESET_TIMEOUT)
           return msReset;
        }
     resetTime = 0;
     }
  return ms;
}

const char *cCamSlot::GetCamName(void)
{
  cMutexLock MutexLock(&mutex);
  return tc[1] ? tc[1]->GetCamName() : NULL;
}

bool cCamSlot::Ready(void)
{
  cMutexLock MutexLock(&mutex);
  return ModuleStatus() == msNone || tc[1] && tc[1]->Ready();
}

bool cCamSlot::HasMMI(void)
{
  return GetSessionByResourceId(RI_MMI);
}

bool cCamSlot::HasUserIO(void)
{
  cMutexLock MutexLock(&mutex);
  return tc[1] && tc[1]->HasUserIO();
}

bool cCamSlot::EnterMenu(void)
{
  cMutexLock MutexLock(&mutex);
  cCiApplicationInformation *api = (cCiApplicationInformation *)GetSessionByResourceId(RI_APPLICATION_INFORMATION);
  return api ? api->EnterMenu() : false;
}

cCiMenu *cCamSlot::GetMenu(void)
{
  cMutexLock MutexLock(&mutex);
  cCiMMI *mmi = (cCiMMI *)GetSessionByResourceId(RI_MMI);
  if (mmi) {
     cCiMenu *Menu = mmi->Menu();
     if (Menu)
        Menu->mutex = &mutex;
     return Menu;
     }
  return NULL;
}

cCiEnquiry *cCamSlot::GetEnquiry(void)
{
  cMutexLock MutexLock(&mutex);
  cCiMMI *mmi = (cCiMMI *)GetSessionByResourceId(RI_MMI);
  if (mmi) {
     cCiEnquiry *Enquiry = mmi->Enquiry();
     if (Enquiry)
        Enquiry->mutex = &mutex;
     return Enquiry;
     }
  return NULL;
}

cCiCaPmtList::~cCiCaPmtList()
{
  for (int i = 0; i < caPmts.Size(); i++)
      delete caPmts[i];
}

cCiCaPmt *cCiCaPmtList::Add(uint8_t CmdId, int Source, int Transponder, int ProgramNumber, const int *CaSystemIds)
{
  cCiCaPmt *p = new cCiCaPmt(CmdId, Source, Transponder, ProgramNumber, CaSystemIds);
  caPmts.Append(p);
  return p;
}

void cCiCaPmtList::Del(cCiCaPmt *CaPmt)
{
  if (caPmts.RemoveElement(CaPmt))
     delete CaPmt;
}

bool cCamSlot::RepliesToQuery(void)
{
  cMutexLock MutexLock(&mutex);
  cCiConditionalAccessSupport *cas = (cCiConditionalAccessSupport *)GetSessionByResourceId(RI_CONDITIONAL_ACCESS_SUPPORT);
  return cas && cas->RepliesToQuery();
}

void cCamSlot::BuildCaPmts(uint8_t CmdId, cCiCaPmtList &CaPmtList, cMtdMapper *MtdMapper)
{
  cMutexLock MutexLock(&mutex);
  CaPmtList.caPmts.Clear();
  const int *CaSystemIds = GetCaSystemIds();
  if (CaSystemIds && *CaSystemIds) {
     if (caProgramList.Count()) {
        for (cCiCaProgramData *p = caProgramList.First(); p; p = caProgramList.Next(p)) {
            if (p->modified || resendPmt) {
               bool Active = p->Active();
               cCiCaPmt *CaPmt = CaPmtList.Add(Active ? CmdId : CPCI_NOT_SELECTED, source, transponder, p->programNumber, CaSystemIds);
               for (cCiCaPidData *q = p->pidList.First(); q; q = p->pidList.Next(q)) {
                   if (q->active)
                      CaPmt->AddPid(q->pid, q->streamType);
                   }
               if (caPidReceiver) {
                  int CaPids[MAXRECEIVEPIDS + 1];
                  if (GetCaPids(source, transponder, p->programNumber, CaSystemIds, MAXRECEIVEPIDS + 1, CaPids) > 0) {
                     if (Active)
                        caPidReceiver->AddPids(CaPids);
                     else {
                        KeepSharedCaPids(p->programNumber, CaSystemIds, CaPids);
                        caPidReceiver->DelPids(CaPids);
                        }
                     }
                  }
               if (RepliesToQuery())
                  CaPmt->SetListManagement(Active ? CPLM_ADD : CPLM_UPDATE);
               if (MtdMapper)
                  CaPmt->MtdMapPids(MtdMapper);
               p->modified = false;
               }
            }
        }
     else if (CmdId == CPCI_NOT_SELECTED)
        CaPmtList.Add(CmdId, 0, 0, 0, NULL);
     }
}

void cCamSlot::KeepSharedCaPids(int ProgramNumber, const int *CaSystemIds, int *CaPids)
{
  int numPids = 0;
  int *pCaPids = CaPids;
  while (*pCaPids) {
        numPids++;
        pCaPids++;
        }
  if (numPids <= 0)
     return;
  int CaPids2[MAXRECEIVEPIDS + 1];
  for (cCiCaProgramData *p = caProgramList.First(); p; p = caProgramList.Next(p)) {
      if (p->Active()) {
         if (GetCaPids(source, transponder, p->programNumber, CaSystemIds, MAXRECEIVEPIDS + 1, CaPids2) > 0) {
            int *pCaPids2 = CaPids2;
            while (*pCaPids2) {
                  pCaPids = CaPids;
                  while (*pCaPids) {
                        if (*pCaPids == *pCaPids2) {
                           dsyslog("CAM %d: keeping shared CA pid %d", SlotNumber(), *pCaPids);
                           // To remove *pCaPids from CaPids we overwrite it with the last valie in the list, and then strip the last value:
                           *pCaPids = CaPids[numPids - 1];
                           numPids--;
                           CaPids[numPids] = 0;
                           if (numPids <= 0)
                              return;
                           }
                        else
                           pCaPids++;
                        }
                  pCaPids2++;
                  }
            }
         }
      }
}

void cCamSlot::SendCaPmts(cCiCaPmtList &CaPmtList)
{
  cMutexLock MutexLock(&mutex);
  cCiConditionalAccessSupport *cas = (cCiConditionalAccessSupport *)GetSessionByResourceId(RI_CONDITIONAL_ACCESS_SUPPORT);
  if (cas) {
     for (int i = 0; i < CaPmtList.caPmts.Size(); i++)
         cas->SendPMT(CaPmtList.caPmts[i]);
     }
  resendPmt = false;
}

void cCamSlot::SendCaPmt(uint8_t CmdId)
{
  cMutexLock MutexLock(&mutex);
  cCiCaPmtList CaPmtList;
  BuildCaPmts(CmdId, CaPmtList);
  SendCaPmts(CaPmtList);
}

void cCamSlot::MtdEnable(void)
{
  mtdAvailable = true;
}

void cCamSlot::MtdActivate(bool On)
{
  if (McdAvailable() && MtdAvailable()) {
     if (On) {
        if (!mtdHandler) {
           dsyslog("CAM %d: activating MTD support", SlotNumber());
           mtdHandler = new cMtdHandler;
           }
        }
     else if (mtdHandler) {
        dsyslog("CAM %d: deactivating MTD support", SlotNumber());
        delete mtdHandler;
        mtdHandler = NULL;
        }
     }
}

int cCamSlot::MtdPutData(uchar *Data, int Count)
{
  return mtdHandler->Put(Data, Count);
}

const int *cCamSlot::GetCaSystemIds(void)
{
  cMutexLock MutexLock(&mutex);
  cCiConditionalAccessSupport *cas = (cCiConditionalAccessSupport *)GetSessionByResourceId(RI_CONDITIONAL_ACCESS_SUPPORT);
  return cas ? cas->GetCaSystemIds() : NULL;
}

int cCamSlot::Priority(void)
{
  if (mtdHandler)
     return mtdHandler->Priority();
  cDevice *d = Device();
  return d ? d->Priority() : IDLEPRIORITY;
}

bool cCamSlot::ProvidesCa(const int *CaSystemIds)
{
  cMutexLock MutexLock(&mutex);
  cCiConditionalAccessSupport *cas = (cCiConditionalAccessSupport *)GetSessionByResourceId(RI_CONDITIONAL_ACCESS_SUPPORT);
  if (cas) {
     for (const int *ids = cas->GetCaSystemIds(); ids && *ids; ids++) {
         for (const int *id = CaSystemIds; *id; id++) {
             if (*id == *ids)
                return true;
             }
         }
     }
  return false;
}

void cCamSlot::AddPid(int ProgramNumber, int Pid, int StreamType)
{
  cMutexLock MutexLock(&mutex);
  cCiCaProgramData *ProgramData = NULL;
  for (cCiCaProgramData *p = caProgramList.First(); p; p = caProgramList.Next(p)) {
      if (p->programNumber == ProgramNumber) {
         ProgramData = p;
         for (cCiCaPidData *q = p->pidList.First(); q; q = p->pidList.Next(q)) {
             if (q->pid == Pid)
                return;
             }
         }
      }
  if (!ProgramData)
     caProgramList.Add(ProgramData = new cCiCaProgramData(ProgramNumber));
  ProgramData->pidList.Add(new cCiCaPidData(Pid, StreamType));
}

void cCamSlot::SetPid(int Pid, bool Active)
{
  if (caPidReceiver && caPidReceiver->HandlingPid())
     return;
  cMutexLock MutexLock(&mutex);
  for (cCiCaProgramData *p = caProgramList.First(); p; p = caProgramList.Next(p)) {
      for (cCiCaPidData *q = p->pidList.First(); q; q = p->pidList.Next(q)) {
          if (q->pid == Pid) {
             if (q->active != Active) {
                q->active = Active;
                p->modified = true;
                }
             return;
             }
          }
      }
}

// see ISO/IEC 13818-1
#define STREAM_TYPE_VIDEO    0x02
#define STREAM_TYPE_AUDIO    0x04
#define STREAM_TYPE_PRIVATE  0x06

void cCamSlot::AddChannel(const cChannel *Channel)
{
  cMutexLock MutexLock(&mutex);
  if (source != Channel->Source() || transponder != Channel->Transponder())
     StopDecrypting();
  source = Channel->Source();
  transponder = Channel->Transponder();
  if (Channel->Ca() >= CA_ENCRYPTED_MIN) {
     AddPid(Channel->Sid(), Channel->Vpid(), STREAM_TYPE_VIDEO);
     for (const int *Apid = Channel->Apids(); *Apid; Apid++)
         AddPid(Channel->Sid(), *Apid, STREAM_TYPE_AUDIO);
     for (const int *Dpid = Channel->Dpids(); *Dpid; Dpid++)
         AddPid(Channel->Sid(), *Dpid, STREAM_TYPE_PRIVATE);
     for (const int *Spid = Channel->Spids(); *Spid; Spid++)
         AddPid(Channel->Sid(), *Spid, STREAM_TYPE_PRIVATE);
     }
}

#define QUERY_REPLY_WAIT  100 // ms to wait between checks for a reply

bool cCamSlot::CanDecrypt(const cChannel *Channel, cMtdMapper *MtdMapper)
{
  if (Channel->Ca() < CA_ENCRYPTED_MIN)
     return true; // channel not encrypted
  if (!IsDecrypting())
     return true; // any CAM can decrypt at least one channel
  cMutexLock MutexLock(&mutex);
  cCiConditionalAccessSupport *cas = (cCiConditionalAccessSupport *)GetSessionByResourceId(RI_CONDITIONAL_ACCESS_SUPPORT);
  if (cas && cas->RepliesToQuery()) {
     cCiCaPmt CaPmt(CPCI_QUERY, Channel->Source(), Channel->Transponder(), Channel->Sid(), GetCaSystemIds());
     CaPmt.SetListManagement(CPLM_ADD); // WORKAROUND: CPLM_ONLY doesn't work with Alphacrypt 3.09 (deletes existing CA_PMTs)
     CaPmt.AddPid(Channel->Vpid(), STREAM_TYPE_VIDEO);
     for (const int *Apid = Channel->Apids(); *Apid; Apid++)
         CaPmt.AddPid(*Apid, STREAM_TYPE_AUDIO);
     for (const int *Dpid = Channel->Dpids(); *Dpid; Dpid++)
         CaPmt.AddPid(*Dpid, STREAM_TYPE_PRIVATE);
     for (const int *Spid = Channel->Spids(); *Spid; Spid++)
         CaPmt.AddPid(*Spid, STREAM_TYPE_PRIVATE);
     if (MtdMapper)
        CaPmt.MtdMapPids(MtdMapper);
     cas->SendPMT(&CaPmt);
     cTimeMs Timeout(QUERY_REPLY_TIMEOUT);
     do {
        processed.TimedWait(mutex, QUERY_REPLY_WAIT);
        if ((cas = (cCiConditionalAccessSupport *)GetSessionByResourceId(RI_CONDITIONAL_ACCESS_SUPPORT)) != NULL) { // must re-fetch it, there might have been a reset
           if (cas->ReceivedReply())
              return cas->CanDecrypt();
           }
        else
           return false;
        } while (!Timeout.TimedOut());
     dsyslog("CAM %d: didn't reply to QUERY", SlotNumber());
     }
  return false;
}

void cCamSlot::StartDecrypting(void)
{
  SendCaPmt(CPCI_OK_DESCRAMBLING);
}

void cCamSlot::StopDecrypting(void)
{
  cMutexLock MutexLock(&mutex);
  if (mtdHandler) {
     mtdHandler->StopDecrypting();
     return;
     }
  if (caProgramList.Count()) {
     caProgramList.Clear();
     if (!dynamic_cast<cMtdCamSlot *>(this) || !MasterSlot()->IsDecrypting())
        SendCaPmt(CPCI_NOT_SELECTED);
     }
}

bool cCamSlot::IsDecrypting(void)
{
  cMutexLock MutexLock(&mutex);
  if (mtdHandler)
     return mtdHandler->IsDecrypting();
  if (caProgramList.Count()) {
     for (cCiCaProgramData *p = caProgramList.First(); p; p = caProgramList.Next(p)) {
         if (p->modified)
            return true; // any modifications need to be processed before we can assume it's no longer decrypting
         for (cCiCaPidData *q = p->pidList.First(); q; q = p->pidList.Next(q)) {
             if (q->active)
                return true;
             }
         }
     }
  return false;
}

uchar *cCamSlot::Decrypt(uchar *Data, int &Count)
{
  if (Data)
     Count = TS_SIZE;
  return Data;
}

bool cCamSlot::TsPostProcess(uchar *Data)
{
  return tc[1] ? tc[1]->TsPostProcess(Data) : false;
}

bool cCamSlot::Inject(uchar *Data, int Count)
{
  return true;
}

void cCamSlot::InjectEit(int Sid)
{
  cEitGenerator Eit(Sid);
  Inject(Eit.Data(), Eit.Length());
}

// --- cCamSlots -------------------------------------------------------------

cCamSlots CamSlots;

int cCamSlots::NumReadyMasterSlots(void)
{
  int n = 0;
  for (cCamSlot *CamSlot = CamSlots.First(); CamSlot; CamSlot = CamSlots.Next(CamSlot)) {
      if (CamSlot->IsMasterSlot() && CamSlot->ModuleStatus() == msReady)
         n++;
      }
  return n;
}

bool cCamSlots::WaitForAllCamSlotsReady(int Timeout)
{
  bool ready = true;
  for (time_t t0 = time(NULL); time(NULL) - t0 < Timeout; ) {
      ready = true;
      for (cCamSlot *CamSlot = CamSlots.First(); CamSlot; CamSlot = CamSlots.Next(CamSlot)) {
          if (!CamSlot->Ready()) {
             ready = false;
             cCondWait::SleepMs(100);
             }
          }
      if (ready)
         break;
      }
  for (cCamSlot *CamSlot = CamSlots.First(); CamSlot; CamSlot = CamSlots.Next(CamSlot))
      dsyslog("CAM %d: %sready, %s", CamSlot->SlotNumber(), CamSlot->Ready() ? "" : "not ", CamSlot->IsMasterSlot() ? *cString::sprintf("master (%s)", CamSlot->GetCamName() ? CamSlot->GetCamName() : "empty") : *cString::sprintf("slave of CAM %d", CamSlot->MasterSlotNumber()));
  return ready;
}

// --- cChannelCamRelation ---------------------------------------------------

#define CAM_CHECKED_TIMEOUT  15 // seconds before a CAM that has been checked for a particular channel will be checked again

class cChannelCamRelation : public cListObject {
private:
  tChannelID channelID;
  uint32_t camSlotsChecked;
  uint32_t camSlotsDecrypt;
  time_t lastChecked;
public:
  cChannelCamRelation(tChannelID ChannelID);
  bool TimedOut(void);
  tChannelID ChannelID(void) { return channelID; }
  bool CamChecked(int CamSlotNumber);
  bool CamDecrypt(int CamSlotNumber);
  void SetChecked(int CamSlotNumber);
  void SetDecrypt(int CamSlotNumber);
  void ClrChecked(int CamSlotNumber);
  void ClrDecrypt(int CamSlotNumber);
  };

cChannelCamRelation::cChannelCamRelation(tChannelID ChannelID)
{
  channelID = ChannelID;
  camSlotsChecked = 0;
  camSlotsDecrypt = 0;
  lastChecked = 0;
}

bool cChannelCamRelation::TimedOut(void)
{
  return !camSlotsDecrypt && time(NULL) - lastChecked > CAM_CHECKED_TIMEOUT;
}

bool cChannelCamRelation::CamChecked(int CamSlotNumber)
{
  if (lastChecked && time(NULL) - lastChecked > CAM_CHECKED_TIMEOUT) {
     lastChecked = 0;
     camSlotsChecked = 0;
     }
  return camSlotsChecked & (1 << (CamSlotNumber - 1));
}

bool cChannelCamRelation::CamDecrypt(int CamSlotNumber)
{
  return camSlotsDecrypt & (1 << (CamSlotNumber - 1));
}

void cChannelCamRelation::SetChecked(int CamSlotNumber)
{
  camSlotsChecked |= (1 << (CamSlotNumber - 1));
  lastChecked = time(NULL);
  ClrDecrypt(CamSlotNumber);
}

void cChannelCamRelation::SetDecrypt(int CamSlotNumber)
{
  camSlotsDecrypt |= (1 << (CamSlotNumber - 1));
  ClrChecked(CamSlotNumber);
}

void cChannelCamRelation::ClrChecked(int CamSlotNumber)
{
  camSlotsChecked &= ~(1 << (CamSlotNumber - 1));
  lastChecked = 0;
}

void cChannelCamRelation::ClrDecrypt(int CamSlotNumber)
{
  camSlotsDecrypt &= ~(1 << (CamSlotNumber - 1));
}

// --- cChannelCamRelations --------------------------------------------------

#define MAX_CAM_NUMBER 32
#define CHANNEL_CAM_RELATIONS_CLEANUP_INTERVAL 3600 // seconds between cleanups

cChannelCamRelations ChannelCamRelations;

cChannelCamRelations::cChannelCamRelations(void)
{
  lastCleanup = time(NULL);
}

void cChannelCamRelations::Cleanup(void)
{
  cMutexLock MutexLock(&mutex);
  if (time(NULL) - lastCleanup > CHANNEL_CAM_RELATIONS_CLEANUP_INTERVAL) {
     for (cChannelCamRelation *ccr = First(); ccr; ) {
         cChannelCamRelation *c = ccr;
         ccr = Next(ccr);
         if (c->TimedOut())
            Del(c);
         }
     lastCleanup = time(NULL);
     }
}

cChannelCamRelation *cChannelCamRelations::GetEntry(tChannelID ChannelID)
{
  cMutexLock MutexLock(&mutex);
  Cleanup();
  for (cChannelCamRelation *ccr = First(); ccr; ccr = Next(ccr)) {
      if (ccr->ChannelID() == ChannelID)
         return ccr;
      }
  return NULL;
}

cChannelCamRelation *cChannelCamRelations::AddEntry(tChannelID ChannelID)
{
  cMutexLock MutexLock(&mutex);
  cChannelCamRelation *ccr = GetEntry(ChannelID);
  if (!ccr)
     Add(ccr = new cChannelCamRelation(ChannelID));
  return ccr;
}

void cChannelCamRelations::Reset(int CamSlotNumber)
{
  cMutexLock MutexLock(&mutex);
  for (cChannelCamRelation *ccr = First(); ccr; ccr = Next(ccr)) {
      ccr->ClrChecked(CamSlotNumber);
      ccr->ClrDecrypt(CamSlotNumber);
      }
}

bool cChannelCamRelations::CamChecked(tChannelID ChannelID, int CamSlotNumber)
{
  cMutexLock MutexLock(&mutex);
  cChannelCamRelation *ccr = GetEntry(ChannelID);
  return ccr ? ccr->CamChecked(CamSlotNumber) : false;
}

bool cChannelCamRelations::CamDecrypt(tChannelID ChannelID, int CamSlotNumber)
{
  cMutexLock MutexLock(&mutex);
  cChannelCamRelation *ccr = GetEntry(ChannelID);
  return ccr ? ccr->CamDecrypt(CamSlotNumber) : false;
}

void cChannelCamRelations::SetChecked(tChannelID ChannelID, int CamSlotNumber)
{
  cMutexLock MutexLock(&mutex);
  cChannelCamRelation *ccr = AddEntry(ChannelID);
  if (ccr)
     ccr->SetChecked(CamSlotNumber);
}

void cChannelCamRelations::SetDecrypt(tChannelID ChannelID, int CamSlotNumber)
{
  cMutexLock MutexLock(&mutex);
  cChannelCamRelation *ccr = AddEntry(ChannelID);
  if (ccr)
     ccr->SetDecrypt(CamSlotNumber);
}

void cChannelCamRelations::ClrChecked(tChannelID ChannelID, int CamSlotNumber)
{
  cMutexLock MutexLock(&mutex);
  cChannelCamRelation *ccr = GetEntry(ChannelID);
  if (ccr)
     ccr->ClrChecked(CamSlotNumber);
}

void cChannelCamRelations::ClrDecrypt(tChannelID ChannelID, int CamSlotNumber)
{
  cMutexLock MutexLock(&mutex);
  cChannelCamRelation *ccr = GetEntry(ChannelID);
  if (ccr)
     ccr->ClrDecrypt(CamSlotNumber);
}

void cChannelCamRelations::Load(const char *FileName)
{
  cMutexLock MutexLock(&mutex);
  fileName = FileName;
  if (access(fileName, R_OK) == 0) {
     dsyslog("loading %s", *fileName);
     if (FILE *f = fopen(fileName, "r")) {
        cReadLine ReadLine;
        char *s;
        while ((s = ReadLine.Read(f)) != NULL) {
              if (char *p = strchr(s, ' ')) {
                 *p = 0;
                 if (*++p) {
                    tChannelID ChannelID = tChannelID::FromString(s);
                    if (ChannelID.Valid()) {
                       char *q;
                       char *strtok_next;
                       while ((q = strtok_r(p, " ", &strtok_next)) != NULL) {
                             int CamSlotNumber = atoi(q);
                             if (CamSlotNumber >= 1 && CamSlotNumber <= MAX_CAM_NUMBER)
                                SetDecrypt(ChannelID, CamSlotNumber);
                             p = NULL;
                             }
                       }
                    }
                 }
              }
        fclose(f);
        }
     else
        LOG_ERROR_STR(*fileName);
     }
}

void cChannelCamRelations::Save(void)
{
  if (!*fileName)
     return;
  cMutexLock MutexLock(&mutex);
  struct stat st;
  if (stat(fileName, &st) == 0) {
     if ((st.st_mode & S_IWUSR) == 0) {
        dsyslog("not saving %s (file is read-only)", *fileName);
        return;
        }
     }
  dsyslog("saving %s", *fileName);
  cSafeFile f(fileName);
  if (f.Open()) {
     for (cChannelCamRelation *ccr = First(); ccr; ccr = Next(ccr)) {
         if (ccr->ChannelID().Valid()) {
            cString s;
            for (int i = 1; i <= MAX_CAM_NUMBER; i++) {
                if (ccr->CamDecrypt(i))
                   s = cString::sprintf("%s%s%d", *s ? *s : "", *s ? " " : "", i);
                }
            if (*s)
               fprintf(f, "%s %s\n", *ccr->ChannelID().ToString(), *s);
            }
         }
     f.Close();
     }
  else
     LOG_ERROR_STR(*fileName);
}
