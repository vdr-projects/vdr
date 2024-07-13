/*
 * skins.c: The optical appearance of the OSD
 *
 * See the main source file 'vdr.c' for copyright information and
 * how to reach the author.
 *
 * $Id: skins.c 5.2 2024/07/13 09:12:18 kls Exp $
 */

#include "skins.h"
#include "interface.h"
#include "status.h"

// --- cSkinQueuedMessage ----------------------------------------------------

class cSkinQueuedMessage : public cListObject {
  friend class cSkins;
private:
  eMessageType type;
  char *message;
  int seconds;
  int timeout;
  tThreadId threadId;
  eKeys key;
  int state;
  cMutex mutex;
  cCondVar condVar;
public:
  cSkinQueuedMessage(eMessageType Type, const char *s, int Seconds, int Timeout);
  virtual ~cSkinQueuedMessage();
  };

cSkinQueuedMessage::cSkinQueuedMessage(eMessageType Type, const char *s, int Seconds, int Timeout)
{
  type = Type;
  message = s ? strdup(s) : NULL;
  seconds = Seconds;
  timeout = Timeout;
  threadId = cThread::ThreadId();
  key = kNone;
  state = 0; // waiting
}

cSkinQueuedMessage::~cSkinQueuedMessage()
{
  free(message);
}

cList<cSkinQueuedMessage> SkinQueuedMessages;

// --- cSkinDisplay ----------------------------------------------------------

cSkinDisplay *cSkinDisplay::current = NULL;

cSkinDisplay::cSkinDisplay(void)
{
  current = this;
  editableWidth = 100; //XXX
}

cSkinDisplay::~cSkinDisplay()
{
  current = NULL;
}

// --- cSkinDisplayChannel ---------------------------------------------------

cSkinDisplayChannel::cSkinDisplayChannel(void)
{
  positioner = NULL;
}

void cSkinDisplayChannel::SetPositioner(const cPositioner *Positioner)
{
  if (positioner && Positioner != positioner)
     SetMessage(mtInfo, NULL);
  positioner = Positioner;
  if (positioner)
     SetMessage(mtInfo, cString::sprintf(tr("Moving dish to %.1f..."), double(positioner->TargetLongitude()) / 10));
}

// --- cSkinDisplayMenu ------------------------------------------------------

cSkinDisplayMenu::cSkinDisplayMenu(void)
{
  menuCategory = mcUndefined;
  SetTabs(0);
}

void cSkinDisplayMenu::SetMenuCategory(eMenuCategory MenuCategory)
{
  menuCategory = MenuCategory;
}

void cSkinDisplayMenu::SetTabs(int Tab1, int Tab2, int Tab3, int Tab4, int Tab5)
{
  tabs[0] = 0;
  tabs[1] = Tab1 ? tabs[0] + Tab1 : 0;
  tabs[2] = Tab2 ? tabs[1] + Tab2 : 0;
  tabs[3] = Tab3 ? tabs[2] + Tab3 : 0;
  tabs[4] = Tab4 ? tabs[3] + Tab4 : 0;
  tabs[5] = Tab5 ? tabs[4] + Tab5 : 0;
  for (int i = 1; i < MaxTabs; i++)
      tabs[i] *= AvgCharWidth();
}

void cSkinDisplayMenu::Scroll(bool Up, bool Page)
{
  textScroller.Scroll(Up, Page);
}

const char *cSkinDisplayMenu::GetTabbedText(const char *s, int Tab)
{
  if (!s)
     return NULL;
  static char buffer[1000];
  const char *a = s;
  const char *b = strchrnul(a, '\t');
  while (*b && Tab-- > 0) {
        a = b + 1;
        b = strchrnul(a, '\t');
        }
  if (!*b)
     return (Tab <= 0) ? a : NULL;
  unsigned int n = b - a;
  if (n >= sizeof(buffer))
     n = sizeof(buffer) - 1;
  strncpy(buffer, a, n);
  buffer[n] = 0;
  return buffer;
}

void cSkinDisplayMenu::SetScrollbar(int Total, int Offset)
{
}

int cSkinDisplayMenu::GetTextAreaWidth(void) const
{
  return 0;
}

const cFont *cSkinDisplayMenu::GetTextAreaFont(bool  FixedFont) const
{
  return cFont::GetFont(FixedFont ? fontFix : fontOsd);
}

// --- cSkinDisplayReplay::cProgressBar --------------------------------------

