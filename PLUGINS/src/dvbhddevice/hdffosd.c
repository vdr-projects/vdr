/*
 * hdffosd.c: Implementation of the DVB HD Full Featured On Screen Display
 *
 * See the README file for copyright information and how to reach the author.
 */

#include "hdffosd.h"
#include <linux/dvb/osd.h>
#include <sys/ioctl.h>
#include <sys/time.h>
#include "hdffcmd.h"
#include "setup.h"

#define MAX_NUM_FONTFACES   8
#define MAX_NUM_FONTS       8
#define MAX_BITMAP_SIZE     (1024*1024)

typedef struct _tFontFace
{
    cString Name;
    uint32_t Handle;
} tFontFace;

typedef struct _tFont
{
    uint32_t hFontFace;
    int Size;
    uint32_t Handle;
} tFont;

class cHdffOsd : public cOsd
{
private:
    HDFF::cHdffCmdIf * mHdffCmdIf;
    int mLeft;
    int mTop;
    int mDispWidth;
    int mDispHeight;
    bool mChanged;
    uint32_t mDisplay;
    tFontFace mFontFaces[MAX_NUM_FONTFACES];
    tFont mFonts[MAX_NUM_FONTS];
    uint32_t mBitmapPalette;
    uint32_t mBitmapColors[256];

    bool mSupportsUtf8Text;

protected:
    virtual void SetActive(bool On);
public:
    cHdffOsd(int Left, int Top, HDFF::cHdffCmdIf * pHdffCmdIf, uint Level);
    virtual ~cHdffOsd();
    virtual eOsdError CanHandleAreas(const tArea *Areas, int NumAreas);
    virtual eOsdError SetAreas(const tArea *Areas, int NumAreas);
    virtual void SaveRegion(int x1, int y1, int x2, int y2);
    virtual void RestoreRegion(void);
    virtual void DrawPixel(int x, int y, tColor Color);
    virtual void DrawBitmap(int x, int y, const cBitmap &Bitmap, tColor ColorFg = 0, tColor ColorBg = 0, bool ReplacePalette = false, bool Overlay = false);
    virtual void DrawText(int x, int y, const char *s, tColor ColorFg, tColor ColorBg, const cFont *Font, int Width = 0, int Height = 0, int Alignment = taDefault);
    virtual void DrawRectangle(int x1, int y1, int x2, int y2, tColor Color);
    virtual void DrawEllipse(int x1, int y1, int x2, int y2, tColor Color, int Quadrants = 0);
    virtual void DrawSlope(int x1, int y1, int x2, int y2, tColor Color, int Type);
    virtual void Flush(void);
};

cHdffOsd::cHdffOsd(int Left, int Top, HDFF::cHdffCmdIf * pHdffCmdIf, uint Level)
:   cOsd(Left, Top, Level)
{
    double pixelAspect;
    HdffOsdConfig_t config;

    //printf("cHdffOsd %d, %d, %d\n", Left, Top, Level);
    mHdffCmdIf = pHdffCmdIf;
    mLeft = Left;
    mTop = Top;
    mChanged = false;
    mBitmapPalette = HDFF_INVALID_HANDLE;

    mSupportsUtf8Text = false;
    if (mHdffCmdIf->CmdGetFirmwareVersion(NULL, 0) >= 0x309)
        mSupportsUtf8Text = true;

    memset(&config, 0, sizeof(config));
    config.FontKerning = true;
    config.FontAntialiasing = Setup.AntiAlias ? true : false;
    mHdffCmdIf->CmdOsdConfigure(&config);

    gHdffSetup.GetOsdSize(mDispWidth, mDispHeight, pixelAspect);
    mDisplay = mHdffCmdIf->CmdOsdCreateDisplay(mDispWidth, mDispHeight, HDFF_COLOR_TYPE_ARGB8888);
    mHdffCmdIf->CmdOsdSetDisplayOutputRectangle(mDisplay, 0, 0, HDFF_SIZE_FULL_SCREEN, HDFF_SIZE_FULL_SCREEN);
    for (int i = 0; i < MAX_NUM_FONTFACES; i++)
    {
        mFontFaces[i].Name = "";
        mFontFaces[i].Handle = HDFF_INVALID_HANDLE;
    }
    for (int i = 0; i < MAX_NUM_FONTS; i++)
    {
        mFonts[i].hFontFace = HDFF_INVALID_HANDLE;
        mFonts[i].Size = 0;
        mFonts[i].Handle = HDFF_INVALID_HANDLE;
    }
}

