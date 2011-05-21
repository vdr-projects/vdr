/*
 * ci.c: Common Interface
 *
 * See the main source file 'vdr.c' for copyright information and
 * how to reach the author.
 *
 * $Id: ci.c 2.7 2011/05/21 15:21:33 kls Exp $
 */

#include "ci.h"
#include <ctype.h>
#include <linux/dvb/ca.h>
#include <malloc.h>
#include <netinet/in.h>
#include <poll.h>
#include <string.h>
#include <sys/ioctl.h>
#include <time.h>
#include <unistd.h>
#include "device.h"
#include "pat.h"
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
///< \return Returns a pointer to the first byte after the length and
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
///< \return Returns a pointer to the first byte after the length.
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
///< \return Returns a pointer to a newly allocated string.
{
  // Some CAMs send funny characters at the beginning of strings.
  // Let's just skip them:
  while (Length > 0 && (*Data == ' ' || *Data == 0x05 || *Data == 0x96 || *Data == 0x97)) {
        Length--;
        Data++;
        }
  char *s = MALLOC(char, Length + 1);
  strncpy(s, (char *)Data, Length);
  s[Length] = 0;
  // The character 0x8A is used as newline, so let's put a real '\n' in there:
  strreplace(s, 0x8A, '\n');
  return s;
}

static char *GetString(int &Length, const uint8_t **Data)
///< Gets the string at Data.
///< \return Returns a pointer to a newly allocated string, or NULL in case of error.
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

// --- cTPDU -----------------------------------------------------------------

#define MAX_TPDU_SIZE  2048
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
  cCamSlot *camSlot;
  uint8_t tcid;
  eState state;
  bool createConnectionRequested;
  bool deleteConnectionRequested;
  bool hasUserIO;
  cTimeMs alive;
  cTimeMs timer;
  cCiSession *sessions[MAX_SESSIONS_PER_TC + 1]; // session numbering starts with 1
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

class cCiSession {
private:
  uint16_t sessionId;
  uint32_t resourceId;
  cCiTransportConnection *tc;
protected:
  int GetTag(int &Length, const uint8_t **Data);
  const uint8_t *GetData(const uint8_t *Data, int &Length);
  void SendData(int Tag, int Length = 0, const uint8_t *Data = NULL);
  cCiTransportConnection *Tc(void) { return tc; }
public:
  cCiSession(uint16_t SessionId, uint32_t ResourceId, cCiTransportConnection *Tc);
  virtual ~cCiSession();
  uint16_t SessionId(void) { return sessionId; }
  uint32_t ResourceId(void) { return resourceId; }
  virtual bool HasUserIO(void) { return false; }
  virtual void Process(int Length = 0, const uint8_t *Data = NULL);
  };

cCiSession::cCiSession(uint16_t SessionId, uint32_t ResourceId, cCiTransportConnection *Tc)
{
  sessionId = SessionId;
  resourceId = ResourceId;
  tc = Tc;
}

cCiSession::~cCiSession()
{
}

int cCiSession::GetTag(int &Length, const uint8_t **Data)
///< Gets the tag at Data.
///< \return Returns the actual tag, or AOT_NONE in case of error.
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
  uint8_t buffer[2048];
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
     memcpy(p, Data, Length);
     p += Length;
     tc->SendData(p - buffer, buffer);
     }
  else
     esyslog("ERROR: CAM %d: data length (%d) exceeds buffer size", Tc()->CamSlot()->SlotNumber(), Length);
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
  dbgprotocol("Slot %d: new Resource Manager (session id %d)\n", Tc->CamSlot()->SlotNumber(), SessionId);
  state = 0;
}

