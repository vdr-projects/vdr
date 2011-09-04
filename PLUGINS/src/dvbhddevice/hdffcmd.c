/*
 * hdffcmd.c: TODO(short description)
 *
 * See the README file for copyright information and how to reach the author.
 *
 * $Id: hdffcmd.c 1.21 2011/08/27 09:34:18 kls Exp $
 */

#include "hdffcmd.h"
#include <linux/dvb/osd.h>
#include <stdio.h>
#include <string.h>
#include <sys/ioctl.h>
#include <vdr/tools.h>

#if !defined OSD_RAW_CMD
typedef struct osd_raw_cmd_s {
    const void *cmd_data;
    int cmd_len;
    void *result_data;
    int result_len;
} osd_raw_cmd_t;

typedef struct osd_raw_data_s {
    const void *data_buffer;
    int data_length;
    int data_handle;
} osd_raw_data_t;

#define OSD_RAW_CMD            _IOWR('o', 162, osd_raw_cmd_t)
#define OSD_RAW_DATA           _IOWR('o', 163, osd_raw_data_t)
#endif


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

void cHdffCmdIf::CmdBuildHeader(cBitBuffer & MsgBuf, eMessageType MsgType, eMessageGroup MsgGroup, eMessageId MsgId)
{
    MsgBuf.SetBits(16, 0); // length field will be set later
    MsgBuf.SetBits(6, 0); // reserved
    MsgBuf.SetBits(2, MsgType);
    MsgBuf.SetBits(8, MsgGroup);
    MsgBuf.SetBits(16, MsgId);
}

uint32_t cHdffCmdIf::CmdSetLength(cBitBuffer & MsgBuf)
{
    uint32_t length;

    length = MsgBuf.GetByteLength() - 2;
    MsgBuf.SetDataByte(0, (uint8_t) (length >> 8));
    MsgBuf.SetDataByte(1, (uint8_t) length);

    return length + 2;
}


uint32_t cHdffCmdIf::CmdGetFirmwareVersion(char * pString, uint32_t MaxLength)
{
    cBitBuffer cmdBuf(MAX_CMD_LEN);
    cBitBuffer resBuf(MAX_CMD_LEN);
    osd_raw_cmd_t osd_cmd;

    memset(&osd_cmd, 0, sizeof(osd_raw_cmd_t));
    osd_cmd.cmd_data = cmdBuf.GetData();
    osd_cmd.result_data = resBuf.GetData();
    osd_cmd.result_len = resBuf.GetMaxLength();
    CmdBuildHeader(cmdBuf, msgTypeCommand, msgGroupGeneric, msgGenGetFirmwareVersion);
    osd_cmd.cmd_len = CmdSetLength(cmdBuf);
    ioctl(mOsdDev, OSD_RAW_CMD, &osd_cmd);
    if (osd_cmd.result_len > 0)
    {
        uint8_t * result = resBuf.GetData();
        uint8_t textLength = result[9];
        if (textLength >= MaxLength)
            textLength = MaxLength - 1;
        memcpy(pString, &result[10], textLength);
        pString[textLength] = 0;
        return (result[6] << 16) | (result[7] << 8) | result[8];
    }
    return 0;
}

uint32_t cHdffCmdIf::CmdGetInterfaceVersion(char * pString, uint32_t MaxLength)
{
    cBitBuffer cmdBuf(MAX_CMD_LEN);
    cBitBuffer resBuf(MAX_CMD_LEN);
    osd_raw_cmd_t osd_cmd;

    memset(&osd_cmd, 0, sizeof(osd_raw_cmd_t));
    osd_cmd.cmd_data = cmdBuf.GetData();
    osd_cmd.result_data = resBuf.GetData();
    osd_cmd.result_len = resBuf.GetMaxLength();
    CmdBuildHeader(cmdBuf, msgTypeCommand, msgGroupGeneric, msgGenGetInterfaceVersion);
    osd_cmd.cmd_len = CmdSetLength(cmdBuf);
    ioctl(mOsdDev, OSD_RAW_CMD, &osd_cmd);
    if (osd_cmd.result_len > 0)
    {
        uint8_t * result = resBuf.GetData();
        uint8_t textLength = result[9];
        if (textLength >= MaxLength)
            textLength = MaxLength - 1;
        memcpy(pString, &result[10], textLength);
        pString[textLength] = 0;
        return (result[6] << 16) | (result[7] << 8) | result[8];
    }
    return 0;
}

uint32_t cHdffCmdIf::CmdGetCopyrights(uint8_t Index, char * pString, uint32_t MaxLength)
{
    cBitBuffer cmdBuf(MAX_CMD_LEN);
    cBitBuffer resBuf(MAX_CMD_LEN);
    osd_raw_cmd_t osd_cmd;

    memset(&osd_cmd, 0, sizeof(osd_raw_cmd_t));
    osd_cmd.cmd_data = cmdBuf.GetData();
    osd_cmd.result_data = resBuf.GetData();
    osd_cmd.result_len = resBuf.GetMaxLength();
    CmdBuildHeader(cmdBuf, msgTypeCommand, msgGroupGeneric, msgGenGetCopyrights);
    cmdBuf.SetBits(8, Index);
    osd_cmd.cmd_len = CmdSetLength(cmdBuf);
    ioctl(mOsdDev, OSD_RAW_CMD, &osd_cmd);
    if (osd_cmd.result_len > 0)
    {
        uint8_t * result = resBuf.GetData();
        uint8_t index = result[6];
        uint8_t textLen = result[7];
        if (index == Index && textLen > 0)
        {
            if (textLen >= MaxLength)
            {
                textLen = MaxLength - 1;
            }
            memcpy(pString, result + 8, textLen);
            pString[textLen] = 0;
            return textLen;
        }
    }
    return 0;
}


void cHdffCmdIf::CmdAvSetPlayMode(uint8_t PlayMode, bool Realtime)
{
    cBitBuffer cmdBuf(MAX_CMD_LEN);
    osd_raw_cmd_t osd_cmd;

    memset(&osd_cmd, 0, sizeof(osd_raw_cmd_t));
    osd_cmd.cmd_data = cmdBuf.GetData();
    CmdBuildHeader(cmdBuf, msgTypeCommand, msgGroupAvDec, msgAvSetPlayMode);
    cmdBuf.SetBits(1, Realtime ? 1 : 0);
    cmdBuf.SetBits(7, PlayMode);
    osd_cmd.cmd_len = CmdSetLength(cmdBuf);
    ioctl(mOsdDev, OSD_RAW_CMD, &osd_cmd);
}

