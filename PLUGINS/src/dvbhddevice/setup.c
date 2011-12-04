/*
 * setup.c: Setup for the DVB HD Full Featured On Screen Display
 *
 * See the README file for copyright information and how to reach the author.
 *
 * $Id: setup.c 1.14 2011/12/04 15:31:58 kls Exp $
 */

#include "setup.h"
#include "hdffcmd.h"

const int kResolution1080i = 0;
const int kResolution720p = 1;
const int kResolution576p = 2;
const int kResolution576i = 3;


cHdffSetup gHdffSetup;

cHdffSetup::cHdffSetup(void)
{
    Resolution = kResolution1080i;
    VideoModeAdaption = HDFF_VIDEO_MODE_ADAPT_OFF;
    TvFormat = HDFF_TV_FORMAT_16_BY_9;
    VideoConversion = HDFF_VIDEO_CONVERSION_PILLARBOX;
    AnalogueVideo = HDFF_VIDEO_OUT_CVBS_YUV;
    AudioDelay = 0;
    AudioDownmix = HDFF_AUDIO_DOWNMIX_AUTOMATIC;
    OsdSize = 0;
    CecEnabled = 1;
    RemoteProtocol = 1;
    RemoteAddress = -1;
    HighLevelOsd = 1;
    TrueColorOsd = 1;
}

