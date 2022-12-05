/*
 * player.c: A player for still pictures
 *
 * See the README file for copyright information and how to reach the author.
 *
 * $Id: player.c 5.1 2022/12/05 15:26:23 kls Exp $
 */

#include "player.h"
#include <vdr/remote.h>
#include <vdr/tools.h>

int SlideShowDelay = 3; // seconds

cString HandleUnderscores(const char *s)
{
  // Skip anything before and including the first '_' and replace
  // any remaining '_' with blanks:
  const char *p = strchr(s, '_');
  if (p) {
     p++;
     char buf[strlen(p) + 1];
     strcpy(buf, p);
     return strreplace(buf, '_', ' ');
     }
  return s;
}

// --- cPicturePlayer --------------------------------------------------------

class cPicturePlayer : public cPlayer {
private:
  int size;
  int length;
  uchar *buffer;
  virtual void Activate(bool On);
public:
  cPicturePlayer(void);
  ~cPicturePlayer();
  void SetPicture(const char *FileName);
  };

cPicturePlayer::cPicturePlayer(void)
{
  size = KILOBYTE(100); // will be adjusted automatically if files are larger
  length = 0;
  buffer = MALLOC(uchar, size);
}

cPicturePlayer::~cPicturePlayer()
{
  free(buffer);
}

void cPicturePlayer::Activate(bool On)
{
  if (length > 0)
     DeviceStillPicture(buffer, length);
}

void cPicturePlayer::SetPicture(const char *FileName)
{
  int f = open(FileName, O_RDONLY);
  if (f >= 0) {
     for (;;) {
         length = read(f, buffer, size);
         if (length > 0) {
            if (length >= size) {
               int NewSize = size * 3 / 2;
               if (uchar *NewBuffer = (uchar *)realloc(buffer, NewSize)) {
                  buffer = NewBuffer;
                  size = NewSize;
                  }
               else {
                  LOG_ERROR_STR("out of memory");
                  break;
                  }
               lseek(f, 0, SEEK_SET);
               continue;
               }
            DeviceStillPicture(buffer, length);
            }
         else
            LOG_ERROR_STR(FileName);
         break;
         }
     close(f);
     }
  else
     LOG_ERROR_STR(FileName);
}

// --- cPictureControl -------------------------------------------------------

int cPictureControl::active = 0;
cString cPictureControl::lastDisplayed;

cPictureControl::cPictureControl(cPictureEntry *Pictures, const cPictureEntry *PictureEntry, bool SlideShow)
:cControl(NULL)
{
  player = new cPicturePlayer;
  SetPlayer(player);
  pictures = Pictures;
  pictureEntry = PictureEntry;
  osd = NULL;
  lastPath = "/";
  slideShowDelay.Set(SlideShowDelay * 1000);
  slideShow = SlideShow;
  alwaysDisplayCaption = false;
  NextPicture(slideShow && pictureEntry->IsDirectory() ? 1 : 0);
  active++;
}

cPictureControl::~cPictureControl()
{
  active--;
  delete osd;
  delete player;
  delete pictures;
}

void cPictureControl::NextPicture(int Direction)
{
  if (Direction) {
     const cPictureEntry *pe = Direction > 0 ? pictureEntry->NextPicture() : pictureEntry->PrevPicture();
     if (pe)
        pictureEntry = pe;
     }
  if (pictureEntry) {
     player->SetPicture(pictureEntry->Path());
     DisplayCaption();
     }
}

void cPictureControl::NextDirectory(int Direction)
{
  // This only works reliable if a directory contains only subdirectories or pictures, not both!
  if (Direction) {
     const cPictureEntry *pe = Direction > 0 ? pictureEntry->Parent()->Entries()->Last()->NextPicture() : pictureEntry->Parent()->Entries()->First()->PrevPicture();
     if (pe && Direction < 0)
        pe = pe->Parent()->Entries()->First();
     if (pe && pe != pictureEntry) {
        pictureEntry = pe;
        player->SetPicture(pictureEntry->Path());
        DisplayCaption();
        }
     }
}

