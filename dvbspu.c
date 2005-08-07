/*
 * SPU decoder for DVB devices
 *
 * Copyright (C) 2001.2002 Andreas Schultz <aschultz@warp10.net>
 *
 * This code is distributed under the terms and conditions of the
 * GNU GENERAL PUBLIC LICENSE. See the file COPYING for details.
 *
 * parts of this file are derived from the OMS program.
 *
 * $Id: dvbspu.c 1.15 2005/08/07 12:06:32 kls Exp $
 */

#include <assert.h>
#include <string.h>
#include <inttypes.h>
#include <math.h>

#include "device.h"
#include "dvbspu.h"

/*
 * cDvbSpubitmap:
 *
 * this is a bitmap of the full screen and two palettes
 * the normal palette for the background and the highlight palette
 *
 * Inputs:
 *  - a SPU rle encoded image on creation, which will be decoded into
 *    the full screen indexed bitmap
 *  
 * Output:
 *  - a minimal sized cDvbSpuBitmap a given palette, the indexed bitmap
 *    will be scanned to get the smallest possible resulting bitmap considering
 *    transparencies
 */

// #define SPUDEBUG

#ifdef SPUDEBUG
#define DEBUG(format, args...) printf (format, ## args)
#else
#define DEBUG(format, args...)
#endif

// --- cDvbSpuPalette----------------------------------

void cDvbSpuPalette::setPalette(const uint32_t * pal)
{
    for (int i = 0; i < 16; i++)
        palette[i] = yuv2rgb(pal[i]);
}

// --- cDvbSpuBitmap --------------------------------------------

#define setMin(a, b) if (a > b) a = b
#define setMax(a, b) if (a < b) a = b

#define spuXres   720
#define spuYres   576

#define revRect(r1, r2) { r1.x1 = r2.x2; r1.y1 = r2.y2; r1.x2 = r2.x1; r1.y2 = r2.y1; }

cDvbSpuBitmap::cDvbSpuBitmap(sDvbSpuRect size,
                             uint8_t * fodd, uint8_t * eodd,
                             uint8_t * feven, uint8_t * eeven)
{
    if (size.x1 < 0 || size.y1 < 0 || size.x2 >= spuXres
        || size.y2 >= spuYres)
        throw;

    bmpsize = size;
    revRect(minsize[0], size);
    revRect(minsize[1], size);
    revRect(minsize[2], size);
    revRect(minsize[3], size);

    if (!(bmp = new uint8_t[spuXres * spuYres * sizeof(uint8_t)]))
        throw;

    memset(bmp, 0, spuXres * spuYres * sizeof(uint8_t));
    putFieldData(0, fodd, eodd);
    putFieldData(1, feven, eeven);
}

cDvbSpuBitmap::~cDvbSpuBitmap()
{
    delete[]bmp;
}

cBitmap *cDvbSpuBitmap::getBitmap(const aDvbSpuPalDescr paldescr,
                                  const cDvbSpuPalette & pal,
                                  sDvbSpuRect & size) const
{
    int h = size.height();
    int w = size.width();

    if (size.y1 + h >= spuYres)
        h = spuYres - size.y1 - 1;
    if (size.x1 + w >= spuXres)
        w = spuXres - size.x1 - 1;

    if (w & 0x03)
        w += 4 - (w & 0x03);

    cBitmap *ret = new cBitmap(w, h, 2);

    // set the palette
    for (int i = 0; i < 4; i++) {
        uint32_t color =
            pal.getColor(paldescr[i].index, paldescr[i].trans);
        ret->SetColor(i, (tColor) color);
    }

    // set the content
    for (int yp = 0; yp < h; yp++) {
        for (int xp = 0; xp < w; xp++) {
            uint8_t idx = bmp[(size.y1 + yp) * spuXres + size.x1 + xp];
            ret->SetIndex(xp, yp, idx);
        }
    }
    return ret;
}

