/*
 * i18n.h: Internationalization
 *
 * See the main source file 'vdr.c' for copyright information and
 * how to reach the author.
 *
 * $Id: i18n.h 1.14 2004/11/02 17:21:19 kls Exp $
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
const char * I18nLanguageCode(int Index);
int I18nLanguageIndex(const char *Code);
const char *I18nNormalizeLanguageCode(const char *Code);
bool I18nIsPreferredLanguage(int *PreferredLanguages, int LanguageIndex, int &OldPreference);

#ifdef PLUGIN_NAME_I18N
#define tr(s)  I18nTranslate(s, PLUGIN_NAME_I18N)
#else
#define tr(s)  I18nTranslate(s)
#endif

#endif //__I18N_H
