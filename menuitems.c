/*
 * menuitems.c: General purpose menu items
 *
 * See the main source file 'vdr.c' for copyright information and
 * how to reach the author.
 *
 * $Id: menuitems.c 1.16 2004/03/28 09:17:51 kls Exp $
 */

#include "menuitems.h"
#include <ctype.h>
#include "i18n.h"
#include "plugin.h"
#include "status.h"

const char *FileNameChars = " abcdefghijklmnopqrstuvwxyz0123456789-.#~";

// --- cMenuEditItem ---------------------------------------------------------

cMenuEditItem::cMenuEditItem(const char *Name)
{
  name = strdup(Name);
  value = NULL;
}

cMenuEditItem::~cMenuEditItem()
{
  free(name);
  free(value);
}

void cMenuEditItem::SetValue(const char *Value)
{
  free(value);
  value = strdup(Value);
  char *buffer = NULL;
  asprintf(&buffer, "%s:\t%s", name, value);
  SetText(buffer, false);
  Display();
  cStatus::MsgOsdCurrentItem(buffer);
}

// --- cMenuEditIntItem ------------------------------------------------------

cMenuEditIntItem::cMenuEditIntItem(const char *Name, int *Value, int Min, int Max)
:cMenuEditItem(Name)
{
  value = Value;
  min = Min;
  max = Max;
  Set();
}

void cMenuEditIntItem::Set(void)
{
  char buf[16];
  snprintf(buf, sizeof(buf), "%d", *value);
  SetValue(buf);
}

eOSState cMenuEditIntItem::ProcessKey(eKeys Key)
{
  eOSState state = cMenuEditItem::ProcessKey(Key);

  if (state == osUnknown) {
     int newValue = *value;
     Key = NORMALKEY(Key);
     switch (Key) {
       case kNone: break;
       case k0 ... k9:
            if (fresh) {
               *value = 0;
               fresh = false;
               }
            newValue = *value * 10 + (Key - k0);
            break;
       case kLeft: // TODO might want to increase the delta if repeated quickly?
            newValue = *value - 1;
            fresh = true;
            break;
       case kRight:
            newValue = *value + 1;
            fresh = true;
            break;
       default:
            if (*value < min) { *value = min; Set(); }
            if (*value > max) { *value = max; Set(); }
            return state;
       }
     if ((!fresh || min <= newValue) && newValue <= max) {
        *value = newValue;
        Set();
        }
     state = osContinue;
     }
  return state;
}

// --- cMenuEditBoolItem -----------------------------------------------------

cMenuEditBoolItem::cMenuEditBoolItem(const char *Name, int *Value, const char *FalseString, const char *TrueString)
:cMenuEditIntItem(Name, Value, 0, 1)
{
  falseString = FalseString ? FalseString : tr("no");
  trueString = TrueString ? TrueString : tr("yes");
  Set();
}

void cMenuEditBoolItem::Set(void)
{
  char buf[16];
  snprintf(buf, sizeof(buf), "%s", *value ? trueString : falseString);
  SetValue(buf);
}

// --- cMenuEditBitItem ------------------------------------------------------

cMenuEditBitItem::cMenuEditBitItem(const char *Name, int *Value, int Mask, const char *FalseString, const char *TrueString)
:cMenuEditBoolItem(Name, &bit, FalseString, TrueString)
{
  value = Value;
  bit = (*value & Mask) != 0;
  mask = Mask;
  Set();
}

void cMenuEditBitItem::Set(void)
{
  *value = bit ? *value | mask : *value & ~mask;
  cMenuEditBoolItem::Set();
}

// --- cMenuEditNumItem ------------------------------------------------------

cMenuEditNumItem::cMenuEditNumItem(const char *Name, char *Value, int Length, bool Blind)
:cMenuEditItem(Name)
{
  value = Value;
  length = Length;
  blind = Blind;
  Set();
}

void cMenuEditNumItem::Set(void)
{
  if (blind) {
     char buf[length + 1];
     int i;
     for (i = 0; i < length && value[i]; i++)
         buf[i] = '*';
     buf[i] = 0;
     SetValue(buf);
     }
  else
     SetValue(value);
}

