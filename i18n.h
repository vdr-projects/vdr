/*
 * i18n.h: Internationalization
 *
 * See the main source file 'vdr.c' for copyright information and
 * how to reach the author.
 *
 * $Id: i18n.h 1.6 2003/05/16 12:48:52 kls Exp $
 */

#ifndef __I18N_H
#define __I18N_H

#include <stdio.h>

const int I18nNumLanguages = 16;

typedef const char *tI18nPhrase[I18nNumLanguages];

void I18nRegister(const tI18nPhrase * const Phrases, const char *Plugin);

const char *I18nTranslate(const char *s, const char *Plugin = NULL);

const char * const * I18nLanguages(void);

#ifdef PLUGIN_NAME_I18N
#define tr(s)  I18nTranslate(s, PLUGIN_NAME_I18N)
#else
#define tr(s)  I18nTranslate(s)
#endif

#endif //__I18N_H
