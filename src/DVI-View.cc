////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//                                                                                                                //
// $Id: DVI-View.cc,v 2.3 1998/08/20 11:16:08 achim Exp $
//                                                                                                                //
// BeDVI                                                                                                          //
// by Achim Blumensath                                                                                            //
// blume@corona.oche.de                                                                                           //
//                                                                                                                //
// This program is free software! It may be distributed according to the GNU Public License (see COPYING).        //
//                                                                                                                //
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

#include <InterfaceKit.h>
#include <stdio.h>
#include <string.h>
#include <Debug.h>
#include "BeDVI.h"
#include "DVI-View.h"
#include "TeXFont.h"
#include "log.h"

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//                                                                                                                //
// DVIView::DVIView(BRect rect, const char *Name)                                                                 //
//                                                                                                                //
// Initialises a DVI-View.                                                                                        //
//                                                                                                                //
// BRect      rect                      bounds of the view                                                        //
// const char *Name                     name of the view                                                          //
//                                                                                                                //
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

DVIView::DVIView(BRect rect, const char *Name):
  DocView(rect, Name),
  DocLock(B_ERROR),
  Document(NULL),
  BufferLock(B_ERROR),
  Buffer(NULL),
  BufferView(NULL),
  MagnifyBuffer(NULL),
  MagnifyWinSize(BaseMagnifyWinSize),
  ShowMagnify(false),
  PageNo(1)
{
  if((DocLock = create_sem(1, "document")) < B_NO_ERROR)
    throw(exception("can't create semaphore"));

  if((BufferLock = create_sem(1, "buffer")) < B_NO_ERROR)
    throw(exception("can't create semaphore"));

  BufferView = new BView(BRect(0, 0, 1, 1), NULL, 0, 0);

  SetViewColor(B_TRANSPARENT_32_BIT);
}


////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//                                                                                                                //
// DVIView::~DVIView()                                                                                            //
//                                                                                                                //
// Deletes a DVI-View.                                                                                            //
//                                                                                                                //
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

