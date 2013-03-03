/*
 * hdffosd.h: Implementation of the DVB HD Full Featured On Screen Display
 *
 * See the README file for copyright information and how to reach the author.
 */

#ifndef _HDFF_OSD_H_
#define _HDFF_OSD_H_

#include <vdr/osd.h>

#include "hdffcmd.h"

class cHdffOsdProvider : public cOsdProvider
{
private:
    HDFF::cHdffCmdIf * mHdffCmdIf;
public:
    cHdffOsdProvider(HDFF::cHdffCmdIf * pHdffCmdIf);
    virtual cOsd *CreateOsd(int Left, int Top, uint Level);
    virtual bool ProvidesTrueColor(void);
};

#endif
