/**********************************************************************
 *
 * HDFF firmware command interface library
 *
 * Copyright (C) 2011  Andreas Regel
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.

 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.

 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the
 * Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
 *
 *********************************************************************/

#ifndef HDFFCMD_OSD_H
#define HDFFCMD_OSD_H


#define HDFF_INVALID_HANDLE             0xFFFFFFFF
#define HDFF_SCREEN_DISPLAY_HANDLE      0xFFFFFFFE

#define HDFF_POSITION_SCREEN_CENTERED   0xFFFF

#define HDFF_SIZE_FULL_SCREEN           0xFFFF
#define HDFF_SIZE_SAME_AS_SOURCE        0xFFFE

#define HDFF_FONT_FACE_TIRESIAS         0x00000000


typedef struct HdffOsdConfig_t
{
    int FontAntialiasing;
    int FontKerning;
    uint16_t FontDpi;
} HdffOsdConfig_t;

typedef enum HdffColorType_t
{
    HDFF_COLOR_TYPE_CLUT1,
    HDFF_COLOR_TYPE_CLUT2,
    HDFF_COLOR_TYPE_CLUT4,
    HDFF_COLOR_TYPE_CLUT8,
    HDFF_COLOR_TYPE_ARGB8888,
    HDFF_COLOR_TYPE_ARGB8565,
    HDFF_COLOR_TYPE_ARGB4444,
    HDFF_COLOR_TYPE_ARGB1555,
    HDFF_COLOR_TYPE_RGB888,
    HDFF_COLOR_TYPE_RGB565
} HdffColorType_t;

typedef enum HdffColorFormat_t
{
    HDFF_COLOR_FORMAT_ARGB,
    HDFF_COLOR_FORMAT_ACBYCR
} HdffColorFormat_t;

typedef enum HdffDrawingFlags_t
{
    HDFF_DRAW_FULL,
    HDFF_DRAW_HALF_TOP,
    HDFF_DRAW_HALF_LEFT,
    HDFF_DRAW_HALF_BOTTOM,
    HDFF_DRAW_HALF_RIGHT,
    HDFF_DRAW_QUARTER_TOP_LEFT,
    HDFF_DRAW_QUARTER_TOP_RIGHT,
    HDFF_DRAW_QUARTER_BOTTOM_LEFT,
    HDFF_DRAW_QUARTER_BOTTOM_RIGHT,
    HDFF_DRAW_QUARTER_TOP_LEFT_INVERTED,
    HDFF_DRAW_QUARTER_TOP_RIGHT_INVERTED,
    HDFF_DRAW_QUARTER_BOTTOM_LEFT_INVERTED,
    HDFF_DRAW_QUARTER_BOTTOM_RIGHT_INVERTED
} HdffDrawingFlags_t;


int HdffCmdOsdConfigure(int OsdDevice, const HdffOsdConfig_t * Config);

int HdffCmdOsdReset(int OsdDevice);


int HdffCmdOsdCreateDisplay(int OsdDevice, uint16_t Width, uint16_t Height,
                            HdffColorType_t ColorType, uint32_t * NewDisplay);

int HdffCmdOsdDeleteDisplay(int OsdDevice, uint32_t Display);

int HdffCmdOsdEnableDisplay(int OsdDevice, uint32_t Display, int Enable);

int HdffCmdOsdSetDisplayOutputRectangle(int OsdDevice, uint32_t Display,
                                        uint16_t X, uint16_t Y,
                                        uint16_t Width, uint16_t Height);

int HdffCmdOsdSetDisplayClippingArea(int OsdDevice, uint32_t Display,
                                     int Enable, uint16_t X, uint16_t Y,
                                     uint16_t Width, uint16_t Height);

int HdffCmdOsdRenderDisplay(int OsdDevice, uint32_t Display);

int HdffCmdOsdSaveRegion(int OsdDevice, uint32_t Display,
                         uint16_t X, uint16_t Y,
                         uint16_t Width, uint16_t Height);

int HdffCmdOsdRestoreRegion(int OsdDevice, uint32_t Display);


int HdffCmdOsdCreatePalette(int OsdDevice, HdffColorType_t ColorType,
                            HdffColorFormat_t ColorFormat,
                            uint32_t NumColors, const uint32_t * Colors,
                            uint32_t * NewPalette);

int HdffCmdOsdDeletePalette(int OsdDevice, uint32_t Palette);

int HdffCmdOsdSetDisplayPalette(int OsdDevice, uint32_t Display,
                                uint32_t Palette);

int HdffCmdOsdSetPaletteColors(int OsdDevice, uint32_t Palette,
                               HdffColorFormat_t ColorFormat,
                               uint8_t StartColor, uint32_t NumColors,
                               const uint32_t * Colors);


int HdffCmdOsdCreateFontFace(int OsdDevice, const uint8_t * FontData,
                             uint32_t DataSize, uint32_t * NewFontFace);

int HdffCmdOsdDeleteFontFace(int OsdDevice, uint32_t FontFace);

int HdffCmdOsdCreateFont(int OsdDevice, uint32_t FontFace, uint32_t Size,
                         uint32_t * NewFont);

int HdffCmdOsdDeleteFont(int OsdDevice, uint32_t Font);


int HdffCmdOsdDrawRectangle(int OsdDevice, uint32_t Display, uint16_t X,
                            uint16_t Y, uint16_t Width, uint16_t Height,
                            uint32_t Color);

int HdffCmdOsdDrawEllipse(int OsdDevice, uint32_t Display, uint16_t CX,
                          uint16_t CY, uint16_t RadiusX, uint16_t RadiusY,
                          uint32_t Color, uint32_t Flags);

int HdffCmdOsdDrawSlope(int OsdDevice, uint32_t Display, uint16_t X,
                        uint16_t Y, uint16_t Width, uint16_t Height,
                        uint32_t Color, uint32_t Type);

int HdffCmdOsdDrawText(int OsdDevice, uint32_t Display, uint32_t Font,
                       uint16_t X, uint16_t Y, const char * Text,
                       uint32_t Color);

int HdffCmdOsdDrawUtf8Text(int OsdDevice, uint32_t Display, uint32_t Font,
                           uint16_t X, uint16_t Y, const char * Text,
                           uint32_t Color);

int HdffCmdOsdDrawWideText(int OsdDevice, uint32_t Display, uint32_t Font,
                           uint16_t X, uint16_t Y, const uint16_t * Text,
                           uint32_t Color);

int HdffCmdOsdDrawBitmap(int OsdDevice, uint32_t Display, uint16_t X,
                         uint16_t Y, const uint8_t * Bitmap, uint16_t BmpWidth,
                         uint16_t BmpHeight, uint32_t BmpSize,
                         HdffColorType_t ColorType, uint32_t Palette);


#endif /* HDFFCMD_OSD_H */
