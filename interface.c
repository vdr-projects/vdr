/*
 * interface.c: Abstract user interface layer
 *
 * See the main source file 'vdr.c' for copyright information and
 * how to reach the author.
 *
 * $Id: interface.c 1.47 2002/01/27 15:48:46 kls Exp $
 */

#include "interface.h"
#include <ctype.h>
#include <unistd.h>
#include "i18n.h"

cInterface *Interface = NULL;

cInterface::cInterface(int SVDRPport)
{
  open = 0;
  cols[0] = 0;
  width = height = 0;
  keyFromWait = kNone;
  interrupted = false;
  rcIo = NULL;
  SVDRP = NULL;
#if defined(REMOTE_RCU)
  rcIo = new cRcIoRCU("/dev/ttyS1");
#elif defined(REMOTE_LIRC)
  rcIo = new cRcIoLIRC("/dev/lircd");
#elif defined(REMOTE_KBD)
  rcIo = new cRcIoKBD;
#else
  rcIo = new cRcIoBase; // acts as a dummy device
#endif
  rcIo->SetCode(Keys.code, Keys.address);
  if (SVDRPport)
     SVDRP = new cSVDRP(SVDRPport);
}

cInterface::~cInterface()
{
  delete rcIo;
  delete SVDRP;
}

void cInterface::Open(int NumCols, int NumLines)
{
  if (!open++) {
     if (NumCols == 0)
        NumCols = Setup.OSDwidth;
     if (NumLines == 0)
        NumLines = Setup.OSDheight;
     cDvbApi::PrimaryDvbApi->Open(width = NumCols, height = NumLines);
     }
}

void cInterface::Close(void)
{
  if (open == 1)
     Clear();
  if (!--open) {
     cDvbApi::PrimaryDvbApi->Close();
     width = height = 0;
     }
}

unsigned int cInterface::GetCh(bool Wait, bool *Repeat, bool *Release)
{
  Flush();
  if (!rcIo->InputAvailable())
     cFile::AnyFileReady(-1, Wait ? 1000 : 0);
  unsigned int Command;
  return rcIo->GetCommand(&Command, Repeat, Release) ? Command : 0;
}

eKeys cInterface::GetKey(bool Wait)
{
  Flush();
  if (SVDRP) {
     SVDRP->Process();
     if (!open) {
        char *message = SVDRP->GetMessage();
        if (message) {
           Info(message);
           delete message;
           }
        }
     }
  eKeys Key = keyFromWait;
  if (Key == kNone) {
     bool Repeat = false, Release = false;
     Key = Keys.Get(GetCh(Wait, &Repeat, &Release));
     if (Repeat)
        Key = eKeys(Key | k_Repeat);
     if (Release)
        Key = eKeys(Key | k_Release);
     }
  keyFromWait = kNone;
  return Key;
}

void cInterface::PutKey(eKeys Key)
{
  keyFromWait = Key;
}

eKeys cInterface::Wait(int Seconds, bool KeepChar)
{
  if (Seconds == 0)
     Seconds = Setup.OSDMessageTime;
  Flush();
  eKeys Key = kNone;
  time_t timeout = time(NULL) + Seconds;
  for (;;) {
      Key = GetKey();
      if ((Key != kNone && (RAWKEY(Key) != kOk || RAWKEY(Key) == Key)) || time(NULL) > timeout || interrupted)
         break;
      }
  if (KeepChar && ISRAWKEY(Key))
     keyFromWait = Key;
  interrupted = false;
  return Key;
}

void cInterface::Clear(void)
{
  if (open)
     cDvbApi::PrimaryDvbApi->Clear();
}

void cInterface::ClearEol(int x, int y, eDvbColor Color)
{
  if (open)
     cDvbApi::PrimaryDvbApi->ClrEol(x, y, Color);
}

void cInterface::Fill(int x, int y, int w, int h, eDvbColor Color)
{
  if (open)
     cDvbApi::PrimaryDvbApi->Fill(x, y, w, h, Color);
}

void cInterface::SetBitmap(int x, int y, const cBitmap &Bitmap)
{
  if (open)
     cDvbApi::PrimaryDvbApi->SetBitmap(x, y, Bitmap);
}

void cInterface::Flush(void)
{
  if (open)
     cDvbApi::PrimaryDvbApi->Flush();
}

void cInterface::SetCols(int *c)
{
  for (int i = 0; i < MaxCols; i++) {
      cols[i] = *c++;
      if (cols[i] == 0)
         break;
      }
}

eDvbFont cInterface::SetFont(eDvbFont Font)
{
  return cDvbApi::PrimaryDvbApi->SetFont(Font);
}