cSkinDisplayReplay::cProgressBar::cProgressBar(int Width, int Height, int Current, int Total, const cMarks *Marks, tColor ColorSeen, tColor ColorRest, tColor ColorSelected, tColor ColorMark, tColor ColorCurrent)
:cBitmap(Width, Height, 2)
{
  total = Total;
  if (total > 0) {
     int p = Pos(Current);
     DrawRectangle(0, 0, p, Height - 1, ColorSeen);
     DrawRectangle(p + 1, 0, Width - 1, Height - 1, ColorRest);
     if (Marks) {
        bool Start = true;
        for (const cMark *m = Marks->First(); m; m = Marks->Next(m)) {
            int p1 = Pos(m->Position());
            if (Start) {
               const cMark *m2 = Marks->Next(m);
               int p2 = Pos(m2 ? m2->Position() : total);
               int h = Height / 3;
               DrawRectangle(p1, h, p2, Height - h - 1, ColorSelected);
               }
            Mark(p1, Start, m->Position() == Current, ColorMark, ColorCurrent);
            Start = !Start;
            }
        }
     }
}

void cSkinDisplayReplay::cProgressBar::Mark(int x, bool Start, bool Current, tColor ColorMark, tColor ColorCurrent)
{
  DrawRectangle(x, 0, x, Height() - 1, ColorMark);
  const int d = Height() / (Current ? 3 : 9);
  for (int i = 0; i < d; i++) {
      int h = Start ? i : Height() - 1 - i;
      DrawRectangle(x - d + i, h, x + d - i, h, Current ? ColorCurrent : ColorMark);
      }
}

// --- cSkinDisplayReplay ----------------------------------------------------

cSkinDisplayReplay::cSkinDisplayReplay(void)
{
  marks = NULL;
}

void cSkinDisplayReplay::SetRecording(const cRecording *Recording)
{
  SetTitle(Recording->Title());
}

void cSkinDisplayReplay::SetMarks(const cMarks *Marks)
{
  marks = Marks;
}

// --- cSkin -----------------------------------------------------------------

cSkin::cSkin(const char *Name, cTheme *Theme)
{
  name = strdup(Name);
  theme = Theme;
  if (theme)
     cThemes::Save(name, theme);
  Skins.Add(this);
}

cSkin::~cSkin()
{
  free(name);
}

// --- cSkins ----------------------------------------------------------------

cSkins Skins;

cSkins::cSkins(void)
{
  displayMessage = NULL;
}

cSkins::~cSkins()
{
  delete displayMessage;
}

bool cSkins::SetCurrent(const char *Name)
{
  if (Name) {
     for (cSkin *Skin = First(); Skin; Skin = Next(Skin)) {
         if (strcmp(Skin->Name(), Name) == 0) {
            isyslog("setting current skin to \"%s\"", Name);
            current = Skin;
            return true;
            }
         }
     }
  current = First();
  if (current)
     isyslog("skin \"%s\" not available - using \"%s\" instead", Name, current->Name());
  else
     esyslog("ERROR: no skin available");
  return current != NULL;
}

eKeys cSkins::Message(eMessageType Type, const char *s, int Seconds)
{
  if (!cThread::IsMainThread()) {
     if (Type != mtStatus)
        QueueMessage(Type, s, Seconds);
     else
        dsyslog("cSkins::Message(%d, \"%s\", %d) called from background thread - ignored! (Use cSkins::QueueMessage() instead)", Type, s, Seconds);
     return kNone;
     }
  switch (Type) {
    case mtInfo:    isyslog("info: %s", s); break;
    case mtWarning: isyslog("warning: %s", s); break;
    case mtError:   esyslog("ERROR: %s", s); break;
    default: ;
    }
  if (!Current())
     return kNone;
  if (!cSkinDisplay::Current()) {
     if (displayMessage)
        delete displayMessage;
     displayMessage = Current()->DisplayMessage();
     }
  cSkinDisplay::Current()->SetMessage(Type, s);
  cSkinDisplay::Current()->Flush();
  cStatus::MsgOsdStatusMessage(s);
  eKeys k = kNone;
  if (Type != mtStatus) {
     k = Interface->Wait(Seconds);
     if (displayMessage) {
        delete displayMessage;
        displayMessage = NULL;
        cStatus::MsgOsdClear();
        }
     else {
        cSkinDisplay::Current()->SetMessage(Type, NULL);
        cStatus::MsgOsdStatusMessage(NULL);
        }
     }
  else if (!s && displayMessage) {
     delete displayMessage;
     displayMessage = NULL;
     cStatus::MsgOsdClear();
     }
  return k;
}

