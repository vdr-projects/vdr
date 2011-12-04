/*
 * setup.h: Setup for the DVB HD Full Featured On Screen Display
 *
 * See the README file for copyright information and how to reach the author.
 *
 * $Id: setup.h 1.10 2011/12/04 15:32:13 kls Exp $
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
