////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//                                                                                                                //
// $Id: DocView.cc,v 2.0 1997/10/12 10:21:14 achim Exp $
//                                                                                                                //
// BeDVI                                                                                                          //
// by Achim Blumensath                                                                                            //
// blume@corona.oche.de                                                                                           //
//                                                                                                                //
// This program is free software! It may be distributed according to the GNU Public License (see COPYING).        //
//                                                                                                                //
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

#include "DocView.h"

DocView::DocView(BRect rect, const char *Name):
  BView(rect, Name, B_FOLLOW_ALL_SIDES, B_WILL_DRAW | B_FRAME_EVENTS),
  DocWidth(0),
  DocHeight(0)
{}

DocView::~DocView() {}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//                                                                                                                //
// void DocView::FrameResized(float Width, float Height)                                                          //
//                                                                                                                //
// updates the scrollbars of the view.                                                                            //
//                                                                                                                //
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

void DocView::FrameResized(float /* Width */, float /* Height */)
{
  UpdateScrollBars();
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//                                                                                                                //
// void DocView::UpdateScrollBars()                                                                               //
//                                                                                                                //
// updates position and proportion of the scrollbars.                                                             //
//                                                                                                                //
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

void DocView::UpdateScrollBars()
{
  if(DocWidth <= 0)
  {
    ScrollBar(B_VERTICAL)->SetRange(0, 0);
    ScrollBar(B_HORIZONTAL)->SetRange(0, 0);
  }
  else
  {
    BRect  r;
    int    Width;
    int    Height;

    r = Bounds();

    Width  = r.right - r.left + 1;
    Height = r.bottom - r.top + 1;

    ScrollBar(B_VERTICAL)->  SetRange(0, DocHeight - Height);
    ScrollBar(B_HORIZONTAL)->SetRange(0, DocWidth  - Width);
    ScrollBar(B_VERTICAL)->  SetProportion((float)Height / DocHeight);
    ScrollBar(B_HORIZONTAL)->SetProportion((float)Width  / DocWidth);
  }
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//                                                                                                                //
// void DocView::UpdateWindowSize()                                                                               //
//                                                                                                                //
// updates size and size limits of the window.                                                                    //
//                                                                                                                //
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

void DocView::UpdateWindowSize()
{
  BRect r1, r2;
  int   WinWidth,  WinHeight;
  int   ViewWidth, ViewHeight;

  if(DocWidth > 0)
  {
    r1 = Window()->Bounds();
    r2 = Bounds();

    WinWidth   = r1.IntegerWidth()  + 1;
    WinHeight  = r1.IntegerHeight() + 1;
    ViewWidth  = r2.IntegerWidth()  + 1;
    ViewHeight = r2.IntegerHeight() + 1;

    Window()->SetSizeLimits(120.0, DocWidth  + WinWidth  - ViewWidth  - 1.0,
                            120.0, DocHeight + WinHeight - ViewHeight - 1.0);
    Window()->SetZoomLimits(DocWidth  + WinWidth  - ViewWidth  - 1.0,
                            DocHeight + WinHeight - ViewHeight - 1.0);

    Window()->GetSizeLimits(&r2.left, &r2.right, &r2.top, &r2.bottom);

    if(r2.right < r1.right || r2.bottom < r1.bottom)
      Window()->ResizeTo(min_c(r1.right, r2.right) - r1.left + 1.0, min_c(r1.bottom, r2.bottom) - r1.top + 1.0);
  }
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//                                                                                                                //
// void DocView::UpdateWindow()                                                                                   //
//                                                                                                                //
// updates the window completely.                                                                                 //
//                                                                                                                //
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

void DocView::UpdateWindow()
{
  if(Window()->Lock())
  {
    UpdateWindowSize();
    UpdateScrollBars();
    Invalidate();

    Window()->Unlock();
  }
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//                                                                                                                //
// void DocView::SetSize(int w, int h)                                                                            //
//                                                                                                                //
// sets the size of the document.                                                                                 //
//                                                                                                                //
// int w, h                  width and height of the document, 0 if no document                                   //
//                                                                                                                //
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

void DocView::SetSize(int w, int h)
{
  DocWidth  = w;
  DocHeight = h;

  UpdateWindow();
}
