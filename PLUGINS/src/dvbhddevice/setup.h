/*
 * setup.h: Setup for the DVB HD Full Featured On Screen Display
 *
 * See the README file for copyright information and how to reach the author.
 *
 * $Id: setup.h 1.9 2011/08/27 09:36:02 kls Exp $
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
    HDFF::eHdmiVideoMode GetVideoMode(void);

    int Resolution;
    int VideoModeAdaption;
    int TvFormat;
    int VideoConversion;
    int AnalogueVideo;
    int AudioDelay;
    int AudioDownmix;
    int OsdSize;
    int CecEnabled;
    int RemoteProtocol;
    int RemoteAddress;

    int HighLevelOsd;
    int TrueColorOsd;
};

extern cHdffSetup gHdffSetup;

class cHdffSetupPage : public cMenuSetupPage
{
private:
    HDFF::cHdffCmdIf * mHdffCmdIf;
    cHdffSetup mNewHdffSetup;

protected:
    virtual void Store(void);

public:
    cHdffSetupPage(HDFF::cHdffCmdIf * pHdffCmdIf);
    virtual ~cHdffSetupPage(void);
};

#endif
