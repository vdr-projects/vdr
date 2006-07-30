/*
 * menuitems.c: General purpose menu items
 *
 * See the main source file 'vdr.c' for copyright information and
 * how to reach the author.
 *
 * $Id: menuitems.c 1.47 2006/07/30 09:09:30 kls Exp $
 */

#include "menuitems.h"
#include <ctype.h>
#include "i18n.h"
#include "plugin.h"
#include "remote.h"
#include "skins.h"
#include "status.h"

#define AUTO_ADVANCE_TIMEOUT  1500 // ms before auto advance when entering characters via numeric keys

const char *FileNameChars = " abcdefghijklmnopqrstuvwxyz0123456789-.#~,/_@";

// --- cMenuEditItem ---------------------------------------------------------

cMenuEditItem::cMenuEditItem(const char *Name)
{
  name = strdup(Name ? Name : "???");
}

cMenuEditItem::~cMenuEditItem()
{
  free(name);
}

void cMenuEditItem::SetValue(const char *Value)
{
  char *buffer = NULL;
  asprintf(&buffer, "%s:\t%s", name, Value);
  SetText(buffer, false);
  cStatus::MsgOsdCurrentItem(buffer);
}

// --- cMenuEditIntItem ------------------------------------------------------

cMenuEditIntItem::cMenuEditIntItem(const char *Name, int *Value, int Min, int Max, const char *MinString, const char *MaxString)
:cMenuEditItem(Name)
{
  value = Value;
  min = Min;
  max = Max;
  minString = MinString;
  maxString = MaxString;
  if (*value < min)
     *value = min;
  else if (*value > max)
     *value = max;
  Set();
}

void cMenuEditIntItem::Set(void)
{
  if (minString && *value == min)
     SetValue(minString);
  else if (maxString && *value == max)
     SetValue(maxString);
  else {
     char buf[16];
     snprintf(buf, sizeof(buf), "%d", *value);
     SetValue(buf);
     }
}

