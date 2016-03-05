////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//                                                                                                                //
// $Id: DVI-View.cc,v 2.6 1999/07/22 13:36:38 achim Exp $
//                                                                                                                //
// BeDVI                                                                                                          //
// by Achim Blumensath                                                                                            //
// blume@corona.oche.de                                                                                           //
//                                                                                                                //
// This program is free software! It may be distributed according to the GNU Public License (see COPYING).        //
//                                                                                                                //
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

#include <InterfaceKit.h>
#include <TranslationKit.h>
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
  Settings(((ViewApplication *)be_app)->Settings),
  BufferLock(B_ERROR),
  Buffer(NULL),
  BufferView(NULL),
  MagnifyBuffer(NULL),
  MagnifyWinSize(BaseMagnifyWinSize),
  ShowMagnify(false),
  PageNo(1)
{
  if ((DocLock = create_sem(1, "document")) < B_OK)
    throw(runtime_error("can't create semaphore"));

  if ((BufferLock = create_sem(1, "buffer")) < B_OK)
    throw(runtime_error("can't create semaphore"));

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
  if (DocLock >= B_OK)
  {
    acquire_sem(DocLock);

    delete Document;
    
    delete_sem(DocLock);
  }
  else
    delete Document;

  if (BufferLock >= B_OK)
  {
    acquire_sem(BufferLock);

    if (Buffer)
      delete Buffer;
    else                    // deleted automatically if BBitmap is deleted
      delete BufferView;

    delete_sem(BufferLock);
  }
  else
  {
    if (Buffer)
      delete Buffer;
    else                    // deleted automatically if BBitmap is deleted
      delete BufferView;
  }

  ((ViewApplication *)be_app)->Settings = Settings;
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
  if (IsPrinting())
  {
    PrintPage(PageNo);
    return;
  }

  if (Buffer != NULL && acquire_sem_etc(BufferLock, 1, B_TIMEOUT, 0) == B_OK)
  {
    DrawBitmapAsync(Buffer, r, r);

    if (ShowMagnify && MagnifyBuffer)
      DrawBitmapAsync(MagnifyBuffer, BPoint(MousePos.x - MagnifyWinSize, MousePos.y - MagnifyWinSize));

    release_sem(BufferLock);

    if (((ViewApplication *)be_app)->MeasureWinOpen && !ShowMagnify && Window()->IsActive())
    {
      SetDrawingMode(B_OP_INVERT);
      StrokeLine(BPoint(MousePos.x, 0), BPoint(MousePos.x, DocHeight - 1));
      StrokeLine(BPoint(0, MousePos.y), BPoint(DocWidth - 1, MousePos.y));
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
  if (msg->WasDropped() && msg->HasRef("refs"))
  {
    msg->what = MsgLoadFile;

    Window()->MessageReceived(msg);
  }
  else
    switch (msg->what)
    {
      case MsgReload:
        Reload();
        break;

      case MsgPrintPage:
        if (Document)
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
                            72.0 / Settings.DspInfo.PixelsPerInch * Document->UnshrunkPageWidth  - 1,
                            72.0 / Settings.DspInfo.PixelsPerInch * Document->UnshrunkPageHeight - 1),
                      origin);

          pj.SpoolPage();
          pj.CommitJob();
        }
        break;

      case MsgSearchForwards:
      case MsgSearchBackwards:
      {
        char *str;

        if (msg->FindString("Search Text", &str) != B_OK)
        {
          log_warn("invalide message received!");
          break;
        }

        Search(str, (msg->what == MsgSearchForwards));
        break;
      }
      case MsgAntiAliasing:
        ToggleAntiAliasing();
        break;

      case MsgBorderLine:
        ToggleBorderLine();
        break;

      case MsgNext:
        SetPage(PageNo + 1);
        break;

      case MsgPrev:
        SetPage(PageNo - 1);
        break;

      case MsgFirst:
        SetPage(1);      
        break;

      case MsgLast:
        SetPage(INT_MAX);
        break;

      case B_GET_PROPERTY:
        GetProperty(msg);
        break;

      case B_SET_PROPERTY:
        SetProperty(msg);
        break;

      case B_MOUSE_UP:
      {
        BPoint p;

        if (msg->FindPoint("where", &p) == B_OK)
          MouseUp(p);
        break;
      }
      default:
        inherited::MessageReceived(msg);
        break;
    }
}

