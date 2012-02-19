/*
 * menu.h: The DVB HD Full Featured device main menu
 *
 * See the README file for copyright information and how to reach the author.
 */

#ifndef _HDFF_MENU_H_
#define _HDFF_MENU_H_

#include <vdr/osd.h>
#include <vdr/plugin.h>

#include "hdffcmd.h"

class cHdffMenu : public cOsdMenu
{
private:
    HDFF::cHdffCmdIf * mHdffCmdIf;

    cOsdItem * mVideoConversionItem;

    void SetVideoConversion(void);
public:
    cHdffMenu(HDFF::cHdffCmdIf * pHdffCmdIf);
    virtual ~cHdffMenu();
    virtual eOSState ProcessKey(eKeys Key);
};

#endif