cHdffOsd::~cHdffOsd()
{
    //printf("~cHdffOsd %d %d\n", mLeft, mTop);
    if (Active()) {
        mHdffCmdIf->CmdOsdDrawRectangle(mDisplay, 0, 0, mDispWidth, mDispHeight, 0);
        mHdffCmdIf->CmdOsdRenderDisplay(mDisplay);
    }
    SetActive(false);

    for (int i = 0; i < MAX_NUM_FONTS; i++)
    {
        if (mFonts[i].Handle == HDFF_INVALID_HANDLE)
            break;
        mHdffCmdIf->CmdOsdDeleteFont(mFonts[i].Handle);
    }
    for (int i = 0; i < MAX_NUM_FONTFACES; i++)
    {
        if (mFontFaces[i].Handle == HDFF_INVALID_HANDLE)
            break;
        mHdffCmdIf->CmdOsdDeleteFontFace(mFontFaces[i].Handle);
    }

    if (mBitmapPalette != HDFF_INVALID_HANDLE)
        mHdffCmdIf->CmdOsdDeletePalette(mBitmapPalette);
    mHdffCmdIf->CmdOsdDeleteDisplay(mDisplay);
}

eOsdError cHdffOsd::CanHandleAreas(const tArea *Areas, int NumAreas)
{
    eOsdError Result = cOsd::CanHandleAreas(Areas, NumAreas);
    if (Result == oeOk)
    {
        for (int i = 0; i < NumAreas; i++)
        {
            if (Areas[i].bpp != 1 && Areas[i].bpp != 2 && Areas[i].bpp != 4 && Areas[i].bpp != 8)
                return oeBppNotSupported;
        }
    }
    return Result;
}

eOsdError cHdffOsd::SetAreas(const tArea *Areas, int NumAreas)
{
    eOsdError error;
    cBitmap * bitmap;

    for (int i = 0; i < NumAreas; i++)
    {
        //printf("SetAreas %d: %d %d %d %d %d\n", i, Areas[i].x1, Areas[i].y1, Areas[i].x2, Areas[i].y2, Areas[i].bpp);
    }
    if (Active() && mDisplay != HDFF_INVALID_HANDLE)
    {
        mHdffCmdIf->CmdOsdDrawRectangle(mDisplay, 0, 0, mDispWidth, mDispHeight, 0);
        mHdffCmdIf->CmdOsdRenderDisplay(mDisplay);
    }
    error = cOsd::SetAreas(Areas, NumAreas);

    for (int i = 0; (bitmap = GetBitmap(i)) != NULL; i++)
    {
        bitmap->Clean();
    }

    return error;
}

void cHdffOsd::SetActive(bool On)
{
    if (On != Active())
    {
        cOsd::SetActive(On);
        if (On)
        {
            if (GetBitmap(0)) // only flush here if there are already bitmaps
                Flush();
        }
        else if (mDisplay != HDFF_INVALID_HANDLE)
        {
            mHdffCmdIf->CmdOsdDrawRectangle(mDisplay, 0, 0, mDispWidth, mDispHeight, 0);
            mHdffCmdIf->CmdOsdRenderDisplay(mDisplay);
        }
    }
}

void cHdffOsd::SaveRegion(int x1, int y1, int x2, int y2)
{
    mHdffCmdIf->CmdOsdSaveRegion(mDisplay, mLeft + x1, mTop + y1, x2 - x1 + 1, y2 - y1 + 1);
    mChanged = true;
}

