/*
 * hdffcmd.h: TODO(short description)
 *
 * See the README file for copyright information and how to reach the author.
 */

#ifndef _HDFF_CMD_H_
#define _HDFF_CMD_H_

#include "libhdffcmd/hdffcmd.h"

namespace HDFF
{

class cHdffCmdIf
{
private:
    int mOsdDev;

public:
    cHdffCmdIf(int OsdDev);
    ~cHdffCmdIf(void);

    uint32_t CmdGetFirmwareVersion(char * pString, uint32_t MaxLength);
    uint32_t CmdGetInterfaceVersion(char * pString, uint32_t MaxLength);
    uint32_t CmdGetCopyrights(uint8_t Index, char * pString, uint32_t MaxLength);

    void CmdAvSetPlayMode(uint8_t PlayMode, bool Realtime);
    void CmdAvSetVideoPid(uint8_t DecoderIndex, uint16_t VideoPid, HdffVideoStreamType_t StreamType, bool PlaybackMode = false);
    void CmdAvSetAudioPid(uint8_t DecoderIndex, uint16_t AudioPid, HdffAudioStreamType_t StreamType, HdffAvContainerType_t ContainerType = HDFF_AV_CONTAINER_PES);
    void CmdAvSetPcrPid(uint8_t DecoderIndex, uint16_t PcrPid);
    void CmdAvSetTeletextPid(uint8_t DecoderIndex, uint16_t TeletextPid);
    void CmdAvSetVideoWindow(uint8_t DecoderIndex, bool Enable, uint16_t X, uint16_t Y, uint16_t Width, uint16_t Height);
    void CmdAvShowStillImage(uint8_t DecoderIndex, const uint8_t * pStillImage, int Size, HdffVideoStreamType_t StreamType);
    void CmdAvSetDecoderInput(uint8_t DecoderIndex, uint8_t DemultiplexerIndex);
    void CmdAvSetDemultiplexerInput(uint8_t DemultiplexerIndex, uint8_t TsInputIndex);
    void CmdAvSetVideoFormat(uint8_t DecoderIndex, const HdffVideoFormat_t * pVideoFormat);
    void CmdAvSetVideoOutputMode(uint8_t DecoderIndex, HdffVideoOutputMode_t OutputMode);
    void CmdAvSetStc(uint8_t DecoderIndex, uint64_t Stc);
    void CmdAvFlushBuffer(uint8_t DecoderIndex, bool FlushAudio, bool FlushVideo);
    void CmdAvEnableSync(uint8_t DecoderIndex, bool EnableSync);
    void CmdAvSetVideoSpeed(uint8_t DecoderIndex, int32_t Speed);
    void CmdAvSetAudioSpeed(uint8_t DecoderIndex, int32_t Speed);
    void CmdAvEnableVideoAfterStop(uint8_t DecoderIndex, bool EnableVideoAfterStop);
    void CmdAvSetAudioDelay(int16_t Delay);
    void CmdAvSetAudioDownmix(HdffAudioDownmixMode_t DownmixMode);
    void CmdAvSetAudioChannel(uint8_t AudioChannel);
    void CmdAvSetSyncShift(int16_t SyncShift);
    void CmdAvMuteAudio(uint8_t DecoderIndex, bool Mute);

    void CmdOsdConfigure(const HdffOsdConfig_t * pConfig);
    void CmdOsdReset(void);

    uint32_t CmdOsdCreateDisplay(uint32_t Width, uint32_t Height, HdffColorType_t ColorType);
    void CmdOsdDeleteDisplay(uint32_t hDisplay);
    void CmdOsdEnableDisplay(uint32_t hDisplay, bool Enable);
    void CmdOsdSetDisplayOutputRectangle(uint32_t hDisplay, uint32_t X, uint32_t Y, uint32_t Width, uint32_t Height);
    void CmdOsdSetDisplayClippingArea(uint32_t hDisplay, bool Enable, uint32_t X, uint32_t Y, uint32_t Width, uint32_t Height);
    void CmdOsdRenderDisplay(uint32_t hDisplay);

    uint32_t CmdOsdCreatePalette(HdffColorType_t ColorType, HdffColorFormat_t ColorFormat,
                                 uint32_t NumColors, const uint32_t * pColors);
    void CmdOsdDeletePalette(uint32_t hPalette);
    void CmdOsdSetDisplayPalette(uint32_t hDisplay, uint32_t hPalette);
    void CmdOsdSetPaletteColors(uint32_t hPalette, HdffColorFormat_t ColorFormat,
                                uint8_t StartColor, uint32_t NumColors, const uint32_t * pColors);

    uint32_t CmdOsdCreateFontFace(const uint8_t * pFontData, uint32_t DataSize);
    void CmdOsdDeleteFontFace(uint32_t hFontFace);
    uint32_t CmdOsdCreateFont(uint32_t hFontFace, uint32_t Size);
    void CmdOsdDeleteFont(uint32_t hFont);

    void CmdOsdDrawRectangle(uint32_t hDisplay, int X, int Y, int Width, int Height, uint32_t Color);
    void CmdOsdDrawEllipse(uint32_t hDisplay, int CX, int CY, int RadiusX, int RadiusY,
                           uint32_t Color, uint32_t Flags);
    void CmdOsdDrawSlope(uint32_t hDisplay, int X, int Y, int Width, int Height, uint32_t Color, uint32_t Type);
    void CmdOsdDrawText(uint32_t hDisplay, uint32_t hFont, int X, int Y, const char * pText, uint32_t Color);
    void CmdOsdDrawUtf8Text(uint32_t hDisplay, uint32_t hFont, int X, int Y, const char * pText, uint32_t Color);
    void CmdOsdDrawTextW(uint32_t hDisplay, uint32_t hFont, int X, int Y, const uint16_t * pText, uint32_t Color);
    void CmdOsdDrawBitmap(uint32_t hDisplay, int X, int Y, const uint8_t * pBitmap,
                          int BmpWidth, int BmpHeight, int BmpSize,
                          HdffColorType_t ColorType, uint32_t hPalette);
    void CmdOsdSaveRegion(uint32_t hDisplay, int X, int Y, int Width, int Height);
    void CmdOsdRestoreRegion(uint32_t hDisplay);

    void CmdMuxSetVideoOut(HdffVideoOut_t VideoOut);
    void CmdMuxSetVolume(uint8_t Volume);
    void CmdMuxMuteAudio(bool Mute);

    void CmdHdmiSetVideoMode(HdffVideoMode_t VideoMode);
    void CmdHdmiConfigure(const HdffHdmiConfig_t * pConfig);
    void CmdHdmiSendCecCommand(HdffCecCommand_t Command);

    void CmdRemoteSetProtocol(HdffRemoteProtocol_t Protocol);
    void CmdRemoteSetAddressFilter(bool Enable, uint32_t Address);
};

} // end of namespace

#endif
