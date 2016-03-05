////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//                                                                                                                //
// $Id: DVI-View.h,v 2.5 1999/07/22 13:36:38 achim Exp $
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

#include <View.h>

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

    // document

    sem_id       DocLock;
    DVI          *Document;

    // state

    DrawSettings Settings;
    string       SearchString;
    uint         PageNo;

    // bitmaps

    sem_id       BufferLock;
    BBitmap      *Buffer;          // whole page
    BBitmap      *MagnifyBuffer;   // magnified part of the page
    BView        *BufferView;      // used to draw into the bitmaps

    // Magnify-Window

    BPoint       MousePos;
    int          MagnifyWinSize;
    bool         ShowMagnify;

    // scripting

    static property_info PropertyList[];

  public:
            DVIView(BRect frame, const char *Name);
    virtual ~DVIView();

    virtual void     Draw(BRect updateRect);
    virtual void     KeyDown(const char *bytes, int32 numBytes);
    virtual void     MouseDown(BPoint where);
    virtual void     MouseUp(BPoint where);
    virtual void     MouseMoved(BPoint where, uint32 code, const BMessage *msg);
    virtual void     MessageReceived(BMessage *msg);
    virtual BHandler *ResolveSpecifier(BMessage *msg, int32 index, BMessage *specifier, int32 form, const char *property);
    virtual status_t GetSupportedSuites(BMessage *message);

    virtual void UpdateWindow();

    void SetDocument(DVI *doc, uint NewPageNo = 1);
    DVI  *UnsetDocument();
    bool SetPage(int no);
    void UpdateMenus();
    void UpdatePageCounter();

  private:
    void GetProperty(BMessage *msg);
    void SetProperty(BMessage *msg);

    void SetShrinkFactor(ushort sf);
    void ToggleBorderLine();
    void ToggleAntiAliasing();
    void RedrawBuffer();
    void RedrawMagnifyBuffer();
    bool DocumentChanged();
    void PrintPage(uint no);
    void SetDspInfo(DisplayInfo *dsp);

    static int32 ReloadThread(void *arg);

    bool Reload();
    void Search(const char *str, bool direction);
    void SavePage(BFile *File, uint32 Translator, uint32 Type);

  public:
    bool Ok()
    {
      return (DocLock    >= B_OK) &&
             (BufferLock >= B_OK) &&
             (BufferView != NULL);
    }

  friend class ViewWindow;
};

#endif