void cHdffCmdIf::CmdAvSetVideoPid(uint8_t DecoderIndex, uint16_t VideoPid, eVideoStreamType StreamType, bool PlaybackMode)
{
    //printf("SetVideoPid %d %d\n", VideoPid, StreamType);
    cBitBuffer cmdBuf(MAX_CMD_LEN);
    osd_raw_cmd_t osd_cmd;

    memset(&osd_cmd, 0, sizeof(osd_raw_cmd_t));
    osd_cmd.cmd_data = cmdBuf.GetData();
    CmdBuildHeader(cmdBuf, msgTypeCommand, msgGroupAvDec, msgAvSetVideoPid);
    cmdBuf.SetBits(4, DecoderIndex);
    cmdBuf.SetBits(4, StreamType);
    cmdBuf.SetBits(1, PlaybackMode ? 1 : 0);
    cmdBuf.SetBits(15, VideoPid);
    osd_cmd.cmd_len = CmdSetLength(cmdBuf);
    ioctl(mOsdDev, OSD_RAW_CMD, &osd_cmd);
}

void cHdffCmdIf::CmdAvSetAudioPid(uint8_t DecoderIndex, uint16_t AudioPid, eAudioStreamType StreamType, eAVContainerType ContainerType)
{
    //printf("SetAudioPid %d %d %d\n", AudioPid, StreamType, ContainerType);
    cBitBuffer cmdBuf(MAX_CMD_LEN);
    osd_raw_cmd_t osd_cmd;

    memset(&osd_cmd, 0, sizeof(osd_raw_cmd_t));
    osd_cmd.cmd_data = cmdBuf.GetData();
    CmdBuildHeader(cmdBuf, msgTypeCommand, msgGroupAvDec, msgAvSetAudioPid);
    cmdBuf.SetBits(4, DecoderIndex);
    cmdBuf.SetBits(4, StreamType);
    cmdBuf.SetBits(2, 0); // reserved
    cmdBuf.SetBits(1, ContainerType);
    cmdBuf.SetBits(13, AudioPid);
    osd_cmd.cmd_len = CmdSetLength(cmdBuf);
    ioctl(mOsdDev, OSD_RAW_CMD, &osd_cmd);
}

void cHdffCmdIf::CmdAvSetPcrPid(uint8_t DecoderIndex, uint16_t PcrPid)
{
    //printf("SetPcrPid %d\n", PcrPid);
    cBitBuffer cmdBuf(MAX_CMD_LEN);
    osd_raw_cmd_t osd_cmd;

    memset(&osd_cmd, 0, sizeof(osd_raw_cmd_t));
    osd_cmd.cmd_data = cmdBuf.GetData();
    CmdBuildHeader(cmdBuf, msgTypeCommand, msgGroupAvDec, msgAvSetPcrPid);
    cmdBuf.SetBits(4, DecoderIndex);
    cmdBuf.SetBits(4, 0); // reserved
    cmdBuf.SetBits(16, PcrPid);
    osd_cmd.cmd_len = CmdSetLength(cmdBuf);
    ioctl(mOsdDev, OSD_RAW_CMD, &osd_cmd);
}

void cHdffCmdIf::CmdAvSetTeletextPid(uint8_t DecoderIndex, uint16_t TeletextPid)
{
    cBitBuffer cmdBuf(MAX_CMD_LEN);
    osd_raw_cmd_t osd_cmd;

    memset(&osd_cmd, 0, sizeof(osd_raw_cmd_t));
    osd_cmd.cmd_data = cmdBuf.GetData();
    CmdBuildHeader(cmdBuf, msgTypeCommand, msgGroupAvDec, msgAvSetTeletextPid);
    cmdBuf.SetBits(4, DecoderIndex);
    cmdBuf.SetBits(4, 0); // reserved
    cmdBuf.SetBits(16, TeletextPid);
    osd_cmd.cmd_len = CmdSetLength(cmdBuf);
    ioctl(mOsdDev, OSD_RAW_CMD, &osd_cmd);
}

void cHdffCmdIf::CmdAvSetVideoWindow(uint8_t DecoderIndex, bool Enable, uint16_t X, uint16_t Y, uint16_t Width, uint16_t Height)
{
    cBitBuffer cmdBuf(MAX_CMD_LEN);
    osd_raw_cmd_t osd_cmd;

    memset(&osd_cmd, 0, sizeof(osd_raw_cmd_t));
    osd_cmd.cmd_data = cmdBuf.GetData();
    CmdBuildHeader(cmdBuf, msgTypeCommand, msgGroupAvDec, msgAvSetVideoWindow);
    cmdBuf.SetBits(4, DecoderIndex);
    cmdBuf.SetBits(3, 0); // reserved
    if (Enable)
        cmdBuf.SetBits(1, 1);
    else
        cmdBuf.SetBits(1, 0);
    cmdBuf.SetBits(16, X);
    cmdBuf.SetBits(16, Y);
    cmdBuf.SetBits(16, Width);
    cmdBuf.SetBits(16, Height);
    osd_cmd.cmd_len = CmdSetLength(cmdBuf);
    ioctl(mOsdDev, OSD_RAW_CMD, &osd_cmd);
}

void cHdffCmdIf::CmdAvShowStillImage(uint8_t DecoderIndex, const uint8_t * pStillImage, int Size, eVideoStreamType StreamType)
{
    cBitBuffer cmdBuf(MAX_CMD_LEN);
    osd_raw_cmd_t osd_cmd;
    osd_raw_data_t osd_data;

    memset(&osd_data, 0, sizeof(osd_raw_data_t));
    osd_data.data_buffer = (void *) pStillImage;
    osd_data.data_length = Size;
    ioctl(mOsdDev, OSD_RAW_DATA, &osd_data);

    memset(&osd_cmd, 0, sizeof(osd_raw_cmd_t));
    osd_cmd.cmd_data = cmdBuf.GetData();
    CmdBuildHeader(cmdBuf, msgTypeCommand, msgGroupAvDec, msgAvShowStillImage);
    cmdBuf.SetBits(4, DecoderIndex);
    cmdBuf.SetBits(4, StreamType);
    cmdBuf.SetBits(16, osd_data.data_handle);
    osd_cmd.cmd_len = CmdSetLength(cmdBuf);
    ioctl(mOsdDev, OSD_RAW_CMD, &osd_cmd);
}

void cHdffCmdIf::CmdAvSetDecoderInput(uint8_t DecoderIndex, uint8_t DemultiplexerIndex)
{
    cBitBuffer cmdBuf(MAX_CMD_LEN);
    osd_raw_cmd_t osd_cmd;

    memset(&osd_cmd, 0, sizeof(osd_raw_cmd_t));
    osd_cmd.cmd_data = cmdBuf.GetData();
    CmdBuildHeader(cmdBuf, msgTypeCommand, msgGroupAvDec, msgAvSetDecoderInput);
    cmdBuf.SetBits(4, DecoderIndex);
    cmdBuf.SetBits(4, DemultiplexerIndex);
    osd_cmd.cmd_len = CmdSetLength(cmdBuf);
    ioctl(mOsdDev, OSD_RAW_CMD, &osd_cmd);
}