// find the minimum non-transparent area
bool cDvbSpuBitmap::getMinSize(const aDvbSpuPalDescr paldescr,
                               sDvbSpuRect & size) const
{
    bool ret = false;
    for (int i = 0; i < 4; i++) {
        if (paldescr[i].trans != 0) {
            if (!ret)
                size = minsize[i];
            else {
                setMin(size.x1, minsize[i].x1);
                setMin(size.y1, minsize[i].y1);
                setMax(size.x2, minsize[i].x2);
                setMax(size.y2, minsize[i].y2);
            }
            ret = true;
        }
    }
    if (ret)
        DEBUG("MinSize: (%d, %d) x (%d, %d)\n",
              size.x1, size.y1, size.x2, size.y2);
    if (size.x1 > size.x2 || size.y1 > size.y2)
       return false;

    return ret;
}

void cDvbSpuBitmap::putPixel(int xp, int yp, int len, uint8_t colorid)
{
    memset(bmp + spuXres * yp + xp, colorid, len);
    setMin(minsize[colorid].x1, xp);
    setMin(minsize[colorid].y1, yp);
    setMax(minsize[colorid].x2, xp + len - 1);
    setMax(minsize[colorid].y2, yp);
}

static uint8_t getBits(uint8_t * &data, uint8_t & bitf)
{
    uint8_t ret = *data;
    if (bitf)
        ret >>= 4;
    else
        data++;
    bitf ^= 1;

    return (ret & 0xf);
}

void cDvbSpuBitmap::putFieldData(int field, uint8_t * data, uint8_t * endp)
{
    int xp = bmpsize.x1;
    int yp = bmpsize.y1 + field;
    uint8_t bitf = 1;

    while (data < endp) {
        uint16_t vlc = getBits(data, bitf);
        if (vlc < 0x0004) {
            vlc = (vlc << 4) | getBits(data, bitf);
            if (vlc < 0x0010) {
                vlc = (vlc << 4) | getBits(data, bitf);
                if (vlc < 0x0040) {
                    vlc = (vlc << 4) | getBits(data, bitf);
                }
            }
        }

        uint8_t color = vlc & 0x03;
        int len = vlc >> 2;

        // if len == 0 -> end sequence - fill to end of line
        len = len ? len : bmpsize.x2 - xp + 1;
        putPixel(xp, yp, len, color);
        xp += len;

        if (xp > bmpsize.x2) {
            // nextLine
            if (!bitf)
                data++;
            bitf = 1;
            xp = bmpsize.x1;
            yp += 2;
            if (yp > bmpsize.y2)
                return;
        }
    }
}

// --- cDvbSpuDecoder-----------------------------

#define CMD_SPU_MENU            0x00
#define CMD_SPU_SHOW            0x01
#define CMD_SPU_HIDE            0x02
#define CMD_SPU_SET_PALETTE     0x03
#define CMD_SPU_SET_ALPHA       0x04
#define CMD_SPU_SET_SIZE        0x05
#define CMD_SPU_SET_PXD_OFFSET  0x06
#define CMD_SPU_CHG_COLCON      0x07
#define CMD_SPU_EOF             0xff

#define spuU32(i)  ((spu[i] << 8) + spu[i+1])

cDvbSpuDecoder::cDvbSpuDecoder()
{
    clean = true;
    scaleMode = eSpuNormal;
    spu = NULL;
    osd = NULL;
    spubmp = NULL;
    allowedShow = false;
}

cDvbSpuDecoder::~cDvbSpuDecoder()
{
    delete spubmp;
    delete spu;
    delete osd;
}

void cDvbSpuDecoder::processSPU(uint32_t pts, uint8_t * buf, bool AllowedShow)
{
    setTime(pts);

    DEBUG("SPU pushData: pts: %d\n", pts);

    delete spubmp;
    spubmp = NULL;
    delete[]spu;
    spu = buf;
    spupts = pts;

    DCSQ_offset = cmdOffs();
    prev_DCSQ_offset = 0;

    clean = true;
    allowedShow = AllowedShow;
}

void cDvbSpuDecoder::setScaleMode(cSpuDecoder::eScaleMode ScaleMode)
{
    scaleMode = ScaleMode;
}

void cDvbSpuDecoder::setPalette(uint32_t * pal)
{
    palette.setPalette(pal);
}

