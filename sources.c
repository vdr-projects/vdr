/*
 * sources.c: Source handling
 *
 * See the main source file 'vdr.c' for copyright information and
 * how to reach the author.
 *
 * $Id: sources.c 1.3 2004/12/26 11:58:52 kls Exp $
 */

#include "sources.h"
#include <ctype.h>

// -- cSource ----------------------------------------------------------------

cSource::cSource(void)
{
  code = stNone;
  description = NULL;
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
  switch (Code & st_Mask) {
    case stCable: *q++ = 'C'; break;
    case stSat:   *q++ = 'S';
                  {
                    int pos = Code & ~st_Mask;
                    q += snprintf(q, sizeof(buffer) - 2, "%u.%u", (pos & ~st_Neg) / 10, (pos & ~st_Neg) % 10); // can't simply use "%g" here since the silly 'locale' messes up the decimal point
                    *q++ = (Code & st_Neg) ? 'E' : 'W';
                  }
                  break;
    case stTerr:  *q++ = 'T'; break;
    default:      *q++ = Code + '0'; // backward compatibility
    }
  *q = 0;
  return buffer;
}

int cSource::FromString(const char *s)
{
  int type = stNone;
  switch (toupper(*s)) {
    case 'C': type = stCable; break;
    case 'S': type = stSat;   break;
    case 'T': type = stTerr;  break;
    case '0' ... '9': type = *s - '0'; break; // backward compatibility
    default: esyslog("ERROR: unknown source key '%c'", *s);
             return stNone;
    }
  int code = type;
  if (type == stSat) {
     int pos = 0;
     bool dot = false;
     bool neg = false;
     while (*++s) {
           switch (toupper(*s)) {
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
        pos |= st_Neg;
     code |= pos;
     }
  return code;
}

int cSource::FromData(eSourceType SourceType, int Position, bool East)
{
  int code = SourceType;
  if (SourceType == stSat) {
     if (East)
        code |= st_Neg;
     code |= (Position & st_Pos);;
     }
  return code;
}

// -- cSources ---------------------------------------------------------------

cSources Sources;

cSource *cSources::Get(int Code)
{
  for (cSource *p = First(); p; p = Next(p)) {
      if (p->Code() == Code)
         return p;
      }
  return NULL;
}