void cHdffCmdIf::CmdAvSetDemultiplexerInput(uint8_t DemultiplexerIndex, uint8_t TsInputIndex)
{
    cBitBuffer cmdBuf(MAX_CMD_LEN);
    osd_raw_cmd_t osd_cmd;

    memset(&osd_cmd, 0, sizeof(osd_raw_cmd_t));
    osd_cmd.cmd_data = cmdBuf.GetData();
    CmdBuildHeader(cmdBuf, msgTypeCommand, msgGroupAvDec, msgAvSetDemultiplexerInput);
    cmdBuf.SetBits(4, DemultiplexerIndex);
    cmdBuf.SetBits(4, TsInputIndex);
    osd_cmd.cmd_len = CmdSetLength(cmdBuf);
    ioctl(mOsdDev, OSD_RAW_CMD, &osd_cmd);
}

void cHdffCmdIf::CmdAvSetVideoFormat(uint8_t DecoderIndex, const tVideoFormat * pVideoFormat)
{
    cBitBuffer cmdBuf(MAX_CMD_LEN);
    osd_raw_cmd_t osd_cmd;

    memset(&osd_cmd, 0, sizeof(osd_raw_cmd_t));
    osd_cmd.cmd_data = cmdBuf.GetData();
    CmdBuildHeader(cmdBuf, msgTypeCommand, msgGroupAvDec, msgAvSetVideoFormat);
    cmdBuf.SetBits(4, DecoderIndex);
    if (pVideoFormat->AutomaticEnabled)
    {
        cmdBuf.SetBits(1, 1);
    }
    else
    {
        cmdBuf.SetBits(1, 0);
    }
    if (pVideoFormat->AfdEnabled)
    {
        cmdBuf.SetBits(1, 1);
    }
    else
    {
        cmdBuf.SetBits(1, 0);
    }
    cmdBuf.SetBits(2, pVideoFormat->TvFormat);
    cmdBuf.SetBits(8, pVideoFormat->VideoConversion);
    osd_cmd.cmd_len = CmdSetLength(cmdBuf);
    ioctl(mOsdDev, OSD_RAW_CMD, &osd_cmd);
}

void cHdffCmdIf::CmdAvSetVideoOutputMode(uint8_t DecoderIndex, eVideoOutputMode OutputMode)
{
    cBitBuffer cmdBuf(MAX_CMD_LEN);
    osd_raw_cmd_t osd_cmd;

    memset(&osd_cmd, 0, sizeof(osd_raw_cmd_t));
    osd_cmd.cmd_data = cmdBuf.GetData();
    CmdBuildHeader(cmdBuf, msgTypeCommand, msgGroupAvDec, msgAvSetVideoOutputMode);
    cmdBuf.SetBits(8, OutputMode);
    osd_cmd.cmd_len = CmdSetLength(cmdBuf);
    ioctl(mOsdDev, OSD_RAW_CMD, &osd_cmd);
}

void cHdffCmdIf::CmdAvSetStc(uint8_t DecoderIndex, uint64_t Stc)
{
    cBitBuffer cmdBuf(MAX_CMD_LEN);
    osd_raw_cmd_t osd_cmd;

    memset(&osd_cmd, 0, sizeof(osd_raw_cmd_t));
    osd_cmd.cmd_data = cmdBuf.GetData();
    CmdBuildHeader(cmdBuf, msgTypeCommand, msgGroupAvDec, msgAvSetStc);
    cmdBuf.SetBits(4, DecoderIndex);
    cmdBuf.SetBits(3, 0); // reserved
    cmdBuf.SetBits(1, (uint32_t) (Stc >> 32));
    cmdBuf.SetBits(32, (uint32_t) Stc);
    osd_cmd.cmd_len = CmdSetLength(cmdBuf);
    ioctl(mOsdDev, OSD_RAW_CMD, &osd_cmd);
}

void cHdffCmdIf::CmdAvFlushBuffer(uint8_t DecoderIndex, bool FlushAudio, bool FlushVideo)
{
    cBitBuffer cmdBuf(MAX_CMD_LEN);
    osd_raw_cmd_t osd_cmd;

    memset(&osd_cmd, 0, sizeof(osd_raw_cmd_t));
    osd_cmd.cmd_data = cmdBuf.GetData();
    CmdBuildHeader(cmdBuf, msgTypeCommand, msgGroupAvDec, msgAvFlushBuffer);
    cmdBuf.SetBits(4, DecoderIndex);
    if (FlushAudio)
    {
        cmdBuf.SetBits(1, 1);
    }
    else
    {
        cmdBuf.SetBits(1, 0);
    }
    if (FlushVideo)
    {
        cmdBuf.SetBits(1, 1);
    }
    else
    {
        cmdBuf.SetBits(1, 0);
    }
    osd_cmd.cmd_len = CmdSetLength(cmdBuf);
    ioctl(mOsdDev, OSD_RAW_CMD, &osd_cmd);
}

void cHdffCmdIf::CmdAvEnableSync(uint8_t DecoderIndex, bool EnableSync)
{
    cBitBuffer cmdBuf(MAX_CMD_LEN);
    osd_raw_cmd_t osd_cmd;

    memset(&osd_cmd, 0, sizeof(osd_raw_cmd_t));
    osd_cmd.cmd_data = cmdBuf.GetData();
    CmdBuildHeader(cmdBuf, msgTypeCommand, msgGroupAvDec, msgAvEnableSync);
    cmdBuf.SetBits(4, DecoderIndex);
    if (EnableSync)
    {
        cmdBuf.SetBits(1, 1);
        cmdBuf.SetBits(1, 1);
    }
    else
    {
        cmdBuf.SetBits(1, 0);
        cmdBuf.SetBits(1, 0);
    }
    osd_cmd.cmd_len = CmdSetLength(cmdBuf);
    ioctl(mOsdDev, OSD_RAW_CMD, &osd_cmd);
}

void cHdffCmdIf::CmdAvSetVideoSpeed(uint8_t DecoderIndex, int32_t Speed)
{
    cBitBuffer cmdBuf(MAX_CMD_LEN);
    osd_raw_cmd_t osd_cmd;

    memset(&osd_cmd, 0, sizeof(osd_raw_cmd_t));
    osd_cmd.cmd_data = cmdBuf.GetData();
    CmdBuildHeader(cmdBuf, msgTypeCommand, msgGroupAvDec, msgAvSetVideoSpeed);
    cmdBuf.SetBits(4, DecoderIndex);
    cmdBuf.SetBits(4, 0);
    cmdBuf.SetBits(32, Speed);
    osd_cmd.cmd_len = CmdSetLength(cmdBuf);
    ioctl(mOsdDev, OSD_RAW_CMD, &osd_cmd);
}

void cHdffCmdIf::CmdAvSetAudioSpeed(uint8_t DecoderIndex, int32_t Speed)
{
    cBitBuffer cmdBuf(MAX_CMD_LEN);
    osd_raw_cmd_t osd_cmd;

    memset(&osd_cmd, 0, sizeof(osd_raw_cmd_t));
    osd_cmd.cmd_data = cmdBuf.GetData();
    CmdBuildHeader(cmdBuf, msgTypeCommand, msgGroupAvDec, msgAvSetAudioSpeed);
    cmdBuf.SetBits(4, DecoderIndex);
    cmdBuf.SetBits(4, 0);
    cmdBuf.SetBits(32, Speed);
    osd_cmd.cmd_len = CmdSetLength(cmdBuf);
    ioctl(mOsdDev, OSD_RAW_CMD, &osd_cmd);
}

