/*
 * hdffcmd.h: TODO(short description)
 *
 * See the README file for copyright information and how to reach the author.
 *
 * $Id: hdffcmd.h 1.17 2011/04/17 11:20:22 kls Exp $
 */

#ifndef _HDFF_CMD_H_
#define _HDFF_CMD_H_

#include "bitbuffer.h"
#include "hdffmsgdef.h"

namespace HDFF
{

class cHdffCmdIf
{
private:
    int mOsdDev;

    void CmdBuildHeader(cBitBuffer & MsgBuf, eMessageType MsgType, eMessageGroup MsgGroup, eMessageId MsgId);
    uint32_t CmdSetLength(cBitBuffer & MsgBuf);

public:
    cHdffCmdIf(int OsdDev);
    ~cHdffCmdIf(void);

    uint32_t CmdGetFirmwareVersion(char * pString, uint32_t MaxLength);
    uint32_t CmdGetInterfaceVersion(char * pString, uint32_t MaxLength);
    uint32_t CmdGetCopyrights(uint8_t Index, char * pString, uint32_t MaxLength);

    void CmdAvSetPlayMode(uint8_t PlayMode, bool Realtime);
    void CmdAvSetVideoPid(uint8_t DecoderIndex, uint16_t VideoPid, eVideoStreamType StreamType, bool PlaybackMode = false);
    void CmdAvSetAudioPid(uint8_t DecoderIndex, uint16_t AudioPid, eAudioStreamType StreamType, eAVContainerType ContainerType = avContainerPes);
    void CmdAvSetPcrPid(uint8_t DecoderIndex, uint16_t PcrPid);
    void CmdAvSetTeletextPid(uint8_t DecoderIndex, uint16_t TeletextPid);
    void CmdAvSetVideoWindow(uint8_t DecoderIndex, bool Enable, uint16_t X, uint16_t Y, uint16_t Width, uint16_t Height);
    void CmdAvShowStillImage(uint8_t DecoderIndex, const uint8_t * pStillImage, int Size, eVideoStreamType StreamType);
    void CmdAvSetDecoderInput(uint8_t DecoderIndex, uint8_t DemultiplexerIndex);
    void CmdAvSetDemultiplexerInput(uint8_t DemultiplexerIndex, uint8_t TsInputIndex);
    void CmdAvSetVideoFormat(uint8_t DecoderIndex, const tVideoFormat * pVideoFormat);
    void CmdAvSetVideoOutputMode(uint8_t DecoderIndex, eVideoOutputMode OutputMode);
    void CmdAvSetStc(uint8_t DecoderIndex, uint64_t Stc);
    void CmdAvFlushBuffer(uint8_t DecoderIndex, bool FlushAudio, bool FlushVideo);
    void CmdAvEnableSync(uint8_t DecoderIndex, bool EnableSync);
    void CmdAvSetVideoSpeed(uint8_t DecoderIndex, int32_t Speed);
    void CmdAvSetAudioSpeed(uint8_t DecoderIndex, int32_t Speed);
    void CmdAvEnableVideoAfterStop(uint8_t DecoderIndex, bool EnableVideoAfterStop);
    void CmdAvSetAudioDelay(int16_t Delay);
    void CmdAvSetAudioDownmix(eDownmixMode DownmixMode);
    void CmdAvSetAudioChannel(uint8_t AudioChannel);

    void CmdOsdConfigure(const tOsdConfig * pConfig);
    void CmdOsdReset(void);

    uint32_t CmdOsdCreateDisplay(uint32_t Width, uint32_t Height, eColorType ColorType);
    void CmdOsdDeleteDisplay(uint32_t hDisplay);
    void CmdOsdEnableDisplay(uint32_t hDisplay, bool Enable);
    void CmdOsdSetDisplayOutputRectangle(uint32_t hDisplay, uint32_t X, uint32_t Y, uint32_t Width, uint32_t Height);
    void CmdOsdSetDisplayClippingArea(uint32_t hDisplay, bool Enable, uint32_t X, uint32_t Y, uint32_t Width, uint32_t Height);
    void CmdOsdRenderDisplay(uint32_t hDisplay);

    uint32_t CmdOsdCreatePalette(eColorType ColorType, eColorFormat ColorFormat,
                                 uint32_t NumColors, const uint32_t * pColors);
    void CmdOsdDeletePalette(uint32_t hPalette);
    void CmdOsdSetDisplayPalette(uint32_t hDisplay, uint32_t hPalette);
    void CmdOsdSetPaletteColors(uint32_t hPalette, eColorFormat ColorFormat,
                                uint8_t StartColor, uint32_t NumColors, const uint32_t * pColors);

    uint32_t CmdOsdCreateFontFace(const uint8_t * pFontData, uint32_t DataSize);
    void CmdOsdDeleteFontFace(uint32_t hFontFace);
    uint32_t CmdOsdCreateFont(uint32_t hFontFace, uint32_t Size);
    void CmdOsdDeleteFont(uint32_t hFont);

    void CmdOsdDrawRectangle(uint32_t hDisplay, int X, int Y, int Width, int Height, uint32_t Color);
    void CmdOsdDrawEllipse(uint32_t hDisplay, int CX, int CY, int RadiusX, int RadiusY,
                           uint32_t Color, uint32_t Flags);
    void CmdOsdDrawText(uint32_t hDisplay, uint32_t hFont, int X, int Y, const char * pText, uint32_t Color);
    void CmdOsdDrawTextW(uint32_t hDisplay, uint32_t hFont, int X, int Y, const uint16_t * pText, uint32_t Color);
    void CmdOsdDrawBitmap(uint32_t hDisplay, int X, int Y, const uint8_t * pBitmap,
                          int BmpWidth, int BmpHeight, int BmpSize,
                          eColorType ColorType, uint32_t hPalette);
    void CmdOsdSaveRegion(uint32_t hDisplay, int X, int Y, int Width, int Height);
    void CmdOsdRestoreRegion(uint32_t hDisplay);

    void CmdMuxSetVideoOut(eVideoOut VideoOut);
    void CmdMuxSetVolume(uint8_t Volume);
    void CmdMuxMuteAudio(bool Mute);

    void CmdHdmiSetVideoMode(eHdmiVideoMode VideoMode);
    void CmdHdmiConfigure(const tHdmiConfig * pConfig);
    void CmdHdmiSendCecCommand(eCecCommand Command);

    void CmdRemoteSetProtocol(eRemoteProtocol Protocol);
    void CmdRemoteSetAddressFilter(bool Enable, uint32_t Address);
};

} // end of namespace

#endif
