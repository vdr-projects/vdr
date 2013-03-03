/*
 * hdffcmd.c: TODO(short description)
 *
 * See the README file for copyright information and how to reach the author.
 */

#include <stdint.h>

#include "hdffcmd.h"
#include "libhdffcmd/hdffcmd.h"
#include <stdio.h>
#include <string.h>
#include <vdr/tools.h>


namespace HDFF
{

cHdffCmdIf::cHdffCmdIf(int OsdDev)
{
    mOsdDev = OsdDev;
    if (mOsdDev < 0)
    {
        //printf("ERROR: invalid OSD device handle (%d)!\n", mOsdDev);
    }
}

cHdffCmdIf::~cHdffCmdIf(void)
{
}


uint32_t cHdffCmdIf::CmdGetFirmwareVersion(char * pString, uint32_t MaxLength)
{
    uint32_t version;
    int err;

    err = HdffCmdGetFirmwareVersion(mOsdDev, &version, pString, MaxLength);
    if (err == 0)
        return version;
    return 0;
}

uint32_t cHdffCmdIf::CmdGetInterfaceVersion(char * pString, uint32_t MaxLength)
{
    uint32_t version;
    int err;

    err = HdffCmdGetInterfaceVersion(mOsdDev, &version, pString, MaxLength);
    if (err == 0)
        return version;
    return 0;
}

uint32_t cHdffCmdIf::CmdGetCopyrights(uint8_t Index, char * pString, uint32_t MaxLength)
{
    int err;

    err = HdffCmdGetCopyrights(mOsdDev, Index, pString, MaxLength);
    if (err == 0)
        return strlen(pString);
    return 0;
}


void cHdffCmdIf::CmdAvSetPlayMode(uint8_t PlayMode, bool Realtime)
{
    HdffCmdAvSetPlayMode(mOsdDev, PlayMode, Realtime);
}

void cHdffCmdIf::CmdAvSetVideoPid(uint8_t DecoderIndex, uint16_t VideoPid, HdffVideoStreamType_t StreamType, bool PlaybackMode)
{
    //printf("SetVideoPid %d %d\n", VideoPid, StreamType);
    HdffCmdAvSetVideoPid(mOsdDev, DecoderIndex, VideoPid, StreamType);
}

void cHdffCmdIf::CmdAvSetAudioPid(uint8_t DecoderIndex, uint16_t AudioPid, HdffAudioStreamType_t StreamType, HdffAvContainerType_t ContainerType)
{
    //printf("SetAudioPid %d %d %d\n", AudioPid, StreamType, ContainerType);
    HdffCmdAvSetAudioPid(mOsdDev, DecoderIndex, AudioPid, StreamType,
                         ContainerType);
}

void cHdffCmdIf::CmdAvSetPcrPid(uint8_t DecoderIndex, uint16_t PcrPid)
{
    //printf("SetPcrPid %d\n", PcrPid);
    HdffCmdAvSetPcrPid(mOsdDev, DecoderIndex, PcrPid);
}

void cHdffCmdIf::CmdAvSetTeletextPid(uint8_t DecoderIndex, uint16_t TeletextPid)
{
    HdffCmdAvSetTeletextPid(mOsdDev, DecoderIndex, TeletextPid);
}

void cHdffCmdIf::CmdAvSetVideoWindow(uint8_t DecoderIndex, bool Enable, uint16_t X, uint16_t Y, uint16_t Width, uint16_t Height)
{
    HdffCmdAvSetVideoWindow(mOsdDev, DecoderIndex, Enable, X, Y, Width, Height);
}

void cHdffCmdIf::CmdAvShowStillImage(uint8_t DecoderIndex, const uint8_t * pStillImage, int Size, HdffVideoStreamType_t StreamType)
{
    HdffCmdAvShowStillImage(mOsdDev, DecoderIndex, pStillImage, Size,
                            StreamType);
}

void cHdffCmdIf::CmdAvSetDecoderInput(uint8_t DecoderIndex, uint8_t DemultiplexerIndex)
{
    HdffCmdAvSetDecoderInput(mOsdDev, DecoderIndex, DemultiplexerIndex);
}

void cHdffCmdIf::CmdAvSetDemultiplexerInput(uint8_t DemultiplexerIndex, uint8_t TsInputIndex)
{
    HdffCmdAvSetDemultiplexerInput(mOsdDev, DemultiplexerIndex, TsInputIndex);
}

void cHdffCmdIf::CmdAvSetVideoFormat(uint8_t DecoderIndex, const HdffVideoFormat_t * pVideoFormat)
{
    HdffCmdAvSetVideoFormat(mOsdDev, DecoderIndex, pVideoFormat);
}

void cHdffCmdIf::CmdAvSetVideoOutputMode(uint8_t DecoderIndex, HdffVideoOutputMode_t OutputMode)
{
    HdffCmdAvSetVideoOutputMode(mOsdDev, DecoderIndex, OutputMode);
}

void cHdffCmdIf::CmdAvSetStc(uint8_t DecoderIndex, uint64_t Stc)
{
    HdffCmdAvSetStc(mOsdDev, DecoderIndex, Stc);
}

void cHdffCmdIf::CmdAvFlushBuffer(uint8_t DecoderIndex, bool FlushAudio, bool FlushVideo)
{
    HdffCmdAvFlushBuffer(mOsdDev, DecoderIndex, FlushAudio, FlushVideo);
}

void cHdffCmdIf::CmdAvEnableSync(uint8_t DecoderIndex, bool EnableSync)
{
    HdffCmdAvEnableSync(mOsdDev, DecoderIndex, EnableSync, EnableSync);
}

void cHdffCmdIf::CmdAvSetVideoSpeed(uint8_t DecoderIndex, int32_t Speed)
{
    HdffCmdAvSetVideoSpeed(mOsdDev, DecoderIndex, Speed);
}

void cHdffCmdIf::CmdAvSetAudioSpeed(uint8_t DecoderIndex, int32_t Speed)
{
    HdffCmdAvSetAudioSpeed(mOsdDev, DecoderIndex, Speed);
}

void cHdffCmdIf::CmdAvEnableVideoAfterStop(uint8_t DecoderIndex, bool EnableVideoAfterStop)
{
    HdffCmdAvEnableVideoAfterStop(mOsdDev, DecoderIndex, EnableVideoAfterStop);
}

void cHdffCmdIf::CmdAvSetAudioDelay(int16_t Delay)
{
    HdffCmdAvSetAudioDelay(mOsdDev, Delay);
}

void cHdffCmdIf::CmdAvSetAudioDownmix(HdffAudioDownmixMode_t DownmixMode)
{
    HdffCmdAvSetAudioDownmix(mOsdDev, DownmixMode);
}

void cHdffCmdIf::CmdAvSetAudioChannel(uint8_t AudioChannel)
{
    HdffCmdAvSetAudioChannel(mOsdDev, AudioChannel);
}

void cHdffCmdIf::CmdAvSetSyncShift(int16_t SyncShift)
{
    HdffCmdAvSetSyncShift(mOsdDev, SyncShift);
}

void cHdffCmdIf::CmdAvMuteAudio(uint8_t DecoderIndex, bool Mute)
{
    HdffCmdAvMuteAudio(mOsdDev, DecoderIndex, Mute);
}

void cHdffCmdIf::CmdOsdConfigure(const HdffOsdConfig_t * pConfig)
{
    HdffCmdOsdConfigure(mOsdDev, pConfig);
}

void cHdffCmdIf::CmdOsdReset(void)
{
    HdffCmdOsdReset(mOsdDev);
}

uint32_t cHdffCmdIf::CmdOsdCreateDisplay(uint32_t Width, uint32_t Height, HdffColorType_t ColorType)
{
    //printf("CreateDisplay %d %d %d\n", Width, Height, ColorType);
    uint32_t newDisplay;

    if (HdffCmdOsdCreateDisplay(mOsdDev, Width, Height, ColorType, &newDisplay) == 0)
        return newDisplay;
    LOG_ERROR_STR("Error creating display");
    return HDFF_INVALID_HANDLE;
}

void cHdffCmdIf::CmdOsdDeleteDisplay(uint32_t hDisplay)
{
    //printf("DeleteDisplay\n");
    HdffCmdOsdDeleteDisplay(mOsdDev, hDisplay);
}

void cHdffCmdIf::CmdOsdEnableDisplay(uint32_t hDisplay, bool Enable)
{
    //printf("EnableDisplay\n");
    HdffCmdOsdEnableDisplay(mOsdDev, hDisplay, Enable);
}

void cHdffCmdIf::CmdOsdSetDisplayOutputRectangle(uint32_t hDisplay, uint32_t X, uint32_t Y, uint32_t Width, uint32_t Height)
{
    //printf("SetOutputRect %d %d %d %d %d\n", hDisplay, X, Y, Width, Height);
    HdffCmdOsdSetDisplayOutputRectangle(mOsdDev, hDisplay, X, Y, Width, Height);
}

void cHdffCmdIf::CmdOsdSetDisplayClippingArea(uint32_t hDisplay, bool Enable, uint32_t X, uint32_t Y, uint32_t Width, uint32_t Height)
{
    //printf("SetClippingArea %d %d %d %d %d %d\n", hDisplay, Enable, X, Y, Width, Height);
    HdffCmdOsdSetDisplayClippingArea(mOsdDev, hDisplay, Enable, X, Y, Width, Height);
}

void cHdffCmdIf::CmdOsdRenderDisplay(uint32_t hDisplay)
{
    //printf("Render %08X\n", hDisplay);
    HdffCmdOsdRenderDisplay(mOsdDev, hDisplay);
}

uint32_t cHdffCmdIf::CmdOsdCreatePalette(HdffColorType_t ColorType, HdffColorFormat_t ColorFormat,
                                         uint32_t NumColors, const uint32_t * pColors)
{
    uint32_t newPalette;
    int err;

    err = HdffCmdOsdCreatePalette(mOsdDev, ColorType, ColorFormat, NumColors,
                                  pColors, &newPalette);
    if (err == 0)
        return newPalette;
    LOG_ERROR_STR("Error creating palette");
    return HDFF_INVALID_HANDLE;
}

void cHdffCmdIf::CmdOsdDeletePalette(uint32_t hPalette)
{
    HdffCmdOsdDeletePalette(mOsdDev, hPalette);
}

void cHdffCmdIf::CmdOsdSetDisplayPalette(uint32_t hDisplay, uint32_t hPalette)
{
    HdffCmdOsdSetDisplayPalette(mOsdDev, hDisplay, hPalette);
}

void cHdffCmdIf::CmdOsdSetPaletteColors(uint32_t hPalette, HdffColorFormat_t ColorFormat,
                                        uint8_t StartColor, uint32_t NumColors, const uint32_t * pColors)
{
    HdffCmdOsdSetPaletteColors(mOsdDev, hPalette, ColorFormat, StartColor,
                               NumColors, pColors);
}

uint32_t cHdffCmdIf::CmdOsdCreateFontFace(const uint8_t * pFontData, uint32_t DataSize)
{
    //printf("CreateFontFace %d\n", DataSize);
    uint32_t newFontFace;
    int err;

    err = HdffCmdOsdCreateFontFace(mOsdDev, pFontData, DataSize, &newFontFace);
    if (err == 0)
        return newFontFace;
    LOG_ERROR_STR("Error creating font face");
    return HDFF_INVALID_HANDLE;
}

void cHdffCmdIf::CmdOsdDeleteFontFace(uint32_t hFontFace)
{
    //printf("DeleteFontFace %08X\n", hFontFace);
    HdffCmdOsdDeleteFontFace(mOsdDev, hFontFace);
}

uint32_t cHdffCmdIf::CmdOsdCreateFont(uint32_t hFontFace, uint32_t Size)
{
    //printf("CreateFont %d\n", Size);
    uint32_t newFont;
    int err;

    err = HdffCmdOsdCreateFont(mOsdDev, hFontFace, Size, &newFont);
    if (err == 0)
        return newFont;
    LOG_ERROR_STR("Error creating font");
    return HDFF_INVALID_HANDLE;
}

void cHdffCmdIf::CmdOsdDeleteFont(uint32_t hFont)
{
    //printf("DeleteFont %08X\n", hFont);
    HdffCmdOsdDeleteFont(mOsdDev, hFont);
}

void cHdffCmdIf::CmdOsdDrawRectangle(uint32_t hDisplay, int X, int Y, int Width, int Height, uint32_t Color)
{
    //printf("Rect (%d,%d) %d x %d, %08X\n", X, Y, Width, Height, Color);
    HdffCmdOsdDrawRectangle(mOsdDev, hDisplay, X, Y, Width, Height, Color);
}

void cHdffCmdIf::CmdOsdDrawEllipse(uint32_t hDisplay, int CX, int CY, int RadiusX, int RadiusY,
                                 uint32_t Color, uint32_t Flags)
{
    //printf("Ellipse (%d,%d) %d x %d, %08X, %d\n", CX, CY, RadiusX, RadiusY, Color, Flags);
    HdffCmdOsdDrawEllipse(mOsdDev, hDisplay, CX, CY, RadiusX, RadiusY, Color, Flags);
}

void cHdffCmdIf::CmdOsdDrawSlope(uint32_t hDisplay, int X, int Y, int Width, int Height,
                                 uint32_t Color, uint32_t Type)
{
    //printf("Slope (%d,%d) %d x %d, %08X, %X\n", X, Y, Width, Height, Color, Type);
    HdffCmdOsdDrawSlope(mOsdDev, hDisplay, X, Y, Width, Height, Color, Type);
}

void cHdffCmdIf::CmdOsdDrawText(uint32_t hDisplay, uint32_t hFont, int X, int Y, const char * pText, uint32_t Color)
{
    //printf("Text %08X (%d,%d), %s, %08X\n", hFont, X, Y, pText, Color);
    HdffCmdOsdDrawText(mOsdDev, hDisplay, hFont, X, Y, pText, Color);
}

void cHdffCmdIf::CmdOsdDrawUtf8Text(uint32_t hDisplay, uint32_t hFont, int X, int Y, const char * pText, uint32_t Color)
{
    //printf("Text(UTF8) %08X (%d,%d), %s, %08X\n", hFont, X, Y, pText, Color);
    HdffCmdOsdDrawUtf8Text(mOsdDev, hDisplay, hFont, X, Y, pText, Color);
}

void cHdffCmdIf::CmdOsdDrawTextW(uint32_t hDisplay, uint32_t hFont, int X, int Y, const uint16_t * pText, uint32_t Color)
{
    //printf("TextW %08X (%d,%d), %08X\n", hFont, X, Y, Color);
    HdffCmdOsdDrawWideText(mOsdDev, hDisplay, hFont, X, Y, pText, Color);
}

void cHdffCmdIf::CmdOsdDrawBitmap(uint32_t hDisplay, int X, int Y, const uint8_t * pBitmap,
                                  int BmpWidth, int BmpHeight, int BmpSize,
                                  HdffColorType_t ColorType, uint32_t hPalette)
{
    //printf("Bitmap %08X (%d,%d) %d x %d, %08X\n", hDisplay, X, Y, BmpWidth, BmpHeight, hPalette);
    HdffCmdOsdDrawBitmap(mOsdDev, hDisplay, X, Y, pBitmap, BmpWidth, BmpHeight,
                         BmpSize, ColorType, hPalette);
}

void cHdffCmdIf::CmdOsdSaveRegion(uint32_t hDisplay, int X, int Y, int Width, int Height)
{
    HdffCmdOsdSaveRegion(mOsdDev, hDisplay, X, Y, Width, Height);
}

void cHdffCmdIf::CmdOsdRestoreRegion(uint32_t hDisplay)
{
    HdffCmdOsdRestoreRegion(mOsdDev, hDisplay);
}

void cHdffCmdIf::CmdMuxSetVideoOut(HdffVideoOut_t VideoOut)
{
    HdffCmdMuxSetVideoOut(mOsdDev, VideoOut);
}

void cHdffCmdIf::CmdMuxSetVolume(uint8_t Volume)
{
    HdffCmdMuxSetVolume(mOsdDev, Volume);
}

void cHdffCmdIf::CmdMuxMuteAudio(bool Mute)
{
    HdffCmdMuxMuteAudio(mOsdDev, Mute);
}

void cHdffCmdIf::CmdHdmiSetVideoMode(HdffVideoMode_t VideoMode)
{
    //printf("HdmiSetVideoMode %d\n", VideoMode);
    HdffCmdHdmiSetVideoMode(mOsdDev, VideoMode);
}

void cHdffCmdIf::CmdHdmiConfigure(const HdffHdmiConfig_t * pConfig)
{
    HdffCmdHdmiConfigure(mOsdDev, pConfig);
}

void cHdffCmdIf::CmdHdmiSendCecCommand(HdffCecCommand_t Command)
{
    HdffCmdHdmiSendCecCommand(mOsdDev, Command);
}

void cHdffCmdIf::CmdRemoteSetProtocol(HdffRemoteProtocol_t Protocol)
{
    //printf("%s %d\n", __func__, Protocol);
    HdffCmdRemoteSetProtocol(mOsdDev, Protocol);
}

void cHdffCmdIf::CmdRemoteSetAddressFilter(bool Enable, uint32_t Address)
{
    //printf("%s %d %d\n", __func__, Enable, Address);
    HdffCmdRemoteSetAddressFilter(mOsdDev, Enable, Address);
}

} // end of namespace