eOSState cMenuEditNumItem::ProcessKey(eKeys Key)
{
  eOSState state = cMenuEditItem::ProcessKey(Key);

  if (state == osUnknown) {
     Key = NORMALKEY(Key);
     switch (Key) {
       case kLeft: {
            int l = strlen(value);
            if (l > 0)
               value[l - 1] = 0;
            }
            break;
       case k0 ... k9: {
            int l = strlen(value);
            if (l < length) {
               value[l] = Key - k0 + '0';
               value[l + 1] = 0;
               }
            }
            break;
       default: return state;
       }
     Set();
     state = osContinue;
     }
  return state;
}

// --- cMenuEditChrItem ------------------------------------------------------

cMenuEditChrItem::cMenuEditChrItem(const char *Name, char *Value, const char *Allowed)
:cMenuEditItem(Name)
{
  value = Value;
  allowed = strdup(Allowed);
  current = strchr(allowed, *Value);
  if (!current)
     current = allowed;
  Set();
}

cMenuEditChrItem::~cMenuEditChrItem()
{
  free(allowed);
}

void cMenuEditChrItem::Set(void)
{
  char buf[2];
  snprintf(buf, sizeof(buf), "%c", *value);
  SetValue(buf);
}

eOSState cMenuEditChrItem::ProcessKey(eKeys Key)
{
  eOSState state = cMenuEditItem::ProcessKey(Key);

  if (state == osUnknown) {
     if (NORMALKEY(Key) == kLeft) {
        if (current > allowed)
           current--;
        }
     else if (NORMALKEY(Key) == kRight) {
        if (*(current + 1))
           current++;
        }
     else
        return state;
     *value = *current;
     Set();
     state = osContinue;
     }
  return state;
}

// --- cMenuEditStrItem ------------------------------------------------------

cMenuEditStrItem::cMenuEditStrItem(const char *Name, char *Value, int Length, const char *Allowed)
:cMenuEditItem(Name)
{
  value = Value;
  length = Length;
  allowed = strdup(Allowed);
  pos = -1;
  insert = uppercase = false;
  newchar = true;
  Set();
}

cMenuEditStrItem::~cMenuEditStrItem()
{
  free(allowed);
}

void cMenuEditStrItem::SetHelpKeys(void)
{
  if (pos >= 0)
     Interface->Help(tr("ABC/abc"), tr(insert ? "Overwrite" : "Insert"), tr("Delete"));
  else
     Interface->Help(NULL);
}

void cMenuEditStrItem::Set(void)
{
  char buf[1000];
  const char *fmt = insert && newchar ? "[]%c%s" : "[%c]%s";

  if (pos >= 0) {
     strncpy(buf, value, pos);
     snprintf(buf + pos, sizeof(buf) - pos - 2, fmt, *(value + pos), value + pos + 1);
     int width = Interface->Width() - Interface->GetCols()[0];
     if (cOsd::WidthInCells(buf) <= width) {
        // the whole buffer fits on the screen
        SetValue(buf);
        return;
        }
     width *= cOsd::CellWidth();
     width -= cOsd::Width('>'); // assuming '<' and '>' have the same with
     int w = 0;
     int i = 0;
     int l = strlen(buf);
     while (i < l && w <= width)
           w += cOsd::Width(buf[i++]);
     if (i >= pos + 4) {
        // the cursor fits on the screen
        buf[i - 1] = '>';
        buf[i] = 0;
        SetValue(buf);
        return;
        }
     // the cursor doesn't fit on the screen
     w = 0;
     if (buf[i = pos + 3]) {
        buf[i] = '>';
        buf[i + 1] = 0;
        }
     else
        i--;
     while (i >= 0 && w <= width)
           w += cOsd::Width(buf[i--]);
     buf[++i] = '<';
     SetValue(buf + i);
     }
  else
     SetValue(value);
}

char cMenuEditStrItem::Inc(char c, bool Up)
{
  const char *p = strchr(allowed, c);
  if (!p)
     p = allowed;
  if (Up) {
     if (!*++p)
        p = allowed;
     }
  else if (--p < allowed)
     p = allowed + strlen(allowed) - 1;
  return *p;
}

