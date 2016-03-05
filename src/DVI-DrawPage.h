////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//                                                                                                                //
// $Id: DVI-DrawPage.h,v 2.3 1999/07/25 13:24:19 achim Exp $
//                                                                                                                //
// BeDVI                                                                                                          //
// by Achim Blumensath                                                                                            //
// blume@corona.oche.de                                                                                           //
//                                                                                                                //
// This program is free software! It may be distributed according to the GNU Public License (see COPYING).        //
//                                                                                                                //
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

#ifndef DVI_DRAWPAGE_H
#define DVI_DRAWPAGE_H

#include <InterfaceKit.h>
#include <deque.h>
#include <stack.h>

#ifndef DVI_H
#include "DVI.h"
#endif

typedef void (*SetCharProc)(DrawPage *, wchar, wchar);

// interface to the PS interpreter

class PSInterface
{
  public:
    virtual void EndPage()                                           = 0;
    virtual void DrawBegin(const DrawPage *, int, int, const char *) = 0;
    virtual void DrawRaw(const char *)                               = 0;
    virtual void DrawFile(const char *)                              = 0;
    virtual void DrawEnd(const char *)                               = 0;
};

// information needed during the drawing of a page

class DrawPage
{
  private:
    class SearchStatus
    {
      private:
        DrawPage *dp;
        char     *SearchString;
        ssize_t  *Offset;
        BRect    *Rects;
        ssize_t  CurPosition;
        size_t   Length;

      public:
        bool     Found;

        SearchStatus(DrawPage *draw, const char *str);
        ~SearchStatus();

        SearchStatus &operator =(const SearchStatus &s);

        void MatchChar(char c, const BRect &rect);

      private:
        void DrawRects();
    };

    // current cursor position

    struct FrameData
    {
      long Horiz;
      long Vert;
      long w, x, y, z;
      int  PixelV;
    };

    typedef stack<FrameData, deque<FrameData, allocator<FrameData> > > FrameStack;

  public:
    DVI          *Document;
    BView        *vw;          // view to draw into
    DrawSettings Settings;

    // drawing state

    FrameData    Data;         // current position
    double       TPicConvert;
    double       DimConvert;
    int          DrawDir;      // drawing direction: 1: left-to-right; -1: right-to-left

  private:
    FrameStack   Frames;
    FrameData    *ScanFrame;

    SearchStatus SearchState;

    // font data

    Font        *CurFont;     // current font
    FontTable   *VirtTable;
    Font        *Virtual;
    wchar       MaxChar;
    SetCharProc SetChar;

    // I/O

    BPositionIO *File;        // DVI file
    uchar       *BufferPos;   // for buffered I/O
    uchar       *BufferEnd;

  public:
    static PSInterface *PSIface;

    DrawPage(const DrawSettings &set);

    void   InitPSIface();

  private:
    void   ChangeFont(ulong n);
    void   Special(long len);
    void   DrawPart();

    static void SetEmptyChar (DrawPage *dp, wchar cmd, wchar c);
    static void SetNoChar    (DrawPage *dp, wchar cmd, wchar c);
    static void SetNormalChar(DrawPage *dp, wchar cmd, wchar c);
    static void SetVFChar    (DrawPage *dp, wchar cmd, wchar c);

    void   DrawRule(long w, long h);

    uint32 ReadInt (ssize_t Size);
    int32  ReadSInt(ssize_t Size);
    void   ReadString(char *Buffer, size_t len);
    void   Skip(off_t off);
    bool   EndOfFile();

  public:
    bool StringFound()
    {
      return SearchState.Found;
    }

  friend class DVI;
  friend class Font;
  friend class SearchStatus;
};

PSInterface* InitPSInterface(DrawPage *dp);

#endif
