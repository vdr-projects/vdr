/*
 * player.c: The basic player interface
 *
 * See the main source file 'vdr.c' for copyright information and
 * how to reach the author.
 *
 * $Id: player.c 1.2 2002/06/23 11:23:17 kls Exp $
 */

#include "player.h"
#include "i18n.h"

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

cControl *cControl::control = NULL;

cControl::cControl(cPlayer *Player)
{
  attached = false;
  player = Player;
}

cControl::~cControl()
{
  if (this == control)
     control = NULL;
}

void cControl::Launch(cControl *Control)
{
  delete control;
  control = Control;
}

void cControl::Attach(void)
{
  if (control && !control->attached && control->player && !control->player->IsAttached()) {
     if (cDevice::PrimaryDevice()->AttachPlayer(control->player))
        control->attached = true;
     else {
        Interface->Error(tr("Channel locked (recording)!"));
        Shutdown();
        }
     }
}

void cControl::Shutdown(void)
{
  delete control;
  control = NULL;
}
