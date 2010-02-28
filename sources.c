/*
 * sources.c: Source handling
 *
 * See the main source file 'vdr.c' for copyright information and
 * how to reach the author.
 *
 * $Id: sources.c 2.2 2010/02/28 15:15:39 kls Exp $
 */

#include "sources.h"

// --- cSource ---------------------------------------------------------------

cSource::cSource(void)
{
  code = stNone;
  description = NULL;
}

cSource::cSource(char Source, const char *Description)
{
  code = int(Source) << 24;
  description = strdup(Description);
}

cSource::~cSource()
{
  free(description);
}

bool cSource::Parse(const char *s)
{
  char *codeBuf = NULL;
  if (2 == sscanf(s, "%a[^ ] %a[^\n]", &codeBuf, &description))
     code = FromString(codeBuf);
  free(codeBuf);
  return code != stNone && description && *description;
}

cString cSource::ToString(int Code)
{
  char buffer[16];
  char *q = buffer;
  *q++ = (Code & st_Mask) >> 24;
  int n = (Code & st_Pos);
  if (n > 0x00007FFF)
     n |= 0xFFFF0000;
  if (n) {
     q += snprintf(q, sizeof(buffer) - 2, "%u.%u", abs(n) / 10, abs(n) % 10); // can't simply use "%g" here since the silly 'locale' messes up the decimal point
     *q++ = (n < 0) ? 'E' : 'W';
     }
  *q = 0;
  return buffer;
}

int cSource::FromString(const char *s)
{
  if (!isempty(s)) {
     if ('A' <= *s && *s <= 'Z') {
        int code = int(*s) << 24;
        if (code == stSat) {
           int pos = 0;
           bool dot = false;
           bool neg = false;
           while (*++s) {
                 switch (*s) {
                   case '0' ... '9': pos *= 10;
                                     pos += *s - '0';
                                     break;
                   case '.':         dot = true;
                                     break;
                   case 'E':         neg = true; // fall through to 'W'
                   case 'W':         if (!dot)
                                        pos *= 10;
                                     break;
                   default: esyslog("ERROR: unknown source character '%c'", *s);
                            return stNone;
                   }
                 }
           if (neg)
              pos = -pos;
           code |= (pos & st_Pos);
           }
        return code;
        }
     else
       esyslog("ERROR: unknown source key '%c'", *s);
     }
  return stNone;
}

int cSource::FromData(eSourceType SourceType, int Position, bool East)
{
  int code = SourceType;
  if (SourceType == stSat) {
     if (East)
        Position = -Position;
     code |= (Position & st_Pos);;
     }
  return code;
}

// --- cSources --------------------------------------------------------------

cSources Sources;

cSource *cSources::Get(int Code)
{
  for (cSource *p = First(); p; p = Next(p)) {
      if (p->Code() == Code)
         return p;
      }
  return NULL;
}
