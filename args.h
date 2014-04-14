/*
 * args.h: Read arguments from files
 *
 * See the main source file 'vdr.c' for copyright information and
 * how to reach the author.
 *
 * Original version written by Lars Hanisch <dvb@flensrocker.de>.
 *
 * $Id: args.h 1.1 2014/04/14 11:54:21 kls Exp $
 */

#ifndef __ARGS_H
#define __ARGS_H

#include "tools.h"

class cArgs {
private:
  cString argv0;
  cStringList args;
  cString lastArg;
  bool inVdrSection;
  int argc;
  char **argv;
  bool AddArg(const char *s);
public:
  cArgs(const char *Argv0);
  ~cArgs(void);
  bool ReadDirectory(const char *Directory);
  int GetArgc(void) const { return argc; };
  char **GetArgv(void) const { return argv; };
  };

#endif //__ARGS_H
