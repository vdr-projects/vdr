/*
 * i18n.c: Internationalization
 *
 * See the main source file 'vdr.c' for copyright information and
 * how to reach the author.
 *
 * $Id: i18n.c 1.306 2007/08/11 14:27:02 kls Exp $
 *
 *
 */

/*
 * In case an English phrase is used in more than one context (and might need
 * different translations in other languages) it can be preceded with an
 * arbitrary string to describe its context, separated from the actual phrase
 * by a '$' character (see for instance "Button$Stop" vs. "Stop").
 * Of course this means that no English phrase may contain the '$' character!
 * If this should ever become necessary, the existing '$' would have to be
 * replaced with something different...
 */

#include "i18n.h"
#include <ctype.h>
#include <libintl.h>
#include <locale.h>
#include "tools.h"

// TRANSLATORS: The name of the language, as written natively
const char *LanguageName = trNOOP("LanguageName$English");
// TRANSLATORS: The 3-letter code(s) of the language (separated by commas)
const char *LanguageCode = trNOOP("LanguageCode$eng,dos");

static char *I18nLocaleDir = LOCDIR;

static cStringList LanguageLocales;
static cStringList LanguageNames;
static cStringList LanguageCodes;

static int CurrentLanguage = 0;

static const char *SkipContext(const char *s)
{
  const char *p = strchr(s, '$');
  return p ? p + 1 : s;
}

void I18nInitialize(void)
{
  LanguageNames.Append(strdup(SkipContext(LanguageName)));
  LanguageCodes.Append(strdup(SkipContext(LanguageCode)));
  LanguageLocales.Append(strdup(I18N_DEFAULT_LOCALE));
  textdomain("vdr");
  bindtextdomain("vdr", I18nLocaleDir);
  cFileNameList Locales(I18nLocaleDir, true);
  if (Locales.Size() > 0) {
     dsyslog("found %d locales in %s", Locales.Size(), I18nLocaleDir);
     char *OldLocale = strdup(setlocale(LC_MESSAGES, NULL));
     for (int i = 0; i < Locales.Size(); i++) {
         if (i < I18N_MAX_LANGUAGES - 1) {
            if (setlocale(LC_MESSAGES, Locales[i])) {
               LanguageLocales.Append(strdup(Locales[i]));
               LanguageNames.Append(strdup(gettext(LanguageName)));
               LanguageCodes.Append(strdup(gettext(LanguageCode)));
               if (strstr(OldLocale, Locales[i]) == OldLocale)
                  CurrentLanguage = LanguageLocales.Size() - 1;
               }
            }
         else
            esyslog("ERROR: too many locales - increase I18N_MAX_LANGUAGES!");
         }
     setlocale(LC_MESSAGES, OldLocale);
     free(OldLocale);
     }
}

void I18nRegister(const char *Plugin)
{
  bindtextdomain(Plugin, I18nLocaleDir);
}

void I18nSetLocale(const char *Locale)
{
  int i = LanguageLocales.Find(Locale);
  if (i >= 0) {
     CurrentLanguage = i;
     setlocale(LC_MESSAGES, Locale);
     }
  else
     dsyslog("unknown locale: '%s'", Locale);
}

int I18nCurrentLanguage(void)
{
  return CurrentLanguage;
}

void I18nSetLanguage(int Language)
{
  if (Language < LanguageNames.Size()) {
     CurrentLanguage = Language;
     I18nSetLocale(I18nLocale(CurrentLanguage));
     }
}

const cStringList *I18nLanguages(void)
{
  return &LanguageNames;
}

const char *I18nTranslate(const char *s, const char *Plugin)
{
  if (CurrentLanguage) {
     const char *t = s;
     if (Plugin)
        t = dgettext(Plugin, s);
     if (t == s)
        t = gettext(s);
     s = t;
     }
  const char *p = strchr(s, '$');
  return p ? p + 1 : s;
}

const char *I18nLocale(int Language)
{
  return 0 <= Language && Language < LanguageLocales.Size() ? LanguageLocales[Language] : NULL;
}

const char *I18nLanguageCode(int Language)
{
  return 0 <= Language && Language < LanguageCodes.Size() ? LanguageCodes[Language] : NULL;
}

int I18nLanguageIndex(const char *Code)
{
  for (int i = 0; i < LanguageCodes.Size(); i++) {
      const char *s = LanguageCodes[i];
      while (*s) {
            int l = 0;
            for ( ; l < 3 && Code[l]; l++) {
                if (s[l] != tolower(Code[l]))
                   break;
                }
            if (l == 3)
               return i;
            s++;
            }
      }
  //dsyslog("unknown language code: '%s'", Code);
  return -1;
}

const char *I18nNormalizeLanguageCode(const char *Code)
{
  for (int i = 0; i < 3; i++) {
      if (Code[i]) {
         // ETSI EN 300 468 defines language codes as consisting of three letters
         // according to ISO 639-2. This means that they are supposed to always consist
         // of exactly three letters in the range a-z - no digits, UTF-8 or other
         // funny characters. However, some broadcasters apparently don't have a
         // copy of the DVB standard (or they do, but are perhaps unable to read it),
         // so they put all sorts of non-standard stuff into the language codes,
         // like nonsense as "2ch" or "A 1" (yes, they even go as far as using
         // blanks!). Such things should go into the description of the EPG event's
         // ComponentDescriptor.
         // So, as a workaround for this broadcaster stupidity, let's ignore
         // language codes with unprintable characters...
         if (!isprint(Code[i])) {
            //dsyslog("invalid language code: '%s'", Code);
            return "???";
            }
         // ...and replace blanks with underlines (ok, this breaks the 'const'
         // of the Code parameter - but hey, it's them who started this):
         if (Code[i] == ' ')
            *((char *)&Code[i]) = '_';
         }
      else
         break;
      }
  int n = I18nLanguageIndex(Code);
  return n >= 0 ? I18nLanguageCode(n) : Code;
}

bool I18nIsPreferredLanguage(int *PreferredLanguages, const char *LanguageCode, int &OldPreference, int *Position)
{
  int pos = 1;
  bool found = false;
  while (LanguageCode) {
        int LanguageIndex = I18nLanguageIndex(LanguageCode);
        for (int i = 0; i < LanguageCodes.Size(); i++) {
            if (PreferredLanguages[i] < 0)
               break; // the language is not a preferred one
            if (PreferredLanguages[i] == LanguageIndex) {
               if (OldPreference < 0 || i < OldPreference) {
                  OldPreference = i;
                  if (Position)
                     *Position = pos;
                  found = true;
                  break;
                  }
               }
            }
        if ((LanguageCode = strchr(LanguageCode, '+')) != NULL) {
           LanguageCode++;
           pos++;
           }
        else if (pos == 1 && Position)
           *Position = 0;
        }
  if (OldPreference < 0) {
     OldPreference = LanguageCodes.Size(); // higher than the maximum possible value
     return true; // if we don't find a preferred one, we take the first one
     }
  return found;
}
