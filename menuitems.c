/*
 * menuitems.c: General purpose menu items
 *
 * See the main source file 'vdr.c' for copyright information and
 * how to reach the author.
 *
 * $Id: menuitems.c 5.3 2025/01/29 10:20:17 kls Exp $
 */

#include "menuitems.h"
#include <ctype.h>
#include <math.h>
#include <wctype.h>
#include "i18n.h"
#include "plugin.h"
#include "remote.h"
#include "skins.h"
#include "status.h"

#define AUTO_ADVANCE_TIMEOUT  1500 // ms before auto advance when entering characters via numeric keys

const char *FileNameChars = trNOOP("FileNameChars$ abcdefghijklmnopqrstuvwxyz0123456789-.,#~\\^$[]|()*+?{}/:%@&");

// --- cMenuEditItem ---------------------------------------------------------

cMenuEditItem::cMenuEditItem(const char *Name)
{
  name = strdup(Name ? Name : "???");
  SetHelp(NULL);
}

cMenuEditItem::~cMenuEditItem()
{
  free(name);
}

void cMenuEditItem::SetValue(const char *Value)
{
  cString buffer = cString::sprintf("%s:\t%s", name, Value);
  SetText(buffer);
}

void cMenuEditItem::SetHelp(const char *Red, const char *Green, const char *Yellow, const char *Blue)
{
  // strings are NOT copied - must be constants!!!
  helpRed    = Red;
  helpGreen  = Green;
  helpYellow = Yellow;
  helpBlue   = Blue;
  helpDisplayed = false;
}