void cHdffOsd::RestoreRegion(void)
{
    mHdffCmdIf->CmdOsdRestoreRegion(mDisplay);
    mChanged = true;
}

void cHdffOsd::DrawPixel(int x, int y, tColor Color)
{
    //printf("DrawPixel\n");
}

void cHdffOsd::DrawBitmap(int x, int y, const cBitmap &Bitmap, tColor ColorFg, tColor ColorBg, bool ReplacePalette, bool Overlay)
{
    //printf("DrawBitmap %d %d %d x %d\n", x, y, Bitmap.Width(), Bitmap.Height());
    int i;
    int numColors;
    const tColor * colors = Bitmap.Colors(numColors);

    for (i = 0; i < numColors; i++)
    {
        mBitmapColors[i] = colors[i];
        if (ColorFg || ColorBg)
        {
            if (i == 0)
                mBitmapColors[i] = ColorBg;
            else if (i == 1)
                mBitmapColors[i] = ColorFg;
        }
    }
    if (mBitmapPalette == HDFF_INVALID_HANDLE)
    {
        mBitmapPalette = mHdffCmdIf->CmdOsdCreatePalette(HDFF_COLOR_TYPE_CLUT8,
                HDFF_COLOR_FORMAT_ARGB, numColors, mBitmapColors);
    }
    else
    {
        mHdffCmdIf->CmdOsdSetPaletteColors(mBitmapPalette,
                HDFF_COLOR_FORMAT_ARGB, 0, numColors, mBitmapColors);
    }
    int width = Bitmap.Width();
    int height = Bitmap.Height();
    int chunk = MAX_BITMAP_SIZE / width;
    if (chunk > height)
        chunk = height;
    for (int yc = 0; yc < height; yc += chunk)
    {
        int hc = chunk;
        if (yc + hc > height)
            hc = height - yc;
        mHdffCmdIf->CmdOsdDrawBitmap(mDisplay, mLeft + x, mTop + y + yc,
            (uint8_t *) Bitmap.Data(0, yc), width, hc,
            width * hc, HDFF_COLOR_TYPE_CLUT8, mBitmapPalette);
    }
    mChanged = true;
}