static void DrawTextOutlined(cOsd *Osd, int x, int y, const char *s, tColor ColorFg, tColor ColorBg, const cFont *Font)
{
  for (int dx = -1; dx <= 1; dx++) {
      for (int dy = -1; dy <= 1; dy++) {
          if (dx || dy)
             Osd->DrawText(x + dx, y + dy, s, ColorBg, clrTransparent, Font);
          }
      }
  Osd->DrawText(x, y, s, ColorFg, clrTransparent, Font);
}

void cPictureControl::DisplayCaption(void)
{
  bool Force = false;
  cString Path = pictureEntry->Path();
  lastDisplayed = Path + strlen(pictures->Name()) + 1;
  const char *p = strrchr(Path, '/');
  const char *q = strrchr(lastPath, '/');
  if (p && q) {
     int lp = p - Path;
     int lq = q - lastPath;
     if (lp != lq || strncmp(lastPath, Path, lp)) {
        lastPath = Path;
        Force = true;
        }
     }
  if (!alwaysDisplayCaption && !Force) {
     DELETENULL(osd);
     return;
     }
  const cFont *Font = cFont::GetFont(fontOsd);
  int w = cOsd::OsdWidth();
  int h = 2 * Font->Height();
  if (!osd) {
     osd = cOsdProvider::NewOsd(cOsd::OsdLeft(), cOsd::OsdTop() + cOsd::OsdHeight() - h, OSD_LEVEL_SUBTITLES);
     tArea Areas[] = { { 0, 0, w - 1, h - 1, 8 } };
     if (Setup.AntiAlias && osd->CanHandleAreas(Areas, sizeof(Areas) / sizeof(tArea)) == oeOk)
        osd->SetAreas(Areas, sizeof(Areas) / sizeof(tArea));
     else {
        tArea Areas[] = { { 0, 0, w - 1, h - 1, 4 } };
        osd->SetAreas(Areas, sizeof(Areas) / sizeof(tArea));
        }
     }
  const char *Year = pictureEntry->Parent()->Parent() ? pictureEntry->Parent()->Parent()->Name() : "";
  cString Description = HandleUnderscores(pictureEntry->Parent()->Name());
  osd->DrawRectangle(0, 0, w - 1, h - 1, clrTransparent);
  DrawTextOutlined(osd, 0, 0, Description, clrWhite, clrBlack, Font);
  DrawTextOutlined(osd, 0, h / 2, Year, clrWhite, clrBlack, Font);
  struct stat sb;
  if (stat(Path, &sb) == 0) {
     cString Time = DayDateTime(sb.st_mtime);
     DrawTextOutlined(osd, w - Font->Width(Time), h / 2, Time, clrWhite, clrBlack, Font);
     }
  p++;
  Path.Truncate(-4); // don't display the ".mpg" extension
  DrawTextOutlined(osd, w - Font->Width(p), 0, p, clrWhite, clrBlack, Font);
  osd->Flush();
}

cString cPictureControl::GetHeader(void)
{
  return tr("Pictures");
}

eOSState cPictureControl::ProcessKey(eKeys Key)
{
  switch (int(Key)) {
    case kUp:
    case kPlay:   slideShowDelay.Set();
                  slideShow = true;
                  break;
    case kDown:
    case kPause:  slideShow = false;
                  break;
    case kLeft|k_Repeat:
    case kLeft:   NextPicture(-1);
                  slideShow = false;
                  break;
    case kRight|k_Repeat:
    case kRight:  NextPicture(+1);
                  slideShow = false;
                  break;
    case kOk:     if (osd && !alwaysDisplayCaption)
                     DELETENULL(osd);
                  else {
                     alwaysDisplayCaption = !alwaysDisplayCaption;
                     DisplayCaption();
                     }
                  break;
    case kGreen:
    case kPrev:   NextDirectory(-1);
                  slideShow = false;
                  break;
    case kYellow:
    case kNext:   NextDirectory(+1);
                  slideShow = false;
                  break;
    case kBlue:
    case kStop:   return osEnd;
    case kBack:   slideShow = false;
                  cRemote::CallPlugin(PLUGIN_NAME_I18N);
                  break;
    default: break;
    }
  if (slideShow && slideShowDelay.TimedOut()) {
     NextPicture(+1);
     slideShowDelay.Set(SlideShowDelay * 1000);
     }
  return osContinue;
}

const char *cPictureControl::LastDisplayed(void)
{
  return lastDisplayed;
}
