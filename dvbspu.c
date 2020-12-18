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
 * $Id: dvbspu.c 4.1 2020/12/18 14:51:57 kls Exp $
 */

#include "dvbspu.h"
#include <assert.h>
#include <string.h>
#include <inttypes.h>
#include <math.h>

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
#define DEBUG(format, args...) void()
#endif

// --- cDvbSpuPalette---------------------------------------------------------

void cDvbSpuPalette::setPalette(const uint32_t * pal)
{
    for (int i = 0; i < 16; i++)
        palette[i] = yuv2rgb(pal[i]);
}

// --- cDvbSpuBitmap ---------------------------------------------------------

#define setMin(a, b) if (a > b) a = b
#define setMax(a, b) if (a < b) a = b

// DVD SPU bitmaps cover max. 720 x 576 - this sizes the SPU bitmap
#define spuXres   720
#define spuYres   576

#define revRect(r1, r2) { r1.x1 = r2.x2; r1.y1 = r2.y2; r1.x2 = r2.x1; r1.y2 = r2.y1; }

cDvbSpuBitmap::cDvbSpuBitmap(sDvbSpuRect size,
                             uint8_t * fodd, uint8_t * eodd,
                             uint8_t * feven, uint8_t * eeven)
{
    size.x1 = max(size.x1, 0);
    size.y1 = max(size.y1, 0);
    size.x2 = min(size.x2, spuXres - 1);
    size.y2 = min(size.y2, spuYres - 1);

    bmpsize = size;
    revRect(minsize[0], size);
    revRect(minsize[1], size);
    revRect(minsize[2], size);
    revRect(minsize[3], size);

    int MemSize = spuXres * spuYres * sizeof(uint8_t);
    bmp = new uint8_t[MemSize];

    if (bmp)
       memset(bmp, 0, MemSize);
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
    if (bmp) {
       for (int yp = 0; yp < h; yp++) {
           for (int xp = 0; xp < w; xp++) {
               uint8_t idx = bmp[(size.y1 + yp) * spuXres + size.x1 + xp];
               ret->SetIndex(xp, yp, idx);
           }
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
    if (bmp)
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

// --- cDvbSpuDecoder---------------------------------------------------------

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

// SPUs must be scaled if screensize is not 720x576

void cDvbSpuDecoder::SetSpuScaling(void)
{
    int Width = spuXres;
    int Height = spuYres;
    int OsdWidth = 0;
    int OsdHeight = 0;
    double VideoAspect;
    cDevice::PrimaryDevice()->GetOsdSize(OsdWidth, OsdHeight, VideoAspect);
    DEBUG("dvbspu SetSpuScaling OsdSize %d x %d\n", OsdWidth, OsdHeight);
    if (!OsdWidth) { // guess correct size
        if (Setup.OSDWidth <= 720 || Setup.OSDHeight <= 576)
            xscaling = yscaling = 1.0;
        else if (Setup.OSDWidth <= 1280 || Setup.OSDHeight <= 720) {
            xscaling = 1280.0 / Width;
            yscaling = 720.0 / Height;
        }
        else {
            xscaling = 1920.0 / Width;
            yscaling = 1080.0/ Height;
        }
    }
    else {
        xscaling = (double)OsdWidth / Width;
        yscaling = (double)OsdHeight / Height;
    }
    DEBUG("dvbspu xscaling = %f yscaling = %f\n", xscaling, yscaling);
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
        Draw(); // we have to trigger Draw() here
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

int cDvbSpuBitmap::getMinBpp(const aDvbSpuPalDescr paldescr)
{
    int col = 1;
    for (int i = 0; i < 4; i++) {
        if (paldescr[i].trans != 0) {
                col++;
        }
    }
    return col > 2 ? 2 : 1;
}

int cDvbSpuDecoder::CalcAreaBpp(cBitmap *fgbmp, cBitmap *bgbmp)
{
        int fgbpp = 0;
        int bgbpp = 0;
        int ret;
    if (fgbmp) {
            fgbpp = spubmp->getMinBpp(hlpDescr);
    }
    if (bgbmp) {
            bgbpp = spubmp->getMinBpp(palDescr);
    }
    ret = fgbpp + bgbpp;
    if (ret > 2)
            ret = 4;
    return ret;
}

void cDvbSpuDecoder::Draw(void)
{
    cMutexLock MutexLock(&mutex);
    if (!spubmp) {
        Hide();
        return;
    }
    sDvbSpuRect bgsize;
    sDvbSpuRect drawsize;
    sDvbSpuRect bgdrawsize;
    cBitmap *fg = NULL;
    cBitmap *bg = NULL;
    cBitmap *tmp = NULL;

    SetSpuScaling(); // always set current scaling, size could have changed

    if (highlight) {
        tmp = spubmp->getBitmap(hlpDescr, palette, hlpsize);
        fg = tmp->Scaled(xscaling, yscaling, true);
        drawsize.x1 = hlpsize.x1 * xscaling;
        drawsize.y1 = hlpsize.y1 * yscaling;
        drawsize.x2 = drawsize.x1 + fg->Width();
        drawsize.y2 = drawsize.y1 + fg->Height();
    }

    if (spubmp->getMinSize(palDescr, bgsize)) {
        tmp = spubmp->getBitmap(palDescr, palette, bgsize);
        bg = tmp->Scaled(xscaling, yscaling, true);
        bgdrawsize.x1 = bgsize.x1 * xscaling;
        bgdrawsize.y1 = bgsize.y1 * yscaling;
        bgdrawsize.x2 = bgdrawsize.x1 + bg->Width();
        bgdrawsize.y2 = bgdrawsize.y1 + bg->Height();
    }

    if (osd) // always rewrite OSD
        Hide();

    if (osd == NULL) {
            restricted_osd = false;
            osd = cOsdProvider::NewOsd(0, 0);

            sDvbSpuRect areaSize = CalcAreaSize(drawsize, fg, bgdrawsize, bg); // combine
            tArea Area = { areaSize.x1, areaSize.y1, areaSize.x2, areaSize.y2, 4 };
            if (osd->CanHandleAreas(&Area, 1) != oeOk) {
                DEBUG("dvbspu CanHandleAreas (%d,%d)x(%d,%d), 4 failed\n", areaSize.x1, areaSize.y1, areaSize.x2, areaSize.y2);
                restricted_osd = true;
            }
            else
                osd->SetAreas(&Area, 1);
    }
    if (restricted_osd) {
            sDvbSpuRect hlsize;
            bool setarea = false;
            /* reduce fg area */
            if (fg) {
                    spubmp->getMinSize(hlpDescr,hlsize);
                    /* clip to the highligh area */
                    setMax(hlsize.x1, hlpsize.x1);
                    setMax(hlsize.y1, hlpsize.y1);
                    setMin(hlsize.x2, hlpsize.x2);
                    setMin(hlsize.y2, hlpsize.y2);
                    if (hlsize.x1 > hlsize.x2 || hlsize.y1 > hlsize.y2)
                            hlsize.x1 = hlsize.x2 = hlsize.y1 = hlsize.y2 = 0;
                    /* resize scaled fg */
                    drawsize.x1=hlsize.x1 * xscaling;
                    drawsize.y1=hlsize.y1 * yscaling;
                    drawsize.x2=hlsize.x2 * xscaling;
                    drawsize.y2=hlsize.y2 * yscaling;
            }
            sDvbSpuRect areaSize = CalcAreaSize(drawsize, fg, bgdrawsize, bg);

#define DIV(a, b) (a/b)?:1
            for (int d = 1; !setarea && d <= 2; d++) {

                    /* first try old behaviour */
                    tArea Area = { areaSize.x1, areaSize.y1, areaSize.x2, areaSize.y2, DIV(CalcAreaBpp(fg, bg), d) };

                    if ((Area.Width() & 7) != 0)
                            Area.x2 += 8 - (Area.Width() & 7);

                    if (osd->CanHandleAreas(&Area, 1) == oeOk &&
                        osd->SetAreas(&Area, 1) == oeOk)
                            setarea = true;

                    /* second try to split area if there is both area */
                    if (!setarea && fg && bg) {
                            tArea Area_Both [2] = {
                                    { bgdrawsize.x1, bgdrawsize.y1, bgdrawsize.x2, bgdrawsize.y2, DIV(CalcAreaBpp(0, bg), d) },
                                    { drawsize.x1, drawsize.y1, drawsize.x2, drawsize.y2, DIV(CalcAreaBpp(fg, 0), d) }
                            };
                            if (!Area_Both[0].Intersects(Area_Both[1])) {
                                    /* there is no intersection. We can try with split areas */
                                    if ((Area_Both[0].Width() & 7) != 0)
                                            Area_Both[0].x2 += 8 - (Area_Both[0].Width() & 7);
                                    if ((Area_Both[1].Width() & 7) != 0)
                                            Area_Both[1].x2 += 8 - (Area_Both[1].Width() & 7);
                                    if (osd->CanHandleAreas(Area_Both, 2) == oeOk &&
                                        osd->SetAreas(Area_Both, 2) == oeOk)
                                            setarea = true;
                            }
                    }
            }
            if (setarea)
                DEBUG("dvbspu: reduced AreaSize (%d, %d) (%d, %d) Bpp %d\n", areaSize.x1, areaSize.y1, areaSize.x2, areaSize.y2, (fg && bg) ? 4 : 2);
            else
                dsyslog("dvbspu: reduced AreaSize (%d, %d) (%d, %d) Bpp %d failed", areaSize.x1, areaSize.y1, areaSize.x2, areaSize.y2, (fg && bg) ? 4 : 2);
    }

    /* we could draw use DrawPixel on osd */
    if (bg || fg) {
        if (bg)
           osd->DrawBitmap(bgdrawsize.x1, bgdrawsize.y1, *bg);
        if (fg)
           osd->DrawBitmap(drawsize.x1, drawsize.y1, *fg);
        delete fg;
        delete bg;
        delete tmp;

        osd->Flush();
    }

    clean = true;
}

void cDvbSpuDecoder::Hide(void)
{
    cMutexLock MutexLock(&mutex);
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

    if (!clean)
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
                    esyslog("invalid sequence in control header (%.2x)",
                            spu[i]);
                    Empty();
                    return 0;
                }
            }
            if (fodd != 0 && feven != 0) {
                Hide();
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