void cHdffCmdIf::CmdAvEnableVideoAfterStop(uint8_t DecoderIndex, bool EnableVideoAfterStop)
{
    cBitBuffer cmdBuf(MAX_CMD_LEN);
    osd_raw_cmd_t osd_cmd;

    memset(&osd_cmd, 0, sizeof(osd_raw_cmd_t));
    osd_cmd.cmd_data = cmdBuf.GetData();
    CmdBuildHeader(cmdBuf, msgTypeCommand, msgGroupAvDec, msgAvEnableVideoAfterStop);
    cmdBuf.SetBits(4, DecoderIndex);
    if (EnableVideoAfterStop)
    {
        cmdBuf.SetBits(1, 1);
    }
    else
    {
        cmdBuf.SetBits(1, 0);
    }
    osd_cmd.cmd_len = CmdSetLength(cmdBuf);
    ioctl(mOsdDev, OSD_RAW_CMD, &osd_cmd);
}

void cHdffCmdIf::CmdAvSetAudioDelay(int16_t Delay)
{
    cBitBuffer cmdBuf(MAX_CMD_LEN);
    osd_raw_cmd_t osd_cmd;

    memset(&osd_cmd, 0, sizeof(osd_raw_cmd_t));
    osd_cmd.cmd_data = cmdBuf.GetData();
    CmdBuildHeader(cmdBuf, msgTypeCommand, msgGroupAvDec, msgAvSetAudioDelay);
    cmdBuf.SetBits(16, Delay);
    osd_cmd.cmd_len = CmdSetLength(cmdBuf);
    ioctl(mOsdDev, OSD_RAW_CMD, &osd_cmd);
}

void cHdffCmdIf::CmdAvSetAudioDownmix(eDownmixMode DownmixMode)
{
    cBitBuffer cmdBuf(MAX_CMD_LEN);
    osd_raw_cmd_t osd_cmd;

    memset(&osd_cmd, 0, sizeof(osd_raw_cmd_t));
    osd_cmd.cmd_data = cmdBuf.GetData();
    CmdBuildHeader(cmdBuf, msgTypeCommand, msgGroupAvDec, msgAvSetAudioDownmix);
    cmdBuf.SetBits(8, DownmixMode);
    osd_cmd.cmd_len = CmdSetLength(cmdBuf);
    ioctl(mOsdDev, OSD_RAW_CMD, &osd_cmd);
}

void cHdffCmdIf::CmdAvSetAudioChannel(uint8_t AudioChannel)
{
    cBitBuffer cmdBuf(MAX_CMD_LEN);
    osd_raw_cmd_t osd_cmd;

    memset(&osd_cmd, 0, sizeof(osd_raw_cmd_t));
    osd_cmd.cmd_data = cmdBuf.GetData();
    CmdBuildHeader(cmdBuf, msgTypeCommand, msgGroupAvDec, msgAvSetAudioChannel);
    cmdBuf.SetBits(8, AudioChannel);
    osd_cmd.cmd_len = CmdSetLength(cmdBuf);
    ioctl(mOsdDev, OSD_RAW_CMD, &osd_cmd);
}


void cHdffCmdIf::CmdOsdConfigure(const tOsdConfig * pConfig)
{
    cBitBuffer cmdBuf(MAX_CMD_LEN);
    osd_raw_cmd_t osd_cmd;

    memset(&osd_cmd, 0, sizeof(osd_raw_cmd_t));
    osd_cmd.cmd_data = cmdBuf.GetData();
    CmdBuildHeader(cmdBuf, msgTypeCommand, msgGroupOsd, msgOsdConfigure);
    if (pConfig->FontAntialiasing)
    {
        cmdBuf.SetBits(1, 1);
    }
    else
    {
        cmdBuf.SetBits(1, 0);
    }
    if (pConfig->FontKerning)
    {
        cmdBuf.SetBits(1, 1);
    }
    else
    {
        cmdBuf.SetBits(1, 0);
    }
    osd_cmd.cmd_len = CmdSetLength(cmdBuf);
    ioctl(mOsdDev, OSD_RAW_CMD, &osd_cmd);
}

void cHdffCmdIf::CmdOsdReset(void)
{
    cBitBuffer cmdBuf(MAX_CMD_LEN);
    osd_raw_cmd_t osd_cmd;

    memset(&osd_cmd, 0, sizeof(osd_raw_cmd_t));
    osd_cmd.cmd_data = cmdBuf.GetData();
    CmdBuildHeader(cmdBuf, msgTypeCommand, msgGroupOsd, msgOsdReset);
    osd_cmd.cmd_len = CmdSetLength(cmdBuf);
    ioctl(mOsdDev, OSD_RAW_CMD, &osd_cmd);
}

uint32_t cHdffCmdIf::CmdOsdCreateDisplay(uint32_t Width, uint32_t Height, eColorType ColorType)
{
    //printf("CreateDisplay %d %d %d\n", Width, Height, ColorType);
    cBitBuffer cmdBuf(MAX_CMD_LEN);
    cBitBuffer resBuf(MAX_CMD_LEN);
    osd_raw_cmd_t osd_cmd;

    memset(&osd_cmd, 0, sizeof(osd_raw_cmd_t));
    osd_cmd.cmd_data = cmdBuf.GetData();
    osd_cmd.result_data = resBuf.GetData();
    osd_cmd.result_len = resBuf.GetMaxLength();
    CmdBuildHeader(cmdBuf, msgTypeCommand, msgGroupOsd, msgOsdCreateDisplay);
    cmdBuf.SetBits(16, Width);
    cmdBuf.SetBits(16, Height);
    cmdBuf.SetBits(8, ColorType);
    osd_cmd.cmd_len = CmdSetLength(cmdBuf);
    ioctl(mOsdDev, OSD_RAW_CMD, &osd_cmd);
    if (osd_cmd.result_len > 0)
    {
        uint8_t * result = resBuf.GetData();
        return (result[6] << 24) | (result[7] << 16) | (result[8] << 8) | result[9];
    }
    return InvalidHandle;
}

void cHdffCmdIf::CmdOsdDeleteDisplay(uint32_t hDisplay)
{
    //printf("DeleteDisplay\n");
    cBitBuffer cmdBuf(MAX_CMD_LEN);
    osd_raw_cmd_t osd_cmd;

    memset(&osd_cmd, 0, sizeof(osd_raw_cmd_t));
    osd_cmd.cmd_data = cmdBuf.GetData();
    CmdBuildHeader(cmdBuf, msgTypeCommand, msgGroupOsd, msgOsdDeleteDisplay);
    cmdBuf.SetBits(32, hDisplay);
    osd_cmd.cmd_len = CmdSetLength(cmdBuf);
    ioctl(mOsdDev, OSD_RAW_CMD, &osd_cmd);
}