char *cInterface::WrapText(const char *Text, int Width, int *Height)
{
  // Wraps the Text to make it fit into the area defined by the given Width
  // (which is given in character cells).
  // The actual number of lines resulting from this operation is returned in
  // Height.
  // The returned string is newly created on the heap and the caller
  // is responsible for deleting it once it is no longer used.
  // Wrapping is done by inserting the necessary number of newline
  // characters into the string.

  int Lines = 1;
  char *t = strdup(Text);
  char *Blank = NULL;
  char *Delim = NULL;
  int w = 0;

  Width *= cDvbApi::PrimaryDvbApi->CellWidth();

  while (*t && t[strlen(t) - 1] == '\n')
        t[strlen(t) - 1] = 0; // skips trailing newlines

  for (char *p = t; *p; ) {
      if (*p == '\n') {
         Lines++;
         w = 0;
         Blank = Delim = NULL;
         p++;
         continue;
         }
      else if (isspace(*p))
         Blank = p;
      int cw = cDvbApi::PrimaryDvbApi->Width(*p);
      if (w + cw > Width) {
         if (Blank) {
            *Blank = '\n';
            p = Blank;
            continue;
            }
         else {
            // Here's the ugly part, where we don't have any whitespace to
            // punch in a newline, so we need to make room for it:
            if (Delim)
               p = Delim + 1; // let's fall back to the most recent delimiter
            char *s = new char[strlen(t) + 2]; // The additional '\n' plus the terminating '\0'
            int l = p - t;
            strncpy(s, t, l);
            s[l] = '\n';
            strcpy(s + l + 1, p);
            delete t;
            t = s;
            p = t + l;
            continue;
            }
         }
      else
         w += cw;
      if (strchr("-.,:;!?_", *p)) {
         Delim = p;
         Blank = NULL;
         }
      p++;
      }

  *Height = Lines;
  return t;
}

void cInterface::Write(int x, int y, const char *s, eDvbColor FgColor, eDvbColor BgColor)
{
  if (open)
     cDvbApi::PrimaryDvbApi->Text(x, y, s, FgColor, BgColor);
}

void cInterface::WriteText(int x, int y, const char *s, eDvbColor FgColor, eDvbColor BgColor)
{
  if (open) {
     ClearEol(x, y, BgColor);
     int col = 0;
     for (;;) {
         const char *t = strchr(s, '\t');
         const char *p = s;
         char buf[1000];
         if (t && col < MaxCols && cols[col] > 0) {
            unsigned int n = t - s;
            if (n >= sizeof(buf))
               n = sizeof(buf) - 1;
            strncpy(buf, s, n);
            buf[n] = 0;
            p = buf;
            s = t + 1;
            }
         Write(x, y, p, FgColor, BgColor);
         if (p == s)
            break;
         x += cols[col++];
         }
     }
}

void cInterface::Title(const char *s)
{
  ClearEol(0, 0, clrCyan);
  const char *t = strchr(s, '\t');
  if (t) {
     char buffer[Width() + 1];
     unsigned int n = t - s;
     if (n >= sizeof(buffer))
        n = sizeof(buffer) - 1;
     strn0cpy(buffer, s, n + 1);
     Write(1, 0, buffer, clrBlack, clrCyan);
     t++;
     Write(-(cDvbApi::PrimaryDvbApi->WidthInCells(t) + 1), 0, t, clrBlack, clrCyan);
     }
  else {
     int x = (Width() - strlen(s)) / 2;
     if (x < 0)
        x = 0;
     Write(x, 0, s, clrBlack, clrCyan);
     }
}

void cInterface::Status(const char *s, eDvbColor FgColor, eDvbColor BgColor)
{
  int Line = (abs(height) == 1) ? 0 : -2;
  ClearEol(0, Line, s ? BgColor : clrBackground);
  if (s) {
     int x = (Width() - strlen(s)) / 2;
     if (x < 0)
        x = 0;
     Write(x, Line, s, FgColor, BgColor);
     }
}

void cInterface::Info(const char *s)
{
  Open(Setup.OSDwidth, -1);
  isyslog(LOG_INFO, "info: %s", s);
  Status(s, clrBlack, clrGreen);
  Wait();
  Status(NULL);
  Close();
}

void cInterface::Error(const char *s)
{
  Open(Setup.OSDwidth, -1);
  esyslog(LOG_ERR, "ERROR: %s", s);
  Status(s, clrWhite, clrRed);
  Wait();
  Status(NULL);
  Close();
}

bool cInterface::Confirm(const char *s, int Seconds, bool WaitForTimeout)
{
  Open(Setup.OSDwidth, -1);
  isyslog(LOG_INFO, "confirm: %s", s);
  Status(s, clrBlack, clrYellow);
  eKeys k = Wait(Seconds);
  bool result = WaitForTimeout ? k == kNone : k == kOk;
  Status(NULL);
  Close();
  isyslog(LOG_INFO, "%sconfirmed", result ? "" : "not ");
  return result;
}

