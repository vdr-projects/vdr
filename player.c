/*
 * player.c: The basic player interface
 *
 * See the main source file 'vdr.c' for copyright information and
 * how to reach the author.
 *
 * $Id: player.c 1.1 2002/06/16 10:34:50 kls Exp $
 */

#include "player.h"

// --- cPlayer ---------------------------------------------------------------

cPlayer::cPlayer(void)
{
  device = NULL;
  deviceFileHandle = -1;
}

cPlayer::~cPlayer()
{
  Detach();
}

int cPlayer::PlayVideo(const uchar *Data, int Length)
{
  if (device)
     return device->PlayVideo(Data, Length);
  esyslog("ERROR: attempt to use cPlayer::PlayVideo() without attaching to a cDevice!");
  return -1;
}

int cPlayer::PlayAudio(const uchar *Data, int Length)
{
  if (device)
     return device->PlayAudio(Data, Length);
  esyslog("ERROR: attempt to use cPlayer::PlayAudio() without attaching to a cDevice!");
  return -1;
}

void cPlayer::Detach(void)
{
  if (device)
     device->Detach(this);
}

// --- cControl --------------------------------------------------------------

cControl::cControl(void)
{
}

cControl::~cControl()
{
}
