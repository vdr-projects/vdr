/*
 * player.c: The basic player interface
 *
 * See the main source file 'vdr.c' for copyright information and
 * how to reach the author.
 *
 * $Id: player.c 1.8 2004/11/20 11:33:08 kls Exp $
 */

#include "player.h"
#include "i18n.h"

// --- cPlayer ---------------------------------------------------------------

cPlayer::cPlayer(ePlayMode PlayMode)
{
  device = NULL;
  playMode = PlayMode;
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

void cPlayer::PlayAudio(const uchar *Data, int Length)
{
  if (device) {
     device->PlayAudio(Data, Length);
     return;
     }
  esyslog("ERROR: attempt to use cPlayer::PlayAudio() without attaching to a cDevice!");
}

void cPlayer::Detach(void)
{
  if (device)
     device->Detach(this);
}

// --- cControl --------------------------------------------------------------

cControl *cControl::control = NULL;

cControl::cControl(cPlayer *Player, bool Hidden)
{
  attached = false;
  hidden = Hidden;
  player = Player;
}

cControl::~cControl()
{
  if (this == control)
     control = NULL;
}

cControl *cControl::Control(void)
{
  return (control && !control->hidden) ? control : NULL;
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
        Skins.Message(mtError, tr("Channel locked (recording)!"));
        Shutdown();
        }
     }
}

void cControl::Shutdown(void)
{
  cControl *c = control; // avoids recursions
  control = NULL;
  delete c;
}
