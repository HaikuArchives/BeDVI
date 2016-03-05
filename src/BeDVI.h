////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//                                                                                                                //
// $Id: BeDVI.h,v 2.5 1998/10/11 13:54:59 achim Exp $
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

#include <Application.h>
#include <libprefs.h>
#include <list.h>
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

#define VERSION "V2.1"

// Messages:

static const uint32 MsgOpenPanel       = 'opnl';
static const uint32 MsgLoadPanel       = 'lpnl';
static const uint32 MsgSavePanel       = 'spnl';
static const uint32 MsgOpenFile        = 'open';
static const uint32 MsgLoadFile        = 'load';
static const uint32 MsgReload          = 'rlod';
static const uint32 MsgSavePage        = 'save';
static const uint32 MsgPrintPage       = 'prtp';
static const uint32 MsgAntiAliasing    = 'grey';
static const uint32 MsgBorderLine      = 'bord';
static const uint32 MsgMeasureWin      = 'mwin';
static const uint32 MsgSearchWin       = 'swin';
static const uint32 MsgSearchForwards  = 'fndf';
static const uint32 MsgSearchBackwards = 'fndb';
static const uint32 MsgNext            = 'next';
static const uint32 MsgPrev            = 'prev';
static const uint32 MsgFirst           = 'frst';
static const uint32 MsgLast            = 'last';
static const uint32 MsgNextWindow      = 'nwin';
static const uint32 MsgPrevWindow      = 'pwin';
static const uint32 MsgSaveSettings    = 'svst';
static const uint32 MsgPage            = 'page';
static const uint32 MsgResChanged      = 'resc';
static const uint32 MsgPoint           = 'pnt ';
static const uint32 MsgRedraw          = 'rdrw';

class ViewApplication: public BApplication
{
  private:
    typedef BApplication inherited;

    typedef list<BWindow *, allocator<BWindow *> > WindowList;


  public:
    struct MenuDef
    {
      char   *Name;
      char   Shortcut;
      bool   TargetIsWindow;
      uint32 Message;
    };

  private:
    WindowList    DocumentWindows;
    sem_id        WinListLock;
    BWindow       *MeasureWin;
    BFilePanel    *OpenPanel;
    entry_ref     PanelDirectory;
    PREFHandle    PrefHandle;
    int32         SleepPointerNestCnt;

  public:
    BWindow       *ActiveWindow;    // last activated document window
    BWindow       *SearchWin;
    DrawSettings  Settings;
    BRect         WindowBounds;
    BRect         SearchWinBounds;
    BPoint        MeasureWinPos;
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
    bool     ReadMenuDef(char *def, size_t len);
    void     LaunchFilePanel(const BMessage *msg, BWindow *win);

  public:
    void     SetResolution(uint PixelsPerInch);
    void     SetSleepCursor();
    void     SetNormalCursor();

    // access to window list

    void    AddDocWindow(BWindow *win);
    void    RemoveDocWindow(BWindow *win);
    BWindow *NextDocWindow(BWindow *win);
    BWindow *PrevDocWindow(BWindow *win);
    int     NumDocWindows();

    static void DisplayError(const char *msg);
};

BWindow *OpenWindow();
BWindow *OpenMeasureWindow(BPoint pos);
BWindow *OpenSearchWindow(BRect Bounds, BWindow *Target);

#endif