eOSState cMenuEditStrItem::ProcessKey(eKeys Key)
{
  switch (Key) {
    case kRed:   // Switch between upper- and lowercase characters
                 if (pos >= 0 && (!insert || !newchar)) {
                    uppercase = !uppercase;
                    value[pos] = uppercase ? toupper(value[pos]) : tolower(value[pos]);
                    }
                 break;
    case kGreen: // Toggle insert/overwrite modes
                 if (pos >= 0) {
                    insert = !insert;
                    newchar = true;
                    }
                 SetHelpKeys();
                 break;
    case kYellow|k_Repeat:
    case kYellow: // Remove the character at current position; in insert mode it is the character to the right of cursor
                 if (pos >= 0) {
                    if (strlen(value) > 1) {
                       if (!insert || pos < int(strlen(value)) - 1)
                          memmove(value + pos, value + pos + 1, strlen(value) - pos);
                       // reduce position, if we removed the last character
                       if (pos == int(strlen(value)))
                          pos--;
                       }
                    else if (strlen(value) == 1)
                       value[0] = ' '; // This is the last character in the string, replace it with a blank
                    if (isalpha(value[pos]))
                       uppercase = isupper(value[pos]);
                    newchar = true;
                    }
                 break;
    case kLeft|k_Repeat:
    case kLeft:  if (pos > 0) {
                    if (!insert || newchar)
                       pos--;
                    newchar = true;
                    }
                 if (!insert && isalpha(value[pos]))
                    uppercase = isupper(value[pos]);
                 break;
    case kRight|k_Repeat:
    case kRight: if (pos < length - 2 && pos < int(strlen(value)) ) {
                    if (++pos >= int(strlen(value))) {
                       if (pos >= 2 && value[pos - 1] == ' ' && value[pos - 2] == ' ')
                          pos--; // allow only two blanks at the end
                       else {
                          value[pos] = ' ';
                          value[pos + 1] = 0;
                          }
                       }
                    }
                 newchar = true;
                 if (!insert && isalpha(value[pos]))
                    uppercase = isupper(value[pos]);
                 if (pos == 0)
                    SetHelpKeys();
                 break;
    case kUp|k_Repeat:
    case kUp:
    case kDown|k_Repeat:
    case kDown:  if (pos >= 0) {
                    if (insert && newchar) {
                       // create a new character in insert mode
                       if (int(strlen(value)) < length - 1) {
                          memmove(value + pos + 1, value + pos, strlen(value) - pos + 1);
                          value[pos] = ' ';
                          }
                       }
                    if (uppercase)
                       value[pos] = toupper(Inc(tolower(value[pos]), NORMALKEY(Key) == kUp));
                    else
                       value[pos] =         Inc(        value[pos],  NORMALKEY(Key) == kUp);
                    newchar = false;
                    }
                 else
                    return cMenuEditItem::ProcessKey(Key);
                 break;
    case kOk:    if (pos >= 0) {
                    pos = -1;
                    newchar = true;
                    stripspace(value);
                    SetHelpKeys();
                    break;
                    }
                 // run into default
    default:     if (pos >= 0 && BASICKEY(Key) == kKbd) {
                    int c = KEYKBD(Key);
                    if (c <= 0xFF) {
                       const char *p = strchr(allowed, tolower(c));
                       if (p) {
                          int l = strlen(value);
                          if (insert && l < length - 1)
                             memmove(value + pos + 1, value + pos, l - pos + 1);
                          value[pos] = c;
                          if (pos < length - 2)
                             pos++;
                          if (pos >= l) {
                             value[pos] = ' ';
                             value[pos + 1] = 0;
                             }
                          }
                       else {
                          switch (c) {
                            case 0x7F: // backspace
                                       if (pos > 0) {
                                          pos--;
                                          return ProcessKey(kYellow);
                                          }
                                       break;
                            }
                          }
                       }
                    else {
                       switch (c) {
                         case kfHome: pos = 0; break;
                         case kfEnd:  pos = strlen(value) - 1; break;
                         case kfIns:  return ProcessKey(kGreen);
                         case kfDel:  return ProcessKey(kYellow);
                         }
                       }
                    }
                 else
                    return cMenuEditItem::ProcessKey(Key);
    }
  Set();
  return osContinue;
}