void cHdffCmdIf::CmdOsdEnableDisplay(uint32_t hDisplay, bool Enable)
{
    //printf("EnableDisplay\n");
    cBitBuffer cmdBuf(MAX_CMD_LEN);
    osd_raw_cmd_t osd_cmd;

    memset(&osd_cmd, 0, sizeof(osd_raw_cmd_t));
    osd_cmd.cmd_data = cmdBuf.GetData();
    CmdBuildHeader(cmdBuf, msgTypeCommand, msgGroupOsd, msgOsdEnableDisplay);
    cmdBuf.SetBits(32, hDisplay);
    if (Enable)
    {
        cmdBuf.SetBits(1, 1);
    }
    else
    {
        cmdBuf.SetBits(1, 0);
    }
    cmdBuf.SetBits(7, 0); // reserved
    osd_cmd.cmd_len = CmdSetLength(cmdBuf);
    ioctl(mOsdDev, OSD_RAW_CMD, &osd_cmd);
}

void cHdffCmdIf::CmdOsdSetDisplayOutputRectangle(uint32_t hDisplay, uint32_t X, uint32_t Y, uint32_t Width, uint32_t Height)
{
    //printf("SetOutputRect %d %d %d %d %d\n", hDisplay, X, Y, Width, Height);
    cBitBuffer cmdBuf(MAX_CMD_LEN);
    osd_raw_cmd_t osd_cmd;

    memset(&osd_cmd, 0, sizeof(osd_raw_cmd_t));
    osd_cmd.cmd_data = cmdBuf.GetData();
    CmdBuildHeader(cmdBuf, msgTypeCommand, msgGroupOsd, msgOsdSetDisplayOutputRectangle);
    cmdBuf.SetBits(32, hDisplay);
    cmdBuf.SetBits(16, X);
    cmdBuf.SetBits(16, Y);
    cmdBuf.SetBits(16, Width);
    cmdBuf.SetBits(16, Height);
    osd_cmd.cmd_len = CmdSetLength(cmdBuf);
    ioctl(mOsdDev, OSD_RAW_CMD, &osd_cmd);
}

void cHdffCmdIf::CmdOsdSetDisplayClippingArea(uint32_t hDisplay, bool Enable, uint32_t X, uint32_t Y, uint32_t Width, uint32_t Height)
{
    //printf("SetClippingArea %d %d %d %d %d %d\n", hDisplay, Enable, X, Y, Width, Height);
    cBitBuffer cmdBuf(MAX_CMD_LEN);
    osd_raw_cmd_t osd_cmd;

    memset(&osd_cmd, 0, sizeof(osd_raw_cmd_t));
    osd_cmd.cmd_data = cmdBuf.GetData();
    CmdBuildHeader(cmdBuf, msgTypeCommand, msgGroupOsd, msgOsdSetDisplayClippingArea);
    cmdBuf.SetBits(32, hDisplay);
    if (Enable)
    {
        cmdBuf.SetBits(1, 1);
    }
    else
    {
        cmdBuf.SetBits(1, 0);
    }
    cmdBuf.SetBits(7, 0); // reserved
    cmdBuf.SetBits(16, X);
    cmdBuf.SetBits(16, Y);
    cmdBuf.SetBits(16, Width);
    cmdBuf.SetBits(16, Height);
    osd_cmd.cmd_len = CmdSetLength(cmdBuf);
    ioctl(mOsdDev, OSD_RAW_CMD, &osd_cmd);
}

void cHdffCmdIf::CmdOsdRenderDisplay(uint32_t hDisplay)
{
    //printf("Render\n");
    cBitBuffer cmdBuf(MAX_CMD_LEN);
    osd_raw_cmd_t osd_cmd;

    memset(&osd_cmd, 0, sizeof(osd_raw_cmd_t));
    osd_cmd.cmd_data = cmdBuf.GetData();
    CmdBuildHeader(cmdBuf, msgTypeCommand, msgGroupOsd, msgOsdRenderDisplay);
    cmdBuf.SetBits(32, hDisplay);
    osd_cmd.cmd_len = CmdSetLength(cmdBuf);
    ioctl(mOsdDev, OSD_RAW_CMD, &osd_cmd);
}

uint32_t cHdffCmdIf::CmdOsdCreatePalette(eColorType ColorType, eColorFormat ColorFormat,
                                         uint32_t NumColors, const uint32_t * pColors)
{
    cBitBuffer cmdBuf(MAX_CMD_LEN);
    cBitBuffer resBuf(MAX_CMD_LEN);
    osd_raw_cmd_t osd_cmd;
    uint32_t i;

    memset(&osd_cmd, 0, sizeof(osd_raw_cmd_t));
    osd_cmd.cmd_data = cmdBuf.GetData();
    osd_cmd.result_data = resBuf.GetData();
    osd_cmd.result_len = resBuf.GetMaxLength();
    CmdBuildHeader(cmdBuf, msgTypeCommand, msgGroupOsd, msgOsdCreatePalette);
    cmdBuf.SetBits(8, ColorType);
    cmdBuf.SetBits(8, ColorFormat);
    if (NumColors > 256)
        NumColors = 256;
    cmdBuf.SetBits(8, NumColors == 256 ? 0 : NumColors);
    for (i = 0; i < NumColors; i++)
    {
        cmdBuf.SetBits(32, pColors[i]);
    }
    osd_cmd.cmd_len = CmdSetLength(cmdBuf);
    ioctl(mOsdDev, OSD_RAW_CMD, &osd_cmd);
    if (osd_cmd.result_len > 0)
    {
        uint8_t * result = resBuf.GetData();
        return (result[6] << 24) | (result[7] << 16) | (result[8] << 8) | result[9];
    }
    return InvalidHandle;
}

void cHdffCmdIf::CmdOsdDeletePalette(uint32_t hPalette)
{
    cBitBuffer cmdBuf(MAX_CMD_LEN);
    osd_raw_cmd_t osd_cmd;

    memset(&osd_cmd, 0, sizeof(osd_raw_cmd_t));
    osd_cmd.cmd_data = cmdBuf.GetData();
    CmdBuildHeader(cmdBuf, msgTypeCommand, msgGroupOsd, msgOsdDeletePalette);
    cmdBuf.SetBits(32, hPalette);
    osd_cmd.cmd_len = CmdSetLength(cmdBuf);
    ioctl(mOsdDev, OSD_RAW_CMD, &osd_cmd);
}

void cHdffCmdIf::CmdOsdSetDisplayPalette(uint32_t hDisplay, uint32_t hPalette)
{
    cBitBuffer cmdBuf(MAX_CMD_LEN);
    osd_raw_cmd_t osd_cmd;

    memset(&osd_cmd, 0, sizeof(osd_raw_cmd_t));
    osd_cmd.cmd_data = cmdBuf.GetData();
    CmdBuildHeader(cmdBuf, msgTypeCommand, msgGroupOsd, msgOsdSetDisplayPalette);
    cmdBuf.SetBits(32, hDisplay);
    cmdBuf.SetBits(32, hPalette);
    osd_cmd.cmd_len = CmdSetLength(cmdBuf);
    ioctl(mOsdDev, OSD_RAW_CMD, &osd_cmd);
}