// input

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//                                                                                                                //
// void DVIView::KeyDown(const char *bytes, int32 numBytes)                                                       //
//                                                                                                                //
// Handles key presses (see the BeBook).                                                                          //
//                                                                                                                //
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

void DVIView::KeyDown(const char *bytes, int32 numBytes)
{
  BRect r;
  ulong w,h;
  float MaxX, MaxY;
  float Factor;

  if (!Document)
    return;

  r    = Bounds();
  w    = DocWidth;
  h    = DocHeight;
  MaxX = w - r.right  + r.left - 1.0;
  MaxY = h - r.bottom + r.top  - 1.0;

  Factor = 1.0 / 20.0;

  if (modifiers() & B_SHIFT_KEY)
    Factor *= 4.0;
  if (modifiers() & B_OPTION_KEY)
    Factor *= 0.25;

  switch (bytes[0])
  {
    case B_PAGE_DOWN:
    case B_SPACE:
      if (modifiers() & B_SHIFT_KEY)
        SetPage(PageNo + 1);
      else
      {
        if (r.top < MaxY)
          ScrollTo(r.left, min_c(r.bottom - 20.0, MaxY));
        else
        {
          if (SetPage(PageNo + 1))
            ScrollTo(r.left, 0.0);
        }
      }
      break;

    case B_PAGE_UP:
    case B_BACKSPACE:
      if (modifiers() & B_SHIFT_KEY)
        SetPage(PageNo - 1);
      else
      {
        if (r.top > 0.0)
          ScrollTo(r.left, max_c(2 * r.top - r.bottom + 20.0, 0.0));
        else
        {
          if (SetPage(PageNo - 1))
            ScrollTo(r.left, MaxY);
        }
      }
      break;

    case B_HOME:
      if (modifiers() & B_SHIFT_KEY)
        SetPage(1);
      else
        ScrollTo(r.left, 0.0);
      break;

    case B_END:
      if (modifiers() & B_SHIFT_KEY)
        SetPage(INT_MAX);
      else
        ScrollTo(r.left, MaxY);
      break;

    case B_DOWN_ARROW:
    case B_RETURN:
      if (r.top < MaxY)
        ScrollTo(r.left, min_c(r.top + Factor * h, MaxY));
      else
      {
        if (SetPage(PageNo + 1))
          ScrollTo(r.left, 0.0);
      }
      break;

    case B_UP_ARROW:
      if (r.top > 0.0)
        ScrollTo(r.left, max_c(r.top - Factor * h, 0.0));
      else
      {
        if (SetPage(PageNo - 1))
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
      SetPage(PageNo + 1);
      break;

    case '-':
      SetPage(PageNo - 1);
      break;

    case '<':
      SetPage(1);
      break;

    case '>':
      SetPage(INT_MAX);
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
  if (!Document)
    return;

  if (((ViewApplication *)be_app)->MeasureWinOpen && !ShowMagnify && Window()->IsActive())
  {
    SetDrawingMode(B_OP_INVERT);
    StrokeLine(BPoint(MousePos.x, 0), BPoint(MousePos.x, DocHeight - 1));
    StrokeLine(BPoint(0, MousePos.y), BPoint(DocWidth - 1, MousePos.y));
    SetDrawingMode(B_OP_COPY);    
  }

  MousePos = where;

  if (!ShowMagnify && Settings.ShrinkFactor != 1)
  {
    if (acquire_sem(DocLock) < B_OK)
      return;

    ShowMagnify  = true;

    RedrawMagnifyBuffer();

    release_sem(DocLock);

    be_app->HideCursor();

    if (Window()->Lock())
    {
      Draw(BRect(MousePos.x - MagnifyWinSize, MousePos.y - MagnifyWinSize,
                 MousePos.x + MagnifyWinSize, MousePos.y + MagnifyWinSize));
      Window()->Unlock();
    }
  }
  if (((ViewApplication *)be_app)->MeasureWinOpen && Window()->IsActive())
  {
    BMessage msg(MsgPoint);

    msg.AddFloat("x", (double)MousePos.x * Settings.ShrinkFactor / Settings.DspInfo.PixelsPerInch);
    msg.AddFloat("y", (double)MousePos.y * Settings.ShrinkFactor / Settings.DspInfo.PixelsPerInch);

    be_app->PostMessage(&msg);

    if (!ShowMagnify)
    {
      SetDrawingMode(B_OP_INVERT);
      StrokeLine(BPoint(MousePos.x, 0), BPoint(MousePos.x, DocHeight - 1));
      StrokeLine(BPoint(0, MousePos.y), BPoint(DocWidth - 1, MousePos.y));
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

  if (!Document)
    return;

  if (Window()->CurrentMessage()->FindInt32("buttons", (int32 *)&buttons) != B_OK)
    return;

  if (((ViewApplication *)be_app)->MeasureWinOpen && !ShowMagnify && Window()->IsActive())
  {
    SetDrawingMode(B_OP_INVERT);
    StrokeLine(BPoint(MousePos.x, 0), BPoint(MousePos.x, DocHeight - 1));
    StrokeLine(BPoint(0, MousePos.y), BPoint(DocWidth - 1, MousePos.y));
    SetDrawingMode(B_OP_COPY);    
  }

  if (ShowMagnify)
  {
    ShowMagnify = false;

    be_app->ShowCursor();

    if (((ViewApplication *)be_app)->MeasureWinOpen)
      Invalidate();
    else
      if (LockLooper())
      {
        Draw(BRect(MousePos.x - MagnifyWinSize, MousePos.y - MagnifyWinSize,
                   MousePos.x + MagnifyWinSize, MousePos.y + MagnifyWinSize));
        UnlockLooper();
      }
  }
  else if (((ViewApplication *)be_app)->MeasureWinOpen && Window()->IsActive())
  {
    SetDrawingMode(B_OP_INVERT);
    StrokeLine(BPoint(where.x, 0), BPoint(where.x, DocHeight - 1));
    StrokeLine(BPoint(0, where.y), BPoint(DocWidth - 1, where.y));
    SetDrawingMode(B_OP_COPY);    
  }

  MousePos = where;

  if (((ViewApplication *)be_app)->MeasureWinOpen && Window()->IsActive())
  {
    BMessage msg(MsgPoint);

    msg.AddFloat("x", (double)MousePos.x * Settings.ShrinkFactor / Settings.DspInfo.PixelsPerInch);
    msg.AddFloat("y", (double)MousePos.y * Settings.ShrinkFactor / Settings.DspInfo.PixelsPerInch);

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

  if (!Document)
    return;

  if (Window()->MessageQueue()->FindMessage(B_MOUSE_MOVED, 0))      // process only the last B_MOUSE_MOVE message
    return;

  if (Window()->CurrentMessage()->FindInt32("buttons", (int32 *)&buttons) != B_OK)
    return;

  if (MousePos == where)
    return;

  if (buttons == 0)
  {
    MouseUp(where);
    return;
  }

  if (((ViewApplication *)be_app)->MeasureWinOpen &&
     Window()->IsActive())
  {
    BMessage msg(MsgPoint);

    msg.AddFloat("x", (double)where.x * Settings.ShrinkFactor / Settings.DspInfo.PixelsPerInch);
    msg.AddFloat("y", (double)where.y * Settings.ShrinkFactor / Settings.DspInfo.PixelsPerInch);

    be_app->PostMessage(&msg);

    if (!ShowMagnify)
    {
      SetDrawingMode(B_OP_INVERT);
      StrokeLine(BPoint(MousePos.x, 0), BPoint(MousePos.x, DocHeight - 1));
      StrokeLine(BPoint(0, MousePos.y), BPoint(DocWidth - 1, MousePos.y));
      SetDrawingMode(B_OP_COPY);    
    }
  }

  if (ShowMagnify)
  {
    BRect UpdateRect(MousePos.x, MousePos.y, MousePos.x, MousePos.y);

    MousePos = where;

    if (acquire_sem(DocLock) < B_OK)
      return;

    RedrawMagnifyBuffer();

    release_sem(DocLock);

    if (UpdateRect.left   > MousePos.x)  UpdateRect.left   = MousePos.x;
    if (UpdateRect.top    > MousePos.y)  UpdateRect.top    = MousePos.y;
    if (UpdateRect.right  < MousePos.x)  UpdateRect.right  = MousePos.x;
    if (UpdateRect.bottom < MousePos.y)  UpdateRect.bottom = MousePos.y;

    UpdateRect.left   -= MagnifyWinSize;
    UpdateRect.top    -= MagnifyWinSize;
    UpdateRect.right  += MagnifyWinSize;
    UpdateRect.bottom += MagnifyWinSize;

    if (LockLooper())
    {
      Draw(UpdateRect);
      UnlockLooper();
    }
  }
  else if (((ViewApplication *)be_app)->MeasureWinOpen && Window()->IsActive())
  {
    MousePos = where;

    SetDrawingMode(B_OP_INVERT);
    StrokeLine(BPoint(MousePos.x, 0), BPoint(MousePos.x, DocHeight - 1));
    StrokeLine(BPoint(0, MousePos.y), BPoint(DocWidth - 1, MousePos.y));
    SetDrawingMode(B_OP_COPY);    
  }
  else
    MousePos = where;
}

// scripting

// properties understood by a DVIView

property_info DVIView::PropertyList[] =
{
  {"Page",    {B_GET_PROPERTY, B_SET_PROPERTY, 0}, {B_DIRECT_SPECIFIER, 0}, "get or set displayed page",          0},
  {"IncPage", {B_SET_PROPERTY, 0},                 {B_DIRECT_SPECIFIER, 0}, "increment number of displayed page", 0},
  {"Shrink",  {B_GET_PROPERTY, B_SET_PROPERTY, 0}, {B_DIRECT_SPECIFIER, 0}, "get or set shrink factor",           0},
  0
};

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//                                                                                                                //
// BHandler *DVIView::ResolveSpecifier(BMessage *msg, int32 index, BMessage *specifier, int32 form,               //
//                                     const char *property)                                                      //
//                                                                                                                //
// Resolves scripting specifiers.                                                                                 //
//                                                                                                                //
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

BHandler *DVIView::ResolveSpecifier(BMessage *msg, int32 index, BMessage *specifier, int32 form, const char *property)
{
  if (strcmp(property, "Page")    == 0 ||
      strcmp(property, "IncPage") == 0 ||
      strcmp(property, "Shrink")  == 0)
    return this;

  return inherited::ResolveSpecifier(msg, index, specifier, form, property);
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//                                                                                                                //
// status_t DVIView::GetSupportedSuites(BMessage *message)                                                        //
//                                                                                                                //
// Returns the scripting suites supported by the view.                                                            //
//                                                                                                                //
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

status_t DVIView::GetSupportedSuites(BMessage *message)
{
  BPropertyInfo prop_info(PropertyList);

  message->AddString("suites",   "suite/vnd.blume-DVIView");
  message->AddFlat(  "messages", &prop_info);

  return inherited::GetSupportedSuites(message);
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//                                                                                                                //
// void DVIView::GetProperty(BMessage *msg)                                                                       //
//                                                                                                                //
// Handles B_GET_PROPERTY messages.                                                                               //
//                                                                                                                //
// BMessage *msg                        scripting message                                                         //
//                                                                                                                //
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

void DVIView::GetProperty(BMessage *msg)
{
  BMessage   Specifier;
  const char *Property;
  int32      Index;

  if (msg->GetCurrentSpecifier(&Index, &Specifier, NULL, &Property) != B_OK)
    return;

  if (strcmp(Property, "Page") == 0)
  {
    BMessage Reply(B_REPLY);
    status_t err;

    err = Reply.AddInt32("result", PageNo);

    Reply.AddInt32("error", err);

    msg->SendReply(&Reply);
  }
  else if (strcmp(Property, "Shrink") == 0)
  {
    BMessage Reply(B_REPLY);
    status_t err;

    err = Reply.AddInt32("result", Settings.ShrinkFactor);

    Reply.AddInt32("error", err);

    msg->SendReply(&Reply);
  }
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//                                                                                                                //
// void DVIView::SetProperty(BMessage *msg)                                                                       //
//                                                                                                                //
// Handles B_SET_PROPERTY messages.                                                                               //
//                                                                                                                //
// BMessage *msg                        scripting message                                                         //
//                                                                                                                //
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

void DVIView::SetProperty(BMessage *msg)
{
  BMessage   Specifier;
  const char *Property;
  int32      Index;

  if (msg->GetCurrentSpecifier(&Index, &Specifier, NULL, &Property) != B_OK)
    return;

  if (strcmp(Property, "Page") == 0)
  {
    if (msg->FindInt32("data", &Index) != B_OK)
    {
      log_warn("invalid message received!");
      return;
    }
    SetPage(Index);
  }
  else if (strcmp(Property, "IncPage") == 0)
  {
    if (msg->FindInt32("data", &Index) != B_OK)
    {
      log_warn("invalid message received!");
      return;
    }
    SetPage(PageNo + Index);
  }
  else if (strcmp(Property, "Shrink") == 0)
  {
    if (msg->FindInt32("data", &Index) != B_OK)
    {
      log_warn("invalid message received!");
      return;
    }
    SetShrinkFactor(Index);
  }
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//                                                                                                                //
// void DVIView::SetDocument(DVI *doc, uint NewPageNo = 1)                                                        //
//                                                                                                                //
// Sets the document to be displayed.                                                                             //
//                                                                                                                //
// DVI  *doc                            document                                                                  //
// uint NewPageNo                       page number                                                               //
//                                                                                                                //
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

void DVIView::SetDocument(DVI *doc, uint NewPageNo)
{
  DVI  *OldDoc;
  uint OldPageNo;

  if (acquire_sem(DocLock) < B_OK)
  {
    delete doc;
    return;
  }

  OldDoc    = Document;
  OldPageNo = PageNo;

  try
  {
    Document = doc;
    PageNo   = NewPageNo;

    if (DocumentChanged())
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

    delete Document;

    Document = OldDoc;
    PageNo   = OldPageNo;
  }
  catch(...)
  {
    log_error("unknown exception!");
    log_debug("at %s:%d", __FILE__, __LINE__);

    delete Document;

    Document = OldDoc;
    PageNo   = OldPageNo;
  }

  release_sem(DocLock);

  UpdateWindow();
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//                                                                                                                //
// DVI *DVIView::UnsetDocument()                                                                                  //
//                                                                                                                //
// removes the current document without deleting it.                                                              //
//                                                                                                                //
// Result:                              pointer to the document                                                   //
//                                                                                                                //
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

DVI *DVIView::UnsetDocument()
{
  DVI *doc;

  if (!Document)
    return NULL;

  if (acquire_sem(DocLock) < B_OK)
    return NULL;

  doc = Document;

  Document = NULL;

  release_sem(DocLock);

  UpdateWindow();

  return doc;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//                                                                                                                //
// void DVIView::PrintPage(uint no)                                                                               //
//                                                                                                                //
// Prints page number 'no'.                                                                                       //
//                                                                                                                //
// uint no                              page number                                                               //
//                                                                                                                //
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

void DVIView::PrintPage(uint no)
{
  DrawSettings set = Settings;

  set.ShrinkFactor = 1;

  if (!Document)
    return;

  if (acquire_sem(DocLock) < B_OK)
    return;

  SetScale(72.0 / set.DspInfo.PixelsPerInch);
  Document->Draw(this, &set, no);
  Sync();

  release_sem(DocLock);
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//                                                                                                                //
// bool DVIView::SetPage(int no)                                                                                  //
//                                                                                                                //
// Sets the displayed page of the document.                                                                       //
//                                                                                                                //
// int no                               page number                                                               //
//                                                                                                                //
// Result:                              `true' if the page number changed, `false' if it remains the same         //
//                                                                                                                //
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

bool DVIView::SetPage(int no)
{
  log_info("page: %u", no);

  if (no != PageNo && Document)
  {
    if (acquire_sem(DocLock) < B_OK)
      return false;

    if (no < 1)
      no = 1;
    if (no > Document->NumPages)
      no = Document->NumPages;

    if (PageNo != no)
    {
      PageNo = no;

      SearchString.erase();

      RedrawBuffer();
    }
    else
      no = 0;

    release_sem(DocLock);
  }
  else
    no = 0;

  if (LockLooper())
  {
    UpdatePageCounter();
    Invalidate();
    UnlockLooper();
  }

  return no > 0;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//                                                                                                                //
// void DVIView::SetShrinkFactor(ushort sf)                                                                       //
//                                                                                                                //
// Draws or clears the lines indicating the border of the page.                                                   //
//                                                                                                                //
// ushort sf                            factor to shrink document                                                 //
//                                                                                                                //
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

void DVIView::SetShrinkFactor(ushort sf)
{
  Settings.ShrinkFactor = sf;

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
  if (acquire_sem(DocLock) < B_OK)
    return;

  Settings.BorderLine = !Settings.BorderLine;

  ((BMenu *)Window()->FindView("Menu"))->FindItem(MsgBorderLine)->SetMarked(Settings.BorderLine);

  RedrawBuffer();

  release_sem(DocLock);

  if (LockLooper())
  {
    Invalidate();
    UnlockLooper();
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
  if (acquire_sem(DocLock) < B_OK)
    return;

  Settings.AntiAliasing = !Settings.AntiAliasing;

  ((BMenu *)Window()->FindView("Menu"))->FindItem(MsgAntiAliasing)->SetMarked(Settings.AntiAliasing);

  if (Document)
    Document->Fonts.FlushShrinkedGlyphes();

  RedrawBuffer();

  release_sem(DocLock);

  if (LockLooper())
  {
    Invalidate();
    UnlockLooper();
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
  if (LockLooper())
  {
    UpdateMenus();

    UnlockLooper();
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
  BMenu *Menu;
  int   i;

  Menu = ((BMenu *)Window()->FindView("Menu"));

  for (i = 0; i < Settings.DspInfo.NumModes; i++)
    if (strcmp(Settings.DspInfo.Mode, Settings.DspInfo.ModeNames[i]) == 0)
    {
      Menu->FindItem(Settings.DspInfo.ModeDPI[i])->SetMarked(true);
      break;
    }

  Menu->FindItem(Settings.ShrinkFactor)->SetMarked(true);
  Menu->FindItem(MsgAntiAliasing)->SetMarked(Settings.AntiAliasing);
  Menu->FindItem(MsgBorderLine)->SetMarked(Settings.BorderLine);
  Menu->FindItem(MsgMeasureWin)->SetMarked(((ViewApplication *)be_app)->MeasureWinOpen);
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//                                                                                                                //
// void DVIView::UpdatePageCounter()                                                                              //
//                                                                                                                //
// Updates the text control showing the current page number. The window should be locked.                         //
//                                                                                                                //
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

void DVIView::UpdatePageCounter()
{
  char str[10];

  sprintf(str, "%u", PageNo);
  ((BTextControl *)Window()->FindView("PageNumber"))->SetText(str);
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
  if (Document)
  {
    ((ViewApplication *)be_app)->SetSleepCursor();

    BufferView->ResizeTo(DocWidth, DocHeight);
    BufferView->MoveTo(0, 0);
    Buffer->AddChild(BufferView);

    if (BufferView->LockLooper())
    {
      if (acquire_sem(BufferLock) == B_OK)
      {
        Document->Draw(BufferView, &Settings, PageNo);

        release_sem(BufferLock);
      }

      BufferView->UnlockLooper();
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
  DrawSettings set   = Settings;
  rgb_color    Black = {0, 0, 0, 255};
  int          ShrinkFactor;

  if (!Document)
    return;

  if (acquire_sem(BufferLock) < B_OK)
    return;

  if (!MagnifyBuffer)                             // if bitmap isn't allocated yet
    if (!(MagnifyBuffer = new BBitmap(BRect(0, 0, 2 * MagnifyWinSize, 2 * MagnifyWinSize), B_COLOR_8_BIT, true)))
      return;

  ShrinkFactor     = Settings.ShrinkFactor;
  set.ShrinkFactor = 1;

  BufferView->ResizeTo(Document->UnshrunkPageWidth, Document->UnshrunkPageHeight);
  MagnifyBuffer->AddChild(BufferView);

  if (BufferView->LockLooper())
  {
    BufferView->MoveTo(-MousePos.x * ShrinkFactor + MagnifyWinSize,
                       -MousePos.y * ShrinkFactor + MagnifyWinSize);

    Document->Draw(BufferView, &set, PageNo);

    BufferView->MoveTo(0, 0);

    BufferView->BeginLineArray(4);
    BufferView->AddLine(BPoint(0, 0),                   BPoint(2 * MagnifyWinSize, 0),                   Black);
    BufferView->AddLine(BPoint(2 * MagnifyWinSize, 0),  BPoint(2 * MagnifyWinSize, 2 * MagnifyWinSize),  Black);
    BufferView->AddLine(BPoint(2 * MagnifyWinSize, 2 * MagnifyWinSize),  BPoint(0, 2 * MagnifyWinSize),  Black);
    BufferView->AddLine(BPoint(0, 2 * MagnifyWinSize),  BPoint(0, 0),                                    Black);
    BufferView->EndLineArray();

    BufferView->UnlockLooper();
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
  Settings.DspInfo = *dsp;

  Reload();
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

  if (!Document)
    return true;

  SetSize(Document->PageWidth, Document->PageHeight);

  // resize buffer if necessary

  if (Buffer)
    r = Buffer->Bounds();

  if (Buffer    == NULL                  ||
      DocWidth  != r.right  - r.left + 1 ||
      DocHeight != r.bottom - r.top  + 1)
  {
    BBitmap *NewBuffer = NULL;

    try
    {
      if (!(NewBuffer = new BBitmap(BRect(0, 0, DocWidth - 1, DocHeight - 1), B_COLOR_8_BIT, true)))
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

  MagnifyWinSize = BaseMagnifyWinSize * Settings.DspInfo.PixelsPerInch / 100;

  delete MagnifyBuffer;
  MagnifyBuffer = NULL;

  return true;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//                                                                                                                //
// int32 DVIView::ReloadThread(void *arg)                                                                         //
//                                                                                                                //
// Reloads the current document.                                                                                  //
//                                                                                                                //
// void *arg                            pointer to the view                                                       //
//                                                                                                                //
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

int32 DVIView::ReloadThread(void *arg)
{
  DVIView *vw = (DVIView *)arg;
  DVI     *doc;
  uint    PageNo;

  try
  {
    doc = vw->UnsetDocument();

    if (acquire_sem(vw->DocLock) < B_OK)
      return 0;
 
    ((ViewApplication *)be_app)->SetSleepCursor();

    PageNo = vw->PageNo;

    if (doc->Reload(&vw->Settings))
      vw->DocumentChanged();

    release_sem(vw->DocLock);

    vw->SetDocument(doc, PageNo);

    ((ViewApplication *)be_app)->SetNormalCursor();
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
  return 0;
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
  thread_id tid;

  if (!Document)
    return true;

  if ((tid = spawn_thread(ReloadThread, "reload file", B_NORMAL_PRIORITY, this)) < B_OK)
    return false;

  resume_thread(tid);

  return true;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//                                                                                                                //
// void DVIView::Search(const char *str, bool direction)                                                          //
//                                                                                                                //
// Searchs for the given string in the document.                                                                  //
//                                                                                                                //
// const char *str                      string to search for                                                      //
// bool       direction                 `true': search forwards; `false': search backwards                        //
//                                                                                                                //
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

void DVIView::Search(const char *str, bool direction)
{
  if (!Document)
    return;

  if (acquire_sem(DocLock) < B_OK)
    return;

  ((ViewApplication *)be_app)->SetSleepCursor();

  BufferView->ResizeTo(DocWidth, DocHeight);
  BufferView->MoveTo(0, 0);
  Buffer->AddChild(BufferView);

  if (BufferView->LockLooper())
  {
    if (acquire_sem(BufferLock) == B_OK)
    {
      DrawSettings set = Settings;
      int          increment;
      int          bound;
      uint         OldPageNo = PageNo;

      if (direction)
      {
        increment = 1;
        bound     = Document->NumPages;
      }
      else
      {
        increment = -1;
        bound     = 1;
      }

      set.SearchString = str;

      if (SearchString != str)                   // first search: start with this page
        SearchString = str;

      else                                       // else: start with next/previous page
        if (PageNo != bound)
          PageNo += increment;

      for (set.StringFound = false; !set.StringFound; PageNo += increment)
      {
        Document->Draw(BufferView, &set, PageNo);

        if (!set.StringFound && PageNo == bound)
          break;
      }
      if (set.StringFound)
        PageNo -= increment;

      else
      {
        PageNo = OldPageNo;

        set.SearchString = NULL;

        Document->Draw(BufferView, &set, PageNo);
      }

      release_sem(BufferLock);
    }

    BufferView->UnlockLooper();
  }
  Buffer->RemoveChild(BufferView);

  ((ViewApplication *)be_app)->SetNormalCursor();

  release_sem(DocLock);

  if (LockLooper())
  {
    UpdatePageCounter();
    Invalidate();
    UnlockLooper();
  }
}

static status_t SetFileType(BFile *File, int32 Translator, uint32 Type)
{
  const translation_format *formats;
  const char               *mime = NULL;
  int32                    count;
  status_t                 err;

  if ((err = BTranslatorRoster::Default()->GetOutputFormats(Translator, &formats, &count)) < B_OK)
    return err;

  for (int ix = 0; ix < count; ix++)
    if (formats[ix].type == Type)
    {
      mime = formats[ix].MIME;
      break;
    }

  if (mime == NULL)
    return B_ERROR;

  BNodeInfo ninfo(File);
  return ninfo.SetType(mime);
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//                                                                                                                //
// void DVIView::SavePage(BFile *File, uint32 Translator, uint32 Type)                                            //
//                                                                                                                //
// Write the current page to a file.                                                                              //
//                                                                                                                //
// BFile  *File                         file to write to                                                          //
// uint32 Translator                    translator which should be used                                           //
// uint32 Type                          format the data is written in                                             //
//                                                                                                                //
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

void DVIView::SavePage(BFile *File, uint32 Translator, uint32 Type)
{
  if (!Document)
    return;

  if (Buffer != NULL && acquire_sem(BufferLock) == B_OK)
  {
    BBitmapStream Page(Buffer);

    if (BTranslatorRoster::Default()->Translate(Translator, &Page, NULL, File, Type) == B_OK)
      SetFileType(File, Translator, Type);
    else
      log_error("couldn't translate bitmap!");

    BBitmap *bm;

    Page.DetachBitmap(&bm);     // we still need `Buffer', don't delete it

    release_sem(BufferLock);
  }
}
