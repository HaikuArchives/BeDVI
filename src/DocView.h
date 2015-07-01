////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//                                                                                                                //
// $Id: DocView.h,v 2.0 1997/10/12 10:21:16 achim Exp $
//                                                                                                                //
// BeDVI                                                                                                          //
// by Achim Blumensath                                                                                            //
// blume@corona.oche.de                                                                                           //
//                                                                                                                //
// This program is free software! It may be distributed according to the GNU Public License (see COPYING).        //
//                                                                                                                //
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

#ifndef DOCVIEW_H
#define DOCVIEW_H

#include <InterfaceKit.h>

class DocView: public BView
{
  public:
    int DocWidth;    // width and height of document
    int DocHeight;

            DocView(BRect rect, const char *Name);
    virtual ~DocView();
    virtual void FrameResized(float Width, float Height);
    virtual void UpdateScrollBars();
    virtual void UpdateWindowSize();
    virtual void UpdateWindow();
    virtual void SetSize(int w, int h);
};

#endif
