/*
 * setup.c: Setup for the DVB HD Full Featured On Screen Display
 *
 * See the README file for copyright information and how to reach the author.
 *
 * $Id: setup.c 1.11 2011/04/17 11:45:17 kls Exp $
 */

#include "setup.h"
#include "hdffcmd.h"

const int kResolutions = 4;
const int kTvFormats = 2;
const int kVideoConversions = 6;
const int kAnalogueVideos = 4;
const int kAudioDownmixes = 5;
const int kOsdSizes = 5;
const int kRemoteProtocols = 3;

const int kResolution1080i = 0;
const int kResolution720p = 1;
const int kResolution576p = 2;
const int kResolution576i = 3;

static const char * ResolutionItems[] =
{
    "1080i",
    "720p",
    "576p",
    "576i",
    NULL
};

static const char * TvFormatItems[] =
{
    "4/3",
    "16/9",
    NULL
};

static const char * VideoConversionItems[] =
{
    "Automatic",
    "Letterbox 16/9",
    "Letterbox 14/9",
    "Pillarbox",
    "CentreCutOut",
    "Always 16/9",
    NULL
};

static const char * AnalogueVideoItems[] =
{
    "Disabled",
    "RGB",
    "CVBS + YUV",
    "YC (S-Video)",
    NULL
};

static const char * AudioDownmixItems[] =
{
    "Disabled",
    "Analogue only",
    "Always",
    "Automatic",
    "HDMI only",
    NULL
};

static const char * OsdSizeItems[] =
{
    "Follow resolution",
    "1920x1080",
    "1280x720",
    "1024x576",
    "720x576",
    NULL
};

static const char * RemoteProtocolItems[] =
{
    "none",
    "RC5",
    "RC6",
    NULL
};

cHdffSetup gHdffSetup;

cHdffSetup::cHdffSetup(void)
{
    Resolution = kResolution1080i;
    TvFormat = HDFF::tvFormat16by9;
    VideoConversion = HDFF::videoConversionPillarbox;
    AnalogueVideo = HDFF::videoOutCvbsYuv;
    AudioDelay = 0;
    AudioDownmix = HDFF::downmixAutomatic;
    OsdSize = 0;
    CecEnabled = 1;
    RemoteProtocol = 1;
    RemoteAddress = -1;
    HighLevelOsd = 1;
    TrueColorOsd = 1;
}

bool cHdffSetup::SetupParse(const char *Name, const char *Value)
{
    if      (strcmp(Name, "Resolution")      == 0) Resolution      = atoi(Value);
    else if (strcmp(Name, "TvFormat")        == 0) TvFormat        = atoi(Value);
    else if (strcmp(Name, "VideoConversion") == 0) VideoConversion = atoi(Value);
    else if (strcmp(Name, "AnalogueVideo")   == 0) AnalogueVideo   = atoi(Value);
    else if (strcmp(Name, "AudioDelay")      == 0) AudioDelay      = atoi(Value);
    else if (strcmp(Name, "AudioDownmix")    == 0) AudioDownmix    = atoi(Value);
    else if (strcmp(Name, "OsdSize")         == 0) OsdSize         = atoi(Value);
    else if (strcmp(Name, "CecEnabled")      == 0) CecEnabled      = atoi(Value);
    else if (strcmp(Name, "RemoteProtocol")  == 0) RemoteProtocol  = atoi(Value);
    else if (strcmp(Name, "RemoteAddress")   == 0) RemoteAddress   = atoi(Value);
    else if (strcmp(Name, "HighLevelOsd")    == 0) HighLevelOsd    = atoi(Value);
    else if (strcmp(Name, "TrueColorOsd")    == 0) TrueColorOsd    = atoi(Value);
    else return false;
    return true;
}

void cHdffSetup::GetOsdSize(int &Width, int &Height, double &PixelAspect)
{
    if (OsdSize == 0) {
        if (Resolution == kResolution1080i) {
            Width = 1920;
            Height = 1080;
        }
        else if (Resolution == kResolution720p) {
            Width = 1280;
            Height = 720;
        }
        else {
            Width = 720;
            Height = 576;
        }
        if (TvFormat == HDFF::tvFormat16by9)
            PixelAspect = 16.0 / 9.0;
        else
            PixelAspect = 4.0 / 3.0;
    }
    else if (OsdSize == 1) {
        Width = 1920;
        Height = 1080;
        PixelAspect = 16.0 / 9.0;
    }
    else if (OsdSize == 2) {
        Width = 1280;
        Height = 720;
        PixelAspect = 16.0 / 9.0;
    }
    else if (OsdSize == 3) {
        Width = 1024;
        Height = 576;
        PixelAspect = 16.0 / 9.0;
    }
    else {
        Width = 720;
        Height = 576;
        PixelAspect = 4.0 / 3.0;
    }
  PixelAspect /= double(Width) / Height;
}