void cHdffOsd::DrawText(int x, int y, const char *s, tColor ColorFg, tColor ColorBg, const cFont *Font, int Width, int Height, int Alignment)
{
    int w = Font->Width(s);
    int h = Font->Height();
    int cw = Width ? Width : w;
    int ch = Height ? Height : h;
    int i;
    int size = Font->Size();
    tFontFace * pFontFace;
    tFont * pFont;

    if (ColorBg != clrTransparent)
        mHdffCmdIf->CmdOsdDrawRectangle(mDisplay, mLeft + x, mTop + y, cw, ch, ColorBg);

    if (s == NULL)
        return;

    pFontFace = NULL;
    for (i = 0; i < MAX_NUM_FONTFACES; i++)
    {
        if (mFontFaces[i].Handle == HDFF_INVALID_HANDLE)
            break;

        if (strcmp(mFontFaces[i].Name, Font->FontName()) == 0)
        {
            pFontFace = &mFontFaces[i];
            break;
        }
    }
    if (pFontFace == NULL)
    {
        if (i < MAX_NUM_FONTFACES)
        {
            cString fontFileName = Font->FontName();
            FILE * fp = fopen(fontFileName, "rb");
            if (fp)
            {
                fseek(fp, 0, SEEK_END);
                long fileSize = ftell(fp);
                fseek(fp, 0, SEEK_SET);
                if (fileSize > 0)
                {
                    uint8_t * buffer = new uint8_t[fileSize];
                    if (buffer)
                    {
                        if (fread(buffer, fileSize, 1, fp) == 1)
                        {
                            mFontFaces[i].Handle = mHdffCmdIf->CmdOsdCreateFontFace(buffer, fileSize);
                            if (mFontFaces[i].Handle != HDFF_INVALID_HANDLE)
                            {
                                mFontFaces[i].Name = Font->FontName();
                                pFontFace = &mFontFaces[i];
                            }
                        }
                        delete[] buffer;
                    }
                }
                fclose(fp);
            }
        }
    }
    if (pFontFace == NULL)
        return;

    pFont = NULL;
    for (i = 0; i < MAX_NUM_FONTS; i++)
    {
        if (mFonts[i].Handle == HDFF_INVALID_HANDLE)
            break;

        if (mFonts[i].hFontFace == pFontFace->Handle
          && mFonts[i].Size == size)
        {
            pFont = &mFonts[i];
            break;
        }
    }
    if (pFont == NULL)
    {
        if (i < MAX_NUM_FONTS)
        {
            mFonts[i].Handle = mHdffCmdIf->CmdOsdCreateFont(pFontFace->Handle, size);
            if (mFonts[i].Handle != HDFF_INVALID_HANDLE)
            {
                mFonts[i].hFontFace = pFontFace->Handle;
                mFonts[i].Size = size;
                pFont = &mFonts[i];
            }
        }
    }
    if (pFont == NULL)
        return;

    mHdffCmdIf->CmdOsdSetDisplayClippingArea(mDisplay, true, mLeft + x, mTop + y, cw, ch);

    if (Width || Height)
    {
        if (Width)
        {
            if ((Alignment & taLeft) != 0)
            {
#if (APIVERSNUM >= 10728)
                if ((Alignment & taBorder) != 0)
                    x += max(h / TEXT_ALIGN_BORDER, 1);
#endif
            }
            else if ((Alignment & taRight) != 0)
            {
                if (w < Width)
                    x += Width - w;
#if (APIVERSNUM >= 10728)
                if ((Alignment & taBorder) != 0)
                    x -= max(h / TEXT_ALIGN_BORDER, 1);
#endif
            }
            else
            { // taCentered
                if (w < Width)
                    x += (Width - w) / 2;
            }
        }
        if (Height)
        {
            if ((Alignment & taTop) != 0)
                ;
            else if ((Alignment & taBottom) != 0)
            {
                if (h < Height)
                    y += Height - h;
            }
            else
            { // taCentered
                if (h < Height)
                    y += (Height - h) / 2;
            }
        }
    }
#if 0
    if (mSupportsUtf8Text)
    {
        mHdffCmdIf->CmdOsdDrawUtf8Text(mDisplay, pFont->Handle, x + mLeft, y + mTop + h, s, ColorFg);
    }
    else
#endif
    {
        uint16_t tmp[1000];
        uint16_t len = 0;
        while (*s && (len < (sizeof(tmp) - 1)))
        {
            int sl = Utf8CharLen(s);
            uint sym = Utf8CharGet(s, sl);
            s += sl;
            tmp[len] = sym;
            len++;
        }
        tmp[len] = 0;
        mHdffCmdIf->CmdOsdDrawTextW(mDisplay, pFont->Handle, x + mLeft, y + mTop + h, tmp, ColorFg);
    }
    mHdffCmdIf->CmdOsdSetDisplayClippingArea(mDisplay, false, 0, 0, 0, 0);
    mChanged = true;
}

void cHdffOsd::DrawRectangle(int x1, int y1, int x2, int y2, tColor Color)
{
    mHdffCmdIf->CmdOsdDrawRectangle(mDisplay, mLeft + x1, mTop + y1, x2 - x1 + 1, y2 - y1 + 1, Color);
    mChanged = true;
}

