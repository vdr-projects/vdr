/*
 * themes.c: Color themes used by skins
 *
 * See the main source file 'vdr.c' for copyright information and
 * how to reach the author.
 *
 * $Id: themes.c 2.2 2012/02/17 13:57:32 kls Exp $
 */

#include "themes.h"
#include <dirent.h>
#include <string.h>
#include "config.h"
#include "tools.h"

// --- cTheme ----------------------------------------------------------------

cTheme::cTheme(void)
{
  name = strdup("default");
  memset(colorNames, 0, sizeof(colorNames));
  memset(colorValues, 0, sizeof(colorValues));
  descriptions[0] = strdup("Default");
}

cTheme::~cTheme()
{
  free(name);
  for (int i = 0; i < MaxThemeColors; i++)
      free(colorNames[i]);
}

bool cTheme::FileNameOk(const char *FileName, bool SetName)
{
  const char *error = NULL;
  if (!isempty(FileName)) {
     const char *d = strrchr(FileName, '/');
     if (d)
        FileName = d + 1;
     const char *n = strchr(FileName, '-');
     if (n) {
        if (n > FileName) {
           if (!strchr(++n, '-')) {
              const char *e = strchr(n, '.');
              if (e && strcmp(e, ".theme") == 0) {
                 if (e - n >= 1) {
                    // FileName is ok
                    if (SetName) {
                       free(name);
                       name = strndup(n, e - n);
                       }
                    }
                 else
                    error = "missing theme name";
                 }
              else
                 error = "invalid extension";
              }
           else
              error = "too many '-'";
           }
        else
           error = "missing skin name";
        }
     else
        error = "missing '-'";
     }
  else
     error = "empty";
  if (error)
     esyslog("ERROR: invalid theme file name (%s): '%s'", error, FileName);
  return !error;
}

const char *cTheme::Description(void)
{
  char *s = descriptions[I18nCurrentLanguage()];
  if (!s)
     s = descriptions[0];
  return s ? s : name;
}

bool cTheme::Load(const char *FileName, bool OnlyDescriptions)
{
  if (!FileNameOk(FileName, true))
     return false;
  bool result = false;
  if (!OnlyDescriptions)
     isyslog("loading %s", FileName);
  FILE *f = fopen(FileName, "r");
  if (f) {
     int line = 0;
     result = true;
     char *s;
     const char *error = NULL;
     cReadLine ReadLine;
     while ((s = ReadLine.Read(f)) != NULL) {
           line++;
           char *p = strchr(s, '#');
           if (p)
              *p = 0;
           s = stripspace(skipspace(s));
           if (!isempty(s)) {
              char *n = s;
              char *v = strchr(s, '=');
              if (v) {
                 *v++ = 0;
                 n = stripspace(skipspace(n));
                 v = stripspace(skipspace(v));
                 if (strstr(n, "Description") == n) {
                    int lang = 0;
                    char *l = strchr(n, '.');
                    if (l)
                       lang = I18nLanguageIndex(++l);
                    if (lang >= 0) {
                       free(descriptions[lang]);
                       descriptions[lang] = strdup(v);
                       }
                    else
                       error = "invalid language code";
                    }
                 else if (!OnlyDescriptions) {
                    for (int i = 0; i < MaxThemeColors; i++) {
                        if (colorNames[i]) {
                           if (strcmp(n, colorNames[i]) == 0) {
                              char *p = NULL;
                              errno = 0;
                              tColor c = strtoul(v, &p, 16);
                              if (!errno && !*p)
                                 colorValues[i] = c;
                              else
                                 error = "invalid color value";
                              break;
                              }
                           }
                        else {
                           error = "unknown color name";
                           break;
                           }
                        }
                    }
                 }
              else
                 error = "missing value";
              }
           if (error) {
              result = false;
              break;
              }
           }
     if (!result)
        esyslog("ERROR: error in %s, line %d%s%s", FileName, line, error ? ": " : "", error ? error : "");
     fclose(f);
     }
  else
     LOG_ERROR_STR(FileName);
  return result;
}