bool cMenuEditItem::DisplayHelp(bool Current)
{
  bool HasHelp = helpRed || helpGreen || helpYellow || helpBlue;
  if (HasHelp && !helpDisplayed && Current) {
     cSkinDisplay::Current()->SetButtons(helpRed, helpGreen, helpYellow, helpBlue);
     cStatus::MsgOsdHelpKeys(helpRed, helpGreen, helpYellow, helpBlue);
     }
  helpDisplayed = Current;
  return HasHelp;
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

// --- cMenuEditIntxItem -----------------------------------------------------

cMenuEditIntxItem::cMenuEditIntxItem(const char *Name, int *Value, int Min, int Max, int Factor, const char *NegString, const char *PosString)
:cMenuEditIntItem(Name, Value, Min, Max)
{
  factor = ::max(Factor, 1);
  negString = NegString;
  posString = PosString;
  Set();
}

void cMenuEditIntxItem::SetHelpKeys(void)
{
  if (negString && posString)
     SetHelp(NULL, (*value < 0) ? posString : negString);
}

void cMenuEditIntxItem::Set(void)
{
  const char *s = (*value < 0) ? negString : posString;
  int v = *value;
  if (negString && posString)
     v = abs(v);
  SetValue(cString::sprintf(s ? "%.*f %s" : "%.*f", factor / 10, double(v) / factor, s));
  SetHelpKeys();
}

eOSState cMenuEditIntxItem::ProcessKey(eKeys Key)
{
  eOSState state = cMenuEditIntItem::ProcessKey(Key);
  if (state == osUnknown) {
     switch (Key) {
       case kGreen: if (negString && posString) {
                       *value = -*value;
                       Set();
                       state = osContinue;
                       }
                    break;
       default: ;
       }
     }
  return state;
}

// --- cMenuEditPrcItem ------------------------------------------------------

cMenuEditPrcItem::cMenuEditPrcItem(const char *Name, double *Value, double Min, double Max, int Decimals)
:cMenuEditItem(Name)
{
  value = Value;
  min = Min;
  max = Max;
  decimals = Decimals;
  factor = 100;
  while (Decimals-- > 0)
        factor *= 10;
  if (*value < min)
     *value = min;
  else if (*value > max)
     *value = max;
  Set();
}

void cMenuEditPrcItem::Set(void)
{
  char buf[16];
  snprintf(buf, sizeof(buf), "%.*f", decimals, *value * 100);
  SetValue(buf);
}

eOSState cMenuEditPrcItem::ProcessKey(eKeys Key)
{
  eOSState state = cMenuEditItem::ProcessKey(Key);

  if (state == osUnknown) {
     double newValue = round(*value * factor); // avoids precision problems
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
            newValue--;
            fresh = true;
            break;
       case kRight:
            newValue++;
            fresh = true;
            break;
       default:
            if (*value < min) { *value = min; Set(); }
            if (*value > max) { *value = max; Set(); }
            return state;
       }
     newValue /= factor;
     if (!DoubleEqual(newValue, *value) && (!fresh || min <= newValue) && newValue <= max) {
        *value = newValue;
        Set();
        }
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
  buf[0] = *value;
  buf[1] = '\0';
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
  allowed = Allowed ? Allowed : tr(FileNameChars);
  pos = -1;
  offset = 0;
  keepSpace = false;
  macro = -1;
  lastMacro = -1;
  macros = NULL;
  insert = uppercase = false;
  newchar = true;
  lengthUtf8 = 0;
  valueUtf8 = NULL;
  allowedUtf8 = NULL;
  charMapUtf8 = NULL;
  currentCharUtf8 = NULL;
  lastKey = kNone;
  Set();
}

cMenuEditStrItem::~cMenuEditStrItem()
{
  delete[] valueUtf8;
  delete[] allowedUtf8;
  delete[] charMapUtf8;
}

void cMenuEditStrItem::SetMacros(const char **Macros)
{
  macros = Macros;
  macro = 0;
  lastMacro = -1;
}

void cMenuEditStrItem::EnterEditMode(void)
{
  if (!valueUtf8) {
     valueUtf8 = new uint[length];
     lengthUtf8 = Utf8ToArray(value, valueUtf8, length);
     int l = strlen(allowed) + 1;
     allowedUtf8 = new uint[l];
     Utf8ToArray(allowed, allowedUtf8, l);
     const char *charMap = tr("CharMap$ 0\t-.,1#~\\^$[]|()*+?{}/:%@&\tabc2\tdef3\tghi4\tjkl5\tmno6\tpqrs7\ttuv8\twxyz9");
     l = strlen(charMap) + 1;
     charMapUtf8 = new uint[l];
     Utf8ToArray(charMap, charMapUtf8, l);
     currentCharUtf8 = charMapUtf8;
     AdvancePos();
     }
}

void cMenuEditStrItem::LeaveEditMode(bool SaveValue)
{
  if (valueUtf8) {
     if (SaveValue) {
        Utf8FromArray(valueUtf8, value, length);
        if (!keepSpace)
           stripspace(value);
        }
     lengthUtf8 = 0;
     delete[] valueUtf8;
     valueUtf8 = NULL;
     delete[] allowedUtf8;
     allowedUtf8 = NULL;
     delete[] charMapUtf8;
     charMapUtf8 = NULL;
     pos = -1;
     offset = 0;
     newchar = true;
     }
}

void cMenuEditStrItem::SetHelpKeys(void)
{
  if (InEditMode())
     SetHelp(tr("Button$ABC/abc"), insert ? tr("Button$Overwrite") : tr("Button$Insert"), tr("Button$Delete"), macros ? tr("Button$Macro") : NULL);
  else
     SetHelp(NULL);
}

uint *cMenuEditStrItem::IsAllowed(uint c)
{
  if (allowedUtf8) {
     for (uint *a = allowedUtf8; *a; a++) {
         if (c == *a)
            return a;
         }
     }
  return NULL;
}

void cMenuEditStrItem::AdvancePos(void)
{
  if (pos < length - 2 && pos < lengthUtf8) {
     if (++pos >= lengthUtf8) {
        if (pos >= 2 && valueUtf8[pos - 1] == ' ' && valueUtf8[pos - 2] == ' ')
           pos--; // allow only two blanks at the end
        else {
           valueUtf8[pos] = ' ';
           valueUtf8[pos + 1] = 0;
           lengthUtf8++;
           }
        }
     }
  newchar = true;
  if (!insert && Utf8is(alpha, valueUtf8[pos]))
     uppercase = Utf8is(upper, valueUtf8[pos]);
}

void cMenuEditStrItem::Set(void)
{
  if (InEditMode()) {
     const cFont *font = dynamic_cast<cSkinDisplayMenu *>(cSkinDisplay::Current())->GetTextAreaFont(false);

     int width = cSkinDisplay::Current()->EditableWidth();
     width -= font->Width("[]");
     width -= font->Width("<>"); // reserving this anyway makes the whole thing simpler

     if (pos < offset)
        offset = pos;
     int WidthFromOffset = 0;
     int EndPos = lengthUtf8;
     for (int i = offset; i < lengthUtf8; i++) {
         WidthFromOffset += font->Width(valueUtf8[i]);
         if (WidthFromOffset > width) {
            if (pos >= i) {
               do {
                  WidthFromOffset -= font->Width(valueUtf8[offset]);
                  offset++;
                  } while (WidthFromOffset > width && offset < pos);
               EndPos = pos + 1;
               }
            else {
               EndPos = i;
               break;
               }
            }
         }

     char buf[1000];
     char *p = buf;
     if (offset)
        *p++ = '<';
     p += Utf8FromArray(valueUtf8 + offset, p, sizeof(buf) - (p - buf), pos - offset);
     *p++ = '[';
     if (insert && newchar)
        *p++ = ']';
     p += Utf8FromArray(&valueUtf8[pos], p, sizeof(buf) - (p - buf), 1);
     if (!(insert && newchar))
        *p++ = ']';
     p += Utf8FromArray(&valueUtf8[pos + 1], p, sizeof(buf) - (p - buf), EndPos - pos - 1);
     if (EndPos != lengthUtf8)
        *p++ = '>';
     *p = 0;

     SetValue(buf);
     }
  else
     SetValue(value);
}

uint cMenuEditStrItem::Inc(uint c, bool Up)
{
  uint *p = IsAllowed(c);
  if (!p)
     p = allowedUtf8;
  if (Up) {
     if (!*++p)
        p = allowedUtf8;
     }
  else if (--p < allowedUtf8) {
     p = allowedUtf8;
     while (*p && *(p + 1))
           p++;
     }
  return *p;
}

void cMenuEditStrItem::Type(uint c)
{
  if (insert && lengthUtf8 < length - 1)
     Insert();
  valueUtf8[pos] = c;
  if (pos < length - 2)
     pos++;
  if (pos >= lengthUtf8) {
     valueUtf8[pos] = ' ';
     valueUtf8[pos + 1] = 0;
     lengthUtf8 = pos + 1;
     }
}

void cMenuEditStrItem::Insert(void)
{
  memmove(valueUtf8 + pos + 1, valueUtf8 + pos, (lengthUtf8 - pos + 1) * sizeof(*valueUtf8));
  lengthUtf8++;
  valueUtf8[pos] = ' ';
}

void cMenuEditStrItem::Delete(void)
{
  memmove(valueUtf8 + pos, valueUtf8 + pos + 1, (lengthUtf8 - pos) * sizeof(*valueUtf8));
  lengthUtf8--;
}

void cMenuEditStrItem::InsertMacro(void)
{
  if (!macros)
     return;
  if (lastMacro >= 0) {
     int l = strlen(macros[lastMacro]);
     while (l-- > 0)
           Delete();
     }
  const char *p = macros[macro];
  int oldPos = pos;
  bool oldInsert = insert;
  insert = true;
  newchar = true;
  while (*p) {
        Type(*p);
        p++;
        }
  insert = oldInsert;
  pos = oldPos;
  lastMacro = macro;
  if (!macros[++macro])
     macro = 0;
}

eOSState cMenuEditStrItem::ProcessKey(eKeys Key)
{
  bool SameKey = NORMALKEY(Key) == lastKey;
  if (Key != kNone) {
     lastKey = NORMALKEY(Key);
     if (Key != kBlue)
        lastMacro = -1;
     }
  else if (!newchar && k0 <= lastKey && lastKey <= k9 && autoAdvanceTimeout.TimedOut()) {
     AdvancePos();
     newchar = true;
     currentCharUtf8 = NULL;
     Set();
     return osContinue;
     }
  switch (int(Key)) {
    case kRed:   // Switch between upper- and lowercase characters
                 if (InEditMode()) {
                    if (!insert || !newchar) {
                       uppercase = !uppercase;
                       valueUtf8[pos] = uppercase ? Utf8to(upper, valueUtf8[pos]) : Utf8to(lower, valueUtf8[pos]);
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
    case kYellow: // Remove the character at the current position; in insert mode it is the character to the right of the cursor
                 if (InEditMode()) {
                    if (lengthUtf8 > 1) {
                       if (!insert || pos < lengthUtf8 - 1)
                          Delete();
                       else if (insert && pos == lengthUtf8 - 1)
                          valueUtf8[pos] = ' '; // in insert mode, deleting the last character replaces it with a blank to keep the cursor position
                       // reduce position, if we removed the last character
                       if (pos == lengthUtf8)
                          pos--;
                       }
                    else if (lengthUtf8 == 1)
                       valueUtf8[0] = ' '; // This is the last character in the string, replace it with a blank
                    if (Utf8is(alpha, valueUtf8[pos]))
                       uppercase = Utf8is(upper, valueUtf8[pos]);
                    newchar = true;
                    }
                 else
                    return osUnknown;
                 break;
    case kBlue|k_Repeat:
    case kBlue:  if (InEditMode())
                    InsertMacro();
                 else
                    return osUnknown;
                 break;
    case kLeft|k_Repeat:
    case kLeft:  if (pos > 0) {
                    if (!insert || newchar)
                       pos--;
                    newchar = true;
                    if (!insert && Utf8is(alpha, valueUtf8[pos]))
                       uppercase = Utf8is(upper, valueUtf8[pos]);
                    }
                 break;
    case kRight|k_Repeat:
    case kRight: if (InEditMode())
                    AdvancePos();
                 else {
                    EnterEditMode();
                    SetHelpKeys();
                    }
                 break;
    case kUp|k_Repeat:
    case kUp:
    case kDown|k_Repeat:
    case kDown:  if (InEditMode()) {
                    if (insert && newchar) {
                       // create a new character in insert mode
                       if (lengthUtf8 < length - 1)
                          Insert();
                       }
                    if (uppercase)
                       valueUtf8[pos] = Utf8to(upper, Inc(Utf8to(lower, valueUtf8[pos]), NORMALKEY(Key) == kUp));
                    else
                       valueUtf8[pos] =               Inc(              valueUtf8[pos],  NORMALKEY(Key) == kUp);
                    newchar = false;
                    }
                 else
                    return cMenuEditItem::ProcessKey(Key);
                 break;
    case k0|k_Repeat ... k9|k_Repeat:
    case k0 ... k9: {
                 if (InEditMode()) {
                    if (Setup.NumberKeysForChars) {
                       if (!SameKey) {
                          if (!newchar)
                             AdvancePos();
                          currentCharUtf8 = NULL;
                          }
                       if (!currentCharUtf8 || !*currentCharUtf8 || *currentCharUtf8 == '\t') {
                          // find the beginning of the character map entry for Key
                          int n = NORMALKEY(Key) - k0;
                          currentCharUtf8 = charMapUtf8;
                          while (n > 0 && *currentCharUtf8) {
                                if (*currentCharUtf8++ == '\t')
                                   n--;
                                }
                          // find first allowed character
                          while (*currentCharUtf8 && *currentCharUtf8 != '\t' && !IsAllowed(*currentCharUtf8))
                                currentCharUtf8++;
                          }
                       if (*currentCharUtf8 && *currentCharUtf8 != '\t') {
                          if (insert && newchar) {
                             // create a new character in insert mode
                             if (lengthUtf8 < length - 1)
                                Insert();
                             }
                          valueUtf8[pos] = *currentCharUtf8;
                          if (uppercase)
                             valueUtf8[pos] = Utf8to(upper, valueUtf8[pos]);
                          // find next allowed character
                          do {
                             currentCharUtf8++;
                             } while (*currentCharUtf8 && *currentCharUtf8 != '\t' && !IsAllowed(*currentCharUtf8));
                          newchar = false;
                          autoAdvanceTimeout.Set(AUTO_ADVANCE_TIMEOUT);
                          }
                       }
                    else
                       Type('0' + NORMALKEY(Key) - k0);
                    }
                 else
                    return cMenuEditItem::ProcessKey(Key);
                 }
                 break;
    case kBack:
    case kOk:    if (InEditMode()) {
                    LeaveEditMode(Key == kOk);
                    SetHelpKeys();
                    break;
                    }
                 // run into default
    default:     if (InEditMode() && BASICKEY(Key) == kKbd) {
                    int c = KEYKBD(Key);
                    if (c <= 0xFF) { // FIXME what about other UTF-8 characters?
                       if (IsAllowed(Utf8to(lower, c)))
                          Type(c);
                       else {
                          switch (c) {
                            case 0x7F: // backspace
                                       if (pos > 0) {
                                          pos--;
                                          return ProcessKey(kYellow);
                                          }
                                       break;
                            default: ;
                            }
                          }
                       }
                    else {
                       switch (c) {
                         case kfHome: pos = 0; break;
                         case kfEnd:  pos = lengthUtf8 - 1; break;
                         case kfIns:  return ProcessKey(kGreen);
                         case kfDel:  return ProcessKey(kYellow);
                         default: ;
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

// --- cMenuEditStrlItem -----------------------------------------------------

cMenuEditStrlItem::cMenuEditStrlItem(const char *Name, char *Value, int Length, const cStringList *Strings)
:cMenuEditIntItem(Name, &index, 0, Strings->Size() - 1)
{
  strings = Strings;
  value = Value;
  length = Length;
  index = strings->Find(value);
  if (index < 0)
     index = 0;
  Set();
}

void cMenuEditStrlItem::Set(void)
{
  strn0cpy(value, strings->At(index), length);
  SetValue(value);
}

// --- cMenuEditChanItem -----------------------------------------------------

cMenuEditChanItem::cMenuEditChanItem(const char *Name, int *Value, const char *NoneString)
:cMenuEditIntItem(Name, Value, NoneString ? 0 : 1, cChannels::MaxNumber())
{
  channelID = NULL;
  noneString = NoneString;
  dummyValue = 0;
  Set();
}

cMenuEditChanItem::cMenuEditChanItem(const char *Name, cString *ChannelID, const char *NoneString)
:cMenuEditIntItem(Name, &dummyValue, NoneString ? 0 : 1, cChannels::MaxNumber())
{
  channelID = ChannelID;
  noneString = NoneString;
  LOCK_CHANNELS_READ;
  const cChannel *Channel = Channels->GetByChannelID(tChannelID::FromString(*ChannelID));
  dummyValue = Channel ? Channel->Number() : 0;
  Set();
}

void cMenuEditChanItem::Set(void)
{
  if (*value > 0) {
     char buf[255];
     LOCK_CHANNELS_READ;
     const cChannel *Channel = Channels->GetByNumber(*value);
     snprintf(buf, sizeof(buf), "%d %s", *value, Channel ? Channel->Name() : "");
     SetValue(buf);
     if (channelID)
        *channelID = Channel ? Channel->GetChannelID().ToString() : "";
     }
  else if (noneString) {
     SetValue(noneString);
     if (channelID)
        *channelID = "";
     }
}

eOSState cMenuEditChanItem::ProcessKey(eKeys Key)
{
  int delta = 1;

  switch (int(Key)) {
    case kLeft|k_Repeat:
    case kLeft:  delta = -1;
    case kRight|k_Repeat:
    case kRight:
                 {
                   LOCK_CHANNELS_READ
                   const cChannel *Channel = Channels->GetByNumber(*value + delta, delta);
                   if (Channel)
                      *value = Channel->Number();
                   else if (delta < 0 && noneString)
                      *value = 0;
                   if (channelID)
                      *channelID = Channel ? Channel->GetChannelID().ToString() : "";
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
  LOCK_CHANNELS_READ;
  const cChannel *Channel = Channels->First();
  while (Channel) {
        if (!Channel->GroupSep() && *source == Channel->Source() && ISTRANSPONDER(Channel->Transponder(), *Value)) {
           number = Channel->Number();
           break;
           }
        Channel = Channels->Next(Channel);
        }
  Set();
}

eOSState cMenuEditTranItem::ProcessKey(eKeys Key)
{
  eOSState state = cMenuEditChanItem::ProcessKey(Key);
  LOCK_CHANNELS_READ
  if (const cChannel *Channel = Channels->GetByNumber(number)) {
     *source = Channel->Source();
     *transponder = Channel->Transponder();
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
  oldweekdays = 0;
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
     SetValue(cTimer::PrintDay(0, *weekdays, false));
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

void cMenuEditDateItem::ToggleRepeating(void)
{
  if (weekdays) {
     if (*weekdays) {
        *value = cTimer::SetTime(oldvalue ? oldvalue : time(NULL), 0);
        oldvalue = 0;
        oldweekdays = *weekdays;
        *weekdays = 0;
        }
     else {
        *weekdays = oldweekdays ? oldweekdays : days[cTimer::GetWDay(*value)];
        oldweekdays = 0;
        dayindex = FindDayIndex(*weekdays);
        oldvalue = *value;
        *value = 0;
        }
     Set();
     }
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
           ToggleRepeating();
           return osContinue; // ToggleRepeating) has already called Set()
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
  switch (pos) {
    case 1:  SetValue(cString::sprintf("%01d-:--", hh / 10)); break;
    case 2:  SetValue(cString::sprintf("%02d:--", hh)); break;
    case 3:  SetValue(cString::sprintf("%02d:%01d-", hh, mm / 10)); break;
    default: SetValue(cString::sprintf("%02d:%02d", hh, mm));
    }
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
          default: ;
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

// --- cMenuEditMapItem ------------------------------------------------------

cMenuEditMapItem::cMenuEditMapItem(const char *Name, int *Value, const tDvbParameterMap *Map, const char *ZeroString)
:cMenuEditItem(Name)
{
  value = Value;
  map = Map;
  zeroString = ZeroString;
  Set();
}

void cMenuEditMapItem::Set(void)
{
  const char *s = NULL;
  int n = MapToUser(*value, map, &s);
  if (n == 0 && zeroString)
     SetValue(zeroString);
  else if (n >= 0) {
     if (s)
        SetValue(s);
     else {
        char buf[16];
        snprintf(buf, sizeof(buf), "%d", n);
        SetValue(buf);
        }
     }
  else
     SetValue("???");
}

eOSState cMenuEditMapItem::ProcessKey(eKeys Key)
{
  eOSState state = cMenuEditItem::ProcessKey(Key);

  if (state == osUnknown) {
     int newValue = *value;
     int n = DriverIndex(*value, map);
     if (NORMALKEY(Key) == kLeft) { // TODO might want to increase the delta if repeated quickly?
        if (n-- > 0)
           newValue = map[n].driverValue;
        }
     else if (NORMALKEY(Key) == kRight) {
        if (map[++n].userValue >= 0)
           newValue = map[n].driverValue;
        }
     else
        return state;
     if (newValue != *value) {
        *value = newValue;
        Set();
        }
     state = osContinue;
     }
  return state;
}

// --- cMenuSetupPage --------------------------------------------------------

cMenuSetupPage::cMenuSetupPage(void)
:cOsdMenu("", 36)
{
  SetMenuCategory(mcSetup);
  plugin = NULL;
}

void cMenuSetupPage::SetSection(const char *Section)
{
  SetTitle(cString::sprintf("%s - %s", tr("Setup"), Section));
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
  SetMenuCategory(mcPluginSetup);
  plugin = Plugin;
  SetSection(cString::sprintf("%s '%s'", tr("Plugin"), plugin->Name()));
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
