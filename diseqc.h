/*
 * diseqc.h: DiSEqC handling
 *
 * See the main source file 'vdr.c' for copyright information and
 * how to reach the author.
 *
 * $Id: diseqc.h 2.5 2011/09/17 13:15:17 kls Exp $
 */

#ifndef __DISEQC_H
#define __DISEQC_H

#include "config.h"
#include "thread.h"

class cScr : public cListObject {
private:
  int devices;
  int channel;
  uint userBand;
  int pin;
  bool used;
public:
  cScr(void);
  bool Parse(const char *s);
  int Devices(void) const { return devices; }
  int Channel(void) const { return channel; }
  uint UserBand(void) const { return userBand; }
  int Pin(void) const { return pin; }
  bool Used(void) const { return used; }
  void SetUsed(bool Used) { used = Used; }
  };

class cScrs : public cConfig<cScr> {
private:
  cMutex mutex;
public:
  cScr *GetUnused(int Device);
  };

extern cScrs Scrs;

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
    daScr,
    daCodes,
    };
  enum { MaxDiseqcCodes = 6 };
private:
  int devices;
  int source;
  int slof;
  char polarization;
  int lof;
  mutable int scrBank;
  char *commands;
  bool parsing;
  uint SetScrFrequency(uint SatFrequency, const cScr *Scr, uint8_t *Codes) const;
  int SetScrPin(const cScr *Scr, uint8_t *Codes) const;
  const char *Wait(const char *s) const;
  const char *GetScrBank(const char *s) const;
  const char *GetCodes(const char *s, uchar *Codes = NULL, uint8_t *MaxCodes = NULL) const;
public:
  cDiseqc(void);
  ~cDiseqc();
  bool Parse(const char *s);
  eDiseqcActions Execute(const char **CurrentAction, uchar *Codes, uint8_t *MaxCodes, const cScr *Scr, uint *Frequency) const;
      ///< Parses the DiSEqC commands and returns the appropriate action code
      ///< with every call. CurrentAction must be the address of a character pointer,
      ///< which is initialized to NULL. This pointer is used internally while parsing
      ///< the commands and shall not be modified once Execute() has been called with
      ///< it. Call Execute() repeatedly (always providing the same CurrentAction pointer)
      ///< until it returns daNone. After a successful execution of all commands
      ///< *CurrentAction points to the value 0x00.
      ///< If the current action consists of sending code bytes to the device, those
      ///< bytes will be copied into Codes. MaxCodes must be initialized to the maximum
      ///< number of bytes Codes can handle, and will be set to the actual number of
      ///< bytes copied to Codes upon return.
      ///< If this DiSEqC entry requires SCR, the given Scr will be used. This must
      ///< be a pointer returned from a previous call to cDiseqcs::Get().
      ///< Frequency must be the frequency the tuner will be tuned to, and will be
      ///< set to the proper SCR frequency upon return (if SCR is used).
  int Devices(void) const { return devices; }
  int Source(void) const { return source; }
  int Slof(void) const { return slof; }
  char Polarization(void) const { return polarization; }
  int Lof(void) const { return lof; }
  bool IsScr() const { return scrBank >= 0; }
  const char *Commands(void) const { return commands; }
  };

class cDiseqcs : public cConfig<cDiseqc> {
public:
  const cDiseqc *Get(int Device, int Source, int Frequency, char Polarization, const cScr **Scr) const;
      ///< Selects a DiSEqC entry suitable for the given Device and tuning parameters.
      ///< If this DiSEqC entry requires SCR and the given *Scr is NULL
      ///< a free one will be selected from the Scrs and a pointer to that will
      ///< be returned in Scr. The caller shall memorize that pointer and reuse it in
      ///< subsequent calls.
      ///< Scr may be NULL for checking whether there is any DiSEqC entry for the
      ///< given transponder.
  };

extern cDiseqcs Diseqcs;

#endif //__DISEQC_H