bool cTheme::Save(const char *FileName)
{
  if (!FileNameOk(FileName))
     return false;
  bool result = true;
  cSafeFile f(FileName);
  if (f.Open()) {
     for (int i = 0; i < I18nLanguages()->Size(); i++) {
         if (descriptions[i])
            fprintf(f, "Description%s%.*s = %s\n", i ? "." : "", 3, i ? I18nLanguageCode(i) : "", descriptions[i]);
         }
     for (int i = 0; i < MaxThemeColors; i++) {
         if (colorNames[i])
            fprintf(f, "%s = %08X\n", colorNames[i], colorValues[i]);
         }
     if (!f.Close())
        result = false;
     }
  else
     result = false;
  return result;
}

int cTheme::AddColor(const char *Name, tColor Color)
{
  for (int i = 0; i < MaxThemeColors; i++) {
      if (colorNames[i]) {
         if (strcmp(Name, colorNames[i]) == 0) {
            colorValues[i] = Color;
            return i;
            }
         }
      else {
         colorNames[i] = strdup(Name);
         colorValues[i] = Color;
         return i;
         }
      }
  return -1;
}

tColor cTheme::Color(int Subject)
{
  return (Subject >= 0 && Subject < MaxThemeColors) ? colorValues[Subject] : 0;
}

// --- cThemes ---------------------------------------------------------------

char *cThemes::themesDirectory = NULL;

cThemes::cThemes(void)
{
  numThemes = 0;
  names = 0;
  fileNames = NULL;
  descriptions = NULL;
}

cThemes::~cThemes()
{
  Clear();
}

void cThemes::Clear(void)
{
  for (int i = 0; i < numThemes; i++) {
      free(names[i]);
      free(fileNames[i]);
      free(descriptions[i]);
      }
  free(names);
  free(fileNames);
  free(descriptions);
  numThemes = 0;
  names = 0;
  fileNames = NULL;
  descriptions = NULL;
}

bool cThemes::Load(const char *SkinName)
{
  Clear();
  if (themesDirectory) {
     cReadDir d(themesDirectory);
     struct dirent *e;
     while ((e = d.Next()) != NULL) {
           if (strstr(e->d_name, SkinName) == e->d_name && e->d_name[strlen(SkinName)] == '-') {
              cString FileName = AddDirectory(themesDirectory, e->d_name);
              cTheme Theme;
              if (Theme.Load(*FileName, true)) {
                 if (char **NewBuffer = (char **)realloc(names, (numThemes + 1) * sizeof(char *))) {
                    names = NewBuffer;
                    names[numThemes] = strdup(Theme.Name());
                    }
                 else {
                    esyslog("ERROR: out of memory");
                    break;
                    }
                 if (char **NewBuffer = (char **)realloc(fileNames, (numThemes + 1) * sizeof(char *))) {
                    fileNames = NewBuffer;
                    fileNames[numThemes] = strdup(*FileName);
                    }
                 else {
                    esyslog("ERROR: out of memory");
                    break;
                    }
                 if (char **NewBuffer = (char **)realloc(descriptions, (numThemes + 1) * sizeof(char *))) {
                    descriptions = NewBuffer;
                    descriptions[numThemes] = strdup(Theme.Description());
                    }
                 else {
                    esyslog("ERROR: out of memory");
                    break;
                    }
                 numThemes++;
                 }
              }
           }
     return numThemes > 0;
     }
  return false;
}

int cThemes::GetThemeIndex(const char *Description)
{
  int index = 0;
  for (int i = 0; i < numThemes; i++) {
      if (strcmp(descriptions[i], Description) == 0)
         return i;
      if (strcmp(descriptions[i], "Default") == 0)
         index = i;
      }
  return index;
}

void cThemes::SetThemesDirectory(const char *ThemesDirectory)
{
  free(themesDirectory);
  themesDirectory = strdup(ThemesDirectory);
  MakeDirs(themesDirectory, true);
}

void cThemes::Load(const char *SkinName, const char *ThemeName, cTheme *Theme)
{
  cString FileName = cString::sprintf("%s/%s-%s.theme", themesDirectory, SkinName, ThemeName);
  if (access(FileName, F_OK) == 0) // the file exists
     Theme->Load(FileName);
}

void cThemes::Save(const char *SkinName, cTheme *Theme)
{
  cString FileName = cString::sprintf("%s/%s-%s.theme", themesDirectory, SkinName, Theme->Name());
  if (access(FileName, F_OK) != 0) // the file does not exist
     Theme->Save(FileName);
}
