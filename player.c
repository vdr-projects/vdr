/*
 * player.c: The basic player interface
 *
 * See the main source file 'vdr.c' for copyright information and
 * how to reach the author.
 *
 * $Id: player.c 5.2 2024/10/13 09:47:18 kls Exp $
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

int cPlayer::PlayPes(const uchar *Data, int Length, bool VideoOnly)
{
  if (device)
     return device->PlayPes(Data, Length, VideoOnly);
  esyslog("ERROR: attempt to use cPlayer::PlayPes() without attaching to a cDevice!");
  return -1;
}

void cPlayer::Detach(void)
{
  if (device)
     device->Detach(this);
}

// --- cControl --------------------------------------------------------------

cControl *cControl::control = NULL;
cMutex cControl::mutex;

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

cOsdObject *cControl::GetInfo(void)
{
  return NULL;
}

const cRecording *cControl::GetRecording(void)
{
  return NULL;
}

cString cControl::GetHeader(void)
{
  return "";
}

cControl *cControl::Control(cMutexLock &MutexLock, bool Hidden)
{
  MutexLock.Lock(&mutex);
  return (control && (!control->hidden || Hidden)) ? control : NULL;
}

void cControl::Launch(cControl *Control)
{
  cMutexLock MutexLock(&mutex);
  delete control;
  control = Control;
}

void cControl::Attach(void)
{
  cMutexLock MutexLock(&mutex);
  if (control && !control->attached && control->player && !control->player->IsAttached()) {
     if (cDevice::PrimaryDevice()->AttachPlayer(control->player))
        control->attached = true;
     else {
        Skins.Message(mtError, tr("Primary device has no MPEG decoder, can't attach player!"));
        Shutdown();
        }
     }
}

void cControl::Shutdown(void)
{
  cMutexLock MutexLock(&mutex);
  cControl *c = control; // avoids recursions
  control = NULL;
  delete c;
}
