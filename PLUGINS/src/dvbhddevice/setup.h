/*
 * setup.h: Setup for the DVB HD Full Featured On Screen Display
 *
 * See the README file for copyright information and how to reach the author.
 */

#ifndef _HDFF_SETUP_H_
#define _HDFF_SETUP_H_

#include <vdr/plugin.h>
#include "hdffcmd.h"

struct cHdffSetup
{
    cHdffSetup(void);
    bool SetupParse(const char * Name, const char * Value);
    void GetOsdSize(int &Width, int &Height, double &PixelAspect);
    HdffVideoMode_t GetVideoMode(void);
    void SetNextVideoConversion(void);
    const char * GetVideoConversionString(void);

    int Resolution;
    int VideoModeAdaption;
    int TvFormat;
    int VideoConversion;
    int AnalogueVideo;
    int AudioDelay;
    int AudioDownmix;
    int AvSyncShift;
    int OsdSize;
    int CecEnabled;
    int CecTvOn;
    int CecTvOff;
    int RemoteProtocol;
    int RemoteAddress;

    int HighLevelOsd;
    int TrueColorOsd;

    int HideMainMenu;
};

extern cHdffSetup gHdffSetup;

class cHdffSetupPage : public cMenuSetupPage
{
private:
    HDFF::cHdffCmdIf * mHdffCmdIf;
    cHdffSetup mNewHdffSetup;
    cOsdItem * mTvFormatItem;
    int mVideoConversion;

    void BuildVideoConversionItem(void);

protected:
    virtual void Store(void);

public:
    cHdffSetupPage(HDFF::cHdffCmdIf * pHdffCmdIf);
    virtual ~cHdffSetupPage(void);
    virtual eOSState ProcessKey(eKeys Key);
};

#endif