void cHdffCmdIf::CmdOsdSetPaletteColors(uint32_t hPalette, eColorFormat ColorFormat,
                                        uint8_t StartColor, uint32_t NumColors, const uint32_t * pColors)
{
    cBitBuffer cmdBuf(MAX_CMD_LEN);
    osd_raw_cmd_t osd_cmd;
    uint32_t i;

    memset(&osd_cmd, 0, sizeof(osd_raw_cmd_t));
    osd_cmd.cmd_data = cmdBuf.GetData();
    CmdBuildHeader(cmdBuf, msgTypeCommand, msgGroupOsd, msgOsdSetPaletteColors);
    cmdBuf.SetBits(32, hPalette);
    cmdBuf.SetBits(8, ColorFormat);
    cmdBuf.SetBits(8, StartColor);
    if (NumColors > 256)
        NumColors = 256;
    cmdBuf.SetBits(8, NumColors == 256 ? 0 : NumColors);
    for (i = 0; i < NumColors; i++)
    {
        cmdBuf.SetBits(32, pColors[i]);
    }
    osd_cmd.cmd_len = CmdSetLength(cmdBuf);
    ioctl(mOsdDev, OSD_RAW_CMD, &osd_cmd);
}

uint32_t cHdffCmdIf::CmdOsdCreateFontFace(const uint8_t * pFontData, uint32_t DataSize)
{
    //printf("CreateFontFace %d\n", DataSize);
    cBitBuffer cmdBuf(MAX_CMD_LEN);
    cBitBuffer resBuf(MAX_CMD_LEN);
    osd_raw_cmd_t osd_cmd;
    osd_raw_data_t osd_data;

    memset(&osd_data, 0, sizeof(osd_raw_data_t));
    osd_data.data_buffer = pFontData;
    osd_data.data_length = DataSize;
    ioctl(mOsdDev, OSD_RAW_DATA, &osd_data);

    memset(&osd_cmd, 0, sizeof(osd_raw_cmd_t));
    osd_cmd.cmd_data = cmdBuf.GetData();
    osd_cmd.result_data = resBuf.GetData();
    osd_cmd.result_len = resBuf.GetMaxLength();
    CmdBuildHeader(cmdBuf, msgTypeCommand, msgGroupOsd, msgOsdCreateFontFace);
    cmdBuf.SetBits(16, osd_data.data_handle);
    osd_cmd.cmd_len = CmdSetLength(cmdBuf);
    ioctl(mOsdDev, OSD_RAW_CMD, &osd_cmd);
    if (osd_cmd.result_len > 0)
    {
        uint8_t * result = resBuf.GetData();
        return (result[6] << 24) | (result[7] << 16) | (result[8] << 8) | result[9];
    }
    return InvalidHandle;
}

void cHdffCmdIf::CmdOsdDeleteFontFace(uint32_t hFontFace)
{
    //printf("DeleteFontFace %08X\n", hFontFace);
    cBitBuffer cmdBuf(MAX_CMD_LEN);
    osd_raw_cmd_t osd_cmd;

    memset(&osd_cmd, 0, sizeof(osd_raw_cmd_t));
    osd_cmd.cmd_data = cmdBuf.GetData();
    CmdBuildHeader(cmdBuf, msgTypeCommand, msgGroupOsd, msgOsdDeleteFontFace);
    cmdBuf.SetBits(32, hFontFace);
    osd_cmd.cmd_len = CmdSetLength(cmdBuf);
    ioctl(mOsdDev, OSD_RAW_CMD, &osd_cmd);
}

uint32_t cHdffCmdIf::CmdOsdCreateFont(uint32_t hFontFace, uint32_t Size)
{
    //printf("CreateFont %d\n", Size);
    cBitBuffer cmdBuf(MAX_CMD_LEN);
    cBitBuffer resBuf(MAX_CMD_LEN);
    osd_raw_cmd_t osd_cmd;

    memset(&osd_cmd, 0, sizeof(osd_raw_cmd_t));
    osd_cmd.cmd_data = cmdBuf.GetData();
    osd_cmd.result_data = resBuf.GetData();
    osd_cmd.result_len = resBuf.GetMaxLength();
    CmdBuildHeader(cmdBuf, msgTypeCommand, msgGroupOsd, msgOsdCreateFont);
    cmdBuf.SetBits(32, hFontFace);
    cmdBuf.SetBits(32, Size);
    osd_cmd.cmd_len = CmdSetLength(cmdBuf);
    ioctl(mOsdDev, OSD_RAW_CMD, &osd_cmd);
    if (osd_cmd.result_len > 0)
    {
        uint8_t * result = resBuf.GetData();
        return (result[6] << 24) | (result[7] << 16) | (result[8] << 8) | result[9];
    }
    return InvalidHandle;
}

void cHdffCmdIf::CmdOsdDeleteFont(uint32_t hFont)
{
    //printf("DeleteFont %08X\n", hFont);
    cBitBuffer cmdBuf(MAX_CMD_LEN);
    osd_raw_cmd_t osd_cmd;

    memset(&osd_cmd, 0, sizeof(osd_raw_cmd_t));
    osd_cmd.cmd_data = cmdBuf.GetData();
    CmdBuildHeader(cmdBuf, msgTypeCommand, msgGroupOsd, msgOsdDeleteFont);
    cmdBuf.SetBits(32, hFont);
    osd_cmd.cmd_len = CmdSetLength(cmdBuf);
    ioctl(mOsdDev, OSD_RAW_CMD, &osd_cmd);
}

void cHdffCmdIf::CmdOsdDrawRectangle(uint32_t hDisplay, int X, int Y, int Width, int Height, uint32_t Color)
{
    //printf("Rect (%d,%d) %d x %d, %08X\n", X, Y, Width, Height, Color);
    cBitBuffer cmdBuf(MAX_CMD_LEN);
    osd_raw_cmd_t osd_cmd;

    memset(&osd_cmd, 0, sizeof(osd_raw_cmd_t));
    osd_cmd.cmd_data = cmdBuf.GetData();
    CmdBuildHeader(cmdBuf, msgTypeCommand, msgGroupOsd, msgOsdDrawRectangle);
    cmdBuf.SetBits(32, hDisplay);
    cmdBuf.SetBits(16, X);
    cmdBuf.SetBits(16, Y);
    cmdBuf.SetBits(16, Width);
    cmdBuf.SetBits(16, Height);
    cmdBuf.SetBits(32, Color);
    osd_cmd.cmd_len = CmdSetLength(cmdBuf);
    ioctl(mOsdDev, OSD_RAW_CMD, &osd_cmd);
}

