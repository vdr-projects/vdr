/*
 * positioner.h: Steerable dish positioning
 *
 * See the main source file 'vdr.c' for copyright information and
 * how to reach the author.
 *
 * $Id: positioner.h 3.3 2013/12/28 11:15:56 kls Exp $
 */

#ifndef __POSITIONER_H
#define __POSITIONER_H

#include "thread.h"
#include "tools.h"

/// A steerable satellite dish generally points to the south on the northern hemisphere,
/// and to the north on the southern hemisphere (unless you're located directly on the
/// equator, in which case the general direction is "up"). Therefore, moving the dish
/// "east" or "west" means something different on either hemisphere. From the local dish
/// motor's point of view, it makes more sense to speak of turning the dish "left" or
/// "right", which is independent of the actual hemisphere the dish is located in.
/// In the cPositioner class context, when a dish on the northern hemisphere moves "east",
/// it is considered to be moving "left". Imagine standing behind the dish and looking
/// towards the satellites, and clearly "east" is "left". On the southern hemisphere
/// the same move to the "left" would go to the "west". So on the hardware level it is
/// clear what "left" and "right" means. The user interface may present different labels
/// to the viewer, depending on the hemisphere the dish is on.
/// All angles in this context are given in "degrees * 10", which allows for an angular
/// resolution of 0.1 degrees.

