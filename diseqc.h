/*
 * diseqc.h: DiSEqC handling
 *
 * See the main source file 'vdr.c' for copyright information and
 * how to reach the author.
 *
 * $Id: diseqc.h 5.1 2025/03/02 11:03:35 kls Exp $
 */

#ifndef __DISEQC_H
#define __DISEQC_H

#include "config.h"
#include "positioner.h"
#include "thread.h"

class cDiseqcPositioner : public cPositioner {
private:
  void SendDiseqc(uint8_t *Codes, int NumCodes);
public:
  cDiseqcPositioner(void);
  virtual void Drive(ePositionerDirection Direction) override;
  virtual void Step(ePositionerDirection Direction, uint Steps = 1) override;
  virtual void Halt(void) override;
  virtual void SetLimit(ePositionerDirection Direction) override;
  virtual void DisableLimits(void) override;
  virtual void EnableLimits(void) override;
  virtual void StorePosition(uint Number) override;
  virtual void RecalcPositions(uint Number) override;
  virtual void GotoPosition(uint Number, int Longitude) override;
  virtual void GotoAngle(int Longitude) override;
  };

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
  bool Load(const char *FileName, bool AllowComments = false, bool MustExist = false);
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
    daPositionN,
    daPositionA,
    daScr,
    daCodes,
    daWait,
    };
  enum { MaxDiseqcCodes = 6 };
private:
  int devices;
  int source;
  int slof;
  char polarization;
  int lof;
  mutable int position;
  mutable int scrBank;
  char *commands;
  bool parsing;
  int SetScrFrequency(int SatFrequency, const cScr *Scr, uint8_t *Codes) const;
  int SetScrPin(const cScr *Scr, uint8_t *Codes) const;
  const char *Wait(const char *s) const;
  const char *GetPosition(const char *s) const;
  const char *GetScrBank(const char *s) const;
  const char *GetCodes(const char *s, uchar *Codes = NULL, uint8_t *MaxCodes = NULL) const;
public:
  cDiseqc(void);
  ~cDiseqc();
  bool Parse(const char *s);
  eDiseqcActions Execute(const char **CurrentAction, uchar *Codes, uint8_t *MaxCodes, const cScr *Scr, int *Frequency) const;
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
      ///< Returns an integer where each bit represents one of the system's devices.
      ///< If a bit is set, this DiSEqC sequence applies to the corresponding device.
  int Source(void) const { return source; }
      ///< Returns the satellite source this DiSEqC sequence applies to.
  int Slof(void) const { return slof; }
      ///< Returns the switch frequency of the LNB this DiSEqC sequence applies to.
  char Polarization(void) const { return polarization; }
      ///< Returns the signal polarization this DiSEqC sequence applies to.
  int Lof(void) const { return lof; }
      ///< Returns the local oscillator frequency of the LNB this DiSEqC sequence applies to.
  int Position(void) const { return position; }
      ///< Indicates which positioning mode to use in order to move the dish to a given
      ///< satellite position. -1 means "no positioning" (i.e. fixed dish); 0 means the
      ///< positioner can be moved to any arbitrary satellite position (within its
      ///< limits); and a positive number means "move the dish to the position stored
      ///< under the given number".
  bool IsScr(void) const { return scrBank >= 0; }
      ///< Returns true if this DiSEqC sequence uses Satellite Channel Routing.
  const char *Commands(void) const { return commands; }
      ///< Returns a pointer to the actual commands of this DiSEqC sequence.
  };

class cDiseqcs : public cConfig<cDiseqc> {
public:
  bool Load(const char *FileName, bool AllowComments = false, bool MustExist = false);
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
