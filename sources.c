/*
 * sources.c: Source handling
 *
 * See the main source file 'vdr.c' for copyright information and
 * how to reach the author.
 *
 * $Id: sources.c 5.1 2024/10/09 10:36:16 kls Exp $
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
  if (2 == sscanf(s, "%m[^ ] %m[^\n]", &codeBuf, &description))
     code = FromString(codeBuf);
  free(codeBuf);
  return code != stNone && description && *description;
}

bool cSource::Matches(int Code1, int Code2)
{
  if (Code1 == (stSat | st_Any))
     return IsSat(Code2);
  return Code1 == Code2;
}

int cSource::Position(int Code)
{
  return int16_t(Code & st_Pos);
}

cString cSource::ToString(int Code)
{
  char buffer[16];
  char *q = buffer;
  *q++ = (Code & st_Mask) >> 24;
  if (int n = Position(Code)) {
     q += snprintf(q, sizeof(buffer) - 2, "%u.%u", abs(n) / 10, abs(n) % 10); // can't simply use "%g" here since the silly 'locale' messes up the decimal point
     *q++ = (n < 0) ? 'W' : 'E';
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
                   case 'W':         neg = true; // fall through to 'E'
                   case 'E':         if (!dot)
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
     if (!East)
        Position = -Position;
     code |= (Position & st_Pos);
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

bool cSources::ContainsSourceType(char SourceType)
{
  for (cSource *p = First(); p; p = Next(p)) {
      if (cSource::ToChar(p->Code()) == SourceType)
         return true;
      }
  return false;
}
