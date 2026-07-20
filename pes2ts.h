/*
 * pes2ts.h: Convert legacy PES recordings to TS
 *
 * See the main source file 'vdr.c' for copyright information and
 * how to reach the author.
 *
 * $Id: pes2ts.h 1.1 2026/07/20 08:25:33 kls Exp $
 */

#ifndef __PES2TS_H
#define __PES2TS_H

bool Pes2Ts(const char *PesFileName, const char *TsFileName);

#endif //__PES2TS_H