void cCiResourceManager::Process(int Length, const uint8_t *Data)
{
  if (Data) {
     int Tag = GetTag(Length, &Data);
     switch (Tag) {
       case AOT_PROFILE_ENQ: {
            dbgprotocol("Slot %d: <== Profile Enquiry (%d)\n", Tc()->CamSlot()->SlotNumber(), SessionId());
            uint32_t resources[] = { htonl(RI_RESOURCE_MANAGER),
                                     htonl(RI_APPLICATION_INFORMATION),
                                     htonl(RI_CONDITIONAL_ACCESS_SUPPORT),
                                     htonl(RI_DATE_TIME),
                                     htonl(RI_MMI)
                                   };
            dbgprotocol("Slot %d: ==> Profile (%d)\n", Tc()->CamSlot()->SlotNumber(), SessionId());
            SendData(AOT_PROFILE, sizeof(resources), (uint8_t*)resources);
            state = 3;
            }
            break;
       case AOT_PROFILE: {
            dbgprotocol("Slot %d: <== Profile (%d)\n", Tc()->CamSlot()->SlotNumber(), SessionId());
            if (state == 1) {
               int l = 0;
               const uint8_t *d = GetData(Data, l);
               if (l > 0 && d)
                  esyslog("ERROR: CAM %d: resource manager: unexpected data", Tc()->CamSlot()->SlotNumber());
               dbgprotocol("Slot %d: ==> Profile Change (%d)\n", Tc()->CamSlot()->SlotNumber(), SessionId());
               SendData(AOT_PROFILE_CHANGE);
               state = 2;
               }
            else {
               esyslog("ERROR: CAM %d: resource manager: unexpected tag %06X in state %d", Tc()->CamSlot()->SlotNumber(), Tag, state);
               }
            }
            break;
       default: esyslog("ERROR: CAM %d: resource manager: unknown tag %06X", Tc()->CamSlot()->SlotNumber(), Tag);
       }
     }
  else if (state == 0) {
     dbgprotocol("Slot %d: ==> Profile Enq (%d)\n", Tc()->CamSlot()->SlotNumber(), SessionId());
     SendData(AOT_PROFILE_ENQ);
     state = 1;
     }
}

// --- cCiApplicationInformation ---------------------------------------------

class cCiApplicationInformation : public cCiSession {
private:
  int state;
  uint8_t applicationType;
  uint16_t applicationManufacturer;
  uint16_t manufacturerCode;
  char *menuString;
public:
  cCiApplicationInformation(uint16_t SessionId, cCiTransportConnection *Tc);
  virtual ~cCiApplicationInformation();
  virtual void Process(int Length = 0, const uint8_t *Data = NULL);
  bool EnterMenu(void);
  const char *GetMenuString(void) { return menuString; }
  };

cCiApplicationInformation::cCiApplicationInformation(uint16_t SessionId, cCiTransportConnection *Tc)
:cCiSession(SessionId, RI_APPLICATION_INFORMATION, Tc)
{
  dbgprotocol("Slot %d: new Application Information (session id %d)\n", Tc->CamSlot()->SlotNumber(), SessionId);
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
            dbgprotocol("Slot %d: <== Application Info (%d)\n", Tc()->CamSlot()->SlotNumber(), SessionId());
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
            isyslog("CAM %d: %s, %02X, %04X, %04X", Tc()->CamSlot()->SlotNumber(), menuString, applicationType, applicationManufacturer, manufacturerCode);
            state = 2;
            }
            break;
       default: esyslog("ERROR: CAM %d: application information: unknown tag %06X", Tc()->CamSlot()->SlotNumber(), Tag);
       }
     }
  else if (state == 0) {
     dbgprotocol("Slot %d: ==> Application Info Enq (%d)\n", Tc()->CamSlot()->SlotNumber(), SessionId());
     SendData(AOT_APPLICATION_INFO_ENQ);
     state = 1;
     }
}