// --- cMenuEditStraItem -----------------------------------------------------

cMenuEditStraItem::cMenuEditStraItem(const char *Name, int *Value, int NumStrings, const char * const *Strings)
:cMenuEditIntItem(Name, Value, 0, NumStrings - 1)
{
  strings = Strings;
  Set();
}

void cMenuEditStraItem::Set(void)
{
  SetValue(strings[*value]);
}

// --- cMenuTextItem ---------------------------------------------------------

cMenuTextItem::cMenuTextItem(const char *Text, int X, int Y, int W, int H, eDvbColor FgColor, eDvbColor BgColor, eDvbFont Font)
{
  x = X;
  y = Y;
  w = W;
  h = H;
  fgColor = FgColor;
  bgColor = BgColor;
  font = Font;
  offset = 0;
  eDvbFont oldFont = Interface->SetFont(font);
  text = Interface->WrapText(Text, w - 1, &lines);
  Interface->SetFont(oldFont);
  if (h < 0)
     h = lines;
}

cMenuTextItem::~cMenuTextItem()
{
  free(text);
}

void cMenuTextItem::Clear(void)
{
  Interface->Fill(x, y, w, h, bgColor);
}

void cMenuTextItem::Display(int Offset, eDvbColor FgColor, eDvbColor BgColor)
{
  int l = 0;
  char *t = text;
  eDvbFont oldFont = Interface->SetFont(font);
  while (*t) {
        char *n = strchr(t, '\n');
        if (l >= offset) {
           if (n)
              *n = 0;
           Interface->Write(x, y + l - offset, t, fgColor, bgColor);
           if (n)
              *n = '\n';
           else
              break;
           }
        if (!n)
           break;
        t = n + 1;
        if (++l >= h + offset)
           break;
        }
  Interface->SetFont(oldFont);
  // scroll indicators use inverted color scheme!
  if (CanScrollUp())   Interface->Write(x + w - 1, y,         "^", bgColor, fgColor);
  if (CanScrollDown()) Interface->Write(x + w - 1, y + h - 1, "v", bgColor, fgColor);
  cStatus::MsgOsdTextItem(text);
}

void cMenuTextItem::ScrollUp(bool Page)
{
  if (CanScrollUp()) {
     Clear();
     offset = max(offset - (Page ? h : 1), 0);
     Display();
     }
  cStatus::MsgOsdTextItem(NULL, true);
}

void cMenuTextItem::ScrollDown(bool Page)
{
  if (CanScrollDown()) {
     Clear();
     offset = min(offset + (Page ? h : 1), lines - h);
     Display();
     }
  cStatus::MsgOsdTextItem(NULL, false);
}

eOSState cMenuTextItem::ProcessKey(eKeys Key)
{
  switch (Key) {
    case kLeft|k_Repeat:
    case kLeft:
    case kUp|k_Repeat:
    case kUp:            ScrollUp(NORMALKEY(Key) == kLeft);    break;
    case kRight|k_Repeat:
    case kRight:
    case kDown|k_Repeat:
    case kDown:          ScrollDown(NORMALKEY(Key) == kRight); break;
    default:             return osUnknown;
    }
  return osContinue;
}

// --- cMenuEditChanItem -----------------------------------------------------

cMenuEditChanItem::cMenuEditChanItem(const char *Name, int *Value)
:cMenuEditIntItem(Name, Value, 1, Channels.MaxNumber())
{
  Set();
}

void cMenuEditChanItem::Set(void)
{
  char buf[255];
  cChannel *channel = Channels.GetByNumber(*value);
  snprintf(buf, sizeof(buf), "%d %s", *value, channel ? channel->Name() : "");
  SetValue(buf);
}

