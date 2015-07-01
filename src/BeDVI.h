////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//                                                                                                                //
// $Id: BeDVI.h,v 2.3 1998/08/20 11:16:07 achim Exp $
//                                                                                                                //
// BeDVI                                                                                                          //
// by Achim Blumensath                                                                                            //
// blume@corona.oche.de                                                                                           //
//                                                                                                                //
// This program is free software! It may be distributed according to the GNU Public License (see COPYING).        //
//                                                                                                                //
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

#ifndef VIEWDVI_H
#define VIEWDVI_H

#ifndef _APPLICATION_H
#include <Application.h>
#endif
#ifndef LIBPREFS_H
#include <libprefs.h>
#endif
#ifndef DEFINES_H
#include "defines.h"
#endif
#ifndef DVI_H
#include "DVI.h"
#endif

#include "Support.h"

class BPositionIO;
class BWindow;
class DisplayInfo;
class DStream;
class DVIView;

#define VERSION "V2.0"

// Messages:

static const uint32 MsgOpenPanel      = 'opnl';
static const uint32 MsgLoadPanel      = 'lpnl';
static const uint32 MsgOpenFile       = 'open';
static const uint32 MsgLoadFile       = 'load';
static const uint32 MsgReload         = 'rlod';
static const uint32 MsgPrintPage      = 'prtp';
static const uint32 MsgAntiAliasing   = 'grey';
static const uint32 MsgBorderLine     = 'bord';
static const uint32 MsgMeasureWin     = 'mwin';
static const uint32 MsgSearch         = 'find';
static const uint32 MsgNext           = 'next';
static const uint32 MsgPrev           = 'prev';
static const uint32 MsgFirst          = 'frst';
static const uint32 MsgLast           = 'last';
static const uint32 MsgNextWindow     = 'nwin';
static const uint32 MsgPrevWindow     = 'pwin';
static const uint32 MsgSaveSettings   = 'svst';
static const uint32 MsgPage           = 'page';
static const uint32 MsgResChanged     = 'resc';
static const uint32 MsgPoint          = 'pnt ';
static const uint32 MsgRedraw         = 'rdrw';

class ViewApplication: public BApplication
{
  private:
    typedef BApplication inherited;

    struct MenuDef
    {
      char   *Name;
      char   Shortcut;
      uint32 Message;
    };

    BWindow     *MeasureWin;
    BFilePanel  *OpenPanel;
    entry_ref   PanelDirectory;
    PREFHandle  PrefHandle;
    int32       SleepPointerNestCnt;

  public:
    DrawSettings  Settings;
    BRect         WindowBounds;
    BPoint        MeasureWinPos;
    int32         NumWindows;
    const MenuDef *MenuDefs;
    bool          NoDocLoaded;
    bool          MeasureWinOpen;

    static const int             NumMenus;
    static const int  * const    NumSubMenus;
    static const bool * const    RadioModeMenu;
    static const MenuDef * const DefaultMenu;

            ViewApplication();
    virtual ~ViewApplication();

    virtual void ArgvReceived(int32 argc, char **argv);
    virtual void RefsReceived(BMessage *msg);
    virtual bool QuitRequested();
    virtual void AboutRequested();
    virtual void CloseFilePanel();
    virtual void MessageReceived(BMessage *msg);

  private:
    bool ReadMenuDef(char *def, size_t len);

  public:
    void LaunchFilePanel(BWindow *win);
    void SetResolution(u_int PixelsPerInch);
    void SetSleepCursor();
    void SetNormalCursor();

    static void DisplayError(const char *msg);
};

BWindow *OpenWindow();
BWindow *OpenMeasureWindow(BPoint pos);

#endif
