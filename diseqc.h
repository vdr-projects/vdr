/*
 * diseqc.h: DiSEqC handling
 *
 * See the main source file 'vdr.c' for copyright information and
 * how to reach the author.
 *
 * $Id: diseqc.h 1.1 2002/10/05 13:02:52 kls Exp $
 */

#ifndef __DISEQC_H
#define __DISEQC_H

#include "config.h"

class cDiseqc : public cListObject {
public:
  enum eDiseqcActions {
    daNone,
    daToneOff,
    daToneOn,
    daVoltage13,
    daVoltage18,
    daMiniA,
    daMiniB,
    daCodes,
    };
  enum { MaxDiseqcCodes = 6 };
private:
  int source;
  int slof;
  char polarization;
  int lof;
  char *commands;
  char *currentAction;
  bool parsing;
  uchar codes[MaxDiseqcCodes];
  int numCodes;
  char *Wait(char *s);
  char *Codes(char *s);
public:
  cDiseqc(void);
  ~cDiseqc();
  bool Parse(const char *s);
  eDiseqcActions Execute(bool Start = false);
  int Source(void) const { return source; }
  int Slof(void) const { return slof; }
  char Polarization(void) const { return polarization; }
  int Lof(void) const { return lof; }
  const char *Commands(void) const { return commands; }
  uchar *Codes(int &NumCodes) { NumCodes = numCodes; return numCodes ? codes : NULL; }
  };

class cDiseqcs : public cConfig<cDiseqc> {
public:
  cDiseqc *Get(int Source, int Frequency, char Polarization);
  };

extern cDiseqcs Diseqcs;

#endif //__DISEQC_H