void cHdffCmdIf::CmdOsdDrawEllipse(uint32_t hDisplay, int CX, int CY, int RadiusX, int RadiusY,
                                 uint32_t Color, uint32_t Flags)
{
    //printf("Ellipse (%d,%d) %d x %d, %08X, %d\n", CX, CY, RadiusX, RadiusY, Color, Flags);
    cBitBuffer cmdBuf(MAX_CMD_LEN);
    osd_raw_cmd_t osd_cmd;

    memset(&osd_cmd, 0, sizeof(osd_raw_cmd_t));
    osd_cmd.cmd_data = cmdBuf.GetData();
    CmdBuildHeader(cmdBuf, msgTypeCommand, msgGroupOsd, msgOsdDrawEllipse);
    cmdBuf.SetBits(32, hDisplay);
    cmdBuf.SetBits(16, CX);
    cmdBuf.SetBits(16, CY);
    cmdBuf.SetBits(16, RadiusX);
    cmdBuf.SetBits(16, RadiusY);
    cmdBuf.SetBits(32, Color);
    cmdBuf.SetBits(32, Flags);
    osd_cmd.cmd_len = CmdSetLength(cmdBuf);
    ioctl(mOsdDev, OSD_RAW_CMD, &osd_cmd);
}

void cHdffCmdIf::CmdOsdDrawText(uint32_t hDisplay, uint32_t hFont, int X, int Y, const char * pText, uint32_t Color)
{
    //printf("Text %08X (%d,%d), %s, %08X\n", hFont, X, Y, pText, Color);
    cBitBuffer cmdBuf(MAX_CMD_LEN);
    osd_raw_cmd_t osd_cmd;
    int i;
    int length;

    length = 0;
    while (pText[length])
    {
        length++;
    }
    if (length > 980)
        length = 980;

    memset(&osd_cmd, 0, sizeof(osd_raw_cmd_t));
    osd_cmd.cmd_data = cmdBuf.GetData();
    CmdBuildHeader(cmdBuf, msgTypeCommand, msgGroupOsd, msgOsdDrawText);
    cmdBuf.SetBits(32, hDisplay);
    cmdBuf.SetBits(32, hFont);
    cmdBuf.SetBits(16, X);
    cmdBuf.SetBits(16, Y);
    cmdBuf.SetBits(32, Color);
    cmdBuf.SetBits(16, length);
    for (i = 0; i < length; i++)
    {
        cmdBuf.SetBits(8, pText[i]);
    }
    osd_cmd.cmd_len = CmdSetLength(cmdBuf);
    ioctl(mOsdDev, OSD_RAW_CMD, &osd_cmd);
}

void cHdffCmdIf::CmdOsdDrawTextW(uint32_t hDisplay, uint32_t hFont, int X, int Y, const uint16_t * pText, uint32_t Color)
{
    //printf("TextW %08X (%d,%d), %08X\n", hFont, X, Y, Color);
    cBitBuffer cmdBuf(MAX_CMD_LEN);
    osd_raw_cmd_t osd_cmd;
    int i;
    int length;

    length = 0;
    while (pText[length])
    {
        length++;
    }
    if (length > 480)
        length = 480;

    memset(&osd_cmd, 0, sizeof(osd_raw_cmd_t));
    osd_cmd.cmd_data = cmdBuf.GetData();
    CmdBuildHeader(cmdBuf, msgTypeCommand, msgGroupOsd, msgOsdDrawTextW);
    cmdBuf.SetBits(32, hDisplay);
    cmdBuf.SetBits(32, hFont);
    cmdBuf.SetBits(16, X);
    cmdBuf.SetBits(16, Y);
    cmdBuf.SetBits(32, Color);
    cmdBuf.SetBits(16, length);
    for (i = 0; i < length; i++)
    {
        cmdBuf.SetBits(16, pText[i]);
    }
    osd_cmd.cmd_len = CmdSetLength(cmdBuf);
    ioctl(mOsdDev, OSD_RAW_CMD, &osd_cmd);
}

void cHdffCmdIf::CmdOsdDrawBitmap(uint32_t hDisplay, int X, int Y, const uint8_t * pBitmap,
                                  int BmpWidth, int BmpHeight, int BmpSize,
                                  eColorType ColorType, uint32_t hPalette)
{
    //printf("Bitmap (%d,%d) %d x %d\n", X, Y, BmpWidth, BmpHeight);
    cBitBuffer cmdBuf(MAX_CMD_LEN);
    osd_raw_cmd_t osd_cmd;
    osd_raw_data_t osd_data;

    memset(&osd_data, 0, sizeof(osd_raw_data_t));
    osd_data.data_buffer = pBitmap;
    osd_data.data_length = BmpSize;
    ioctl(mOsdDev, OSD_RAW_DATA, &osd_data);

    memset(&osd_cmd, 0, sizeof(osd_raw_cmd_t));
    osd_cmd.cmd_data = cmdBuf.GetData();
    CmdBuildHeader(cmdBuf, msgTypeCommand, msgGroupOsd, msgOsdDrawBitmap);
    cmdBuf.SetBits(32, hDisplay);
    cmdBuf.SetBits(16, X);
    cmdBuf.SetBits(16, Y);
    cmdBuf.SetBits(16, BmpWidth);
    cmdBuf.SetBits(16, BmpHeight);
    cmdBuf.SetBits(8, ColorType);
    cmdBuf.SetBits(6, 0); // reserved
    cmdBuf.SetBits(2, 0); // uncompressed
    cmdBuf.SetBits(32, hPalette);
    cmdBuf.SetBits(16, osd_data.data_handle);
    cmdBuf.SetBits(32, 0);
    osd_cmd.cmd_len = CmdSetLength(cmdBuf);
    ioctl(mOsdDev, OSD_RAW_CMD, &osd_cmd);
}

void cHdffCmdIf::CmdOsdSaveRegion(uint32_t hDisplay, int X, int Y, int Width, int Height)
{
    cBitBuffer cmdBuf(MAX_CMD_LEN);
    osd_raw_cmd_t osd_cmd;

    memset(&osd_cmd, 0, sizeof(osd_raw_cmd_t));
    osd_cmd.cmd_data = cmdBuf.GetData();
    CmdBuildHeader(cmdBuf, msgTypeCommand, msgGroupOsd, msgOsdSaveRegion);
    cmdBuf.SetBits(32, hDisplay);
    cmdBuf.SetBits(16, X);
    cmdBuf.SetBits(16, Y);
    cmdBuf.SetBits(16, Width);
    cmdBuf.SetBits(16, Height);
    osd_cmd.cmd_len = CmdSetLength(cmdBuf);
    ioctl(mOsdDev, OSD_RAW_CMD, &osd_cmd);
}

void cHdffCmdIf::CmdOsdRestoreRegion(uint32_t hDisplay)
{
    cBitBuffer cmdBuf(MAX_CMD_LEN);
    osd_raw_cmd_t osd_cmd;

    memset(&osd_cmd, 0, sizeof(osd_raw_cmd_t));
    osd_cmd.cmd_data = cmdBuf.GetData();
    CmdBuildHeader(cmdBuf, msgTypeCommand, msgGroupOsd, msgOsdRestoreRegion);
    cmdBuf.SetBits(32, hDisplay);
    osd_cmd.cmd_len = CmdSetLength(cmdBuf);
    ioctl(mOsdDev, OSD_RAW_CMD, &osd_cmd);
}