void cHdffOsd::DrawEllipse(int x1, int y1, int x2, int y2, tColor Color, int Quadrants)
{
    uint32_t flags;
    int cx;
    int cy;
    int rx;
    int ry;

    switch (abs(Quadrants))
    {
        case 1:
            if (Quadrants > 0)
                flags = HDFF_DRAW_QUARTER_TOP_RIGHT;
            else
                flags = HDFF_DRAW_QUARTER_TOP_RIGHT_INVERTED;
            cx = x1;
            cy = y2;
            rx = x2 - x1;
            ry = y2 - y1;
            break;
        case 2:
            if (Quadrants > 0)
                flags = HDFF_DRAW_QUARTER_TOP_LEFT;
            else
                flags = HDFF_DRAW_QUARTER_TOP_LEFT_INVERTED;
            cx = x2;
            cy = y2;
            rx = x2 - x1;
            ry = y2 - y1;
            break;
        case 3:
            if (Quadrants > 0)
                flags = HDFF_DRAW_QUARTER_BOTTOM_LEFT;
            else
                flags = HDFF_DRAW_QUARTER_BOTTOM_LEFT_INVERTED;
            cx = x2;
            cy = y1;
            rx = x2 - x1;
            ry = y2 - y1;
            break;
        case 4:
            if (Quadrants > 0)
                flags = HDFF_DRAW_QUARTER_BOTTOM_RIGHT;
            else
                flags = HDFF_DRAW_QUARTER_BOTTOM_RIGHT_INVERTED;
            cx = x1;
            cy = y1;
            rx = x2 - x1;
            ry = y2 - y1;
            break;
        case 5:
            flags = HDFF_DRAW_HALF_RIGHT;
            cx = x1;
            cy = (y1 + y2) / 2;
            rx = x2 - x1;
            ry = (y2 - y1) / 2;
            break;
        case 6:
            flags = HDFF_DRAW_HALF_TOP;
            cx = (x1 + x2) / 2;
            cy = y2;
            rx = (x2 - x1) / 2;
            ry = y2 - y1;
            break;
        case 7:
            flags = HDFF_DRAW_HALF_LEFT;
            cx = x2;
            cy = (y1 + y2) / 2;
            rx = x2 - x1;
            ry = (y2 - y1) / 2;
            break;
        case 8:
            flags = HDFF_DRAW_HALF_BOTTOM;
            cx = (x1 + x2) / 2;
            cy = y1;
            rx = (x2 - x1) / 2;
            ry = y2 - y1;
            break;
        default:
            flags = HDFF_DRAW_FULL;
            cx = (x1 + x2) / 2;
            cy = (y1 + y2) / 2;
            rx = (x2 - x1) / 2;
            ry = (y2 - y1) / 2;
            break;
    }
    mHdffCmdIf->CmdOsdDrawEllipse(mDisplay, mLeft + cx, mTop + cy, rx, ry, Color, flags);
    mChanged = true;
}

void cHdffOsd::DrawSlope(int x1, int y1, int x2, int y2, tColor Color, int Type)
{
    //printf("DrawSlope\n");
    mHdffCmdIf->CmdOsdDrawSlope(mDisplay, mLeft + x1, mTop + y1,
                                x2 - x1 + 1, y2 - y1 + 1, Color, Type);
    mChanged = true;
}

void cHdffOsd::Flush(void)
{
    if (!Active())
        return;

    //printf("Flush\n");
    cBitmap * Bitmap;

    for (int i = 0; (Bitmap = GetBitmap(i)) != NULL; i++)
    {
        int x1;
        int y1;
        int x2;
        int y2;

        if (Bitmap->Dirty(x1, y1, x2, y2))
        {
            //printf("dirty %d %d, %d %d\n", x1, y1, x2, y2);
            DrawBitmap(0, 0, *Bitmap);
            Bitmap->Clean();
        }
    }

    if (!mChanged)
        return;

    mHdffCmdIf->CmdOsdRenderDisplay(mDisplay);

    mChanged = false;
}


class cHdffOsdRaw : public cOsd
{
private:
    HDFF::cHdffCmdIf * mHdffCmdIf;
    int mDispWidth;
    int mDispHeight;
    bool refresh;
    uint32_t mDisplay;
    uint32_t mBitmapPalette;
    uint32_t mBitmapColors[256];

protected:
    virtual void SetActive(bool On);
public:
    cHdffOsdRaw(int Left, int Top, HDFF::cHdffCmdIf * pHdffCmdIf, uint Level);
    virtual ~cHdffOsdRaw();
    virtual eOsdError CanHandleAreas(const tArea *Areas, int NumAreas);
    virtual eOsdError SetAreas(const tArea *Areas, int NumAreas);
    virtual void Flush(void);
};

