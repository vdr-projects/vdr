/*
 * shutdown.h: Handling of shutdown and inactivity
 *
 * See the main source file 'vdr.c' for copyright information and
 * how to reach the author.
 *
 * Original version written by Udo Richter <udo_richter@gmx.de>.
 *
 * $Id: shutdown.h 5.1 2025/03/03 13:12:16 kls Exp $
 */

#ifndef __SHUTDOWN_H
#define __SHUTDOWN_H

#include <time.h>

class cCountdown {
private:
  time_t timeout;      ///< 5-minute countdown timer
  int counter;         ///< last shown time in 10s units
  bool timedOut;       ///< countdown did run down to 0 and was not canceled
  const char *message; ///< message to display, %s is placeholder for time
public:
  cCountdown(void);
  void Start(const char *Message, int Seconds);
       ///< Start the 5 minute shutdown warning countdown.
  void Cancel(void);
       ///< Cancel the 5 minute shutdown warning countdown.
  bool Done(void);
       ///< Check if countdown timer has run out without canceling.
  operator bool(void) const { return timeout != 0; }
       ///< Check if countdown is running.
  bool Update(void);
       ///< Update status display of the countdown.
       ///< Returns true on actual update.
  };

class cShutdownHandler {
private:
  time_t activeTimeout;
       ///< Time when VDR will become non-interactive. 0 means never, 1 means unknown time ago.
  time_t retry;
       ///< Time for retrying the shutdown.
  char *shutdownCommand;
       ///< Command for shutting down VDR.
  int exitCode;
       ///< Exit code, if VDR exit was requested, or -1 if not requested.
  bool emergencyExitRequested;
       ///< The requested exit is an emergency exit.
public:
  cCountdown countdown;
  cShutdownHandler(void);
  ~cShutdownHandler();
  void Exit(int ExitCode) { exitCode = ExitCode; }
       ///< Set VDR exit code and initiate end of VDR main loop.
       ///< This will exit VDR without any confirmation.
  bool DoExit(void) { return exitCode >= 0; }
       ///< Check if an exit code was set, and VDR should exit.
  int GetExitCode(void) { return exitCode >= 0 ? exitCode : 0; }
       ///< Get the currently set exit code of VDR.
  bool EmergencyExitRequested(void) { return emergencyExitRequested; }
       ///< Returns true if an emergency exit was requested.
  void RequestEmergencyExit(void);
       ///< Requests an emergency exit of the VDR main loop.
  void CheckManualStart(int ManualStart);
       ///< Check whether the next timer is in ManualStart time window.
       ///< If yes, assume non-interactive use.
  void SetShutdownCommand(const char *ShutdownCommand);
       ///< Set the command string for shutdown command.
  void CallShutdownCommand(time_t WakeupTime, int Channel, const char *File, bool UserShutdown);
       ///< Call the shutdown command with the given parameters.
  bool IsUserInactive(time_t AtTime = 0) { return activeTimeout && activeTimeout <= (AtTime ? AtTime : time(NULL)); }
       ///< Check whether VDR is in interactive mode or non-interactive mode (waiting for shutdown).
       ///< AtTime checks whether VDR will probably be inactive at that time.
  time_t GetUserInactiveTime(void) { return activeTimeout; }
       ///< Time when user will become non-inactive, or 0 if never, 1 if a long time ago
  void SetUserInactiveTimeout(int Seconds = -1, bool Force = false);
       ///< Set the time in the future when VDR will switch into non-interactive mode or power down.
       ///< Seconds >= 0 means that many seconds in the future.
       ///< Seconds = -1 means Setup.MinUserInactivity in the future.
       ///< Seconds = -2 means never.
       ///< Seconds = -3 means a long, unknown time ago.
       ///< Setup.MinUserInactivity = 0 will overrule this, unless Force = true is given.
       ///< If Setup.MinUserInactivity = 0 and Force = false, Seconds is ignored and VDR will
       ///< stay interactive forever (like Seconds = -2).
  void SetUserInactive(void) { SetUserInactiveTimeout(0, true); }
       ///< Set VDR manually into non-interactive mode from now on.
  bool Retry(time_t AtTime = 0) { return retry <= (AtTime ? AtTime : time(NULL)); }
       ///< Check whether its time to re-try the shutdown.
       ///< AtTime checks whether VDR will probably be inactive at that time.
  time_t GetRetry(void) { return retry; }
       ///< Time when shutdown retry block ends.
  void SetRetry(int Seconds) { retry = time(NULL) + Seconds; }
       ///< Set shutdown retry so that VDR will not try to automatically shut down
       ///< within Seconds.
  bool ConfirmShutdown(bool Ask);
       ///< Check for background activity that blocks shutdown.
       ///< Returns immediately and without user interaction if Ask = false.
       ///< Asks for confirmation if Ask = true.
       ///< Returns true if ready for shutdown.
  bool ConfirmRestart(bool Ask);
       ///< Check for background activity that blocks restart.
       ///< Returns immediately and without user interaction if Ask = false.
       ///< Asks for confirmation if Ask = true.
       ///< Returns true if ready for restart.
  bool DoShutdown(bool Force);
       ///< Call the shutdown script with data of the next pending timer.
       ///< Fails if Force = false and a timer is running or within MinEventTimeout.
       ///< Always calls shutdown on Force = true.
       ///< Returns true on success.
  };

extern cShutdownHandler ShutdownHandler;

#endif //__SHUTDOWN_H