void cHdffCmdIf::CmdMuxSetVideoOut(eVideoOut VideoOut)
{
    cBitBuffer cmdBuf(MAX_CMD_LEN);
    osd_raw_cmd_t osd_cmd;

    memset(&osd_cmd, 0, sizeof(osd_raw_cmd_t));
    osd_cmd.cmd_data = cmdBuf.GetData();
    CmdBuildHeader(cmdBuf, msgTypeCommand, msgGroupAvMux, msgMuxSetVideoOut);
    cmdBuf.SetBits(4, VideoOut);
    cmdBuf.SetBits(4, 0); // reserved
    osd_cmd.cmd_len = CmdSetLength(cmdBuf);
    ioctl(mOsdDev, OSD_RAW_CMD, &osd_cmd);
}

void cHdffCmdIf::CmdMuxSetVolume(uint8_t Volume)
{
    cBitBuffer cmdBuf(MAX_CMD_LEN);
    osd_raw_cmd_t osd_cmd;

    memset(&osd_cmd, 0, sizeof(osd_raw_cmd_t));
    osd_cmd.cmd_data = cmdBuf.GetData();
    CmdBuildHeader(cmdBuf, msgTypeCommand, msgGroupAvMux, msgMuxSetVolume);
    cmdBuf.SetBits(8, Volume);
    osd_cmd.cmd_len = CmdSetLength(cmdBuf);
    ioctl(mOsdDev, OSD_RAW_CMD, &osd_cmd);
}

void cHdffCmdIf::CmdMuxMuteAudio(bool Mute)
{
    cBitBuffer cmdBuf(MAX_CMD_LEN);
    osd_raw_cmd_t osd_cmd;

    memset(&osd_cmd, 0, sizeof(osd_raw_cmd_t));
    osd_cmd.cmd_data = cmdBuf.GetData();
    CmdBuildHeader(cmdBuf, msgTypeCommand, msgGroupAvMux, msgMuxSetAudioMute);
    cmdBuf.SetBits(1, Mute);
    cmdBuf.SetBits(7, 0); // reserved
    osd_cmd.cmd_len = CmdSetLength(cmdBuf);
    ioctl(mOsdDev, OSD_RAW_CMD, &osd_cmd);
}

void cHdffCmdIf::CmdHdmiSetVideoMode(eHdmiVideoMode VideoMode)
{
    //printf("HdmiSetVideoMode %d\n", VideoMode);
    cBitBuffer cmdBuf(MAX_CMD_LEN);
    osd_raw_cmd_t osd_cmd;

    memset(&osd_cmd, 0, sizeof(osd_raw_cmd_t));
    osd_cmd.cmd_data = cmdBuf.GetData();
    CmdBuildHeader(cmdBuf, msgTypeCommand, msgGroupHdmi, msgHdmiSetVideoMode);
    cmdBuf.SetBits(8, VideoMode);
    osd_cmd.cmd_len = CmdSetLength(cmdBuf);
    ioctl(mOsdDev, OSD_RAW_CMD, &osd_cmd);
}

void cHdffCmdIf::CmdHdmiConfigure(const tHdmiConfig * pConfig)
{
    cBitBuffer cmdBuf(MAX_CMD_LEN);
    osd_raw_cmd_t osd_cmd;

    memset(&osd_cmd, 0, sizeof(osd_raw_cmd_t));
    osd_cmd.cmd_data = cmdBuf.GetData();
    CmdBuildHeader(cmdBuf, msgTypeCommand, msgGroupHdmi, msgHdmiConfigure);
    if (pConfig->TransmitAudio)
    {
        cmdBuf.SetBits(1, 1);
    }
    else
    {
        cmdBuf.SetBits(1, 0);
    }
    if (pConfig->ForceDviMode)
    {
        cmdBuf.SetBits(1, 1);
    }
    else
    {
        cmdBuf.SetBits(1, 0);
    }
    if (pConfig->CecEnabled)
    {
        cmdBuf.SetBits(1, 1);
    }
    else
    {
        cmdBuf.SetBits(1, 0);
    }
    cmdBuf.SetBits(3, (uint32_t) pConfig->VideoModeAdaption);
    osd_cmd.cmd_len = CmdSetLength(cmdBuf);
    ioctl(mOsdDev, OSD_RAW_CMD, &osd_cmd);
}

void cHdffCmdIf::CmdHdmiSendCecCommand(eCecCommand Command)
{
    cBitBuffer cmdBuf(MAX_CMD_LEN);
    osd_raw_cmd_t osd_cmd;

    memset(&osd_cmd, 0, sizeof(osd_raw_cmd_t));
    osd_cmd.cmd_data = cmdBuf.GetData();
    CmdBuildHeader(cmdBuf, msgTypeCommand, msgGroupHdmi, msgHdmiSendCecCommand);
    cmdBuf.SetBits(8, Command);
    osd_cmd.cmd_len = CmdSetLength(cmdBuf);
    ioctl(mOsdDev, OSD_RAW_CMD, &osd_cmd);
}

void cHdffCmdIf::CmdRemoteSetProtocol(eRemoteProtocol Protocol)
{
    //printf("%s %d\n", __func__, Protocol);
    cBitBuffer cmdBuf(MAX_CMD_LEN);
    osd_raw_cmd_t osd_cmd;

    memset(&osd_cmd, 0, sizeof(osd_raw_cmd_t));
    osd_cmd.cmd_data = cmdBuf.GetData();
    CmdBuildHeader(cmdBuf, msgTypeCommand, msgGroupRemoteControl, msgRemoteSetProtocol);
    cmdBuf.SetBits(8, Protocol);
    osd_cmd.cmd_len = CmdSetLength(cmdBuf);
    ioctl(mOsdDev, OSD_RAW_CMD, &osd_cmd);
}

void cHdffCmdIf::CmdRemoteSetAddressFilter(bool Enable, uint32_t Address)
{
    //printf("%s %d %d\n", __func__, Enable, Address);
    cBitBuffer cmdBuf(MAX_CMD_LEN);
    osd_raw_cmd_t osd_cmd;

    memset(&osd_cmd, 0, sizeof(osd_raw_cmd_t));
    osd_cmd.cmd_data = cmdBuf.GetData();
    CmdBuildHeader(cmdBuf, msgTypeCommand, msgGroupRemoteControl, msgRemoteSetAddressFilter);
    cmdBuf.SetBits(1, Enable);
    cmdBuf.SetBits(7, 0); // reserved
    cmdBuf.SetBits(32, Address);
    osd_cmd.cmd_len = CmdSetLength(cmdBuf);
    ioctl(mOsdDev, OSD_RAW_CMD, &osd_cmd);
}

} // end of namespace
