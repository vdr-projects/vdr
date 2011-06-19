/*
 * diseqc.h: DiSEqC handling
 *
 * See the main source file 'vdr.c' for copyright information and
 * how to reach the author.
 *
 * $Id: diseqc.h 2.2 2011/05/22 10:35:38 kls Exp $
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
  int devices;
  int source;
  int slof;
  char polarization;
  int lof;
  char *commands;
  bool parsing;
  mutable uchar codes[MaxDiseqcCodes];
  mutable int numCodes;
  const char *Wait(const char *s) const;
  const char *Codes(const char *s) const;
public:
  cDiseqc(void);
  ~cDiseqc();
  bool Parse(const char *s);
  eDiseqcActions Execute(const char **CurrentAction) const;
      // Parses the DiSEqC commands and returns the appropriate action code
      // with every call. CurrentAction must be the address of a character pointer,
      // which is initialized to NULL. This pointer is used internally while parsing
      // the commands and shall not be modified once Execute() has been called with
      // it. Call Execute() repeatedly (always providing the same CurrentAction pointer)
      // until it returns daNone. After a successful execution of all commands
      // *CurrentAction points to the value 0x00.
  int Devices(void) const { return devices; }
  int Source(void) const { return source; }
  int Slof(void) const { return slof; }
  char Polarization(void) const { return polarization; }
  int Lof(void) const { return lof; }
  const char *Commands(void) const { return commands; }
  const uchar *Codes(int &NumCodes) const { NumCodes = numCodes; return numCodes ? codes : NULL; }
  };

class cDiseqcs : public cConfig<cDiseqc> {
public:
  const cDiseqc *Get(int Device, int Source, int Frequency, char Polarization) const;
  };

extern cDiseqcs Diseqcs;

#endif //__DISEQC_H
