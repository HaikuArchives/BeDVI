////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//                                                                                                                //
// $Id: MeasureWin.cc,v 2.3 1999/07/22 13:36:43 achim Exp $
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
#include <stdio.h>
#include "BeDVI.h"
#include "log.h"

class MeasureView: public BView
{
  private:
    typedef BView inherited;

  public:
    float       x, y;
    font_height plain_height;
    float       Width1, Width2;

    MeasureView(BRect frame, font_height *ph, float w1, float w2);

    virtual void Draw(BRect rect);
};

class MeasureWindow: public BWindow
{
  private:
    typedef BWindow inherited;

    MeasureView *vw;

  public:
            MeasureWindow(BRect frame, font_height *ph, float w1, float w2);
    virtual ~MeasureWindow();

    virtual void MessageReceived(BMessage *msg);
};

MeasureView::MeasureView(BRect frame, font_height *ph, float w1, float w2):
  BView(frame, NULL, B_FOLLOW_ALL_SIDES, B_WILL_DRAW),
  plain_height(*ph),
  Width1(w1),
  Width2(w2),
  x(0), y(0)
{
  SetFont(be_plain_font);
}

void MeasureView::Draw(BRect rect)
{
  float Height = plain_height.ascent + plain_height.descent + plain_height.leading;
  char  *Colon;
  char  str_x[16];
  char  str_y[16];

  log_debug("Draw(%f, %f, %f, %f)", rect.left, rect.top, rect.right, rect.bottom);
  
  MovePenTo(0,
            plain_height.leading + plain_height.ascent);
  DrawString(" x:");

  MovePenTo(Width1 + Width2,
            plain_height.leading + plain_height.ascent);
  DrawString(" y:");

  sprintf(str_x, "%.4f in", (double)x);
  sprintf(str_y, "%.4f in", (double)y);

  Colon = strchr(str_x, '.');
  MovePenTo(Width1 - be_plain_font->StringWidth(str_x, Colon - str_x),
            plain_height.leading + plain_height.ascent);
  DrawString(str_x);
  Colon = strchr(str_y, '.');
  MovePenTo(2 * Width1 + Width2 - be_plain_font->StringWidth(str_y, Colon - str_y),
            plain_height.leading + plain_height.ascent);
  DrawString(str_y);

  sprintf(str_x, "%.4f cm", (double)x * 2.54);
  sprintf(str_y, "%.4f cm", (double)y * 2.54);

  Colon = strchr(str_x, '.');
  MovePenTo(Width1 - be_plain_font->StringWidth(str_x, Colon - str_x),
            plain_height.leading + plain_height.ascent + Height);
  DrawString(str_x);
  Colon = strchr(str_y, '.');
  MovePenTo(2 * Width1 + Width2 - be_plain_font->StringWidth(str_y, Colon - str_y),
            plain_height.leading + plain_height.ascent + Height);
  DrawString(str_y);

  sprintf(str_x, "%.4f pt", (double)x * 72.0);
  sprintf(str_y, "%.4f pt", (double)y * 72.0);

  Colon = strchr(str_x, '.');
  MovePenTo(Width1 - be_plain_font->StringWidth(str_x, Colon - str_x),
            plain_height.leading + plain_height.ascent + 2 * Height);
  DrawString(str_x);
  Colon = strchr(str_y, '.');
  MovePenTo(2 * Width1 + Width2 - be_plain_font->StringWidth(str_y, Colon - str_y),
            plain_height.leading + plain_height.ascent + 2 * Height);
  DrawString(str_y);
}

MeasureWindow::MeasureWindow(BRect frame, font_height *ph, float w1, float w2):
  BWindow(frame,
          "Measure",
          B_FLOATING_WINDOW_LOOK,
          B_FLOATING_APP_WINDOW_FEEL,
          B_NOT_RESIZABLE | B_NOT_CLOSABLE | B_NOT_ZOOMABLE | B_AVOID_FOCUS | B_ASYNCHRONOUS_CONTROLS),
  vw(NULL)
{
  vw = new MeasureView(Bounds(), ph, w1, w2);

  AddChild(vw);
}

MeasureWindow::~MeasureWindow()
{
  ((ViewApplication *)be_app)->MeasureWinPos = Frame().LeftTop();
}

void MeasureWindow::MessageReceived(BMessage *msg)
{
  if (msg->what == MsgPoint)
  {
    float x;

    if (msg->FindFloat("x", &x) == B_OK)
      vw->x = x;
    if (msg->FindFloat("y", &x) == B_OK)
      vw->y = x;

    vw->Invalidate();
  }
  else
    inherited::MessageReceived(msg);
}

BWindow *OpenMeasureWindow(BPoint pos)
{
  MeasureWindow *mwin = NULL;

  try
  {
    font_height plain_height;
    float       Width1, Width2;
    BRect       r;

    be_plain_font->GetHeight(&plain_height);

    Width1 = be_plain_font->StringWidth(" x:-0000");
    Width2 = be_plain_font->StringWidth(".0000 cm0");

    r.left   = pos.x;
    r.top    = pos.y;
    r.right  = r.left + 2 * (Width1 + Width2);
    r.bottom = r.top  + 3 * (plain_height.ascent + plain_height.descent) + 4 * plain_height.leading;

    mwin = new MeasureWindow(r, &plain_height, Width1, Width2);
    mwin->Show();
  }
  catch(const exception &e)
  {
    log_error("%s!", e.what());
    log_debug("at %s:%d", __FILE__, __LINE__);

    if (mwin)
      mwin->Quit();
    mwin = NULL;
  }
  catch(...)
  {
    log_error("unknown exception!");
    log_debug("at %s:%d", __FILE__, __LINE__);

    if (mwin)
      mwin->Quit();
    mwin = NULL;
  }

  return mwin;
}