bool cCiApplicationInformation::EnterMenu(void)
{
  if (state == 2) {
     dbgprotocol("Slot %d: ==> Enter Menu (%d)\n", Tc()->CamSlot()->SlotNumber(), SessionId());
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

class cCiCaPmt : public cListObject {
  friend class cCiConditionalAccessSupport;
private:
  uint8_t cmdId;
  int length;
  int esInfoLengthPos;
  uint8_t capmt[2048]; ///< XXX is there a specified maximum?
  int source;
  int transponder;
  int programNumber;
  int caSystemIds[MAXCASYSTEMIDS + 1]; // list is zero terminated!
  void AddCaDescriptors(int Length, const uint8_t *Data);
public:
  cCiCaPmt(uint8_t CmdId, int Source, int Transponder, int ProgramNumber, const int *CaSystemIds);
  uint8_t CmdId(void) { return cmdId; }
  void SetListManagement(uint8_t ListManagement);
  uint8_t ListManagement(void) { return capmt[0]; }
  void AddPid(int Pid, uint8_t StreamType);
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
  uint8_t caDescriptors[512];
  int caDescriptorsLength = GetCaDescriptors(source, transponder, programNumber, caSystemIds, sizeof(caDescriptors), caDescriptors, 0);
  length = 0;
  capmt[length++] = CPLM_ONLY;
  capmt[length++] = (ProgramNumber >> 8) & 0xFF;
  capmt[length++] =  ProgramNumber       & 0xFF;
  capmt[length++] = 0x01; // version_number, current_next_indicator - apparently vn doesn't matter, but cni must be 1
  esInfoLengthPos = length;
  capmt[length++] = 0x00; // program_info_length H (at program level)
  capmt[length++] = 0x00; // program_info_length L
  AddCaDescriptors(caDescriptorsLength, caDescriptors);
}

void cCiCaPmt::SetListManagement(uint8_t ListManagement)
{
  capmt[0] = ListManagement;
}

void cCiCaPmt::AddPid(int Pid, uint8_t StreamType)
{
  if (Pid) {
     uint8_t caDescriptors[512];
     int caDescriptorsLength = GetCaDescriptors(source, transponder, programNumber, caSystemIds, sizeof(caDescriptors), caDescriptors, Pid);
     //XXX buffer overflow check???
     capmt[length++] = StreamType;
     capmt[length++] = (Pid >> 8) & 0xFF;
     capmt[length++] =  Pid       & 0xFF;
     esInfoLengthPos = length;
     capmt[length++] = 0x00; // ES_info_length H (at ES level)
     capmt[length++] = 0x00; // ES_info_length L
     AddCaDescriptors(caDescriptorsLength, caDescriptors);
     }
}

void cCiCaPmt::AddCaDescriptors(int Length, const uint8_t *Data)
{
  if (esInfoLengthPos) {
     if (length + Length < int(sizeof(capmt))) {
        if (Length || cmdId == CPCI_QUERY) {
           capmt[length++] = cmdId;
           memcpy(capmt + length, Data, Length);
           length += Length;
           int l = length - esInfoLengthPos - 2;
           capmt[esInfoLengthPos]     = (l >> 8) & 0xFF;
           capmt[esInfoLengthPos + 1] =  l       & 0xFF;
           }
        }
     else
        esyslog("ERROR: buffer overflow in CA descriptor");
     esInfoLengthPos = 0;
     }
  else
     esyslog("ERROR: adding CA descriptor without Pid!");
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

#define QUERY_WAIT_TIME      1000 // ms to wait before sending a query
#define QUERY_REPLY_TIMEOUT  2000 // ms to wait for a reply to a query

class cCiConditionalAccessSupport : public cCiSession {
private:
  int state;
  int numCaSystemIds;
  int caSystemIds[MAXCASYSTEMIDS + 1]; // list is zero terminated!
  bool repliesToQuery;
  cTimeMs timer;
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
  dbgprotocol("Slot %d: new Conditional Access Support (session id %d)\n", Tc->CamSlot()->SlotNumber(), SessionId);
  state = 0; // inactive
  caSystemIds[numCaSystemIds = 0] = 0;
  repliesToQuery = false;
}

void cCiConditionalAccessSupport::Process(int Length, const uint8_t *Data)
{
  if (Data) {
     int Tag = GetTag(Length, &Data);
     switch (Tag) {
       case AOT_CA_INFO: {
            dbgprotocol("Slot %d: <== Ca Info (%d)", Tc()->CamSlot()->SlotNumber(), SessionId());
            numCaSystemIds = 0;
            int l = 0;
            const uint8_t *d = GetData(Data, l);
            while (l > 1) {
                  uint16_t id = ((uint16_t)(*d) << 8) | *(d + 1);
                  dbgprotocol(" %04X", id);
                  d += 2;
                  l -= 2;
                  if (numCaSystemIds < MAXCASYSTEMIDS)
                     caSystemIds[numCaSystemIds++] = id;
                  else {
                     esyslog("ERROR: CAM %d: too many CA system IDs!", Tc()->CamSlot()->SlotNumber());
                     break;
                     }
                  }
            caSystemIds[numCaSystemIds] = 0;
            dbgprotocol("\n");
            if (state == 1) {
               timer.Set(QUERY_WAIT_TIME); // WORKAROUND: Alphacrypt 3.09 doesn't reply to QUERY immediately after reset
               state = 2; // got ca info
               }
            }
            break;
       case AOT_CA_PMT_REPLY: {
            dbgprotocol("Slot %d: <== Ca Pmt Reply (%d)", Tc()->CamSlot()->SlotNumber(), SessionId());
            if (!repliesToQuery) {
               dsyslog("CAM %d: replies to QUERY - multi channel decryption possible", Tc()->CamSlot()->SlotNumber());
               repliesToQuery = true;
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
       default: esyslog("ERROR: CAM %d: conditional access support: unknown tag %06X", Tc()->CamSlot()->SlotNumber(), Tag);
       }
     }
  else if (state == 0) {
     dbgprotocol("Slot %d: ==> Ca Info Enq (%d)\n", Tc()->CamSlot()->SlotNumber(), SessionId());
     SendData(AOT_CA_INFO_ENQ);
     state = 1; // enquired ca info
     }
  else if (state == 2 && timer.TimedOut()) {
     cCiCaPmt CaPmt(CPCI_QUERY, 0, 0, 0, NULL);
     SendPMT(&CaPmt);
     timer.Set(QUERY_REPLY_TIMEOUT);
     state = 3; // waiting for reply
     }
  else if (state == 3 && timer.TimedOut()) {
     dsyslog("CAM %d: doesn't reply to QUERY - only a single channel can be decrypted", Tc()->CamSlot()->SlotNumber());
     state = 4; // normal operation
     }
}

void cCiConditionalAccessSupport::SendPMT(cCiCaPmt *CaPmt)
{
  if (CaPmt && state >= 2) {
     dbgprotocol("Slot %d: ==> Ca Pmt (%d) %d %d\n", Tc()->CamSlot()->SlotNumber(), SessionId(), CaPmt->ListManagement(), CaPmt->CmdId());
     SendData(AOT_CA_PMT, CaPmt->length, CaPmt->capmt);
     state = 4; // sent ca pmt
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
  dbgprotocol("Slot %d: new Date Time (session id %d)\n", Tc->CamSlot()->SlotNumber(), SessionId);
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
#define DEC2BCD(d) (((d / 10) << 4) + (d % 10))
     struct tTime { uint16_t mjd; uint8_t h, m, s; short offset; };
     tTime T = { mjd : htons(MJD), h : DEC2BCD(tm_gmt.tm_hour), m : DEC2BCD(tm_gmt.tm_min), s : DEC2BCD(tm_gmt.tm_sec), offset : htons(tm_loc.tm_gmtoff / 60) };
     bool OldDumpTPDUDataTransfer = DumpTPDUDataTransfer;
     DumpTPDUDataTransfer &= DumpDateTime;
     if (DumpDateTime)
        dbgprotocol("Slot %d: ==> Date Time (%d)\n", Tc()->CamSlot()->SlotNumber(), SessionId());
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
            dbgprotocol("Slot %d: <== Date Time Enq (%d), interval = %d\n", Tc()->CamSlot()->SlotNumber(), SessionId(), interval);
            lastTime = time(NULL);
            SendDateTime();
            }
            break;
       default: esyslog("ERROR: CAM %d: date time: unknown tag %06X", Tc()->CamSlot()->SlotNumber(), Tag);
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
  dbgprotocol("Slot %d: new MMI (session id %d)\n", Tc->CamSlot()->SlotNumber(), SessionId);
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
///< \return Returns a pointer to a newly allocated string, or NULL in case of error.
///< Upon return Length and Data represent the remaining data after the text has been skipped.
{
  int Tag = GetTag(Length, Data);
  if (Tag == AOT_TEXT_LAST) {
     char *s = GetString(Length, Data);
     dbgprotocol("Slot %d: <== Text Last (%d) '%s'\n", Tc()->CamSlot()->SlotNumber(), SessionId(), s);
     return s;
     }
  else
     esyslog("ERROR: CAM %d: MMI: unexpected text tag: %06X", Tc()->CamSlot()->SlotNumber(), Tag);
  return NULL;
}

void cCiMMI::Process(int Length, const uint8_t *Data)
{
  if (Data) {
     int Tag = GetTag(Length, &Data);
     switch (Tag) {
       case AOT_DISPLAY_CONTROL: {
            dbgprotocol("Slot %d: <== Display Control (%d)\n", Tc()->CamSlot()->SlotNumber(), SessionId());
            int l = 0;
            const uint8_t *d = GetData(Data, l);
            if (l > 0) {
               switch (*d) {
                 case DCC_SET_MMI_MODE:
                      if (l == 2 && *++d == MM_HIGH_LEVEL) {
                         struct tDisplayReply { uint8_t id; uint8_t mode; };
                         tDisplayReply dr = { id : DRI_MMI_MODE_ACK, mode : MM_HIGH_LEVEL };
                         dbgprotocol("Slot %d: ==> Display Reply (%d)\n", Tc()->CamSlot()->SlotNumber(), SessionId());
                         SendData(AOT_DISPLAY_REPLY, 2, (uint8_t *)&dr);
                         }
                      break;
                 default: esyslog("ERROR: CAM %d: MMI: unsupported display control command %02X", Tc()->CamSlot()->SlotNumber(), *d);
                 }
               }
            }
            break;
       case AOT_LIST_LAST:
       case AOT_MENU_LAST: {
            dbgprotocol("Slot %d: <== Menu Last (%d)\n", Tc()->CamSlot()->SlotNumber(), SessionId());
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
               while (l > 0) {
                     char *s = GetText(l, &d);
                     if (s) {
                        if (!menu->AddEntry(s))
                           free(s);
                        }
                     else
                        break;
                     }
               }
            }
            break;
       case AOT_ENQ: {
            dbgprotocol("Slot %d: <== Enq (%d)\n", Tc()->CamSlot()->SlotNumber(), SessionId());
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
            dbgprotocol("Slot %d: <== Close MMI (%d)  id = %02X  delay = %d\n", Tc()->CamSlot()->SlotNumber(), SessionId(), id, delay);
            }
            break;
       default: esyslog("ERROR: CAM %d: MMI: unknown tag %06X", Tc()->CamSlot()->SlotNumber(), Tag);
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
  dbgprotocol("Slot %d: ==> Menu Answ (%d)\n", Tc()->CamSlot()->SlotNumber(), SessionId());
  SendData(AOT_MENU_ANSW, 1, &Selection);
}

bool cCiMMI::SendAnswer(const char *Text)
{
  dbgprotocol("Slot %d: ==> Answ (%d)\n", Tc()->CamSlot()->SlotNumber(), SessionId());
  struct tAnswer { uint8_t id; char text[256]; };//XXX
  tAnswer answer;
  answer.id = Text ? AI_ANSWER : AI_CANCEL;
  if (Text)
     strncpy(answer.text, Text, sizeof(answer.text));
  SendData(AOT_ANSW, Text ? strlen(Text) + 1 : 1, (uint8_t *)&answer);
  return true;
}

bool cCiMMI::SendCloseMMI(void)
{
  dbgprotocol("Slot %d: ==> Close MMI (%d)\n", Tc()->CamSlot()->SlotNumber(), SessionId());
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
}

cCiTransportConnection::~cCiTransportConnection()
{
  for (int i = 1; i <= MAX_SESSIONS_PER_TC; i++)
      delete sessions[i];
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
  for (int i = 1; i <= MAX_SESSIONS_PER_TC; i++) {
      if (sessions[i] && sessions[i]->ResourceId() == ResourceId)
         return sessions[i];
      }
  return NULL;
}

void cCiTransportConnection::OpenSession(int Length, const uint8_t *Data)
{
  if (Length == 6 && *(Data + 1) == 0x04) {
     uint32_t ResourceId = ResourceIdToInt(Data + 2);
     dbgprotocol("Slot %d: open session %08X\n", camSlot->SlotNumber(), ResourceId);
     if (!GetSessionByResourceId(ResourceId)) {
        for (int i = 1; i <= MAX_SESSIONS_PER_TC; i++) {
            if (!sessions[i]) {
               switch (ResourceId) {
                 case RI_RESOURCE_MANAGER:           sessions[i] = new cCiResourceManager(i, this); break;
                 case RI_APPLICATION_INFORMATION:    sessions[i] = new cCiApplicationInformation(i, this); break;
                 case RI_CONDITIONAL_ACCESS_SUPPORT: sessions[i] = new cCiConditionalAccessSupport(i, this); break;
                 case RI_DATE_TIME:                  sessions[i] = new cCiDateTime(i, this); break;
                 case RI_MMI:                        sessions[i] = new cCiMMI(i, this); break;
                 case RI_HOST_CONTROL:               // not implemented
                 default: esyslog("ERROR: CAM %d: unknown resource identifier: %08X (%d/%d)", camSlot->SlotNumber(), ResourceId, camSlot->SlotIndex(), tcid);
                 }
               if (sessions[i])
                  SendTag(ST_OPEN_SESSION_RESPONSE, sessions[i]->SessionId(), sessions[i]->ResourceId(), SS_OK);
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
  };

// --- cCiAdapter ------------------------------------------------------------

cCiAdapter::cCiAdapter(void)
:cThread("CI adapter")
{
  assignedDevice = NULL;
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

bool cCiAdapter::Ready(void)
{
  for (int i = 0; i < MAX_CAM_SLOTS_PER_ADAPTER; i++) {
      if (camSlots[i] && !camSlots[i]->Ready())
         return false;
      }
  return true;
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

cCamSlots CamSlots;

#define MODULE_CHECK_INTERVAL 500 // ms
#define MODULE_RESET_TIMEOUT    2 // s

cCamSlot::cCamSlot(cCiAdapter *CiAdapter)
{
  ciAdapter = CiAdapter;
  slotIndex = -1;
  lastModuleStatus = msReset; // avoids initial reset log message
  resetTime = 0;
  resendPmt = false;
  source = transponder = 0;
  for (int i = 0; i <= MAX_CONNECTIONS_PER_CAM_SLOT; i++) // tc[0] is not used, but initialized anyway
      tc[i] = NULL;
  CamSlots.Add(this);
  slotNumber = Index() + 1;
  if (ciAdapter)
     ciAdapter->AddCamSlot(this);
  Reset();
}

cCamSlot::~cCamSlot()
{
  CamSlots.Del(this, false);
  DeleteAllConnections();
}

bool cCamSlot::Assign(cDevice *Device, bool Query)
{
  cMutexLock MutexLock(&mutex);
  if (ciAdapter) {
     if (ciAdapter->Assign(Device, true)) {
        if (!Device && ciAdapter->assignedDevice)
           ciAdapter->assignedDevice->SetCamSlot(NULL);
        if (!Query) {
           StopDecrypting();
           source = transponder = 0;
           if (ciAdapter->Assign(Device)) {
              ciAdapter->assignedDevice = Device;
              if (Device) {
                 Device->SetCamSlot(this);
                 dsyslog("CAM %d: assigned to device %d", slotNumber, Device->DeviceNumber() + 1);
                 }
              else
                 dsyslog("CAM %d: unassigned", slotNumber);
              }
           else
              return false;
           }
        return true;
        }
     }
  return false;
}

cDevice *cCamSlot::Device(void)
{
  cMutexLock MutexLock(&mutex);
  if (ciAdapter) {
     cDevice *d = ciAdapter->assignedDevice;
     if (d && d->CamSlot() == this)
        return d;
     }
  return NULL;
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
               DeleteAllConnections();
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
               resendPmt = caProgramList.Count() > 0;
               break;
          default:
               esyslog("ERROR: unknown module status %d (%s)", ms, __FUNCTION__);
          }
        lastModuleStatus = ms;
        }
     moduleCheckTimer.Set(MODULE_CHECK_INTERVAL);
     }
  if (resendPmt)
     SendCaPmt(CPCI_OK_DESCRAMBLING);
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
        return true;
        }
     dbgprotocol("failed!\n");
     }
  return false;
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

void cCamSlot::SendCaPmt(uint8_t CmdId)
{
  cMutexLock MutexLock(&mutex);
  cCiConditionalAccessSupport *cas = (cCiConditionalAccessSupport *)GetSessionByResourceId(RI_CONDITIONAL_ACCESS_SUPPORT);
  if (cas) {
     const int *CaSystemIds = cas->GetCaSystemIds();
     if (CaSystemIds && *CaSystemIds) {
        if (caProgramList.Count()) {
           for (int Loop = 1; Loop <= 2; Loop++) {
               for (cCiCaProgramData *p = caProgramList.First(); p; p = caProgramList.Next(p)) {
                   if (p->modified || resendPmt) {
                      bool Active = false;
                      cCiCaPmt CaPmt(CmdId, source, transponder, p->programNumber, CaSystemIds);
                      for (cCiCaPidData *q = p->pidList.First(); q; q = p->pidList.Next(q)) {
                          if (q->active) {
                             CaPmt.AddPid(q->pid, q->streamType);
                             Active = true;
                             }
                          }
                      if ((Loop == 1) != Active) { // first remove, then add
                         if (cas->RepliesToQuery())
                            CaPmt.SetListManagement(Active ? CPLM_ADD : CPLM_UPDATE);
                         if (Active || cas->RepliesToQuery())
                            cas->SendPMT(&CaPmt);
                         p->modified = false;
                         }
                      }
                   }
               }
           resendPmt = false;
           }
        else {
           cCiCaPmt CaPmt(CmdId, 0, 0, 0, NULL);
           cas->SendPMT(&CaPmt);
           }
        }
     }
}

const int *cCamSlot::GetCaSystemIds(void)
{
  cMutexLock MutexLock(&mutex);
  cCiConditionalAccessSupport *cas = (cCiConditionalAccessSupport *)GetSessionByResourceId(RI_CONDITIONAL_ACCESS_SUPPORT);
  return cas ? cas->GetCaSystemIds() : NULL;
}

int cCamSlot::Priority(void)
{
  cDevice *d = Device();
  return d ? d->Priority() : -1;
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

bool cCamSlot::CanDecrypt(const cChannel *Channel)
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
  if (caProgramList.Count()) {
     caProgramList.Clear();
     SendCaPmt(CPCI_NOT_SELECTED);
     }
}

bool cCamSlot::IsDecrypting(void)
{
  cMutexLock MutexLock(&mutex);
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
