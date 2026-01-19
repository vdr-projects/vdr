/*
 * themes.c: Color themes used by skins
 *
 * See the main source file 'vdr.c' for copyright information and
 * how to reach the author.
 *
 * $Id: themes.c 5.2 2026/01/19 11:09:43 kls Exp $
 */

#include "themes.h"
#include <dirent.h>
#include <string.h>
#include "config.h"

// --- cTheme ----------------------------------------------------------------

cTheme::cTheme(void)
{
  name = "default";
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
                    if (SetName)
                       name = cString(n, e);
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
  return s ? s : *name;
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
     descriptions.Clear();
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
                    for (int i = 0; i < colorNames.Size(); i++) {
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
     for (int i = 0; i < colorNames.Size(); i++)
         fprintf(f, "%s = %08X\n", colorNames[i], colorValues[i]);
     if (!f.Close())
        result = false;
     }
  else
     result = false;
  return result;
}

int cTheme::AddColor(const char *Name, tColor Color)
{
  for (int i = 0; i < colorNames.Size(); i++) {
      if (strcmp(Name, colorNames[i]) == 0) {
         colorValues[i] = Color;
         return i;
         }
      }
  colorNames.Append(strdup(Name));
  colorValues.Append(Color);
  return colorValues.Size() - 1;
}

tColor cTheme::Color(int Subject)
{
  return (Subject >= 0 && Subject < colorValues.Size()) ? colorValues[Subject] : 0;
}

// --- cThemes ---------------------------------------------------------------

cString cThemes::themesDirectory;

cThemes::cThemes(void)
{
  numThemes = 0;
}

cThemes::~cThemes()
{
  Clear();
}

void cThemes::Clear(void)
{
  names.Clear();
  fileNames.Clear();
  descriptions.Clear();
  numThemes = 0;
}

bool cThemes::Load(const char *SkinName)
{
  Clear();
  if (*themesDirectory) {
     cStringList Data;
     cReadDir d(themesDirectory);
     struct dirent *e;
     while ((e = d.Next()) != NULL) {
           if (strstr(e->d_name, SkinName) == e->d_name && e->d_name[strlen(SkinName)] == '-') {
              cString FileName = AddDirectory(themesDirectory, e->d_name);
              cTheme Theme;
              if (Theme.Load(FileName, true))
                 Data.Append(strdup(cString::sprintf("%s\t%s\t%s", Theme.Description(), Theme.Name(), *FileName)));
              }
           }
     Data.Sort();
     for (int i = 0; i < Data.Size(); i++) {
         char *s = Data[i];
         char *t = strchr(s, '\t');
         *t = 0;
         if (descriptions.Find(s) >= 0)
            esyslog("ERROR: duplicate themes '%s' in skin '%s'", s, SkinName);
         descriptions.Append(strdup(s));
         s = t + 1;
         t = strchr(s, '\t');
         *t = 0;
         names.Append(strdup(s));
         s = t + 1;
         fileNames.Append(strdup(s));
         numThemes++;
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
  themesDirectory = ThemesDirectory;
  MakeDirs(themesDirectory, true);
}

void cThemes::Load(const char *SkinName, const char *ThemeName, cTheme *Theme)
{
  cString FileName = cString::sprintf("%s/%s-%s.theme", *themesDirectory, SkinName, ThemeName);
  if (access(FileName, F_OK) == 0) // the file exists
     Theme->Load(FileName);
}

void cThemes::Save(const char *SkinName, cTheme *Theme)
{
  cString FileName = cString::sprintf("%s/%s-%s.theme", *themesDirectory, SkinName, Theme->Name());
  if (access(FileName, F_OK) != 0) // the file does not exist
     Theme->Save(FileName);
}