HDFF::eHdmiVideoMode cHdffSetup::GetVideoMode(void)
{
    switch (Resolution)
    {
        case kResolution1080i:
        default:
            return HDFF::videoMode1080i50;
        case kResolution720p:
            return HDFF::videoMode720p50;
        case kResolution576p:
            return HDFF::videoMode576p50;
        case kResolution576i:
            return HDFF::videoMode576i50;
    }
}

cHdffSetupPage::cHdffSetupPage(HDFF::cHdffCmdIf * pHdffCmdIf)
{
    mHdffCmdIf = pHdffCmdIf;
    mNewHdffSetup = gHdffSetup;

    Add(new cMenuEditStraItem("Resolution", &mNewHdffSetup.Resolution, kResolutions, ResolutionItems));
    Add(new cMenuEditStraItem("TV format", &mNewHdffSetup.TvFormat, kTvFormats, TvFormatItems));
    Add(new cMenuEditStraItem("Video Conversion", &mNewHdffSetup.VideoConversion, kVideoConversions, VideoConversionItems));
    Add(new cMenuEditStraItem("Analogue Video", &mNewHdffSetup.AnalogueVideo, kAnalogueVideos, AnalogueVideoItems));
    Add(new cMenuEditIntItem("Audio Delay (ms)", &mNewHdffSetup.AudioDelay, 0, 500));
    Add(new cMenuEditStraItem("Audio Downmix", &mNewHdffSetup.AudioDownmix, kAudioDownmixes, AudioDownmixItems));
    Add(new cMenuEditStraItem("Osd Size", &mNewHdffSetup.OsdSize, kOsdSizes, OsdSizeItems));
    Add(new cMenuEditBoolItem("HDMI CEC", &mNewHdffSetup.CecEnabled));
    Add(new cMenuEditStraItem("Remote Control Protocol", &mNewHdffSetup.RemoteProtocol, kRemoteProtocols, RemoteProtocolItems));
    Add(new cMenuEditIntItem("Remote Control Address", &mNewHdffSetup.RemoteAddress, -1, 31));
    Add(new cMenuEditBoolItem("High Level OSD", &mNewHdffSetup.HighLevelOsd));
    Add(new cMenuEditBoolItem("Allow True Color OSD", &mNewHdffSetup.TrueColorOsd));
}

cHdffSetupPage::~cHdffSetupPage(void)
{
}

void cHdffSetupPage::Store(void)
{
    SetupStore("Resolution", mNewHdffSetup.Resolution);
    SetupStore("TvFormat", mNewHdffSetup.TvFormat);
    SetupStore("VideoConversion", mNewHdffSetup.VideoConversion);
    SetupStore("AnalogueVideo", mNewHdffSetup.AnalogueVideo);
    SetupStore("AudioDelay", mNewHdffSetup.AudioDelay);
    SetupStore("AudioDownmix", mNewHdffSetup.AudioDownmix);
    SetupStore("OsdSize", mNewHdffSetup.OsdSize);
    SetupStore("CecEnabled", mNewHdffSetup.CecEnabled);
    SetupStore("RemoteProtocol", mNewHdffSetup.RemoteProtocol);
    SetupStore("RemoteAddress", mNewHdffSetup.RemoteAddress);
    SetupStore("HighLevelOsd", mNewHdffSetup.HighLevelOsd);
    SetupStore("TrueColorOsd", mNewHdffSetup.TrueColorOsd);

    if (mHdffCmdIf)
    {
        if (mNewHdffSetup.Resolution != gHdffSetup.Resolution)
        {
            mHdffCmdIf->CmdHdmiSetVideoMode(mNewHdffSetup.GetVideoMode());
        }
        HDFF::tVideoFormat videoFormat;
        HDFF::tHdmiConfig hdmiConfig;

        videoFormat.AutomaticEnabled = true;
        videoFormat.AfdEnabled = true;
        videoFormat.TvFormat = (HDFF::eTvFormat) mNewHdffSetup.TvFormat;
        videoFormat.VideoConversion = (HDFF::eVideoConversion) mNewHdffSetup.VideoConversion;
        mHdffCmdIf->CmdAvSetVideoFormat(0, &videoFormat);

        mHdffCmdIf->CmdAvSetAudioDelay(mNewHdffSetup.AudioDelay);
        mHdffCmdIf->CmdAvSetAudioDownmix((HDFF::eDownmixMode) mNewHdffSetup.AudioDownmix);

        mHdffCmdIf->CmdMuxSetVideoOut((HDFF::eVideoOut) mNewHdffSetup.AnalogueVideo);

        hdmiConfig.TransmitAudio = true;
        hdmiConfig.ForceDviMode = false;
        hdmiConfig.CecEnabled = mNewHdffSetup.CecEnabled;
        mHdffCmdIf->CmdHdmiConfigure(&hdmiConfig);

        mHdffCmdIf->CmdRemoteSetProtocol((HDFF::eRemoteProtocol) mNewHdffSetup.RemoteProtocol);
        mHdffCmdIf->CmdRemoteSetAddressFilter(mNewHdffSetup.RemoteAddress >= 0, mNewHdffSetup.RemoteAddress);
    }

    gHdffSetup = mNewHdffSetup;
}
