////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//                                                                                                                //
// $Id: DVI-Window.cc,v 2.3 1998/08/20 11:16:11 achim Exp $
//                                                                                                                //
// BeDVI                                                                                                          //
// by Achim Blumensath                                                                                            //
// blume@corona.oche.de                                                                                           //
//                                                                                                                //
// This program is free software! It may be distributed according to the GNU Public License (see COPYING).        //
//                                                                                                                //
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

#include <Application.h>
#include <InterfaceKit.h>
#include <stdio.h>
#include <string.h>
#include <Debug.h>
#include "BeDVI.h"
#include "DVI-View.h"
#include "log.h"

class ViewWindow: public BWindow
{
  private:
    typedef BWindow inherited;

    DVIView *vw;

  public:
            ViewWindow(BRect frame);
    virtual ~ViewWindow();

    virtual bool QuitRequested();
    virtual void MessageReceived(BMessage *msg);
    virtual void DispatchMessage(BMessage *message, BHandler *handler);
    virtual void WindowActivated(bool active);

  private:
    void         LoadFile(entry_ref *File);
    static int32 LoadFileThread(void *args);

  friend BWindow *OpenWindow();
};

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//                                                                                                                //
// ViewWindow::ViewWindow(BRect frame)                                                                            //
//                                                                                                                //
// Initializes a window (see the BeBook).                                                                         //                                                                                              //
//                                                                                                                //
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

