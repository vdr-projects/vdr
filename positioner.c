/*
 * positioner.c: Steerable dish positioning
 *
 * See the main source file 'vdr.c' for copyright information and
 * how to reach the author.
 *
 * For an explanation (in German) of the theory behind the calculations see
 * http://www.vdr-portal.de/board17-developer/board97-vdr-core/p1154305-grundlagen-und-winkelberechnungen-f%C3%BCr-h-h-diseqc-motor-antennenanlagen
 * by Albert Danis.
 *
 * $Id: positioner.c 4.0 2015/02/14 11:54:31 kls Exp $
 */

#include "positioner.h"
#include <math.h>
#include "config.h"

#define SAT_EARTH_RATIO    0.1513 // the Earth's radius, divided by the distance from the Earth's center to the satellite
#define SAT_VISIBILITY_LAT 812    // the absolute latitude beyond which no satellite can be seen (degrees * 10)

#define RAD(x) ((x) * M_PI / 1800)
#define DEG(x) ((x) * 1800 / M_PI)

cPositioner *cPositioner::positioner = NULL;

cPositioner::cPositioner(void)
{
  capabilities = pcCanNothing;
  frontend = -1;
  targetLongitude = lastLongitude = Setup.PositionerLastLon;
  targetHourAngle = lastHourAngle = CalcHourAngle(lastLongitude);
  swingTime = 0;
  delete positioner;
  positioner = this;
}

cPositioner::~cPositioner()
{
  positioner = NULL;
}

int cPositioner::NormalizeAngle(int Angle)
{
  while (Angle < -1800)
        Angle += 3600;
  while (Angle > 1800)
        Angle -= 3600;
  return Angle;
}

int cPositioner::CalcHourAngle(int Longitude)
{
  double Alpha = RAD(Longitude - Setup.SiteLon);
  double Lat = RAD(Setup.SiteLat);
  int Sign = Setup.SiteLat >= 0 ? -1 : 1; // angles to the right are positive, angles to the left are negative
  return Sign * round(DEG(atan2(sin(Alpha), cos(Alpha) - cos(Lat) * SAT_EARTH_RATIO)));
}

int cPositioner::CalcLongitude(int HourAngle)
{
  double Lat = RAD(Setup.SiteLat);
  double Lon = RAD(Setup.SiteLon);
  double Delta = RAD(HourAngle);
  double Alpha = Delta - asin(sin(M_PI - Delta) * cos(Lat) * SAT_EARTH_RATIO);
  int Sign = Setup.SiteLat >= 0 ? 1 : -1;
  return NormalizeAngle(round(DEG(Lon - Sign * Alpha)));
}

int cPositioner::HorizonLongitude(ePositionerDirection Direction)
{
  double Delta;
  if (abs(Setup.SiteLat) <= SAT_VISIBILITY_LAT)
     Delta = acos(SAT_EARTH_RATIO / cos(RAD(Setup.SiteLat)));
  else
     Delta = 0;
  if ((Setup.SiteLat >= 0) != (Direction == pdLeft))
     Delta = -Delta;
  return NormalizeAngle(round(DEG(RAD(Setup.SiteLon) + Delta)));
}

int cPositioner::HardLimitLongitude(ePositionerDirection Direction) const
{
  return CalcLongitude(Direction == pdLeft ? -Setup.PositionerSwing : Setup.PositionerSwing);
}

void cPositioner::StartMovementTimer(int Longitude)
{
  if (Setup.PositionerSpeed <= 0)
     return;
  cMutexLock MutexLock(&mutex);
  lastLongitude = CurrentLongitude(); // in case the dish was already in motion
  targetLongitude = Longitude;
  lastHourAngle = CalcHourAngle(lastLongitude);
  targetHourAngle = CalcHourAngle(targetLongitude);
  swingTime = abs(targetHourAngle - lastHourAngle) * 1000 / Setup.PositionerSpeed; // time (ms) it takes to move the dish from lastHourAngle to targetHourAngle
  movementStart.Set();
  Setup.PositionerLastLon = targetLongitude;
}

void cPositioner::GotoPosition(uint Number, int Longitude)
{
  if (Longitude != targetLongitude)
     dsyslog("moving positioner to position %d, longitude %d", Number, Longitude);
  StartMovementTimer(Longitude);
}

void cPositioner::GotoAngle(int Longitude)
{
  if (Longitude != targetLongitude)
     dsyslog("moving positioner to longitude %d", Longitude);
  StartMovementTimer(Longitude);
}

int cPositioner::CurrentLongitude(void) const
{
  cMutexLock MutexLock(&mutex);
  if (targetLongitude != lastLongitude) {
     int Elapsed = movementStart.Elapsed(); // it's important to make this 'int', otherwise the expression below yields funny results
     if (swingTime <= Elapsed)
        lastLongitude = targetLongitude;
     else
        return CalcLongitude(lastHourAngle + (targetHourAngle - lastHourAngle) * Elapsed / swingTime);
     }
  return lastLongitude;
}

bool cPositioner::IsMoving(void) const
{
  cMutexLock MutexLock(&mutex);
  return CurrentLongitude() != targetLongitude;
}

cPositioner *cPositioner::GetPositioner(void)
{
  return positioner;
}

void cPositioner::DestroyPositioner(void)
{
  delete positioner;
}