int cSkins::QueueMessage(eMessageType Type, const char *s, int Seconds, int Timeout)
{
  if (Type == mtStatus) {
     dsyslog("cSkins::QueueMessage() called with mtStatus - ignored!");
     return kNone;
     }
  if (isempty(s)) {
     if (!cThread::IsMainThread()) {
        queueMessageMutex.Lock();
        for (cSkinQueuedMessage *m = SkinQueuedMessages.Last(); m; m = SkinQueuedMessages.Prev(m)) {
            if (m->threadId == cThread::ThreadId() && m->state == 0)
               m->state = 2; // done
            }
        queueMessageMutex.Unlock();
        }
     else
        dsyslog("cSkins::QueueMessage() called with empty message from main thread - ignored!");
     return kNone;
     }
  int k = kNone;
  if (Timeout > 0) {
     if (cThread::IsMainThread()) {
        dsyslog("cSkins::QueueMessage() called from main thread with Timeout = %d - ignored!", Timeout);
        return k;
        }
     cSkinQueuedMessage *m = new cSkinQueuedMessage(Type, s, Seconds, Timeout);
     queueMessageMutex.Lock();
     SkinQueuedMessages.Add(m);
     m->mutex.Lock();
     queueMessageMutex.Unlock();
     if (m->condVar.TimedWait(m->mutex, Timeout * 1000))
        k = m->key;
     else
        k = -1; // timeout, nothing has been displayed
     m->state = 2; // done
     m->mutex.Unlock();
     }
  else {
     queueMessageMutex.Lock();
     // Check if there is a waiting message w/o timeout for this thread:
     if (Timeout == -1) {
        for (cSkinQueuedMessage *m = SkinQueuedMessages.Last(); m; m = SkinQueuedMessages.Prev(m)) {
            if (m->threadId == cThread::ThreadId()) {
               if (m->state == 0 && m->timeout == -1)
                  m->state = 2; // done
               break;
               }
            }
         }
     // Add the new message:
     SkinQueuedMessages.Add(new cSkinQueuedMessage(Type, s, Seconds, Timeout));
     queueMessageMutex.Unlock();
     }
  return k;
}

void cSkins::ProcessQueuedMessages(void)
{
  if (!cThread::IsMainThread()) {
     dsyslog("cSkins::ProcessQueuedMessages() called from background thread - ignored!");
     return;
     }
  // Check whether there is a cSkinDisplay object (if any) that implements SetMessage():
  if (cSkinDisplay *sd = cSkinDisplay::Current()) {
     if (!(dynamic_cast<cSkinDisplayChannel *>(sd) ||
           dynamic_cast<cSkinDisplayMenu *>(sd) ||
           dynamic_cast<cSkinDisplayReplay *>(sd) ||
           dynamic_cast<cSkinDisplayMessage *>(sd)))
        return;
     }
  cSkinQueuedMessage *msg = NULL;
  // Get the first waiting message:
  queueMessageMutex.Lock();
  for (cSkinQueuedMessage *m = SkinQueuedMessages.First(); m; m = SkinQueuedMessages.Next(m)) {
      if (m->state == 0) { // waiting
         m->state = 1; // active
         msg = m;
         break;
         }
      }
  queueMessageMutex.Unlock();
  // Display the message:
  if (msg) {
     msg->mutex.Lock();
     if (msg->state == 1) { // might have changed since we got it
        msg->key = Skins.Message(msg->type, msg->message, msg->seconds);
        if (msg->timeout == 0)
           msg->state = 2; // done
        else
           msg->condVar.Broadcast();
        }
     msg->mutex.Unlock();
     }
  // Remove done messages from the queue:
  queueMessageMutex.Lock();
  for (;;) {
      cSkinQueuedMessage *m = SkinQueuedMessages.First();
      if (m && m->state == 2) { // done
         SkinQueuedMessages.Del(m);
         }
      else
         break;
      }
  queueMessageMutex.Unlock();
}

void cSkins::Flush(void)
{
  if (cSkinDisplay::Current())
     cSkinDisplay::Current()->Flush();
}

void cSkins::Clear(void)
{
  if (displayMessage) {
     delete displayMessage;
     displayMessage = NULL;
     }
  cList<cSkin>::Clear();
}