eOSState cMenuEditChanItem::ProcessKey(eKeys Key)
{
  int delta = 1;

  switch (Key) {
    case kLeft|k_Repeat:
    case kLeft:  delta = -1;
    case kRight|k_Repeat:
    case kRight:
                 {
                   cChannel *channel = Channels.GetByNumber(*value + delta, delta);
                   if (channel) {
                      *value = channel->Number();
                      Set();
                      }
                 }
                 break;
    default : return cMenuEditIntItem::ProcessKey(Key);
    }
  return osContinue;
}

// --- cMenuEditTranItem -----------------------------------------------------

cMenuEditTranItem::cMenuEditTranItem(const char *Name, int *Value)
:cMenuEditChanItem(Name, Value)
{
  number = 0;
  transponder = *Value;
  cChannel *channel = Channels.First();
  while (channel) {
        if (!channel->GroupSep() && ISTRANSPONDER(channel->Frequency(), *Value)) {
           number = channel->Number();
           break;
           }
        channel = (cChannel *)channel->Next();
        }
  *Value = number;
  Set();
  *Value = transponder;
}

eOSState cMenuEditTranItem::ProcessKey(eKeys Key)
{
  *value = number;
  eOSState state = cMenuEditChanItem::ProcessKey(Key);
  number = *value;
  cChannel *channel = Channels.GetByNumber(*value);
  if (channel)
     transponder = channel->Frequency();
  *value = transponder;
  return state;
}

// --- cMenuEditDayItem ------------------------------------------------------

int cMenuEditDayItem::days[] ={ cTimer::ParseDay("M------"),
                                cTimer::ParseDay("-T-----"),
                                cTimer::ParseDay("--W----"),
                                cTimer::ParseDay("---T---"),
                                cTimer::ParseDay("----F--"),
                                cTimer::ParseDay("-----S-"),
                                cTimer::ParseDay("------S"),
                                cTimer::ParseDay("MTWTF--"),
                                cTimer::ParseDay("MTWTFS-"),
                                cTimer::ParseDay("MTWTFSS"),
                                cTimer::ParseDay("-----SS"),
                                0 };

cMenuEditDayItem::cMenuEditDayItem(const char *Name, int *Value)
:cMenuEditIntItem(Name, Value, -INT_MAX, 31)
{
  d = -1;
  if (*value < 0) {
     int n = 0;
     while (days[n]) {
           if (days[n] == *value) {
              d = n;
              break;
              }
           n++;
           }
     }
  Set();
}

void cMenuEditDayItem::Set(void)
{
  SetValue(cTimer::PrintDay(*value));
}

eOSState cMenuEditDayItem::ProcessKey(eKeys Key)
{
  switch (Key) {
    case kLeft|k_Repeat:
    case kLeft:  if (d > 0)
                    *value = days[--d];
                 else if (d == 0) {
                    *value = 31;
                    d = -1;
                    }
                 else if (*value == 1) {
                    d = sizeof(days) / sizeof(int) - 2;
                    *value = days[d];
                    }
                 else
                    return cMenuEditIntItem::ProcessKey(Key);
                 Set();
                 break;
    case kRight|k_Repeat:
    case kRight: if (d >= 0) {
                    *value = days[++d];
                    if (*value == 0) {
                       *value = 1;
                       d = -1;
                       }
                    }
                 else if (*value == 31) {
                    d = 0;
                    *value = days[d];
                    }
                 else
                    return cMenuEditIntItem::ProcessKey(Key);
                 Set();
                 break;
    default : return cMenuEditIntItem::ProcessKey(Key);
    }
  return osContinue;
}

// --- cMenuEditDateItem -----------------------------------------------------

cMenuEditDateItem::cMenuEditDateItem(const char *Name, time_t *Value)
:cMenuEditItem(Name)
{
  value = Value;
  Set();
}

void cMenuEditDateItem::Set(void)
{
#define DATEBUFFERSIZE 32
  char buf[DATEBUFFERSIZE];
  if (*value) {
     struct tm tm_r;
     localtime_r(value, &tm_r);
     strftime(buf, DATEBUFFERSIZE, "%Y-%m-%d ", &tm_r);
     strcat(buf, WeekDayName(tm_r.tm_wday));
     }
  else
     *buf = 0;
  SetValue(buf);
}

