/*
 * dvd.h: Functions for handling DVDs
 *
 * See the main source file 'vdr.c' for copyright information and
 * how to reach the author.
 *
 * Initially written by Andreas Schultz <aschultz@warp10.net>
 *
 * $Id: dvd.h 1.4 2001/11/10 13:38:25 kls Exp $
 */

#ifndef __DVD_H
#define __DVD_H

#ifdef DVDSUPPORT

#include <dvdread/dvd_reader.h>
#include <dvdread/ifo_types.h>
#include <dvdread/ifo_read.h>
#include <dvdread/dvd_udf.h>
#include <dvdread/nav_read.h>
#include <dvdread/nav_print.h>

#define aAC3   0x80
#define aDTS   0x88
#define aLPCM  0xA0
#define aMPEG  0xC0

class cDVD {
private:
  static cDVD *dvdInstance;
  static const char *deviceName;
  dvd_reader_t *dvd;
  dvd_file_t *title;
  ifo_handle_t *vmg_file;
  ifo_handle_t *vts_file;
  int titleset;
  static int Command(int Cmd);
public:
  cDVD(void);
  ~cDVD();
  static void SetDeviceName(const char *DeviceName);
  static const char *DeviceName(void);
  static bool DriveExists(void);
  static bool DiscOk(void);
  static void Eject(void);
  void Open(void);
  void Close(void);
  bool isValid(void) { return (dvd != NULL); }
  ifo_handle_t *openVMG(void);
  ifo_handle_t *openVTS(int TitleSet);
  ifo_handle_t *getVTS() { return vts_file; }
  dvd_file_t *openTitle(int Title, dvd_read_domain_t domain);
  static cDVD *getDVD(void);
  int getAudioNrOfTracks() { return getVTS() ? getVTS()->vtsi_mat->nr_of_vts_audio_streams : 0; }
  int getAudioLanguage(int stream) { return getVTS() ? getVTS()->vtsi_mat->vts_audio_attr[stream].lang_code : 0; }
  int getAudioTrack(int stream);
  };

#endif //DVDSUPPORT

#endif //__DVD_H