void cInterface::HelpButton(int Index, const char *Text, eDvbColor FgColor, eDvbColor BgColor)
{
  if (open) {
     const int w = Width() / 4;
     cDvbApi::PrimaryDvbApi->Fill(Index * w, -1, w, 1, Text ? BgColor : clrBackground);
     if (Text) {
        int l = (w - int(strlen(Text))) / 2;
        if (l < 0)
           l = 0;
        cDvbApi::PrimaryDvbApi->Text(Index * w + l, -1, Text, FgColor, BgColor);
        }
     }
}

void cInterface::Help(const char *Red, const char *Green, const char *Yellow, const char *Blue)
{
  HelpButton(0, Red,    clrBlack, clrRed);
  HelpButton(1, Green,  clrBlack, clrGreen);
  HelpButton(2, Yellow, clrBlack, clrYellow);
  HelpButton(3, Blue,   clrWhite, clrBlue);
}

void cInterface::QueryKeys(void)
{
  Keys.Clear();
  Clear();
  WriteText(1, 1, tr("Learning Remote Control Keys"));
  WriteText(1, 3, tr("Phase 1: Detecting RC code type"));
  WriteText(1, 5, tr("Press any key on the RC unit"));
  Flush();
#ifndef REMOTE_KBD
  unsigned char Code = '0';
  unsigned short Address;
#endif
  for (;;) {
#ifdef REMOTE_KBD
      if (GetCh())
         break;
#else
      //TODO on screen display...
      if (rcIo->DetectCode(&Code, &Address)) {
         Keys.code = Code;
         Keys.address = Address;
         WriteText(1, 5, tr("RC code detected!"));
         WriteText(1, 6, tr("Do not press any key..."));
         Flush();
         rcIo->Flush(3000);
         ClearEol(0, 5);
         ClearEol(0, 6);
         Flush();
         break;
         }
#endif
      }
  WriteText(1, 3, tr("Phase 2: Learning specific key codes"));
  tKey *k = Keys.keys;
  while (k->type != kNone) {
        char *Prompt;
        asprintf(&Prompt, tr("Press key for '%s'"), tr(k->name));
        WriteText(1, 5, Prompt);
        delete Prompt;
        for (;;) {
            unsigned int ch = GetCh();
            if (ch != 0) {
               switch (Keys.Get(ch)) {
                 case kUp:   if (k > Keys.keys) {
                                k--;
                                break;
                                }
                 case kDown: if (k > Keys.keys + 1) {
                                WriteText(1, 5, tr("Press 'Up' to confirm"));
                                WriteText(1, 6, tr("Press 'Down' to continue"));
                                ClearEol(0, 7);
                                ClearEol(0, 8);
                                for (;;) {
                                    eKeys key = GetKey();
                                    if (key == kUp) {
                                       Clear();
                                       return;
                                       }
                                    else if (key == kDown) {
                                       ClearEol(0, 6);
                                       break;
                                       }
                                    }
                                break;
                                }
                 case kNone: k->code = ch;
                             k++;
                             break;
                 default:    break;
                 }
               break;
               }
            }
        if (k > Keys.keys)
           WriteText(1, 7, tr("(press 'Up' to go back)"));
        else
           ClearEol(0, 7);
        if (k > Keys.keys + 1)
           WriteText(1, 8, tr("(press 'Down' to end key definition)"));
        else
           ClearEol(0, 8);
        }
}

void cInterface::LearnKeys(void)
{
  isyslog(LOG_INFO, "learning keys");
  Open();
  for (;;) {
      Clear();
      QueryKeys();
      Clear();
      WriteText(1, 1, tr("Learning Remote Control Keys"));
      WriteText(1, 3, tr("Phase 3: Saving key codes"));
      WriteText(1, 5, tr("Press 'Up' to save, 'Down' to cancel"));
      for (;;) {
          eKeys key = GetKey();
          if (key == kUp) {
             Keys.Save();
             Close();
             return;
             }
          else if (key == kDown) {
             Keys.Load();
             Close();
             return;
             }
          }
      }
}

void cInterface::DisplayChannelNumber(int Number)
{
  rcIo->Number(Number);
}

void cInterface::DisplayRecording(int Index, bool On)
{
  rcIo->SetPoints(1 << Index, On);
}

bool cInterface::Recording(void)
{
  // This is located here because the Interface has to do with the "PrimaryDvbApi" anyway
  return cDvbApi::PrimaryDvbApi->Recording();
}
