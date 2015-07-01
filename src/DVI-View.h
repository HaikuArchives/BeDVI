////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//                                                                                                                //
// $Id: DVI-View.h,v 2.2 1998/08/20 11:16:10 achim Exp $
//                                                                                                                //
// BeDVI                                                                                                          //
// by Achim Blumensath                                                                                            //
// blume@corona.oche.de                                                                                           //
//                                                                                                                //
// This program is free software! It may be distributed according to the GNU Public License (see COPYING).        //
//                                                                                                                //
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

#ifndef VW_VIEW_H
#define VW_VIEW_H

#ifndef _VIEW_H
#include <View.h>
#endif
#ifndef DEFINES_H
#include "defines.h"
#endif
#ifndef DVI_H
#include "DVI.h"
#endif
#ifndef DOCVIEW_H
#include "DocView.h"
#endif

class DVIView: public DocView
{
  private:
    typedef DocView inherited;

    enum
    {
      BaseMagnifyWinSize = 20    // half size of the magnify window per 100 dpi resolution
    };

    sem_id     DocLock;
    DVI        *Document;

    sem_id     BufferLock;
    BBitmap    *Buffer;          // whole page
    BBitmap    *MagnifyBuffer;   // magnified part of the page
    BView      *BufferView;      // used to draw into the bitmaps

    u_int      PageNo;

    BPoint     MousePos;
    int        MagnifyWinSize;
    bool       ShowMagnify;

  public:
            DVIView(BRect frame, const char *Name);
    virtual ~DVIView();

    virtual void Draw(BRect updateRect);
    virtual void KeyDown(const char *bytes, int32 numBytes);
    virtual void MouseDown(BPoint where);
    virtual void MouseUp(BPoint where);
    virtual void MouseMoved(BPoint where, uint32 code, const BMessage *msg);
    virtual void MessageReceived(BMessage *msg);

    virtual void UpdateWindow();

    void PrintPage(u_int no);
    void SetDocument(DVI *doc);
    bool SetPage(u_int no);
    bool IncPage();
    bool DecPage();
    void SetShrinkFactor(u_short sf);
    void ToggleBorderLine();
    void ToggleAntiAliasing();
    void UpdateMenus();
    void RedrawBuffer();
    void RedrawMagnifyBuffer();
    bool DocumentChanged();
    void SetDspInfo(DisplayInfo *dsp);
    bool Reload();

    bool Ok()
    {
      return (DocLock    >= B_NO_ERROR) &&
             (BufferLock >= B_NO_ERROR) &&
             (BufferView != NULL);
    }

  friend class DVI;
  friend class Font;
  friend class FontInfo;
};

#endif