cHdffOsdRaw::cHdffOsdRaw(int Left, int Top, HDFF::cHdffCmdIf * pHdffCmdIf, uint Level)
:   cOsd(Left, Top, Level)
{
    double pixelAspect;

    //printf("cHdffOsdRaw %d, %d, %d\n", Left, Top, Level);
    mHdffCmdIf = pHdffCmdIf;
    refresh = true;
    mBitmapPalette = HDFF_INVALID_HANDLE;
    mDisplay = HDFF_INVALID_HANDLE;

    gHdffSetup.GetOsdSize(mDispWidth, mDispHeight, pixelAspect);
}

cHdffOsdRaw::~cHdffOsdRaw()
{
    //printf("~cHdffOsdRaw %d %d\n", Left(), Top());
    if (mDisplay != HDFF_INVALID_HANDLE)
    {
        mHdffCmdIf->CmdOsdDrawRectangle(mDisplay, 0, 0, mDispWidth, mDispHeight, 0);
        mHdffCmdIf->CmdOsdRenderDisplay(mDisplay);
    }
    if (mBitmapPalette != HDFF_INVALID_HANDLE)
        mHdffCmdIf->CmdOsdDeletePalette(mBitmapPalette);
    mBitmapPalette = HDFF_INVALID_HANDLE;
    if (mDisplay != HDFF_INVALID_HANDLE)
       mHdffCmdIf->CmdOsdDeleteDisplay(mDisplay);
    mDisplay = HDFF_INVALID_HANDLE;
}

void cHdffOsdRaw::SetActive(bool On)
{
    if (On != Active())
    {
        cOsd::SetActive(On);
        if (On)
        {
            if (mDisplay == HDFF_INVALID_HANDLE)
            {
                mDisplay = mHdffCmdIf->CmdOsdCreateDisplay(mDispWidth, mDispHeight, HDFF_COLOR_TYPE_ARGB8888);
                if (mDisplay != HDFF_INVALID_HANDLE)
                    mHdffCmdIf->CmdOsdSetDisplayOutputRectangle(mDisplay, 0, 0, HDFF_SIZE_FULL_SCREEN, HDFF_SIZE_FULL_SCREEN);
            }
            refresh = true;
            if (GetBitmap(0)) // only flush here if there are already bitmaps
                Flush();
        }
        else
        {
            if (mDisplay != HDFF_INVALID_HANDLE)
            {
                mHdffCmdIf->CmdOsdDrawRectangle(mDisplay, 0, 0, mDispWidth, mDispHeight, 0);
                mHdffCmdIf->CmdOsdRenderDisplay(mDisplay);
            }
            if (mBitmapPalette != HDFF_INVALID_HANDLE)
                mHdffCmdIf->CmdOsdDeletePalette(mBitmapPalette);
            mBitmapPalette = HDFF_INVALID_HANDLE;
            if (mDisplay != HDFF_INVALID_HANDLE)
                mHdffCmdIf->CmdOsdDeleteDisplay(mDisplay);
            mDisplay = HDFF_INVALID_HANDLE;
        }
    }
}

eOsdError cHdffOsdRaw::CanHandleAreas(const tArea *Areas, int NumAreas)
{
    eOsdError Result = cOsd::CanHandleAreas(Areas, NumAreas);
    if (Result == oeOk)
    {
        for (int i = 0; i < NumAreas; i++)
        {
            if (Areas[i].bpp != 1 && Areas[i].bpp != 2 && Areas[i].bpp != 4 && Areas[i].bpp != 8
                && (Areas[i].bpp != 32 || !gHdffSetup.TrueColorOsd))
                return oeBppNotSupported;
        }
    }
    return Result;
}

