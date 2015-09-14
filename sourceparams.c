/*
 * sourceparams.c: Source parameter handling
 *
 * See the main source file 'vdr.c' for copyright information and
 * how to reach the author.
 *
 * $Id: sourceparams.c 4.1 2015/08/02 11:56:39 kls Exp $
 */

#include "sourceparams.h"
#include "sources.h"

// --- cSourceParam ----------------------------------------------------------

cSourceParam::cSourceParam(char Source, const char *Description)
{
  source = Source;
  if ('A' <= source && source <= 'Z') {
     if (SourceParams.Get(source)) {
        esyslog("ERROR: source parameters for '%c' already defined", source);
        return;
        }
     SourceParams.Add(this);
     if (!Sources.ContainsSourceType(source))
        Sources.Add(new cSource(source, Description));
     dsyslog("registered source parameters for '%c - %s'", source, Description);
     }
  else
     esyslog("ERROR: invalid source '%c'", source);
}

// --- cSourceParams ---------------------------------------------------------

cSourceParams SourceParams;

cSourceParam *cSourceParams::Get(char Source)
{
  for (cSourceParam *sp = First(); sp; sp = Next(sp)) {
      if (sp->Source() == Source)
         return sp;
      }
  return NULL;
}
