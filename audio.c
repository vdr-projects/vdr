/*
 * audio.c: The basic audio interface
 *
 * See the main source file 'vdr.c' for copyright information and
 * how to reach the author.
 *
 * $Id: audio.c 1.3 2005/02/12 12:40:51 kls Exp $
 */

#include "audio.h"
#include "stdlib.h"

// --- cAudio ----------------------------------------------------------------

cAudio::cAudio(void)
{
  Audios.Add(this);
}

cAudio::~cAudio()
{
}

// --- cAudios ---------------------------------------------------------------

cAudios Audios;

void cAudios::PlayAudio(const uchar *Data, int Length, uchar Id)
{
  for (cAudio *audio = First(); audio; audio = Next(audio))
      audio->Play(Data, Length, Id);
}

void cAudios::MuteAudio(bool On)
{
  for (cAudio *audio = First(); audio; audio = Next(audio))
      audio->Mute(On);
}

void cAudios::ClearAudio(void)
{
  for (cAudio *audio = First(); audio; audio = Next(audio))
      audio->Clear();
}

// --- cExternalAudio --------------------------------------------------------

cExternalAudio::cExternalAudio(const char *Command)
{
  command = strdup(Command);
  mute = false;
}

cExternalAudio::~cExternalAudio()
{
  free(command);
}

void cExternalAudio::Play(const uchar *Data, int Length, uchar Id)
{
  if (command && !mute) {
     if (pipe || pipe.Open(command, "w")) {
        if (0x80 <= Id && Id <= 0x87 || Id == 0xBD) { // AC3
           int written = Data[8] + 9; // skips the PES header
           if (Id != 0xBD)
              written += 4; // skips AC3 bytes
           Length -= written;
           while (Length > 0) {
                 int w = fwrite(Data + written, 1, Length, pipe);
                 if (w < 0) {
                    LOG_ERROR;
                    break;
                    }
                 Length -= w;
                 written += w;
                 }
           }
        }
     else {
        esyslog("ERROR: can't open pipe to audio command '%s'", command);
        free(command);
        command = NULL;
        }
     }
}

void cExternalAudio::Mute(bool On)
{
  mute = On;
  if (mute)
     Clear();
}

void cExternalAudio::Clear(void)
{
  pipe.Close();
}
