/*
 * sources.h: Source handling
 *
 * See the main source file 'vdr.c' for copyright information and
 * how to reach the author.
 *
 * $Id: sources.h 1.1 2002/10/04 14:25:03 kls Exp $
 */

#ifndef __SOURCES_H
#define __SOURCES_H

#include "config.h"

class cSource : public cListObject {
public:
  enum eSourceType {
    stNone  = 0x0000,
    stCable = 0x4000,
    stSat   = 0x8000,
    stTerr  = 0xC000,
    st_Mask = 0xC000,
    st_Neg  = 0x0800,
    };
private:
  int code;
  char *description;
public:
  cSource(void);
  ~cSource();
  int Code(void) const { return code; }
  const char *Description(void) const { return description; }
  bool Parse(const char *s);
  static const char *ToString(int Code);
  static int FromString(const char *s);
  };

class cSources : public cConfig<cSource> {
public:
  cSource *Get(int Code);
  };

extern cSources Sources;

#endif //__SOURCES_H
