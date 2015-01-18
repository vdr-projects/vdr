/*
 * setup.c: Setup for the DVB HD Full Featured On Screen Display
 *
 * See the README file for copyright information and how to reach the author.
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
    AvSyncShift = 0;
    OsdSize = 0;
    CecEnabled = 1;
    CecTvOn = 1;
    CecTvOff = 0;
    RemoteProtocol = 1;
    RemoteAddress = -1;
    HighLevelOsd = 1;
    TrueColorOsd = 1;
    TrueColorFormat = 0;
    HideMainMenu = 0;
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
    else if (strcmp(Name, "AvSyncShift")       == 0) AvSyncShift       = atoi(Value);
    else if (strcmp(Name, "OsdSize")           == 0) OsdSize           = atoi(Value);
    else if (strcmp(Name, "CecEnabled")        == 0) CecEnabled        = atoi(Value);
    else if (strcmp(Name, "CecTvOn")           == 0) CecTvOn           = atoi(Value);
    else if (strcmp(Name, "CecTvOff")          == 0) CecTvOff          = atoi(Value);
    else if (strcmp(Name, "RemoteProtocol")    == 0) RemoteProtocol    = atoi(Value);
    else if (strcmp(Name, "RemoteAddress")     == 0) RemoteAddress     = atoi(Value);
    else if (strcmp(Name, "HighLevelOsd")      == 0) HighLevelOsd      = atoi(Value);
    else if (strcmp(Name, "TrueColorOsd")      == 0) TrueColorOsd      = atoi(Value);
    else if (strcmp(Name, "TrueColorFormat")   == 0) TrueColorFormat   = atoi(Value);
    else if (strcmp(Name, "HideMainMenu")      == 0) HideMainMenu      = atoi(Value);
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

void cHdffSetup::SetNextVideoConversion(void)
{
    int nextVideoConversion = HDFF_VIDEO_CONVERSION_AUTOMATIC;

    if (TvFormat == HDFF_TV_FORMAT_16_BY_9)
    {
        switch (VideoConversion)
        {
            case HDFF_VIDEO_CONVERSION_PILLARBOX:
                nextVideoConversion = HDFF_VIDEO_CONVERSION_CENTRE_CUT_OUT;
                break;
            case HDFF_VIDEO_CONVERSION_CENTRE_CUT_OUT:
                nextVideoConversion = HDFF_VIDEO_CONVERSION_ALWAYS_16_BY_9;
                break;
            case HDFF_VIDEO_CONVERSION_ALWAYS_16_BY_9:
                nextVideoConversion = HDFF_VIDEO_CONVERSION_ZOOM_16_BY_9;
                break;
            case HDFF_VIDEO_CONVERSION_ZOOM_16_BY_9:
                nextVideoConversion = HDFF_VIDEO_CONVERSION_PILLARBOX;
                break;
        }
    }
    else
    {
        switch (VideoConversion)
        {
            case HDFF_VIDEO_CONVERSION_LETTERBOX_16_BY_9:
                nextVideoConversion = HDFF_VIDEO_CONVERSION_LETTERBOX_14_BY_9;
                break;
            case HDFF_VIDEO_CONVERSION_LETTERBOX_14_BY_9:
                nextVideoConversion = HDFF_VIDEO_CONVERSION_CENTRE_CUT_OUT;
                break;
            case HDFF_VIDEO_CONVERSION_CENTRE_CUT_OUT:
                nextVideoConversion = HDFF_VIDEO_CONVERSION_LETTERBOX_16_BY_9;
                break;
        }
    }
    VideoConversion = nextVideoConversion;
}

const char * cHdffSetup::GetVideoConversionString(void)
{
    switch (VideoConversion)
    {
        case HDFF_VIDEO_CONVERSION_AUTOMATIC:
        default:
            return tr("Automatic");
        case HDFF_VIDEO_CONVERSION_LETTERBOX_16_BY_9:
            return tr("Letterbox 16/9");
        case HDFF_VIDEO_CONVERSION_LETTERBOX_14_BY_9:
            return tr("Letterbox 14/9");
        case HDFF_VIDEO_CONVERSION_PILLARBOX:
            return tr("Pillarbox");
        case HDFF_VIDEO_CONVERSION_CENTRE_CUT_OUT:
            return tr("CentreCutOut");
        case HDFF_VIDEO_CONVERSION_ALWAYS_16_BY_9:
            return tr("Always 16/9");
        case HDFF_VIDEO_CONVERSION_ZOOM_16_BY_9:
            return tr("Zoom 16/9");
    }
}

void cHdffSetup::SetVideoFormat(HDFF::cHdffCmdIf * HdffCmdIf)
{
    HdffVideoFormat_t videoFormat;

    videoFormat.AutomaticEnabled = true;
    videoFormat.AfdEnabled = false;
    videoFormat.TvFormat = (HdffTvFormat_t) TvFormat;
    videoFormat.VideoConversion = (HdffVideoConversion_t) VideoConversion;
    HdffCmdIf->CmdAvSetVideoFormat(0, &videoFormat);
}

cHdffSetupPage::cHdffSetupPage(HDFF::cHdffCmdIf * pHdffCmdIf)
{
    const int kResolutions = 4;
    const int kVideoModeAdaptions = 4;
    const int kTvFormats = 2;
    const int kAnalogueVideos = 4;
    const int kAudioDownmixes = 5;
    const int kOsdSizes = 5;
    const int kRemoteProtocols = 3;
    const int kTrueColorFormats = 3;

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

    static const char * TrueColorFormatItems[kTrueColorFormats] =
    {
        "ARGB8888",
        "ARGB8565",
        "ARGB4444",
    };

    mHdffCmdIf = pHdffCmdIf;
    mNewHdffSetup = gHdffSetup;

    Add(new cMenuEditStraItem(tr("Resolution"), &mNewHdffSetup.Resolution, kResolutions, ResolutionItems));
    Add(new cMenuEditStraItem(tr("Video Mode Adaption"), &mNewHdffSetup.VideoModeAdaption, kVideoModeAdaptions, VideoModeAdaptionItems));
    mTvFormatItem = new cMenuEditStraItem(tr("TV format"), &mNewHdffSetup.TvFormat, kTvFormats, TvFormatItems);
    Add(mTvFormatItem);
    Add(new cMenuEditStraItem(tr("Analogue Video"), &mNewHdffSetup.AnalogueVideo, kAnalogueVideos, AnalogueVideoItems));
    Add(new cMenuEditIntItem(tr("Audio Delay (ms)"), &mNewHdffSetup.AudioDelay, 0, 500));
    Add(new cMenuEditStraItem(tr("Audio Downmix"), &mNewHdffSetup.AudioDownmix, kAudioDownmixes, AudioDownmixItems));
    Add(new cMenuEditIntItem(tr("A/V Sync Shift (ms)"), &mNewHdffSetup.AvSyncShift, -500, 500));
    Add(new cMenuEditStraItem(tr("OSD Size"), &mNewHdffSetup.OsdSize, kOsdSizes, OsdSizeItems));
    Add(new cMenuEditBoolItem(tr("HDMI CEC"), &mNewHdffSetup.CecEnabled));
    Add(new cMenuEditBoolItem(tr("CEC: Switch TV on"), &mNewHdffSetup.CecTvOn));
    Add(new cMenuEditBoolItem(tr("CEC: Switch TV off"), &mNewHdffSetup.CecTvOff));
    Add(new cMenuEditStraItem(tr("Remote Control Protocol"), &mNewHdffSetup.RemoteProtocol, kRemoteProtocols, RemoteProtocolItems));
    Add(new cMenuEditIntItem(tr("Remote Control Address"), &mNewHdffSetup.RemoteAddress, -1, 31));
    Add(new cMenuEditBoolItem(tr("High Level OSD"), &mNewHdffSetup.HighLevelOsd));
    Add(new cMenuEditBoolItem(tr("Allow True Color OSD"), &mNewHdffSetup.TrueColorOsd));
    Add(new cMenuEditStraItem(tr("True Color format"), &mNewHdffSetup.TrueColorFormat, kTrueColorFormats, TrueColorFormatItems));
    Add(new cMenuEditBoolItem(tr("Hide mainmenu entry"), &mNewHdffSetup.HideMainMenu));

    mVideoConversion = 0;
    if (mNewHdffSetup.TvFormat == HDFF_TV_FORMAT_16_BY_9)
    {
        switch (mNewHdffSetup.VideoConversion)
        {
            case HDFF_VIDEO_CONVERSION_PILLARBOX:
                mVideoConversion = 0;
                break;
            case HDFF_VIDEO_CONVERSION_CENTRE_CUT_OUT:
                mVideoConversion = 1;
                break;
            case HDFF_VIDEO_CONVERSION_ALWAYS_16_BY_9:
                mVideoConversion = 2;
                break;
            case HDFF_VIDEO_CONVERSION_ZOOM_16_BY_9:
                mVideoConversion = 3;
                break;
        }
    }
    else
    {
        switch (mNewHdffSetup.VideoConversion)
        {
            case HDFF_VIDEO_CONVERSION_LETTERBOX_16_BY_9:
                mVideoConversion = 0;
                break;
            case HDFF_VIDEO_CONVERSION_LETTERBOX_14_BY_9:
                mVideoConversion = 1;
                break;
            case HDFF_VIDEO_CONVERSION_CENTRE_CUT_OUT:
                mVideoConversion = 2;
                break;
        }
    }
    BuildVideoConversionItem();
}

cHdffSetupPage::~cHdffSetupPage(void)
{
}

void cHdffSetupPage::BuildVideoConversionItem(void)
{
    const int kVideoConversions4by3 = 3;
    const int kVideoConversions16by9 = 4;

    static const char * VideoConversionItems4by3[kVideoConversions4by3] =
    {
        tr("Letterbox 16/9"),
        tr("Letterbox 14/9"),
        tr("CentreCutOut")
    };

    static const char * VideoConversionItems16by9[kVideoConversions16by9] =
    {
        tr("Pillarbox"),
        tr("CentreCutOut"),
        tr("Always 16/9"),
        tr("Zoom 16/9")
    };

    cOsdItem * item;

    cList<cOsdItem>::Del(mTvFormatItem->Next());
    if (mNewHdffSetup.TvFormat == HDFF_TV_FORMAT_16_BY_9)
    {
        item = new cMenuEditStraItem(tr("Video Conversion"), &mVideoConversion,
                kVideoConversions16by9, VideoConversionItems16by9);
    }
    else
    {
        item = new cMenuEditStraItem(tr("Video Conversion"), &mVideoConversion,
                kVideoConversions4by3, VideoConversionItems4by3);
    }
    Add(item, false, mTvFormatItem);
}

void cHdffSetupPage::Store(void)
{
    if (mNewHdffSetup.TvFormat == HDFF_TV_FORMAT_16_BY_9)
    {
        switch (mVideoConversion)
        {
            case 0:
                mNewHdffSetup.VideoConversion = HDFF_VIDEO_CONVERSION_PILLARBOX;
                break;
            case 1:
                mNewHdffSetup.VideoConversion = HDFF_VIDEO_CONVERSION_CENTRE_CUT_OUT;
                break;
            case 2:
                mNewHdffSetup.VideoConversion = HDFF_VIDEO_CONVERSION_ALWAYS_16_BY_9;
                break;
            case 3:
                mNewHdffSetup.VideoConversion = HDFF_VIDEO_CONVERSION_ZOOM_16_BY_9;
                break;
        }
    }
    else
    {
        switch (mVideoConversion)
        {
            case 0:
                mNewHdffSetup.VideoConversion = HDFF_VIDEO_CONVERSION_LETTERBOX_16_BY_9;
                break;
            case 1:
                mNewHdffSetup.VideoConversion = HDFF_VIDEO_CONVERSION_LETTERBOX_14_BY_9;
                break;
            case 2:
                mNewHdffSetup.VideoConversion = HDFF_VIDEO_CONVERSION_CENTRE_CUT_OUT;
                break;
        }
    }
    SetupStore("Resolution", mNewHdffSetup.Resolution);
    SetupStore("VideoModeAdaption", mNewHdffSetup.VideoModeAdaption);
    SetupStore("TvFormat", mNewHdffSetup.TvFormat);
    SetupStore("VideoConversion", mNewHdffSetup.VideoConversion);
    SetupStore("AnalogueVideo", mNewHdffSetup.AnalogueVideo);
    SetupStore("AudioDelay", mNewHdffSetup.AudioDelay);
    SetupStore("AudioDownmix", mNewHdffSetup.AudioDownmix);
    SetupStore("AvSyncShift", mNewHdffSetup.AvSyncShift);
    SetupStore("OsdSize", mNewHdffSetup.OsdSize);
    SetupStore("CecEnabled", mNewHdffSetup.CecEnabled);
    SetupStore("CecTvOn", mNewHdffSetup.CecTvOn);
    SetupStore("CecTvOff", mNewHdffSetup.CecTvOff);
    SetupStore("RemoteProtocol", mNewHdffSetup.RemoteProtocol);
    SetupStore("RemoteAddress", mNewHdffSetup.RemoteAddress);
    SetupStore("HighLevelOsd", mNewHdffSetup.HighLevelOsd);
    SetupStore("TrueColorOsd", mNewHdffSetup.TrueColorOsd);
    SetupStore("TrueColorFormat", mNewHdffSetup.TrueColorFormat);
    SetupStore("HideMainMenu", mNewHdffSetup.HideMainMenu);

    if (mHdffCmdIf)
    {
        if (mNewHdffSetup.Resolution != gHdffSetup.Resolution)
        {
            mHdffCmdIf->CmdHdmiSetVideoMode(mNewHdffSetup.GetVideoMode());
        }
        HdffHdmiConfig_t hdmiConfig;

        mNewHdffSetup.SetVideoFormat(mHdffCmdIf);

        mHdffCmdIf->CmdAvSetAudioDelay(mNewHdffSetup.AudioDelay);
        mHdffCmdIf->CmdAvSetAudioDownmix((HdffAudioDownmixMode_t) mNewHdffSetup.AudioDownmix);
        mHdffCmdIf->CmdAvSetSyncShift(mNewHdffSetup.AvSyncShift);

        mHdffCmdIf->CmdMuxSetVideoOut((HdffVideoOut_t) mNewHdffSetup.AnalogueVideo);

        memset(&hdmiConfig, 0, sizeof(hdmiConfig));
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

eOSState cHdffSetupPage::ProcessKey(eKeys key)
{
    eOSState state = cMenuSetupPage::ProcessKey(key);

    if (state == osContinue)
    {
        cOsdItem * item;
        switch (key)
        {
            case kLeft:
            case kRight:
                item = Get(Current());
                if (item == mTvFormatItem)
                {
                    mVideoConversion = 0;
                    BuildVideoConversionItem();
                    Display();
                }
                break;
            default:
                break;
        }
    }
    return state;
}