ViewWindow::ViewWindow(BRect frame): BWindow(frame, "BeDVI " VERSION, B_DOCUMENT_WINDOW, 0)
{
  atomic_add(&((ViewApplication *)be_app)->NumWindows, 1);
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//                                                                                                                //
// ViewWindow::~ViewWindow()                                                                                      //
//                                                                                                                //
// Stores the window bounds.                                                                                      //                                                                                              //
//                                                                                                                //
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

ViewWindow::~ViewWindow()
{
  ((ViewApplication *)be_app)->WindowBounds = Frame();

  if(atomic_add(&((ViewApplication *)be_app)->NumWindows, -1) <= 1)
    be_app->PostMessage(B_QUIT_REQUESTED);
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//                                                                                                                //
// bool ViewWindow::QuitRequested()                                                                               //
//                                                                                                                //
// Closes the application (see the BeBook).                                                                       //
//                                                                                                                //
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

bool ViewWindow::QuitRequested()
{
  return true;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//                                                                                                                //
// void ViewWindow::MessageReceived(BMessage *msg)                                                                //
//                                                                                                                //
// Handles messages from the menu (see the BeBook).                                                               //
//                                                                                                                //
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

void ViewWindow::MessageReceived(BMessage *msg)
{
  try
  {
    log_debug("message received: '%c%c%c%c' (0x%08x)",
              ((char *)&msg->what)[0], ((char *)&msg->what)[1],
              ((char *)&msg->what)[2], ((char *)&msg->what)[3], msg->what);

    switch(msg->what)
    {
      case B_ABOUT_REQUESTED:
      case MsgOpenPanel:
      case MsgOpenFile:
      case MsgMeasureWin:
        be_app->PostMessage(msg);
        break;

      case MsgLoadPanel:
        ((ViewApplication *)be_app)->LaunchFilePanel(this);
        break;

      case MsgLoadFile:
      {
        entry_ref *File = new entry_ref;

        ((ViewApplication *)be_app)->CloseFilePanel();

        if(msg->HasRef("refs"))
          if(msg->FindRef("refs", File) >= B_NO_ERROR)
            LoadFile(File);
        break;
      }

      case MsgResChanged:
        vw->SetDspInfo(&((ViewApplication *)be_app)->Settings.DspInfo);
        break;

      case MsgRedraw:
        vw->Invalidate();
        break;

      case B_MOUSE_UP:
      case MsgReload:
      case MsgPrintPage:
      case MsgAntiAliasing:
      case MsgBorderLine:
      case MsgNext:
      case MsgPrev:
      case MsgFirst:
      case MsgLast:
        vw->MessageReceived(msg);
        break;

      case MsgPage:
      {
        u_long no = 1;

        Lock();

        sscanf(((BTextControl *)FindView("PageNumber"))->Text(), "%lu", &no);

        vw->SetPage(no);
        vw->MakeFocus(true);

        Unlock();
        break;
      }
      default:
        if(msg->what <= 15)                                             // shrink factor
          vw->SetShrinkFactor((u_short)msg->what);

        else if(msg->what < 0x10000)                                    // resolution
          ((ViewApplication *)be_app)->SetResolution(msg->what);

        else
          inherited::MessageReceived(msg);
        break;
    }
  }
  catch(const exception &e)
  {
    log_warn("%s!", e.what());
    log_debug("at %s:%d", __FILE__, __LINE__);
  }
  catch(...)
  {
    log_warn("unknown exception!");
    log_debug("at %s:%d", __FILE__, __LINE__);
  }
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//                                                                                                                //
// void ViewWindow::DispatchMessage(BMessage *message, BHandler *handler)                                         //
//                                                                                                                //
// Forwards B_MOUSE_UP messages to the view.                                                                      //
//                                                                                                                //
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

void ViewWindow::DispatchMessage(BMessage *message, BHandler *handler)
{
  if(message->what == B_MOUSE_UP)
    MessageReceived(message);
  else
    inherited::DispatchMessage(message, handler);
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//                                                                                                                //
// void ViewWindow::WindowActivated(bool active)                                                                  //
//                                                                                                                //
// redraws view if Measure-Window is open.                                                                        //
//                                                                                                                //
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

void ViewWindow::WindowActivated(bool active)
{
  if(((ViewApplication *)be_app)->MeasureWinOpen)
  {
    if(active)
    {
      BPoint p;
      uint32 b;

      vw->GetMouse(&p, &b, false);
      vw->MouseMoved(p, 0, NULL);
    }
    vw->Invalidate();
  }
  inherited::WindowActivated(active);
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//                                                                                                                //
// void ViewWindow::LoadFile(const entry_ref &File)                                                               //
//                                                                                                                //
// Loads a file and displays it.                                                                                  //
//                                                                                                                //
// entry_ref *File                      file to load, the entry_ref structure is freed by this function           //
//                                                                                                                //
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

void ViewWindow::LoadFile(entry_ref *File)
{
  thread_id tid;
  void      **args = NULL;

  try
  {
    log_info("loading file '%s'", File->name);

    args    = new void *[2];
    args[0] = this;
    args[1] = File;

    if((tid = spawn_thread(LoadFileThread, "load file", B_NORMAL_PRIORITY, args)) < B_OK)
    {
      delete args;
      return;
    }

    resume_thread(tid);
  }
  catch(const exception &e)
  {
    log_error("%s!", e.what());
    log_debug("at %s:%d", __FILE__, __LINE__);
    delete args;
  }
  catch(...)
  {
    log_error("unknown exception!");
    log_debug("at %s:%d", __FILE__, __LINE__);
    delete args;
  }
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//                                                                                                                //
// static int32 ViewWindow::LoadFileThread(void *args)                                                            //
//                                                                                                                //
// Loads a file and displays it.                                                                                  //
//                                                                                                                //
// void *args                           pointer to two pointers, the 1st one points to the window, the 2nd onw    //
//                                      to the entry_ref of the file. The entry_ref is freed by this function.    //
//                                                                                                                //
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

int32 ViewWindow::LoadFileThread(void *args)
{
  ViewWindow *win         = (ViewWindow *)((void **)args)[0];
  entry_ref  *File        = (entry_ref  *)((void **)args)[1];
  BFile      *f           = NULL;
  BNodeInfo  *ni          = NULL;
  DVI        *NewDocument = NULL;
  char       Buffer[B_MIME_TYPE_LENGTH + 16];

  try
  {
    f = new BFile(File, O_RDONLY);

    if(f->InitCheck() != B_OK)
      throw(exception("can't open file"));

    ((ViewApplication *)be_app)->SetSleepCursor();

    NewDocument = new DVI(f, &((ViewApplication *)be_app)->Settings, ((ViewApplication *)be_app)->DisplayError);

    f = NULL;        // f belongs to NewDocument now

    if(!NewDocument->Ok())
      throw(exception("can't load file"));

    // set icon if not already done
    try
    {
      f = new BFile(File, O_RDWR);

      if(f->InitCheck() == B_OK)
      {
        ni = new BNodeInfo(f);

        if(ni->GetType(Buffer) != B_OK                     ||
           strcmp(Buffer, "application/octet-stream") == 0 ||
           Buffer[0] == 0)
          ni->SetType("application/x-dvi");

        delete ni;
      }
      delete f;
    }
    catch(...)
    {
      delete f;
      delete ni;
    }
    f  = NULL;
    ni = NULL;

    win->vw->SetDocument(NewDocument);

    win->SetTitle(File->name);

    ((ViewApplication *)be_app)->NoDocLoaded = false;
  }
  catch(const exception &e)
  {
    log_error("%s!", e.what());
    log_debug("at %s:%d", __FILE__, __LINE__);

    delete NewDocument;
    delete f;
    delete ni;

    ((ViewApplication *)be_app)->DisplayError(e.what());
  }
  catch(...)
  {
    log_error("unknown exception!");
    log_debug("at %s:%d", __FILE__, __LINE__);

    delete NewDocument;
    delete f;
    delete ni;
  }
  delete File;
  delete args;

  ((ViewApplication *)be_app)->SetNormalCursor();

  return 0;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//                                                                                                                //
// static BMenuBar *InitMenu()                                                                                    //
//                                                                                                                //
// Creates the menu of a window.                                                                                  //
//                                                                                                                //
// Result:                              pointer to the menu or `NULL' if an error occures                         //
//                                                                                                                //
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

static BMenuBar *InitMenu()
{
  BMenuBar  *MenuBar = NULL;
  BMenu     *Menu    = NULL;
  BMenuItem *Item    = NULL;
  u_int     no = 0;
  u_int     i, j;

  try
  {
    if(!(MenuBar = new BMenuBar(BRect(0, 0, 0, 0), "Menu")))
      return NULL;

    for(i = 0; i < ((ViewApplication *)be_app)->NumMenus; i++)
    {
      if(!(Menu = new BMenu(((ViewApplication *)be_app)->MenuDefs[no++].Name)))
        return NULL;
      MenuBar->AddItem(Menu);

      for(j = 0; j < ((ViewApplication *)be_app)->NumSubMenus[i]; j++)
      {
        if(!(Item = new BMenuItem(((ViewApplication *)be_app)->MenuDefs[no].Name,
                                  new BMessage(((ViewApplication *)be_app)->MenuDefs[no].Message),
                                  ((ViewApplication *)be_app)->MenuDefs[no].Shortcut)))
          return NULL;

        Menu->AddItem(Item);
        no++;
      }
      if(((ViewApplication *)be_app)->RadioModeMenu[i])
        Menu->SetRadioMode(true);
    }
  }
  catch(const exception &e)
  {
    log_error("%s!", e.what());
    log_debug("at %s:%d", __FILE__, __LINE__);

    delete MenuBar;
    MenuBar = NULL;
  }
  catch(...)
  {
    log_error("unknown exception!");
    log_debug("at %s:%d", __FILE__, __LINE__);

    delete MenuBar;
    MenuBar = NULL;
  }
  return MenuBar;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//                                                                                                                //
// BWindow *OpenWindow()                                                                                          //
//                                                                                                                //
// Creates a window.                                                                                              //
//                                                                                                                //
// Result:                              pointer to the window or `NULL' if an error occures                       //
//                                                                                                                //
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

BWindow *OpenWindow()
{
  ViewWindow   *win;
  DVIView      *vw;
  BMenuBar     *Menu;
  BTextControl *PageNo;
  BScrollView  *sv;
  BRect        MenuR;
  BRect        WindowR;
  BRect        r;

  try
  {
    log_info("opening new window");

    // create window

    win = new ViewWindow(((ViewApplication *)be_app)->WindowBounds);

    win->SetSizeLimits(120.0, 1e7, 120.0, 1e7);

    // create menu

    if(!(Menu = InitMenu()))
    {
      win->Quit();
      return NULL;
    }

    win->AddChild(Menu);
    win->SetKeyMenuBar(Menu);

    // get bounds of window and menu

    WindowR = win->Bounds();
    MenuR   = Menu->Frame();

    // create DVI-View

    r.Set(B_ORIGIN.x,
          MenuR.bottom + 1.0,
          WindowR.right  - B_V_SCROLL_BAR_WIDTH,
          WindowR.bottom - B_H_SCROLL_BAR_HEIGHT);

    vw = new DVIView(r, "DVI-View");

    if(!vw->Ok())
    {
      delete vw;
      win->Quit();
      return NULL;
    }
    win->vw = vw;

    sv = new BScrollView("Document View", vw, B_FOLLOW_ALL_SIDES, 0, true, true);

    win->AddChild(sv);
    vw->MakeFocus(true);

    // create page counter

    MenuR = Menu->Bounds();

    MenuR.right  -= 2.0;
    MenuR.left   = MenuR.right - 3 * MenuR.Height();
    MenuR.top    += 1.0;
    MenuR.bottom -= 2.0;

    PageNo = new BTextControl(MenuR, "PageNumber", NULL, "1", new BMessage(MsgPage), B_FOLLOW_RIGHT | B_FOLLOW_TOP);

    PageNo->SetTarget(win);
    PageNo->SetAlignment(B_ALIGN_LEFT, B_ALIGN_RIGHT);
    PageNo->SetLowColor(219, 219, 219);
    PageNo->SetViewColor(219, 219, 219);

    BFont font(be_fixed_font);
    font.SetSize(MenuR.Height() - 5);
    PageNo->TextView()->SetFontAndColor(&font, B_FONT_ALL);

    Menu->AddChild(PageNo);

    r = PageNo->TextView()->Bounds();         // TextView isn't resized automatically, sigh...

    r.bottom -= PageNo->Bounds().bottom;
    r.right  -= PageNo->Bounds().right;

    PageNo->ResizeTo(MenuR.Width(), MenuR.Height());
    PageNo->TextView()->ResizeTo(MenuR.Width() + r.right, MenuR.Height() + r.bottom);
    PageNo->SetText("1");

    vw->UpdateMenus();

    win->Show();
  }
  catch(const exception &e)
  {
    log_error("%s!", e.what());
    log_debug("at %s:%d", __FILE__, __LINE__);

    if(win)
      win->Quit();
    win = NULL;
  }
  catch(...)
  {
    log_error("unknown exception!");
    log_debug("at %s:%d", __FILE__, __LINE__);

    if(win)
      win->Quit();
    win = NULL;
  }

  return win;
}
