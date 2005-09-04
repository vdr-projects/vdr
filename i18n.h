/*
 * i18n.h: Internationalization
 *
 * See the main source file 'vdr.c' for copyright information and
 * how to reach the author.
 *
 * $Id: i18n.h 1.15 2005/09/04 14:37:01 kls Exp $
 */

#ifndef __I18N_H
#define __I18N_H

#include <stdio.h>

const int I18nNumLanguages = 20;

typedef const char *tI18nPhrase[I18nNumLanguages];

void I18nRegister(const tI18nPhrase * const Phrases, const char *Plugin);

const char *I18nTranslate(const char *s, const char *Plugin = NULL);

const char * const * I18nLanguages(void);
const char * const * I18nCharSets(void);
const char *I18nLanguageCode(int Index);
int I18nLanguageIndex(const char *Code);
const char *I18nNormalizeLanguageCode(const char *Code);
   ///< Returns a 3 letter language code that may not be zero terminated.
   ///< If no normalized language code can be found, the given Code is returned.
   ///< Make sure at most 3 characters are copied when using it!
bool I18nIsPreferredLanguage(int *PreferredLanguages, const char *LanguageCode, int &OldPreference, int *Position = NULL);

#ifdef PLUGIN_NAME_I18N
#define tr(s)  I18nTranslate(s, PLUGIN_NAME_I18N)
#else
#define tr(s)  I18nTranslate(s)
#endif

#endif //__I18N_H
