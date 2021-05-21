/*
 * i18n.c: Internationalization
 *
 * See the main source file 'vdr.c' for copyright information and
 * how to reach the author.
 *
 * $Id: i18n.c 5.1 2021/05/21 09:50:57 kls Exp $
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
#include <unistd.h>
#include "tools.h"

// TRANSLATORS: The name of the language, as written natively
const char *LanguageName = trNOOP("LanguageName$English");
// TRANSLATORS: The 3-letter code of the language
const char *LanguageCode = trNOOP("LanguageCode$eng");

// List of known language codes with aliases.
// Actually we could list all codes from http://www.loc.gov/standards/iso639-2
// here, but that would be several hundreds - and for most of them it's unlikely
// they're ever going to be used...

const char *LanguageCodeList[] = {
  "eng,dos",
  "deu,ger",
  "alb,sqi",
  "ara",
  "bos",
  "bul",
  "cat,cln",
  "chi,zho",
  "cze,ces",
  "dan",
  "dut,nla,nld",
  "ell,gre",
  "esl,spa",
  "est",
  "eus,baq",
  "fin,suo",
  "fra,fre",
  "hrv",
  "hun",
  "iri,gle", // 'NorDig'
  "ita",
  "jpn",
  "lav",
  "lit",
  "ltz",
  "mac,mkd",
  "mlt",
  "nor",
  "pol",
  "por",
  "rom,rum",
  "rus",
  "slk,slo",
  "slv",
  "smi",     // 'NorDig' Sami language (Norway, Sweden, Finnland, Russia)
  "srb,srp,scr,scc",
  "sve,swe",
  "tur",
  "ukr",
  NULL
  };

struct tSpecialLc { const char *Code; const char *Name; };
const struct tSpecialLc SpecialLanguageCodeList[] = {
  { "qaa", trNOOP("LanguageName$original language (qaa)") },
  { "qad", trNOOP("LanguageName$audio description (qad)") },
  { "mis", trNOOP("LanguageName$uncoded languages (mis)") },
  { "mul", trNOOP("LanguageName$multiple languages (mul)") },
  { "nar", trNOOP("LanguageName$narrative (nar)") },
  { "und", trNOOP("LanguageName$undetermined (und)") },
  { "zxx", trNOOP("LanguageName$no linguistic content (zxx)") },
  { NULL, NULL }
  };

static cString I18nLocaleDir;

static cStringList LanguageLocales;
static cStringList LanguageNames;
static cStringList LanguageCodes;

static int NumLocales = 1;
static int NumLanguages = 1;
static int CurrentLanguage = 0;

static bool ContainsCode(const char *Codes, const char *Code)
{
  while (*Codes) {
        int l = 0;
        for ( ; l < 3 && Code[l]; l++) {
            if (Codes[l] != tolower(Code[l]))
               break;
            }
        if (l == 3)
           return true;
        Codes++;
        }
  return false;
}

static const char *SkipContext(const char *s)
{
  const char *p = strchr(s, '$');
  return p ? p + 1 : s;
}

static void SetEnvLanguage(const char *Locale)
{
  setenv("LANGUAGE", Locale, 1);
  extern int _nl_msg_cat_cntr;
  ++_nl_msg_cat_cntr;
}

static void SetLanguageNames(void)
{
  // Update the translation for special language codes:
  int i = NumLanguages;
  for (const struct tSpecialLc *slc = SpecialLanguageCodeList; slc->Code; slc++) {
      const char *TranslatedName = gettext(slc->Name);
      free(LanguageNames[i]);
      LanguageNames[i++] = strdup(TranslatedName != slc->Name ? TranslatedName : SkipContext(slc->Name));
      }
}

void I18nInitialize(const char *LocaleDir)
{
  I18nLocaleDir = LocaleDir;
  LanguageLocales.Append(strdup(I18N_DEFAULT_LOCALE));
  LanguageNames.Append(strdup(SkipContext(LanguageName)));
  LanguageCodes.Append(strdup(LanguageCodeList[0]));
  textdomain("vdr");
  bindtextdomain("vdr", I18nLocaleDir);
  cFileNameList Locales(I18nLocaleDir, true);
  if (Locales.Size() > 0) {
     char *OldLocale = strdup(setlocale(LC_MESSAGES, NULL));
     for (int i = 0; i < Locales.Size(); i++) {
         cString FileName = cString::sprintf("%s/%s/LC_MESSAGES/vdr.mo", *I18nLocaleDir, Locales[i]);
         if (access(FileName, F_OK) == 0) { // found a locale with VDR texts
            if (NumLocales < I18N_MAX_LANGUAGES - 1) {
               SetEnvLanguage(Locales[i]);
               const char *TranslatedLanguageName = gettext(LanguageName);
               if (TranslatedLanguageName != LanguageName) {
                  NumLocales++;
                  if (strstr(OldLocale, Locales[i]) == OldLocale)
                     CurrentLanguage = LanguageLocales.Size();
                  LanguageLocales.Append(strdup(Locales[i]));
                  LanguageNames.Append(strdup(TranslatedLanguageName));
                  const char *Code = gettext(LanguageCode);
                  for (const char **lc = LanguageCodeList; *lc; lc++) {
                      if (ContainsCode(*lc, Code)) {
                         Code = *lc;
                         break;
                         }
                      }
                  LanguageCodes.Append(strdup(Code));
                  }
               }
            else {
               esyslog("ERROR: too many locales - increase I18N_MAX_LANGUAGES!");
               break;
               }
            }
         }
     SetEnvLanguage(LanguageLocales[CurrentLanguage]);
     free(OldLocale);
     dsyslog("found %d locales in %s", NumLocales - 1, *I18nLocaleDir);
     }
  // Prepare any known language codes for which there was no locale:
  NumLanguages = NumLocales;
  for (const char **lc = LanguageCodeList; *lc; lc++) {
      bool Found = false;
      for (int i = 0; i < LanguageCodes.Size(); i++) {
          if (strcmp(*lc, LanguageCodes[i]) == 0) {
             Found = true;
             break;
             }
          }
      if (!Found) {
         dsyslog("no locale for language code '%s'", *lc);
         NumLanguages++;
         LanguageLocales.Append(strdup(I18N_DEFAULT_LOCALE));
         LanguageNames.Append(strdup(*lc));
         LanguageCodes.Append(strdup(*lc));
         }
      }
  // Add special language codes and names:
  for (const struct tSpecialLc *slc = SpecialLanguageCodeList; slc->Code; slc++) {
      const char *TranslatedName = gettext(slc->Name);
      LanguageNames.Append(strdup( TranslatedName != slc->Name ? TranslatedName : SkipContext(slc->Name)));
      LanguageCodes.Append(strdup(slc->Code));
      }
}

void I18nRegister(const char *Plugin)
{
  cString Domain = cString::sprintf("vdr-%s", Plugin);
  bindtextdomain(Domain, I18nLocaleDir);
}

void I18nSetLocale(const char *Locale)
{
  if (Locale && *Locale) {
     int i = LanguageLocales.Find(Locale);
     if (i >= 0) {
        CurrentLanguage = i;
        SetEnvLanguage(Locale);
        SetLanguageNames();
        }
     else
        dsyslog("unknown locale: '%s'", Locale);
     }
}

int I18nCurrentLanguage(void)
{
  return CurrentLanguage;
}

void I18nSetLanguage(int Language)
{
  if (Language < NumLanguages) {
     CurrentLanguage = Language;
     I18nSetLocale(I18nLocale(CurrentLanguage));
     }
}

int I18nNumLanguagesWithLocale(void)
{
  return NumLocales;
}

const cStringList *I18nLanguages(void)
{
  return &LanguageNames;
}

const char *I18nTranslate(const char *s, const char *Plugin)
{
  if (!s)
     return s;
  if (CurrentLanguage) {
     const char *t = Plugin ? dgettext(Plugin, s) : gettext(s);
     if (t != s)
        return t;
     }
  return SkipContext(s);
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
      if (ContainsCode(LanguageCodes[i], Code))
         return i;
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