void cDvbSpuDecoder::setHighlight(uint16_t sx, uint16_t sy,
                                  uint16_t ex, uint16_t ey,
                                  uint32_t palette)
{
    aDvbSpuPalDescr pld;
    for (int i = 0; i < 4; i++) {
        pld[i].index = 0xf & (palette >> (16 + 4 * i));
        pld[i].trans = 0xf & (palette >> (4 * i));
    }

    bool ne = hlpsize.x1 != sx || hlpsize.y1 != sy ||
        hlpsize.x2 != ex || hlpsize.y2 != ey ||
        pld[0] != hlpDescr[0] || pld[1] != hlpDescr[1] ||
        pld[2] != hlpDescr[2] || pld[3] != hlpDescr[3];

    if (ne) {
        DEBUG("setHighlight: %d,%d x %d,%d\n", sx, sy, ex, ey);
        hlpsize.x1 = sx;
        hlpsize.y1 = sy;
        hlpsize.x2 = ex;
        hlpsize.y2 = ey;
        memcpy(hlpDescr, pld, sizeof(aDvbSpuPalDescr));
        highlight = true;
        clean = false;
    }
}

void cDvbSpuDecoder::clearHighlight(void)
{
    clean &= !highlight;
    highlight = false;
    hlpsize.x1 = -1;
    hlpsize.y1 = -1;
    hlpsize.x2 = -1;
    hlpsize.y2 = -1;
}

sDvbSpuRect cDvbSpuDecoder::CalcAreaSize(sDvbSpuRect fgsize, cBitmap *fgbmp, sDvbSpuRect bgsize, cBitmap *bgbmp)
{
    sDvbSpuRect size;
    if (fgbmp && bgbmp) {
       size.x1 = min(fgsize.x1, bgsize.x1);
       size.y1 = min(fgsize.y1, bgsize.y1);
       size.x2 = max(fgsize.x2, bgsize.x2);
       size.y2 = max(fgsize.y2, bgsize.y2);
       }
    else if (fgbmp) {
       size.x1 = fgsize.x1;
       size.y1 = fgsize.y1;
       size.x2 = fgsize.x2;
       size.y2 = fgsize.y2;
       }
    else if (bgbmp) {
       size.x1 = bgsize.x1;
       size.y1 = bgsize.y1;
       size.x2 = bgsize.x2;
       size.y2 = bgsize.y2;
       }
    else {
       size.x1 = 0;
       size.y1 = 0;
       size.x2 = 0;
       size.y2 = 0;
       }
    return size;
}

void cDvbSpuDecoder::Draw(void)
{
    if (!spubmp) {
        Hide();
        return;
    }

    cBitmap *fg = NULL;
    cBitmap *bg = NULL;
    sDvbSpuRect bgsize;
    sDvbSpuRect hlsize;

    hlsize.x1 = hlpsize.x1;
    hlsize.y1 = hlpsize.y1;
    hlsize.x2 = hlpsize.x2;
    hlsize.y2 = hlpsize.y2;

    if (highlight)
        fg = spubmp->getBitmap(hlpDescr, palette, hlsize);

    if (spubmp->getMinSize(palDescr, bgsize))
        bg = spubmp->getBitmap(palDescr, palette, bgsize);

    sDvbSpuRect areaSize = CalcAreaSize(hlsize, fg, bgsize, bg);

    if (!fg || !bg || !osd) {
       Hide();
       }

    if (bg || fg) {
        if (osd == NULL) {
           osd = cOsdProvider::NewOsd(0, 0);
           int x2 = areaSize.x2;
           while ((x2 - areaSize.x1 + 1) & 0x03)
                 x2++;
           tArea Area = { areaSize.x1, areaSize.y1, x2, areaSize.y2, (fg && bg) ? 4 : 2 };
           osd->SetAreas(&Area, 1);
           }

        if (bg)
           osd->DrawBitmap(bgsize.x1, bgsize.y1, *bg);
        if (fg)
           osd->DrawBitmap(hlsize.x1, hlsize.y1, *fg);
        delete fg;
        delete bg;

        osd->Flush();
    }

    clean = true;
}

void cDvbSpuDecoder::Hide(void)
{
    delete osd;
    osd = NULL;
}