eOSState cMenuEditIntItem::ProcessKey(eKeys Key)
{
  eOSState state = cMenuEditItem::ProcessKey(Key);

  if (state == osUnknown) {
     int newValue = *value;
     bool IsRepeat = Key & k_Repeat;
     Key = NORMALKEY(Key);
     switch (Key) {
       case kNone: break;
       case k0 ... k9:
            if (fresh) {
               newValue = 0;
               fresh = false;
               }
            newValue = newValue * 10 + (Key - k0);
            break;
       case kLeft: // TODO might want to increase the delta if repeated quickly?
            newValue = *value - 1;
            fresh = true;
            if (!IsRepeat && newValue < min && max != INT_MAX)
               newValue = max;
            break;
       case kRight:
            newValue = *value + 1;
            fresh = true;
            if (!IsRepeat && newValue > max && min != INT_MIN)
               newValue = min;
            break;
       default:
            if (*value < min) { *value = min; Set(); }
            if (*value > max) { *value = max; Set(); }
            return state;
       }
     if (newValue != *value && (!fresh || min <= newValue) && newValue <= max) {
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

cMenuEditBitItem::cMenuEditBitItem(const char *Name, uint *Value, uint Mask, const char *FalseString, const char *TrueString)
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
  allowed = strdup(Allowed ? Allowed : "");
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
  orgValue = NULL;
  value = Value;
  length = Length;
  allowed = strdup(Allowed ? Allowed : "");
  pos = -1;
  insert = uppercase = false;
  newchar = true;
  charMap = tr(" 0\t-.#~,/_@1\tabc2\tdef3\tghi4\tjkl5\tmno6\tpqrs7\ttuv8\twxyz9");
  currentChar = NULL;
  lastKey = kNone;
  Set();
}

cMenuEditStrItem::~cMenuEditStrItem()
{
  free(orgValue);
  free(allowed);
}

void cMenuEditStrItem::SetHelpKeys(void)
{
  if (InEditMode())
     cSkinDisplay::Current()->SetButtons(tr("Button$ABC/abc"), tr(insert ? "Button$Overwrite" : "Button$Insert"), tr("Button$Delete"));
  else
     cSkinDisplay::Current()->SetButtons(NULL);
}

void cMenuEditStrItem::AdvancePos(void)
{
  if (pos < length - 2 && pos < int(strlen(value)) ) {
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
}

void cMenuEditStrItem::Set(void)
{
  char buf[1000];

  if (InEditMode()) {
     // This is an ugly hack to make editing strings work with the 'skincurses' plugin.
     const cFont *font = dynamic_cast<cSkinDisplayMenu *>(cSkinDisplay::Current())->GetTextAreaFont(false);
     if (!font || font->Width("W") != 1) // all characters have with == 1 in the font used by 'skincurses'
        font = cFont::GetFont(fontOsd);
     strncpy(buf, value, pos);
     snprintf(buf + pos, sizeof(buf) - pos - 2, insert && newchar ? "[]%c%s" : "[%c]%s", *(value + pos), value + pos + 1);
     int width = cSkinDisplay::Current()->EditableWidth();
     if (font->Width(buf) <= width) {
        // the whole buffer fits on the screen
        SetValue(buf);
        return;
        }
     width -= font->Width('>'); // assuming '<' and '>' have the same width
     int w = 0;
     int i = 0;
     int l = strlen(buf);
     while (i < l && w <= width)
           w += font->Width(buf[i++]);
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
           w += font->Width(buf[i--]);
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
  bool SameKey = NORMALKEY(Key) == lastKey;
  if (Key != kNone)
     lastKey = NORMALKEY(Key);
  else if (!newchar && k0 <= lastKey && lastKey <= k9 && autoAdvanceTimeout.TimedOut()) {
     AdvancePos();
     newchar = true;
     currentChar = NULL;
     Set();
     return osContinue;
     }
  switch (Key) {
    case kRed:   // Switch between upper- and lowercase characters
                 if (InEditMode()) {
                    if (!insert || !newchar) {
                       uppercase = !uppercase;
                       value[pos] = uppercase ? toupper(value[pos]) : tolower(value[pos]);
                       }
                    }
                 else
                    return osUnknown;
                 break;
    case kGreen: // Toggle insert/overwrite modes
                 if (InEditMode()) {
                    insert = !insert;
                    newchar = true;
                    SetHelpKeys();
                    }
                 else
                    return osUnknown;
                 break;
    case kYellow|k_Repeat:
    case kYellow: // Remove the character at current position; in insert mode it is the character to the right of cursor
                 if (InEditMode()) {
                    if (strlen(value) > 1) {
                       if (!insert || pos < int(strlen(value)) - 1)
                          memmove(value + pos, value + pos + 1, strlen(value) - pos);
                       else if (insert && pos == int(strlen(value)) - 1)
                          value[pos] = ' '; // in insert mode, deleting the last character replaces it with a blank to keep the cursor position
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
                 else
                    return osUnknown;
                 break;
    case kBlue|k_Repeat:
    case kBlue:  // consume the key only if in edit-mode
                 if (InEditMode())
                    ;
                 else
                    return osUnknown;
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
    case kRight: AdvancePos();
                 if (pos == 0) {
                    orgValue = strdup(value);
                    SetHelpKeys();
                    }
                 break;
    case kUp|k_Repeat:
    case kUp:
    case kDown|k_Repeat:
    case kDown:  if (InEditMode()) {
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
    case k0|k_Repeat ... k9|k_Repeat:
    case k0 ... k9: {
                 if (InEditMode()) {
                    if (!SameKey) {
                       if (!newchar)
                          AdvancePos();
                       currentChar = NULL;
                       }
                    if (insert && newchar) {
                       // create a new character in insert mode
                       if (int(strlen(value)) < length - 1) {
                          memmove(value + pos + 1, value + pos, strlen(value) - pos + 1);
                          value[pos] = ' ';
                          }
                       }
                    if (!currentChar || !*currentChar || *currentChar == '\t') {
                       // find the beginning of the character map entry for Key
                       int n = NORMALKEY(Key) - k0;
                       currentChar = charMap;
                       while (n > 0 && *currentChar) {
                             if (*currentChar++ == '\t')
                                n--;
                             }
                       }
                    if (*currentChar && *currentChar != '\t') {
                       value[pos] = *currentChar;
                       if (uppercase)
                          value[pos] = toupper(value[pos]);
                       currentChar++;
                       }
                    newchar = false;
                    autoAdvanceTimeout.Set(AUTO_ADVANCE_TIMEOUT);
                    }
                 else
                    return cMenuEditItem::ProcessKey(Key);
                 }
                 break;
    case kBack:
    case kOk:    if (InEditMode()) {
                    if (Key == kBack && orgValue) {
                       strcpy(value, orgValue);
                       free(orgValue);
                       orgValue = NULL;
                       }
                    pos = -1;
                    newchar = true;
                    stripspace(value);
                    SetHelpKeys();
                    break;
                    }
                 // run into default
    default:     if (InEditMode() && BASICKEY(Key) == kKbd) {
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

// --- cMenuEditChanItem -----------------------------------------------------

cMenuEditChanItem::cMenuEditChanItem(const char *Name, int *Value, const char *NoneString)
:cMenuEditIntItem(Name, Value, NoneString ? 0 : 1, Channels.MaxNumber())
{
  noneString = NoneString;
  Set();
}

void cMenuEditChanItem::Set(void)
{
  if (*value > 0) {
     char buf[255];
     cChannel *channel = Channels.GetByNumber(*value);
     snprintf(buf, sizeof(buf), "%d %s", *value, channel ? channel->Name() : "");
     SetValue(buf);
     }
  else if (noneString)
     SetValue(noneString);
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
                   if (channel)
                      *value = channel->Number();
                   else if (delta < 0 && noneString)
                      *value = 0;
                   Set();
                 }
                 break;
    default: return cMenuEditIntItem::ProcessKey(Key);
    }
  return osContinue;
}

// --- cMenuEditTranItem -----------------------------------------------------

cMenuEditTranItem::cMenuEditTranItem(const char *Name, int *Value, int *Source)
:cMenuEditChanItem(Name, &number, "-")
{
  number = 0;
  source = Source;
  transponder = Value;
  cChannel *channel = Channels.First();
  while (channel) {
        if (!channel->GroupSep() && *source == channel->Source() && ISTRANSPONDER(channel->Transponder(), *Value)) {
           number = channel->Number();
           break;
           }
        channel = (cChannel *)channel->Next();
        }
  Set();
}

eOSState cMenuEditTranItem::ProcessKey(eKeys Key)
{
  eOSState state = cMenuEditChanItem::ProcessKey(Key);
  cChannel *channel = Channels.GetByNumber(number);
  if (channel) {
     *source = channel->Source();
     *transponder = channel->Transponder();
     }
  else {
     *source = 0;
     *transponder = 0;
     }
  return state;
}

// --- cMenuEditDateItem -----------------------------------------------------

static int ParseWeekDays(const char *s)
{
  time_t day;
  int weekdays;
  return cTimer::ParseDay(s, day, weekdays) ? weekdays : 0;
}

int cMenuEditDateItem::days[] = { ParseWeekDays("M------"),
                                  ParseWeekDays("-T-----"),
                                  ParseWeekDays("--W----"),
                                  ParseWeekDays("---T---"),
                                  ParseWeekDays("----F--"),
                                  ParseWeekDays("-----S-"),
                                  ParseWeekDays("------S"),
                                  ParseWeekDays("MTWTF--"),
                                  ParseWeekDays("MTWTFS-"),
                                  ParseWeekDays("MTWTFSS"),
                                  ParseWeekDays("-----SS"),
                                  0 };

cMenuEditDateItem::cMenuEditDateItem(const char *Name, time_t *Value, int *WeekDays)
:cMenuEditItem(Name)
{
  value = Value;
  weekdays = WeekDays;
  oldvalue = 0;
  dayindex = weekdays ? FindDayIndex(*weekdays) : 0;
  Set();
}

int cMenuEditDateItem::FindDayIndex(int WeekDays)
{
  for (unsigned int i = 0; i < sizeof(days) / sizeof(int); i++)
      if (WeekDays == days[i])
         return i;
  return 0;
}

void cMenuEditDateItem::Set(void)
{
#define DATEBUFFERSIZE 32
  char buf[DATEBUFFERSIZE];
  if (weekdays && *weekdays) {
     SetValue(cTimer::PrintDay(0, *weekdays));
     return;
     }
  else if (*value) {
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
     time_t now = time(NULL);
     if (NORMALKEY(Key) == kLeft) { // TODO might want to increase the delta if repeated quickly?
        if (!weekdays || !*weekdays) {
           // Decrement single day:
           time_t v = *value;
           v -= SECSINDAY;
           if (v < now) {
              if (now <= v + SECSINDAY) { // switched from tomorrow to today
                 if (!weekdays)
                    v = 0;
                 }
              else if (weekdays) { // switched from today to yesterday, so enter weekdays mode
                 v = 0;
                 dayindex = sizeof(days) / sizeof(int) - 2;
                 *weekdays = days[dayindex];
                 }
              else // don't go before today
                 v = *value;
              }
           *value = v;
           }
        else {
           // Decrement weekday index:
           if (dayindex > 0)
              *weekdays = days[--dayindex];
           }
        }
     else if (NORMALKEY(Key) == kRight) {
        if (!weekdays || !*weekdays) {
           // Increment single day:
           if (!*value)
              *value = cTimer::SetTime(now, 0);
           *value += SECSINDAY;
           }
        else {
           // Increment weekday index:
           *weekdays = days[++dayindex];
           if (!*weekdays) { // was last weekday entry, so switch to today
              *value = cTimer::SetTime(now, 0);
              dayindex = 0;
              }
           }
        }
     else if (weekdays) {
        if (Key == k0) {
           // Toggle between weekdays and single day:
           if (*weekdays) {
              *value = cTimer::SetTime(oldvalue ? oldvalue : now, 0);
              oldvalue = 0;
              *weekdays = 0;
              }
           else {
              *weekdays = days[cTimer::GetWDay(*value)];
              dayindex = FindDayIndex(*weekdays);
              oldvalue = *value;
              *value = 0;
              }
           }
        else if (k1 <= Key && Key <= k7) {
           // Toggle individual weekdays:
           if (*weekdays) {
              int v = *weekdays ^ (1 << (Key - k1));
              if (v != 0)
                 *weekdays = v; // can't let this become all 0
              }
           }
        else
           return state;
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