eOSState cMenuEditDateItem::ProcessKey(eKeys Key)
{
  eOSState state = cMenuEditItem::ProcessKey(Key);

  if (state == osUnknown) {
     if (NORMALKEY(Key) == kLeft) { // TODO might want to increase the delta if repeated quickly?
        *value -= SECSINDAY;
        if (*value < time(NULL))
           *value = 0;
        }
     else if (NORMALKEY(Key) == kRight) {
        if (!*value)
           *value = cTimer::SetTime(time(NULL), 0);
        *value += SECSINDAY;
        }
     else
        return state;
     Set();
     state = osContinue;
     }
  return state;
}

// --- cMenuEditTimeItem -----------------------------------------------------

cMenuEditTimeItem::cMenuEditTimeItem(const char *Name, int *Value)
:cMenuEditItem(Name)
{
  value = Value;
  hh = *value / 100;
  mm = *value % 100;
  pos = 0;
  Set();
}

void cMenuEditTimeItem::Set(void)
{
  char buf[10];
  switch (pos) {
    case 1:  snprintf(buf, sizeof(buf), "%01d-:--", hh / 10); break;
    case 2:  snprintf(buf, sizeof(buf), "%02d:--", hh); break;
    case 3:  snprintf(buf, sizeof(buf), "%02d:%01d-", hh, mm / 10); break;
    default: snprintf(buf, sizeof(buf), "%02d:%02d", hh, mm);
    }
  SetValue(buf);
}

eOSState cMenuEditTimeItem::ProcessKey(eKeys Key)
{
  eOSState state = cMenuEditItem::ProcessKey(Key);

  if (state == osUnknown) {
     if (k0 <= Key && Key <= k9) {
        if (fresh || pos > 3) {
           pos = 0;
           fresh = false;
           }
        int n = Key - k0;
        switch (pos) {
          case 0: if (n <= 2) {
                     hh = n * 10;
                     mm = 0;
                     pos++;
                     }
                  break;
          case 1: if (hh + n <= 23) {
                     hh += n;
                     pos++;
                     }
                  break;
          case 2: if (n <= 5) {
                     mm += n * 10;
                     pos++;
                     }
                  break;
          case 3: if (mm + n <= 59) {
                     mm += n;
                     pos++;
                     }
                  break;
          }
        }
     else if (NORMALKEY(Key) == kLeft) { // TODO might want to increase the delta if repeated quickly?
        if (--mm < 0) {
           mm = 59;
           if (--hh < 0)
              hh = 23;
           }
        fresh = true;
        }
     else if (NORMALKEY(Key) == kRight) {
        if (++mm > 59) {
           mm = 0;
           if (++hh > 23)
              hh = 0;
           }
        fresh = true;
        }
     else
        return state;
     *value = hh * 100 + mm;
     Set();
     state = osContinue;
     }
  return state;
}

// --- cMenuSetupPage --------------------------------------------------------

cMenuSetupPage::cMenuSetupPage(void)
:cOsdMenu("", 33)
{
  plugin = NULL;
}

void cMenuSetupPage::SetSection(const char *Section)
{
  char buf[40];
  snprintf(buf, sizeof(buf), "%s - %s", tr("Setup"), Section);
  SetTitle(buf);
}

eOSState cMenuSetupPage::ProcessKey(eKeys Key)
{
  eOSState state = cOsdMenu::ProcessKey(Key);

  if (state == osUnknown) {
     switch (Key) {
       case kOk: Store();
                 state = osBack;
                 break;
       default: break;
       }
     }
  return state;
}

void cMenuSetupPage::SetPlugin(cPlugin *Plugin)
{
  plugin = Plugin;
  char buf[40];
  snprintf(buf, sizeof(buf), "%s '%s'", tr("Plugin"), plugin->Name());
  SetSection(buf);
}

void cMenuSetupPage::SetupStore(const char *Name, const char *Value)
{
  if (plugin)
     plugin->SetupStore(Name, Value);
}

void cMenuSetupPage::SetupStore(const char *Name, int Value)
{
  if (plugin)
     plugin->SetupStore(Name, Value);
}