void cDvbSpuDecoder::Empty(void)
{
    Hide();

    delete spubmp;
    spubmp = NULL;

    delete[]spu;
    spu = NULL;

    clearHighlight();
    clean = true;
}

int cDvbSpuDecoder::setTime(uint32_t pts)
{
    if (!spu)
        return 0;

    if (spu && !clean)
        Draw();

    while (DCSQ_offset != prev_DCSQ_offset) {   /* Display Control Sequences */
        int i = DCSQ_offset;
        state = spNONE;

        uint32_t exec_time = spupts + spuU32(i) * 1024;
        if ((pts != 0) && (exec_time > pts))
            return 0;
        DEBUG("offs = %d, rel = %d, time = %d, pts = %d, diff = %d\n",
              i, spuU32(i) * 1024, exec_time, pts, exec_time - pts);

        if (pts != 0) {
            uint16_t feven = 0;
            uint16_t fodd = 0;

            i += 2;

            prev_DCSQ_offset = DCSQ_offset;
            DCSQ_offset = spuU32(i);
            DEBUG("offs = %d, DCSQ = %d, prev_DCSQ = %d\n", 
                           i, DCSQ_offset, prev_DCSQ_offset);
            i += 2;

            while (spu[i] != CMD_SPU_EOF) {     // Command Sequence
                switch (spu[i]) {
                case CMD_SPU_SHOW:     // show subpicture
                    DEBUG("\tshow subpicture\n");
                    state = spSHOW;
                    i++;
                    break;

                case CMD_SPU_HIDE:     // hide subpicture
                    DEBUG("\thide subpicture\n");
                    state = spHIDE;
                    i++;
                    break;

                case CMD_SPU_SET_PALETTE:      // CLUT
                    palDescr[0].index = spu[i + 2] & 0xf;
                    palDescr[1].index = spu[i + 2] >> 4;
                    palDescr[2].index = spu[i + 1] & 0xf;
                    palDescr[3].index = spu[i + 1] >> 4;
                    i += 3;
                    break;

                case CMD_SPU_SET_ALPHA:        // transparency palette
                    palDescr[0].trans = spu[i + 2] & 0xf;
                    palDescr[1].trans = spu[i + 2] >> 4;
                    palDescr[2].trans = spu[i + 1] & 0xf;
                    palDescr[3].trans = spu[i + 1] >> 4;
                    i += 3;
                    break;

                case CMD_SPU_SET_SIZE: // image coordinates
                    size.x1 = (spu[i + 1] << 4) | (spu[i + 2] >> 4);
                    size.x2 = ((spu[i + 2] & 0x0f) << 8) | spu[i + 3];

                    size.y1 = (spu[i + 4] << 4) | (spu[i + 5] >> 4);
                    size.y2 = ((spu[i + 5] & 0x0f) << 8) | spu[i + 6];

                    DEBUG("\t(%d, %d) x (%d, %d)\n",
                          size.x1, size.y1, size.x2, size.y2);
                    i += 7;
                    break;

                case CMD_SPU_SET_PXD_OFFSET:   // image 1 / image 2 offsets
                    fodd = spuU32(i + 1);
                    feven = spuU32(i + 3);
                    DEBUG("\todd = %d even = %d\n", fodd, feven);
                    i += 5;
                    break;

                case CMD_SPU_CHG_COLCON: {
                    int size = spuU32(i + 1);
                    i += 1 + size;
                    }
                    break;

                case CMD_SPU_MENU:
                    DEBUG("\tspu menu\n");
                    state = spMENU;

                    i++;
                    break;

                default:
                    esyslog("invalid sequence in control header (%.2x)\n",
                            spu[i]);
                    assert(0);
                    i++;
                    break;
                }
            }
            if (fodd != 0 && feven != 0) {
                delete spubmp;
                spubmp = new cDvbSpuBitmap(size, spu + fodd, spu + feven,
                                           spu + feven, spu + cmdOffs());
            }
        } else if (!clean)
            state = spSHOW;

        if ((state == spSHOW && allowedShow) || state == spMENU)
            Draw();

        if (state == spHIDE)
            Hide();

        if (pts == 0)
            return 0;
    }

    return 1;
}