eOsdError cHdffOsdRaw::SetAreas(const tArea *Areas, int NumAreas)
{
    for (int i = 0; i < NumAreas; i++)
    {
        //printf("SetAreas %d: %d %d %d %d %d\n", i, Areas[i].x1, Areas[i].y1, Areas[i].x2, Areas[i].y2, Areas[i].bpp);
    }
    if (mDisplay != HDFF_INVALID_HANDLE)
    {
        mHdffCmdIf->CmdOsdDrawRectangle(mDisplay, 0, 0, mDispWidth, mDispHeight, 0);
        mHdffCmdIf->CmdOsdRenderDisplay(mDisplay);
        refresh = true;
    }
    return cOsd::SetAreas(Areas, NumAreas);
}

void cHdffOsdRaw::Flush(void)
{
    if (!Active() || (mDisplay == HDFF_INVALID_HANDLE))
        return;
#ifdef MEASURE_OSD_TIME
    struct timeval start;
    struct timeval end;
    struct timezone timeZone;
    gettimeofday(&start, &timeZone);
#endif

    bool render = false;
    if (IsTrueColor())
    {
        uint8_t * buffer = 0;
        if (gHdffSetup.TrueColorFormat != 0)
        {
            buffer = new uint8_t[MAX_BITMAP_SIZE];
            if (!buffer)
                return;
        }
        LOCK_PIXMAPS;
        while (cPixmapMemory *pm = dynamic_cast<cPixmapMemory *>(RenderPixmaps()))
        {
            int w = pm->ViewPort().Width();
            int h = pm->ViewPort().Height();
            int d = w * sizeof(tColor);
            int Chunk = MAX_BITMAP_SIZE / w / sizeof(tColor);
            if (Chunk > h)
                Chunk = h;
            for (int y = 0; y < h; y += Chunk)
            {
                int hc = Chunk;
                if (y + hc > h)
                    hc = h - y;
                if (gHdffSetup.TrueColorFormat == 0) // ARGB8888 (32 bit)
                {
                    mHdffCmdIf->CmdOsdDrawBitmap(mDisplay,
                        Left() + pm->ViewPort().X(), Top() + pm->ViewPort().Y() + y,
                        pm->Data() + y * d, w, hc, hc * d,
                        HDFF_COLOR_TYPE_ARGB8888, HDFF_INVALID_HANDLE);
                }
                else if (gHdffSetup.TrueColorFormat == 1) // ARGB8565 (24 bit)
                {
                    const tColor * pixmapData = (const tColor *) (pm->Data() + y * d);
                    uint8_t * bitmapData = buffer;
                    for (int i = 0; i < hc * w; i++)
                    {
                        bitmapData[2] =  (pixmapData[i] & 0xFF000000) >> 24;
                        bitmapData[1] = ((pixmapData[i] & 0x00F80000) >> 16)
                                      | ((pixmapData[i] & 0x0000E000) >> 13);
                        bitmapData[0] = ((pixmapData[i] & 0x00001C00) >> 5)
                                      | ((pixmapData[i] & 0x000000F8) >> 3);
                        bitmapData += 3;
                    }
                    mHdffCmdIf->CmdOsdDrawBitmap(mDisplay,
                        Left() + pm->ViewPort().X(), Top() + pm->ViewPort().Y() + y,
                        buffer, w, hc, hc * w * 3,
                        HDFF_COLOR_TYPE_ARGB8565, HDFF_INVALID_HANDLE);
                }
                else if (gHdffSetup.TrueColorFormat == 2) // ARGB4444 (16 bit)
                {
                    const tColor * pixmapData = (const tColor *) (pm->Data() + y * d);
                    uint16_t * bitmapData = (uint16_t *) buffer;
                    for (int i = 0; i < hc * w; i++)
                    {
                        bitmapData[i] = ((pixmapData[i] & 0xF0000000) >> 16)
                                      | ((pixmapData[i] & 0x00F00000) >> 12)
                                      | ((pixmapData[i] & 0x0000F000) >> 8)
                                      | ((pixmapData[i] & 0x000000F0) >> 4);
                    }
                    mHdffCmdIf->CmdOsdDrawBitmap(mDisplay,
                        Left() + pm->ViewPort().X(), Top() + pm->ViewPort().Y() + y,
                        buffer, w, hc, hc * w * 2,
                        HDFF_COLOR_TYPE_ARGB4444, HDFF_INVALID_HANDLE);
                }
            }
            DestroyPixmap(pm);
            render = true;
        }
        if (buffer)
            delete[] buffer;
    }
    else
    {
        uint8_t * buffer = new uint8_t[MAX_BITMAP_SIZE];
        if (!buffer)
            return;
        cBitmap * bitmap;
        for (int i = 0; (bitmap = GetBitmap(i)) != NULL; i++)
        {
            int x1 = 0, y1 = 0, x2 = 0, y2 = 0;
            if (refresh || bitmap->Dirty(x1, y1, x2, y2))
            {
                if (refresh)
                {
                    x2 = bitmap->Width() - 1;
                    y2 = bitmap->Height() - 1;
                }
                // commit colors:
                int numColors;
                const tColor * colors = bitmap->Colors(numColors);
                if (colors)
                {
                    for (int c = 0; c < numColors; c++)
                        mBitmapColors[c] = colors[c];
                    if (mBitmapPalette == HDFF_INVALID_HANDLE)
                    {
                        mBitmapPalette = mHdffCmdIf->CmdOsdCreatePalette(HDFF_COLOR_TYPE_CLUT8,
                            HDFF_COLOR_FORMAT_ARGB, numColors, mBitmapColors);
                    }
                    else
                    {
                        mHdffCmdIf->CmdOsdSetPaletteColors(mBitmapPalette,
                            HDFF_COLOR_FORMAT_ARGB, 0, numColors, mBitmapColors);
                    }
                }
                // commit modified data:
                int width = x2 - x1 + 1;
                int height = y2 - y1 + 1;
                int chunk = MAX_BITMAP_SIZE / width;
                if (chunk > height)
                    chunk = height;
                for (int y = 0; y < height; y += chunk)
                {
                    int hc = chunk;
                    if (y + hc > height)
                        hc = height - y;
                    for (int r = 0; r < hc; r++)
                        memcpy(buffer + r * width, bitmap->Data(x1, y1 + y + r), width);
                    mHdffCmdIf->CmdOsdDrawBitmap(mDisplay,
                        Left() + bitmap->X0() + x1, Top() + bitmap->Y0() + y1 + y,
                        buffer, width, hc, hc * width,
                        HDFF_COLOR_TYPE_CLUT8, mBitmapPalette);
                }
                render = true;
            }
            bitmap->Clean();
        }
        delete[] buffer;
    }
    if (render)
    {
        mHdffCmdIf->CmdOsdRenderDisplay(mDisplay);
#ifdef MEASURE_OSD_TIME
        gettimeofday(&end, &timeZone);
        int timeNeeded = end.tv_usec - start.tv_usec;
        timeNeeded += (end.tv_sec - start.tv_sec) * 1000000;
        printf("time = %d\n", timeNeeded);
#endif
    }
    refresh = false;
}




cHdffOsdProvider::cHdffOsdProvider(HDFF::cHdffCmdIf * HdffCmdIf)
{
    mHdffCmdIf = HdffCmdIf;
}

cOsd *cHdffOsdProvider::CreateOsd(int Left, int Top, uint Level)
{
    //printf("CreateOsd %d %d %d\n", Left, Top, Level);
    if (gHdffSetup.HighLevelOsd)
        return new cHdffOsd(Left, Top, mHdffCmdIf, Level);
    else
        return new cHdffOsdRaw(Left, Top, mHdffCmdIf, Level);
}

bool cHdffOsdProvider::ProvidesTrueColor(void)
{
    return gHdffSetup.TrueColorOsd && !gHdffSetup.HighLevelOsd;
}
