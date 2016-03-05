////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//                                                                                                                //
// $Id: SearchWin.cc,v 2.2 1999/07/22 13:36:45 achim Exp $
//                                                                                                                //
// BeDVI                                                                                                          //
// by Achim Blumensath                                                                                            //
// blume@corona.oche.de                                                                                           //
//                                                                                                                //
// This program is free software! It may be distributed according to the GNU Public License (see COPYING).        //
//                                                                                                                //
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

#include <InterfaceKit.h>
#include <string.h>
#include "BeDVI.h"
#include "log.h"

class SearchWindow: public BWindow
{
  private:
    typedef BWindow inherited;

    BWindow      *Target;
    BTextControl *SearchText;

  public:
            SearchWindow(BWindow *target, BRect frame);
    virtual ~SearchWindow();

    virtual void MessageReceived(BMessage *msg);
};

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//                                                                                                                //
// SearchWindow::SearchWindow(BWindow *target, BRect frame)                                                       //
//                                                                                                                //
// creates the Search window.                                                                                     //
//                                                                                                                //
// BWindow *target                      window which should be searched                                           //
// BRect   frame                        frame of the search window                                                //
//                                                                                                                //
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

SearchWindow::SearchWindow(BWindow *target, BRect frame):
  BWindow(frame, "Search", B_TITLED_WINDOW_LOOK, B_NORMAL_WINDOW_FEEL, B_NOT_V_RESIZABLE | B_ASYNCHRONOUS_CONTROLS),
  Target(target),
  SearchText(NULL)
{
  BButton      *but;
  BView        *back;
  BRect        bounds;
  float        width;

  bounds = Bounds();

  back = new BView(bounds, "Background", B_FOLLOW_ALL_SIDES, B_WILL_DRAW);
  AddChild(back);
  back->SetViewColor(ui_color(B_PANEL_BACKGROUND_COLOR));

  but = new BButton(bounds, "Forwards", "Forwards", new BMessage(MsgSearchForwards), B_FOLLOW_LEFT | B_FOLLOW_BOTTOM);

  back->AddChild(but);
  but->ResizeToPreferred();

  frame = but->Frame();
  width = frame.Width();

  but->MoveTo(BPoint(1, bounds.bottom - frame.Height() - 2));

  but = new BButton(bounds, "Backwards", "Backwards", new BMessage(MsgSearchBackwards), B_FOLLOW_RIGHT | B_FOLLOW_BOTTOM);

  back->AddChild(but);
  but->ResizeToPreferred();

  frame = but->Frame();
  width += frame.Width();

  but->MoveTo(BPoint(bounds.right - frame.Width() - 2, bounds.bottom - frame.Height() - 2));

  if (bounds.Width() < width + 5)
  {
    ResizeTo(width + 5, bounds.Height());

    bounds.right = bounds.left + width + 5;
  }

  SetSizeLimits(width + 5, 1024, bounds.Height(), bounds.Height());

  bounds.top += 2;

  SearchText = new BTextControl(bounds, "Search Text", "Search:", NULL, new BMessage(MsgSearchForwards),
                                B_FOLLOW_LEFT_RIGHT | B_FOLLOW_TOP);
  back->AddChild(SearchText);
  SearchText->ResizeToPreferred();
  SearchText->ResizeTo(bounds.Width() - 3, SearchText->Frame().Height());
  SearchText->MakeFocus();
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//                                                                                                                //
// SearchWindow::~SearchWindow()                                                                                  //
//                                                                                                                //
// deletes the Search window.                                                                                     //
//                                                                                                                //
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

SearchWindow::~SearchWindow()
{
  ((ViewApplication *)be_app)->SearchWinBounds = Frame();
  ((ViewApplication *)be_app)->SearchWin       = NULL;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//                                                                                                                //
// void SearchWindow::MessageReceived(BMessage *msg)                                                              //
//                                                                                                                //
// processes messages to the Search window.                                                                       //
//                                                                                                                //
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

void SearchWindow::MessageReceived(BMessage *msg)
{
  if (msg->what == MsgSearchForwards || msg->what == MsgSearchBackwards)
  {
    msg->AddString("Search Text", SearchText->Text());

    Target->PostMessage(msg);
  }
  else
    inherited::MessageReceived(msg);
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//                                                                                                                //
// BWindow *OpenSearchWindow(BRect Bounds, BWindow *Target)                                                       //
//                                                                                                                //
// opens a Search window.                                                                                         //
//                                                                                                                //
// BRect   Bounds                       frame of the search window                                                //
// BWindow *Target                      window which should be searched                                           //
//                                                                                                                //
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

BWindow *OpenSearchWindow(BRect Bounds, BWindow *Target)
{
  SearchWindow *swin = NULL;

  try
  {
    font_height plain_height;

    be_plain_font->GetHeight(&plain_height);

    Bounds.bottom = Bounds.top  + 4 * (plain_height.ascent + plain_height.descent) + 5 * plain_height.leading;

    swin = new SearchWindow(Target, Bounds);
    swin->Show();
  }
  catch(const exception &e)
  {
    log_error("%s!", e.what());
    log_debug("at %s:%d", __FILE__, __LINE__);

    if (swin)
      swin->Quit();
    swin = NULL;
  }
  catch(...)
  {
    log_error("unknown exception!");
    log_debug("at %s:%d", __FILE__, __LINE__);

    if (swin)
      swin->Quit();
    swin = NULL;
  }

  return swin;
}
