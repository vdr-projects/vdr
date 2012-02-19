/*
 * menu.c: The DVB HD Full Featured device main menu
 *
 * See the README file for copyright information and how to reach the author.
 */

#include "menu.h"
#include "setup.h"

cHdffMenu::cHdffMenu(HDFF::cHdffCmdIf * pHdffCmdIf)
:   cOsdMenu("dvbhddevice"),
    mHdffCmdIf(pHdffCmdIf)
{
    mVideoConversionItem = new cOsdItem("", osUnknown, false);
    Add(mVideoConversionItem);
    SetHelp(tr("Video Conversion"), tr("TV on"));
    SetVideoConversion();
}

cHdffMenu::~cHdffMenu()
{
}

eOSState cHdffMenu::ProcessKey(eKeys key)
{
    eOSState state = cOsdMenu::ProcessKey(key);
    if (state == osUnknown)
    {
        switch (key)
        {
            case kRed:
                gHdffSetup.SetNextVideoConversion();
                SetVideoConversion();
                break;

            case kGreen:
                mHdffCmdIf->CmdHdmiSendCecCommand(HDFF_CEC_COMMAND_TV_ON);
                state = osEnd;
                break;

            case kOk:
                state = osEnd;
                break;

            default:
                break;
        }
    }
    return state;
}

void cHdffMenu::SetVideoConversion(void)
{
    HdffVideoFormat_t videoFormat;
    videoFormat.AutomaticEnabled = true;
    videoFormat.AfdEnabled = false;
    videoFormat.TvFormat = (HdffTvFormat_t) gHdffSetup.TvFormat;
    videoFormat.VideoConversion = (HdffVideoConversion_t) gHdffSetup.VideoConversion;
    mHdffCmdIf->CmdAvSetVideoFormat(0, &videoFormat);

    char str[128];
    sprintf(str, "%s: %s", tr("Video Conversion"), gHdffSetup.GetVideoConversionString());
    mVideoConversionItem->SetText(str);
    Display();
}