class cPositioner {
private:
  mutable cMutex mutex;
  static cPositioner *positioner;
  int capabilities;
  int frontend; // file descriptor of the DVB frontend
  mutable int lastLongitude; // the longitude the dish has last been moved to
  int targetLongitude; // the longitude the dish is supposed to be moved to
  mutable int lastHourAngle; // the hour angle the positioner has last been moved to
  int targetHourAngle; // the hour angle the positioner is supposed to be moved to
  int swingTime;
  cTimeMs movementStart;
protected:
  cPositioner(void);
  virtual ~cPositioner();
  void SetCapabilities(int Capabilities) { capabilities = Capabilities; }
      ///< A derived class shall call this function in its constructor to set the
      ///< capability flags it supports.
  int Frontend(void) const { return frontend; }
      ///< Returns the file descriptor of the DVB frontend the positioner is
      ///< connected to. If the positioner is not connected to any DVB device,
      ///< -1 will be returned.
  static int CalcHourAngle(int Longitude);
      ///< Takes the longitude and latitude of the dish location from the system
      ///< setup and the given Longitude to calculate the "hour angle" to which to move
      ///< the dish to in order to point to the satellite at orbital position Longitude.
      ///< An hour angle of zero means the dish shall point directly towards the
      ///< celestial equator (which is south on the northern hemisphere, and north on
      ///< the southern hemisphere). Negative values mean that the dish needs to be
      ///< moved to the left (as seen from behind the dish), while positive values
      ///< require a movement to the right.
  static int CalcLongitude(int HourAngle);
      ///< Returns the longitude of the satellite position the dish points at when the
      ///< positioner is moved to the given HourAngle.
  void StartMovementTimer(int Longitude);
      ///< Starts a timer that estimates how long it will take to move the dish from
      ///< the current position to the one given by Longitude. The default implementation
      ///< of CurrentLongitude() uses this timer.
public:
  enum ePositionerCapabilities {
    pcCanNothing         = 0x0000,
    pcCanDrive           = 0x0001,
    pcCanStep            = 0x0002,
    pcCanHalt            = 0x0004,
    pcCanSetLimits       = 0x0008,
    pcCanDisableLimits   = 0x0010,
    pcCanEnableLimits    = 0x0020,
    pcCanStorePosition   = 0x0040,
    pcCanRecalcPositions = 0x0080,
    pcCanGotoPosition    = 0x0100,
    pcCanGotoAngle       = 0x0200,
    };
  enum ePositionerDirection { pdLeft, pdRight };
  static int NormalizeAngle(int Angle);
          ///< Normalizes the given Angle into the range -1800...1800.
  int Capabilities(void) const { return capabilities; }
          ///< Returns a flag word defining all the things this positioner is
          ///< capable of.
  void SetFrontend(int Frontend) { frontend = Frontend; }
          ///< This function is called whenever the positioner is connected to
          ///< a DVB frontend.
  static int HorizonLongitude(ePositionerDirection Direction);
          ///< Returns the longitude of the satellite position that is just at the
          ///< horizon when looking in the given Direction. Note that this function
          ///< only delivers reasonable values for site latitudes between +/-81 degrees.
          ///< Beyond these limits (i.e. near the north or south pole) a constant value
          ///< of 0 will be returned.
  int HardLimitLongitude(ePositionerDirection Direction) const;
          ///< Returns the longitude of the positioner's hard limit in the given
          ///< Direction. Note that the value returned here may be larger (or smaller,
          ///< depending on the Direction) than that returned by HorizonLongitude(),
          ///< which would mean that it lies below that horizon.
  int LastLongitude(void) const { return lastLongitude; }
          ///< Returns the longitude the dish has last been moved to.
  int TargetLongitude(void) const { return targetLongitude; }
          ///< Returns the longitude the dish is supposed to be moved to. Once the target
          ///< longitude has been reached, this is the same as the value returned by
          ///< CurrentLongitude().
  virtual cString Error(void) const { return NULL; }
          ///< Returns a short, single line string indicating an error condition (if
          ///< the positioner is able to report any errors).
          ///< NULL means there is no error.
  virtual void Drive(ePositionerDirection Direction) {}
          ///< Continuously move the dish to the given Direction until Halt() is
          ///< called or it hits the soft or hard limit.
  virtual void Step(ePositionerDirection Direction, uint Steps = 1) {}
          ///< Move the dish the given number of Steps in the given Direction.
          ///< The maximum number of steps a particular positioner can do in a single
          ///< call may be limited.
          ///< A "step" is the smallest possible movement the positioner can make, which
          ///< is typically 0.1 degrees.
  virtual void Halt(void) {}
          ///< Stop any ongoing motion of the dish.
  virtual void SetLimit(ePositionerDirection Direction) {}
          ///< Set the soft limit of the dish movement in the given Direction to the
          ///< current position.
  virtual void DisableLimits(void) {}
          ///< Disables the soft limits for the dish movement.
  virtual void EnableLimits(void) {}
          ///< Enables the soft limits for the dish movement.
  virtual void StorePosition(uint Number) {}
          ///< Store the current position as a satellite position with the given Number.
          ///< Number can be in the range 1...255. However, a particular positioner
          ///< may only have a limited number of satellite positions it can store.
  virtual void RecalcPositions(uint Number) {}
          ///< Take the difference between the current actual position of the dish and
          ///< the position stored with the given Number, and apply the difference to
          ///< all stored positions.
  virtual void GotoPosition(uint Number, int Longitude);
          ///< Move the dish to the satellite position stored under the given Number.
          ///< Number must be one of the values previously used with StorePosition().
          ///< The special value 0 shall move the dish to a "reference position",
          ///< which usually is due south (or north, if you're on the southern hemisphere).
          ///< Longitude will be used to calculate how long it takes to move the dish
          ///< from its current position to the given Longitude.
          ///< A derived class must call the base class function to have the target
          ///< longitude stored.
  virtual void GotoAngle(int Longitude);
          ///< Move the dish to the given angular position. Longitude can be in the range
          ///< -1800...+1800. A positive sign indicates a position east of Greenwich,
          ///< while western positions have a negative sign. The absolute value is in
          ///< "degrees * 10", which allows for a resolution of 1/10 of a degree.
          ///< A derived class must call the base class function to have the target
          ///< longitude stored.
  virtual int CurrentLongitude(void) const;
          ///< Returns the longitude the dish currently points to. If the dish is in motion,
          ///< this may be an estimate based on the angular speed of the positioner.
          ///< The default implementation takes the last and target longitude as well as
          ///< the rotation speed of the positioner to calculate the estimated current
          ///< longitude the dish points to.
  virtual bool IsMoving(void) const;
          ///< Returns true if the dish is currently moving as a result of a call to
          ///< GotoPosition() or GotoAngle().
  static cPositioner *GetPositioner(void);
          ///< Returns a previously created positioner. If no plugin has created
          ///< a positioner, there will always be the default DiSEqC positioner.
  static void DestroyPositioner(void);
          ///< Destroys a previously created positioner.
  };

#endif //__POSITIONER_H