bool cHdffSetup::SetupParse(const char *Name, const char *Value)
{
    if      (strcmp(Name, "Resolution")        == 0) Resolution        = atoi(Value);
    else if (strcmp(Name, "VideoModeAdaption") == 0) VideoModeAdaption = atoi(Value);
    else if (strcmp(Name, "TvFormat")          == 0) TvFormat          = atoi(Value);
    else if (strcmp(Name, "VideoConversion")   == 0) VideoConversion   = atoi(Value);
    else if (strcmp(Name, "AnalogueVideo")     == 0) AnalogueVideo     = atoi(Value);
    else if (strcmp(Name, "AudioDelay")        == 0) AudioDelay        = atoi(Value);
    else if (strcmp(Name, "AudioDownmix")      == 0) AudioDownmix      = atoi(Value);
    else if (strcmp(Name, "OsdSize")           == 0) OsdSize           = atoi(Value);
    else if (strcmp(Name, "CecEnabled")        == 0) CecEnabled        = atoi(Value);
    else if (strcmp(Name, "RemoteProtocol")    == 0) RemoteProtocol    = atoi(Value);
    else if (strcmp(Name, "RemoteAddress")     == 0) RemoteAddress     = atoi(Value);
    else if (strcmp(Name, "HighLevelOsd")      == 0) HighLevelOsd      = atoi(Value);
    else if (strcmp(Name, "TrueColorOsd")      == 0) TrueColorOsd      = atoi(Value);
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
        if (TvFormat == HDFF_TV_FORMAT_16_BY_9)
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

HdffVideoMode_t cHdffSetup::GetVideoMode(void)
{
    switch (Resolution)
    {
        case kResolution1080i:
        default:
            return HDFF_VIDEO_MODE_1080I50;
        case kResolution720p:
            return HDFF_VIDEO_MODE_720P50;
        case kResolution576p:
            return HDFF_VIDEO_MODE_576P50;
        case kResolution576i:
            return HDFF_VIDEO_MODE_576I50;
    }
}

cHdffSetupPage::cHdffSetupPage(HDFF::cHdffCmdIf * pHdffCmdIf)
{
    const int kResolutions = 4;
    const int kVideoModeAdaptions = 4;
    const int kTvFormats = 2;
    const int kVideoConversions = 6;
    const int kAnalogueVideos = 4;
    const int kAudioDownmixes = 5;
    const int kOsdSizes = 5;
    const int kRemoteProtocols = 3;

    static const char * ResolutionItems[kResolutions] =
    {
        "1080i",
        "720p",
        "576p",
        "576i",
    };

    static const char * VideoModeAdaptionItems[kVideoModeAdaptions] =
    {
        tr("Off"),
        tr("Frame rate"),
        tr("HD Only"),
        tr("Always")
    };

    static const char * TvFormatItems[kTvFormats] =
    {
        "4/3",
        "16/9",
    };

    static const char * VideoConversionItems[kVideoConversions] =
    {
        tr("Automatic"),
        tr("Letterbox 16/9"),
        tr("Letterbox 14/9"),
        tr("Pillarbox"),
        tr("CentreCutOut"),
        tr("Always 16/9"),
    };


    static const char * AnalogueVideoItems[kAnalogueVideos] =
    {
        tr("Disabled"),
        "RGB",
        "CVBS + YUV",
        "YC (S-Video)",
    };

    static const char * AudioDownmixItems[kAudioDownmixes] =
    {
        tr("Disabled"),
        tr("Analogue only"),
        tr("Always"),
        tr("Automatic"),
        tr("HDMI only"),
    };

    static const char * OsdSizeItems[kOsdSizes] =
    {
        tr("Follow resolution"),
        "1920x1080",
        "1280x720",
        "1024x576",
        "720x576",
    };

    static const char * RemoteProtocolItems[] =
    {
        tr("none"),
        "RC5",
        "RC6",
    };

    mHdffCmdIf = pHdffCmdIf;
    mNewHdffSetup = gHdffSetup;

    Add(new cMenuEditStraItem(tr("Resolution"), &mNewHdffSetup.Resolution, kResolutions, ResolutionItems));
    Add(new cMenuEditStraItem(tr("Video Mode Adaption"), &mNewHdffSetup.VideoModeAdaption, kVideoModeAdaptions, VideoModeAdaptionItems));
    Add(new cMenuEditStraItem(tr("TV format"), &mNewHdffSetup.TvFormat, kTvFormats, TvFormatItems));
    Add(new cMenuEditStraItem(tr("Video Conversion"), &mNewHdffSetup.VideoConversion, kVideoConversions, VideoConversionItems));
    Add(new cMenuEditStraItem(tr("Analogue Video"), &mNewHdffSetup.AnalogueVideo, kAnalogueVideos, AnalogueVideoItems));
    Add(new cMenuEditIntItem(tr("Audio Delay (ms)"), &mNewHdffSetup.AudioDelay, 0, 500));
    Add(new cMenuEditStraItem(tr("Audio Downmix"), &mNewHdffSetup.AudioDownmix, kAudioDownmixes, AudioDownmixItems));
    Add(new cMenuEditStraItem(tr("OSD Size"), &mNewHdffSetup.OsdSize, kOsdSizes, OsdSizeItems));
    Add(new cMenuEditBoolItem(tr("HDMI CEC"), &mNewHdffSetup.CecEnabled));
    Add(new cMenuEditStraItem(tr("Remote Control Protocol"), &mNewHdffSetup.RemoteProtocol, kRemoteProtocols, RemoteProtocolItems));
    Add(new cMenuEditIntItem(tr("Remote Control Address"), &mNewHdffSetup.RemoteAddress, -1, 31));
    Add(new cMenuEditBoolItem(tr("High Level OSD"), &mNewHdffSetup.HighLevelOsd));
    Add(new cMenuEditBoolItem(tr("Allow True Color OSD"), &mNewHdffSetup.TrueColorOsd));
}

cHdffSetupPage::~cHdffSetupPage(void)
{
}

void cHdffSetupPage::Store(void)
{
    SetupStore("Resolution", mNewHdffSetup.Resolution);
    SetupStore("VideoModeAdaption", mNewHdffSetup.VideoModeAdaption);
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
        HdffVideoFormat_t videoFormat;
        HdffHdmiConfig_t hdmiConfig;

        videoFormat.AutomaticEnabled = true;
        videoFormat.AfdEnabled = true;
        videoFormat.TvFormat = (HdffTvFormat_t) mNewHdffSetup.TvFormat;
        videoFormat.VideoConversion = (HdffVideoConversion_t) mNewHdffSetup.VideoConversion;
        mHdffCmdIf->CmdAvSetVideoFormat(0, &videoFormat);

        mHdffCmdIf->CmdAvSetAudioDelay(mNewHdffSetup.AudioDelay);
        mHdffCmdIf->CmdAvSetAudioDownmix((HdffAudioDownmixMode_t) mNewHdffSetup.AudioDownmix);

        mHdffCmdIf->CmdMuxSetVideoOut((HdffVideoOut_t) mNewHdffSetup.AnalogueVideo);

        hdmiConfig.TransmitAudio = true;
        hdmiConfig.ForceDviMode = false;
        hdmiConfig.CecEnabled = mNewHdffSetup.CecEnabled;
        hdmiConfig.VideoModeAdaption = (HdffVideoModeAdaption_t) mNewHdffSetup.VideoModeAdaption;
        mHdffCmdIf->CmdHdmiConfigure(&hdmiConfig);

        mHdffCmdIf->CmdRemoteSetProtocol((HdffRemoteProtocol_t) mNewHdffSetup.RemoteProtocol);
        mHdffCmdIf->CmdRemoteSetAddressFilter(mNewHdffSetup.RemoteAddress >= 0, mNewHdffSetup.RemoteAddress);
    }

    gHdffSetup = mNewHdffSetup;
}
