/*
 * i18n.h: Internationalization
 *
 * See the main source file 'vdr.c' for copyright information and
 * how to reach the author.
 *
 * $Id: i18n.h 1.1 2000/11/11 09:27:25 kls Exp $
 */

#ifndef __I18N_H
#define __I18N_H

extern const int NumLanguages;

const char *tr(const char *s);

const char * const * Languages(void);

#endif //__I18N_H