DVIView::~DVIView()
{
  if(DocLock >= B_NO_ERROR)
  {
    acquire_sem(DocLock);

    if(Document)
      ((ViewApplication *)be_app)->Settings = Document->Settings;

    delete Document;
    
    delete_sem(DocLock);
  }
  else
    delete Document;

  if(BufferLock >= B_NO_ERROR)
  {
    acquire_sem(BufferLock);

    if(Buffer)
      delete Buffer;
    else                    // deleted automatically if BBitmap is deleted
      delete BufferView;

    delete_sem(BufferLock);
  }
  else
  {
    if(Buffer)
      delete Buffer;
    else                    // deleted automatically if BBitmap is deleted
      delete BufferView;
  }
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//                                                                                                                //
// void DVIView::Draw(BRect r)                                                                                    //
//                                                                                                                //
// Draws the view (see the BeBook).                                                                               //
//                                                                                                                //
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

void DVIView::Draw(BRect r)
{
  if(IsPrinting())
  {
    PrintPage(PageNo);
    return;
  }

  if(Buffer != NULL && acquire_sem_etc(BufferLock, 1, B_TIMEOUT, 0.0) == B_OK)
  {
    DrawBitmapAsync(Buffer, r, r);

    if(ShowMagnify && MagnifyBuffer)
      DrawBitmapAsync(MagnifyBuffer, BPoint(MousePos.x - MagnifyWinSize, MousePos.y - MagnifyWinSize));

    release_sem(BufferLock);

    if(((ViewApplication *)be_app)->MeasureWinOpen && !ShowMagnify && Window()->IsActive())
    {
      SetDrawingMode(B_OP_INVERT);
      StrokeLine(BPoint(MousePos.x, 0), BPoint(MousePos.x, Document->PageHeight - 1));
      StrokeLine(BPoint(0, MousePos.y), BPoint(Document->PageWidth - 1, MousePos.y));
      SetDrawingMode(B_OP_COPY);    
    }
  }
  else
    FillRect(Bounds(), B_SOLID_LOW);
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//                                                                                                                //
// void DVIView::MessageReceived(BMessage *msg)                                                                   //
//                                                                                                                //
// Handles messages from the menu (see the BeBook).                                                               //
//                                                                                                                //
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

void DVIView::MessageReceived(BMessage *msg)
{
  if(msg->WasDropped() && msg->HasRef("refs"))
  {
    msg->what = MsgLoadFile;

    Window()->MessageReceived(msg);
  }
  else
    switch(msg->what)
    {
      case MsgReload:
        Reload();
        break;

      case MsgPrintPage:
        if(Document)
        {
          BPrintJob pj("page");

          pj.ConfigPage();
          pj.ConfigJob();
          pj.BeginJob();

          BPoint origin = pj.PrintableRect().LeftTop();
          origin.x = -origin.x;
          origin.y = -origin.y;

          pj.DrawView(this,
                      BRect(0, 0,
                            72.0 / Document->Settings.DspInfo.PixelsPerInch * Document->UnshrunkPageWidth  - 1,
                            72.0 / Document->Settings.DspInfo.PixelsPerInch * Document->UnshrunkPageHeight - 1),
                      origin);

          pj.SpoolPage();
          pj.CommitJob();
        }
        break;
      case MsgAntiAliasing:
        ToggleAntiAliasing();
        break;

      case MsgBorderLine:
        ToggleBorderLine();
        break;

      case MsgNext:
        IncPage();
        break;

      case MsgPrev:
        DecPage();
        break;

      case MsgFirst:
        SetPage(1);      
        break;

      case MsgLast:
        SetPage(0xffffffff);
        break;

      case B_MOUSE_UP:
      {
        BPoint p;

        if(msg->FindPoint("where", &p) == B_OK)
          MouseUp(p);
        break;
      }
      default:
        inherited::MessageReceived(msg);
        break;
    }
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//                                                                                                                //
// void DVIView::KeyDown(const char *bytes, int32 numBytes)                                                       //
//                                                                                                                //
// Handles key presses (see the BeBook).                                                                          //
//                                                                                                                //
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

void DVIView::KeyDown(const char *bytes, int32 numBytes)
{
  BRect  r;
  u_long w,h;
  float  MaxX, MaxY;
  float  Factor;

  if(!Document)
    return;

  r    = Bounds();
  w    = Document->PageWidth;
  h    = Document->PageHeight;
  MaxX = w - r.right  + r.left - 1.0;
  MaxY = h - r.bottom + r.top  - 1.0;

  Factor = 1.0 / 20.0;

  if(modifiers() & B_SHIFT_KEY)
    Factor *= 4.0;
  if(modifiers() & B_OPTION_KEY)
    Factor *= 0.25;

  switch(bytes[0])
  {
    case B_PAGE_DOWN:
    case B_SPACE:
      if(modifiers() & B_SHIFT_KEY)
        IncPage();
      else
      {
        if(r.top < MaxY)
          ScrollTo(r.left, min_c(r.bottom - 20.0, MaxY));
        else
        {
          if(IncPage())
            ScrollTo(r.left, 0.0);
        }
      }
      break;

    case B_PAGE_UP:
    case B_BACKSPACE:
      if(modifiers() & B_SHIFT_KEY)
        DecPage();
      else
      {
        if(r.top > 0.0)
          ScrollTo(r.left, max_c(2 * r.top - r.bottom + 20.0, 0.0));
        else
        {
          if(DecPage())
            ScrollTo(r.left, MaxY);
        }
      }
      break;

    case B_HOME:
      if(modifiers() & B_SHIFT_KEY)
        SetPage(1);
      else
        ScrollTo(r.left, 0.0);
      break;

    case B_END:
      if(modifiers() & B_SHIFT_KEY)
        SetPage(~0L);
      else
        ScrollTo(r.left, MaxY);
      break;

    case B_DOWN_ARROW:
    case B_RETURN:
      if(r.top < MaxY)
        ScrollTo(r.left, min_c(r.top + Factor * h, MaxY));
      else
      {
        if(IncPage())
          ScrollTo(r.left, 0.0);
      }
      break;

    case B_UP_ARROW:
      if(r.top > 0.0)
        ScrollTo(r.left, max_c(r.top - Factor * h, 0.0));
      else
      {
        if(DecPage())
          ScrollTo(r.left, MaxY);
      }
      break;

    case B_LEFT_ARROW:
      ScrollTo(max_c(r.left - Factor * w, 0.0), r.top);
      break;

    case B_RIGHT_ARROW:
      ScrollTo(min_c(r.left + Factor * w, MaxX), r.top);
      break;

    case '+':
      IncPage();
      break;

    case '-':
      DecPage();
      break;

    case '<':
      SetPage(1);
      break;

    case '>':
      SetPage(~0L);
      break;

    case '*':
    {
      BMessage msg(MsgNextWindow);

      msg.AddPointer("Window", Window());

      be_app->PostMessage(&msg);
      break;
    }

    case '_':
    {
      BMessage msg(MsgPrevWindow);

      msg.AddPointer("Window", Window());

      be_app->PostMessage(&msg);
      break;
    }

    case 'a':
    case 'A':
      ToggleAntiAliasing();
      break;

    case 'b':
    case 'B':
      ToggleBorderLine();
      break;

    case 'm':
    case 'M':
      be_app->PostMessage(MsgMeasureWin);
      break;

    case 'r':
    case 'R':
      Reload();
      break;

    case '1':
    case '2':
    case '3':
    case '4':
    case '5':
    case '6':
      SetShrinkFactor(bytes[0] - '0');
      break;

    case '7':
      SetShrinkFactor(9);
      break;

    case '8':
      SetShrinkFactor(12);
      break;

    default:
      inherited::KeyDown(bytes, numBytes);
      break;
  }
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//                                                                                                                //
// void DVIView::MouseDown(BPoint where)                                                                          //
//                                                                                                                //
// Opens a magnify window.                                                                                        //
//                                                                                                                //
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

void DVIView::MouseDown(BPoint where)
{
  if(!Document)
    return;

  if(((ViewApplication *)be_app)->MeasureWinOpen && !ShowMagnify && Window()->IsActive())
  {
    SetDrawingMode(B_OP_INVERT);
    StrokeLine(BPoint(MousePos.x, 0), BPoint(MousePos.x, Document->PageHeight - 1));
    StrokeLine(BPoint(0, MousePos.y), BPoint(Document->PageWidth - 1, MousePos.y));
    SetDrawingMode(B_OP_COPY);    
  }

  MousePos = where;

  if(!ShowMagnify && Document->Settings.ShrinkFactor != 1)
  {
    if(acquire_sem(DocLock) < B_NO_ERROR)
      return;

    ShowMagnify  = true;

    RedrawMagnifyBuffer();

    release_sem(DocLock);

    be_app->HideCursor();

    if(Window()->Lock())
    {
      Draw(BRect(MousePos.x - MagnifyWinSize, MousePos.y - MagnifyWinSize,
                 MousePos.x + MagnifyWinSize, MousePos.y + MagnifyWinSize));
      Window()->Unlock();
    }
  }
  if(((ViewApplication *)be_app)->MeasureWinOpen && Window()->IsActive())
  {
    BMessage msg(MsgPoint);

    msg.AddFloat("x", (double)MousePos.x * Document->Settings.ShrinkFactor / Document->Settings.DspInfo.PixelsPerInch);
    msg.AddFloat("y", (double)MousePos.y * Document->Settings.ShrinkFactor / Document->Settings.DspInfo.PixelsPerInch);

    be_app->PostMessage(&msg);

    if(!ShowMagnify)
    {
      SetDrawingMode(B_OP_INVERT);
      StrokeLine(BPoint(MousePos.x, 0), BPoint(MousePos.x, Document->PageHeight - 1));
      StrokeLine(BPoint(0, MousePos.y), BPoint(Document->PageWidth - 1, MousePos.y));
      SetDrawingMode(B_OP_COPY);    
    }
  }
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//                                                                                                                //
// void DVIView::MouseUp(BPoint where)                                                                            //
//                                                                                                                //
// Closes the magnify window.                                                                                     //
//                                                                                                                //
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

void DVIView::MouseUp(BPoint where)
{
  uint32 buttons;

  if(!Document)
    return;

  GetMouse(&where, &buttons, false);    // get currnet position

  if(((ViewApplication *)be_app)->MeasureWinOpen && !ShowMagnify && Window()->IsActive())
  {
    SetDrawingMode(B_OP_INVERT);
    StrokeLine(BPoint(MousePos.x, 0), BPoint(MousePos.x, Document->PageHeight - 1));
    StrokeLine(BPoint(0, MousePos.y), BPoint(Document->PageWidth - 1, MousePos.y));
    SetDrawingMode(B_OP_COPY);    
  }

  if(ShowMagnify)
  {
    ShowMagnify = false;

    be_app->ShowCursor();

    if(((ViewApplication *)be_app)->MeasureWinOpen)
      Invalidate();
    else
      if(Window()->Lock())
      {
        Draw(BRect(MousePos.x - MagnifyWinSize, MousePos.y - MagnifyWinSize,
                   MousePos.x + MagnifyWinSize, MousePos.y + MagnifyWinSize));
        Window()->Unlock();
      }
  }
  else if(((ViewApplication *)be_app)->MeasureWinOpen && Window()->IsActive())
  {
    SetDrawingMode(B_OP_INVERT);
    StrokeLine(BPoint(where.x, 0), BPoint(where.x, Document->PageHeight - 1));
    StrokeLine(BPoint(0, where.y), BPoint(Document->PageWidth - 1, where.y));
    SetDrawingMode(B_OP_COPY);    
  }

  MousePos = where;

  if(((ViewApplication *)be_app)->MeasureWinOpen && Window()->IsActive())
  {
    BMessage msg(MsgPoint);

    msg.AddFloat("x", (double)MousePos.x * Document->Settings.ShrinkFactor / Document->Settings.DspInfo.PixelsPerInch);
    msg.AddFloat("y", (double)MousePos.y * Document->Settings.ShrinkFactor / Document->Settings.DspInfo.PixelsPerInch);

    be_app->PostMessage(&msg);
  }
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//                                                                                                                //
// void DVIView::MouseMoved(BPoint where, uint32 code, const BMessage *msg)                                       //
//                                                                                                                //
// Moves the magnify window.                                                                                      //
//                                                                                                                //
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

void DVIView::MouseMoved(BPoint where, uint32 /* code */, const BMessage * /* msg */)
{
  uint32 buttons;

  if(!Document)
    return;

  if(Window()->MessageQueue()->FindMessage(B_MOUSE_MOVED, 0))      // process only the last B_MOUSE_MOVE message
    return;

  GetMouse(&where, &buttons, false);                               // get current position

  if(MousePos == where)
    return;

  if(buttons == 0)
  {
    MouseUp(where);
    return;
  }

  if(((ViewApplication *)be_app)->MeasureWinOpen &&
     Window()->IsActive())
  {
    BMessage msg(MsgPoint);

    msg.AddFloat("x", (double)where.x * Document->Settings.ShrinkFactor / Document->Settings.DspInfo.PixelsPerInch);
    msg.AddFloat("y", (double)where.y * Document->Settings.ShrinkFactor / Document->Settings.DspInfo.PixelsPerInch);

    be_app->PostMessage(&msg);

    if(!ShowMagnify)
    {
      SetDrawingMode(B_OP_INVERT);
      StrokeLine(BPoint(MousePos.x, 0), BPoint(MousePos.x, Document->PageHeight - 1));
      StrokeLine(BPoint(0, MousePos.y), BPoint(Document->PageWidth - 1, MousePos.y));
      SetDrawingMode(B_OP_COPY);    
    }
  }

  if(ShowMagnify)
  {
    BRect UpdateRect(MousePos.x, MousePos.y, MousePos.x, MousePos.y);

    MousePos = where;

    if(acquire_sem(DocLock) < B_NO_ERROR)
      return;

    RedrawMagnifyBuffer();

    release_sem(DocLock);

    if(UpdateRect.left   > MousePos.x)  UpdateRect.left   = MousePos.x;
    if(UpdateRect.top    > MousePos.y)  UpdateRect.top    = MousePos.y;
    if(UpdateRect.right  < MousePos.x)  UpdateRect.right  = MousePos.x;
    if(UpdateRect.bottom < MousePos.y)  UpdateRect.bottom = MousePos.y;

    UpdateRect.left   -= MagnifyWinSize;
    UpdateRect.top    -= MagnifyWinSize;
    UpdateRect.right  += MagnifyWinSize;
    UpdateRect.bottom += MagnifyWinSize;

    if(Window()->Lock())
    {
      Draw(UpdateRect);
      Window()->Unlock();
    }
  }
  else if(((ViewApplication *)be_app)->MeasureWinOpen && Window()->IsActive())
  {
    MousePos = where;

    SetDrawingMode(B_OP_INVERT);
    StrokeLine(BPoint(MousePos.x, 0), BPoint(MousePos.x, Document->PageHeight - 1));
    StrokeLine(BPoint(0, MousePos.y), BPoint(Document->PageWidth - 1, MousePos.y));
    SetDrawingMode(B_OP_COPY);    
  }
  else
    MousePos = where;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//                                                                                                                //
// void DVIView::SetDocument(DVI *doc)                                                                            //
//                                                                                                                //
// Sets the document to be displayed.                                                                             //
//                                                                                                                //
// DVI *doc                             document                                                                  //
//                                                                                                                //
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

void DVIView::SetDocument(DVI *doc)
{
  DVI   *OldDoc;
  u_int OldPageNo;

  ASSERT(doc != NULL);

  if(acquire_sem(DocLock) < B_NO_ERROR)
  {
    delete doc;
    return;
  }

  try
  {
    OldDoc    = Document;
    OldPageNo = PageNo;

    Document = doc;
    PageNo   = 1;

    if(DocumentChanged())
      delete OldDoc;

    else
    {
      delete Document;

      Document = OldDoc;
      PageNo   = OldPageNo;
    }
  }
  catch(const exception &e)
  {
    log_error("%s!", e.what());
    log_debug("at %s:%d", __FILE__, __LINE__);
  }
  catch(...)
  {
    log_error("unknown exception!");
    log_debug("at %s:%d", __FILE__, __LINE__);
  }

  release_sem(DocLock);

  UpdateWindow();
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//                                                                                                                //
// void DVIView::PrintPage(u_int no)                                                                              //
//                                                                                                                //
// Prints page number 'no'.                                                                                       //
//                                                                                                                //
// u_int no                             page number                                                               //
//                                                                                                                //
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

void DVIView::PrintPage(u_int no)
{
  if(!Document)
    return;

  if(acquire_sem(DocLock) < B_NO_ERROR)
    return;

  SetScale(72.0 / Document->Settings.DspInfo.PixelsPerInch);
  Document->DrawMagnify(this, no);
  Sync();

  release_sem(DocLock);
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//                                                                                                                //
// bool DVIView::SetPage(u_int no)                                                                                //
//                                                                                                                //
// Sets the displayed page of the document.                                                                       //
//                                                                                                                //
// u_int no                             page number                                                               //
//                                                                                                                //
// Result:                              `true' if the page number changed, `false' if it remains the same         //
//                                                                                                                //
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

bool DVIView::SetPage(u_int no)
{
  char str[10];

  log_info("page: %u", no);

  if(no != PageNo && Document)
  {
    if(acquire_sem(DocLock) < B_NO_ERROR)
      return false;

    if(no < 1)
      no = 1;
    if(no > Document->NumPages)
      no = Document->NumPages;

    if(PageNo != no)
    {
      PageNo = no;

      RedrawBuffer();
    }
    else
      no = 0;

    release_sem(DocLock);
  }
  else
    no = 0;

  if(Window()->Lock())
  {
    // update page counter

    sprintf(str, "%u", PageNo);
    ((BTextControl *)Window()->FindView("PageNumber"))->SetText(str);

    Invalidate();
    Window()->Unlock();
  }

  if(no > 0)
    return true;
  else
    return false;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//                                                                                                                //
// bool DVIView::IncPage()                                                                                        //
//                                                                                                                //
// Displays the next page.                                                                                        //
//                                                                                                                //
// Result:                              `true' if the page number changed, `false' if it remains the same         //
//                                                                                                                //
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

bool DVIView::IncPage()
{
  return SetPage(PageNo + 1);
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//                                                                                                                //
// bool DVIView::DecPage()                                                                                        //
//                                                                                                                //
// Displays the previous page.                                                                                    //
//                                                                                                                //
// Result:                              `true' if the page number changed, `false' if it remains the same         //
//                                                                                                                //
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

bool DVIView::DecPage()
{
  if(PageNo > 1)
    return SetPage(PageNo - 1);
  else
    return false;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//                                                                                                                //
// void DVIView::SetShrinkFactor(u_short sf)                                                                      //
//                                                                                                                //
// Draws or clears the lines indicating the border of the page.                                                   //
//                                                                                                                //
// u_short sf                           factor to shrink document                                                 //
//                                                                                                                //
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

void DVIView::SetShrinkFactor(u_short sf)
{
  if(Document)
  {
    if(acquire_sem(DocLock) < B_NO_ERROR)
      return;

    Document->Settings.SetShrinkFactor(sf);

    release_sem(DocLock);
  }
  else
    ((ViewApplication *)be_app)->Settings.SetShrinkFactor(sf);

  Reload();

  UpdateWindow();
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//                                                                                                                //
// void DVIView::ToggleBorderLine()                                                                               //
//                                                                                                                //
// Draws or clears the lines indicating the border of the page.                                                   //
//                                                                                                                //
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

void DVIView::ToggleBorderLine()
{
  DrawSettings *prefs;

  if(acquire_sem(DocLock) < B_NO_ERROR)
    return;

  if(Document)
    prefs = &Document->Settings;

  else
    prefs = &((ViewApplication *)be_app)->Settings;

  prefs->BorderLine = !prefs->BorderLine;

  ((BMenu *)Window()->FindView("Menu"))->FindItem(MsgBorderLine)->SetMarked(prefs->BorderLine);

  RedrawBuffer();

  release_sem(DocLock);

  if(Window()->Lock())
  {
    Invalidate();
    Window()->Unlock();
  }
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//                                                                                                                //
// void DVIView::ToggleAntiAliasing()                                                                             //
//                                                                                                                //
// Draws or clears the lines indicating the border of the page.                                                   //
//                                                                                                                //
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

void DVIView::ToggleAntiAliasing()
{
  DrawSettings *prefs;

  if(acquire_sem(DocLock) < B_NO_ERROR)
    return;

  if(Document)
    prefs = &Document->Settings;

  else
    prefs = &((ViewApplication *)be_app)->Settings;

  prefs->AntiAliasing = !prefs->AntiAliasing;

  ((BMenu *)Window()->FindView("Menu"))->FindItem(MsgAntiAliasing)->SetMarked(prefs->AntiAliasing);

  if(Document)
    Document->Fonts.FlushShrinkedGlyphes();

  RedrawBuffer();

  release_sem(DocLock);

  if(Window()->Lock())
  {
    Invalidate();
    Window()->Unlock();
  }
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//                                                                                                                //
// void DVIView::UpdateWindow()                                                                                   //
//                                                                                                                //
// Updates the window completely.                                                                                 //
//                                                                                                                //
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

void DVIView::UpdateWindow()
{
  if(Window()->Lock())
  {
    UpdateMenus();

    Window()->Unlock();
  }
  inherited::UpdateWindow();
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//                                                                                                                //
// void DVIView::UpdateMenus()                                                                                    //
//                                                                                                                //
// Updates the marked menu items.                                                                                 //
//                                                                                                                //
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

void DVIView::UpdateMenus()
{
  DrawSettings *prefs;
  BMenu        *Menu;
  int          i;

  if(Document)
    prefs = &Document->Settings;
  else
    prefs = &((ViewApplication *)be_app)->Settings;

  Menu = ((BMenu *)Window()->FindView("Menu"));

  for(i = 0; i < prefs->DspInfo.NumModes; i++)
    if(strcmp(prefs->DspInfo.Mode, prefs->DspInfo.ModeNames[i]) == 0)
    {
      Menu->FindItem(prefs->DspInfo.ModeDPI[i])->SetMarked(true);
      break;
    }

  Menu->FindItem(prefs->ShrinkFactor)->SetMarked(true);
  Menu->FindItem(MsgAntiAliasing)->SetMarked(prefs->AntiAliasing);
  Menu->FindItem(MsgBorderLine)->SetMarked(prefs->BorderLine);
  Menu->FindItem(MsgMeasureWin)->SetMarked(((ViewApplication *)be_app)->MeasureWinOpen);
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//                                                                                                                //
// void DVIView::RedrawBuffer()                                                                                   //
//                                                                                                                //
// Redraws the page in the internal buffer. This procedure should be called with `DocLock' locked.                //
//                                                                                                                //
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

void DVIView::RedrawBuffer()
{
  if(Document)
  {
    ((ViewApplication *)be_app)->SetSleepCursor();

    BufferView->ResizeTo(Document->PageWidth, Document->PageHeight);
    BufferView->ResizeTo(Document->PageWidth, Document->PageHeight);
    BufferView->MoveTo(0, 0);
    Buffer->AddChild(BufferView);

    if(BufferView->Window()->Lock())
    {
      if(acquire_sem(BufferLock) == B_NO_ERROR)
      {
        Document->Draw(BufferView, PageNo);

        release_sem(BufferLock);
      }

      BufferView->Window()->Unlock();
    }
    Buffer->RemoveChild(BufferView);

    ((ViewApplication *)be_app)->SetNormalCursor();
  }
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//                                                                                                                //
// void DVIView::RedrawMagnifyBuffer()                                                                            //
//                                                                                                                //
// Redraws the magnified part of the page in the internal buffer. This procedure should be called with `DocLock'  //
// locked.                                                                                                        //
//                                                                                                                //
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

void DVIView::RedrawMagnifyBuffer()
{
  rgb_color Black        = {0, 0, 0, 255};
  int       ShrinkFactor;

  if(!Document)
    return;

  if(acquire_sem(BufferLock) < B_OK)
    return;

  if(!MagnifyBuffer)                             // if bitmap isn't allocated yet
    if(!(MagnifyBuffer = new BBitmap(BRect(0, 0, 2 * MagnifyWinSize, 2 * MagnifyWinSize), B_COLOR_8_BIT, true)))
      return;

  ShrinkFactor = Document->Settings.ShrinkFactor;

  BufferView->ResizeTo(Document->UnshrunkPageWidth, Document->UnshrunkPageHeight);
  MagnifyBuffer->AddChild(BufferView);

  if(BufferView->Window()->Lock())
  {
    BufferView->MoveTo(-MousePos.x * ShrinkFactor + MagnifyWinSize,
                       -MousePos.y * ShrinkFactor + MagnifyWinSize);

    Document->DrawMagnify(BufferView, PageNo);

    BufferView->MoveTo(0, 0);

    BufferView->BeginLineArray(4);
    BufferView->AddLine(BPoint(0, 0),
                        BPoint(2 * MagnifyWinSize, 0),
                        Black);
    BufferView->AddLine(BPoint(2 * MagnifyWinSize, 0),
                        BPoint(2 * MagnifyWinSize, 2 * MagnifyWinSize),
                        Black);
    BufferView->AddLine(BPoint(2 * MagnifyWinSize, 2 * MagnifyWinSize),
                        BPoint(0, 2 * MagnifyWinSize),
                        Black);
    BufferView->AddLine(BPoint(0, 2 * MagnifyWinSize),
                        BPoint(0, 0),
                        Black);
    BufferView->EndLineArray();

    BufferView->Window()->Unlock();
  }
  MagnifyBuffer->RemoveChild(BufferView);

  release_sem(BufferLock);
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//                                                                                                                //
// void DVIView::SetDspInfo(DisplayInfo *dsp)                                                                     //
//                                                                                                                //
// Sets display data.                                                                                             //
//                                                                                                                //
// DisplayInfo *dsp                     display information                                                       //
//                                                                                                                //
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

void DVIView::SetDspInfo(DisplayInfo *dsp)
{
  ASSERT(dsp != NULL);

  if(Document)
  {
    if(acquire_sem(DocLock) < B_NO_ERROR)
      return;

    ((ViewApplication *)be_app)->SetSleepCursor();

    Document->SetDspInfo(dsp);

    DocumentChanged();

    ((ViewApplication *)be_app)->SetNormalCursor();

    release_sem(DocLock);
  }

  UpdateWindow();
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//                                                                                                                //
// bool DVIView::DocumentChanged()                                                                                //
//                                                                                                                //
// Updates the view when the document has changed. This procedure should be called with `DocLock' locked.         //
//                                                                                                                //
// Result:                              `true' if successful, otherwise `false'                                   //
//                                                                                                                //
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

bool DVIView::DocumentChanged()
{
  BRect r;

  // resize buffer if necessary

  if(Buffer)
    r = Buffer->Bounds();

  if(Buffer == NULL                                ||
     Document->PageWidth  != r.right  - r.left + 1 ||
     Document->PageHeight != r.bottom - r.top  + 1)
  {
    BBitmap *NewBuffer = NULL;

    try
    {
      if(!(NewBuffer = new BBitmap(BRect(0, 0, Document->PageWidth - 1, Document->PageHeight - 1), B_COLOR_8_BIT, true)))
        return false;

      delete Buffer;

      Buffer = NewBuffer;
    }
    catch(const exception &e)
    {
      log_warn("%s!", e.what());
      log_debug("at %s:%d", __FILE__, __LINE__);
      return false;
    }
    catch(...)
    {
      log_warn("unknown exception!");
      log_debug("at %s:%d", __FILE__, __LINE__);
      return false;
    }
  }

  RedrawBuffer();

  // resize magnify buffer if necessary

  MagnifyWinSize = BaseMagnifyWinSize * Document->Settings.DspInfo.PixelsPerInch / 100;

  if(MagnifyBuffer)
    delete MagnifyBuffer;
  MagnifyBuffer = NULL;

  SetSize(Document->PageWidth, Document->PageHeight);

  return true;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//                                                                                                                //
// bool DVIView::Reload()                                                                                         //
//                                                                                                                //
// Reloads the current document.                                                                                  //
//                                                                                                                //
// Result:                              `true' if successful, otherwise `false'                                   //
//                                                                                                                //
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

bool DVIView::Reload()
{
  bool success;

  if(!Document)
    return true;

  if(acquire_sem(DocLock) < B_NO_ERROR)
    return false;

  ((ViewApplication *)be_app)->SetSleepCursor();
 
  try
  {
    success = false;

    if(Document->Reload())
      success = DocumentChanged();
  }
  catch(const exception &e)
  {
    log_error("%s!", e.what());
    log_debug("at %s:%d", __FILE__, __LINE__);
  }
  catch(...)
  {
    log_error("unknown exception!");
    log_debug("at %s:%d", __FILE__, __LINE__);
  }

  ((ViewApplication *)be_app)->SetNormalCursor();

  release_sem(DocLock);

  return success;
}
