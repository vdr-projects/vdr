/*
 * player.c: The basic player interface
 *
 * See the main source file 'vdr.c' for copyright information and
 * how to reach the author.
 *
 * $Id: player.c 5.1 2024/07/16 12:33:27 kls Exp $
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

#if DEPRECATED_CCONTROL
cControl *cControl::Control(bool Hidden)
{
  cMutexLock MutexLock(&mutex);
  return (control && (!control->hidden || Hidden)) ? control : NULL;
}
#endif

cControl *cControl::Control(cMutexLock &MutexLock, bool Hidden)
{
  MutexLock.Lock(&mutex);
  return (control && (!control->hidden || Hidden)) ? control : NULL;
}

void cControl::Launch(cControl *Control)
{
  cMutexLock MutexLock(&mutex);
  cControl *c = control; // keeps control from pointing to uninitialized memory TODO obsolete once DEPRECATED_CCONTROL is gone
  control = Control;
  delete c;
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
