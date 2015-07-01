////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//                                                                                                                //
// $Id: DVI.h,v 2.2 1998/07/09 13:36:26 achim Exp $
//                                                                                                                //
// BeDVI                                                                                                          //
// by Achim Blumensath                                                                                            //
// blume@corona.oche.de                                                                                           //
//                                                                                                                //
// This program is free software! It may be distributed according to the GNU Public License (see COPYING).        //
//                                                                                                                //
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

#ifndef DVI_H
#define DVI_H

#include <StorageKit.h>
#include <deque.h>
#include <stack.h>

#ifndef DEFINES_H
#include "defines.h"
#endif
#ifndef FONTLIST_H
#include "FontList.h"
#endif

class  BPositionIO;
class  BView;
struct DrawInfo;
class  DVIView;
class  Font;

typedef void (*SetCharProc)(DrawInfo *, wchar, wchar);

// resolution information

class DisplayInfo
{
  public:
    static u_int      NumModes;
    static const char **ModeNames;
    static u_int      *ModeDPI;

    const char *Mode;
    u_int      PixelsPerInch;

    DisplayInfo();

    void SetModes(u_int num, const char **names, u_int *dpi);
    void SetResolution(u_int dpi);

    DisplayInfo &operator = (const DisplayInfo &di)
    {
      Mode          = di.Mode;            // `Mode' always points to some string in `ModeNames'
      PixelsPerInch = di.PixelsPerInch;   // therefor we can just copy the pointer.

      return *this;
    }
};

// settings how to draw the document

class DrawSettings
{
  public:
    DisplayInfo DspInfo;
    u_short     ShrinkFactor;
    u_short     RealShrinkFactor;
    bool        AntiAliasing;
    bool        BorderLine;

    DrawSettings();

    void SetShrinkFactor(u_short sf);
    void SetMagnifyMode(bool OnOff);

    DrawSettings &operator = (const DrawSettings &ds)
    {
      DspInfo          = ds.DspInfo;
      ShrinkFactor     = ds.ShrinkFactor;
      RealShrinkFactor = ds.RealShrinkFactor;
      AntiAliasing     = ds.AntiAliasing;
      BorderLine       = ds.BorderLine;

      return *this;
    }

    long ToPixel(long x)
    {
      return (x + (ShrinkFactor << 16) - 1) / (ShrinkFactor << 16);
    }

    long PixelConv(long x)
    {
      return (x / ShrinkFactor) >> 16;
    }
};

// current cursor position

struct FrameData
{
  long Horiz;
  long Vert;
  long w, x, y, z;
  int  PixelV;
};

__MSL_FIX_ITERATORS__(FrameData);
__MSL_FIX_ITERATORS__(const FrameData);
__MSL_FIX_ITERATORS__(FrameData *);
__MSL_FIX_ITERATORS__(FrameData * const);
__MSL_FIX_ITERATORS__(const FrameData *);
__MSL_FIX_ITERATORS__(const FrameData * const);

// interface to the PS interpreter

class PSInterface
{
  public:
    virtual void EndPage()                               = 0;
    virtual void DrawBegin(DrawInfo *, int, int, char *) = 0;
    virtual void DrawRaw(char *)                         = 0;
    virtual void DrawFile(char *)                        = 0;
    virtual void DrawEnd(char *)                         = 0;
};

#ifdef __MWERKS__
  static const u_char SetChar0  =   0;
  static const u_char Set1      = 128;
  static const u_char Set2      = 129;
  static const u_char SetRule   = 132;
  static const u_char Put1      = 133;
  static const u_char Put2      = 134;
  static const u_char PutRule   = 137;
  static const u_char NOP       = 138;
  static const u_char BeginOP   = 139;
  static const u_char EndOP     = 140;
  static const u_char Push      = 141;
  static const u_char Pop       = 142;
  static const u_char Right1    = 143;
  static const u_char Right2    = 144;
  static const u_char Right3    = 145;
  static const u_char Right4    = 146;
  static const u_char W0        = 147;
  static const u_char W1        = 148;
  static const u_char W2        = 149;
  static const u_char W3        = 150;
  static const u_char W4        = 151;
  static const u_char X0        = 152;
  static const u_char X1        = 153;
  static const u_char X2        = 154;
  static const u_char X3        = 155;
  static const u_char X4        = 156;
  static const u_char Down1     = 157;
  static const u_char Down2     = 158;
  static const u_char Down3     = 159;
  static const u_char Down4     = 160;
  static const u_char Y0        = 161;
  static const u_char Y1        = 162;
  static const u_char Y2        = 163;
  static const u_char Y3        = 164;
  static const u_char Y4        = 165;
  static const u_char Z0        = 166;
  static const u_char Z1        = 167;
  static const u_char Z2        = 168;
  static const u_char Z3        = 169;
  static const u_char Z4        = 170;
  static const u_char FontNum0  = 171;
  static const u_char Font1     = 235;
  static const u_char Font2     = 236;
  static const u_char Font3     = 237;
  static const u_char Font4     = 238;
  static const u_char XXX1      = 239;
  static const u_char XXX2      = 240;
  static const u_char XXX3      = 241;
  static const u_char XXX4      = 242;
  static const u_char FontDef1  = 243;
  static const u_char FontDef2  = 244;
  static const u_char FontDef3  = 245;
  static const u_char FontDef4  = 246;
  static const u_char Preamble  = 247;
  static const u_char Postamble = 248;
  static const u_char PostPost  = 249;
  static const u_char StartRefl = 250;
  static const u_char EndRefl   = 251;
  static const u_char Trailer   = 223;
#endif

// dvi document

class DVI
{
  private:

#ifndef __MWERKS__
    static const u_char SetChar0  =   0;
    static const u_char Set1      = 128;
    static const u_char Set2      = 129;
    static const u_char SetRule   = 132;
    static const u_char Put1      = 133;
    static const u_char Put2      = 134;
    static const u_char PutRule   = 137;
    static const u_char NOP       = 138;
    static const u_char BeginOP   = 139;
    static const u_char EndOP     = 140;
    static const u_char Push      = 141;
    static const u_char Pop       = 142;
    static const u_char Right1    = 143;
    static const u_char Right2    = 144;
    static const u_char Right3    = 145;
    static const u_char Right4    = 146;
    static const u_char W0        = 147;
    static const u_char W1        = 148;
    static const u_char W2        = 149;
    static const u_char W3        = 150;
    static const u_char W4        = 151;
    static const u_char X0        = 152;
    static const u_char X1        = 153;
    static const u_char X2        = 154;
    static const u_char X3        = 155;
    static const u_char X4        = 156;
    static const u_char Down1     = 157;
    static const u_char Down2     = 158;
    static const u_char Down3     = 159;
    static const u_char Down4     = 160;
    static const u_char Y0        = 161;
    static const u_char Y1        = 162;
    static const u_char Y2        = 163;
    static const u_char Y3        = 164;
    static const u_char Y4        = 165;
    static const u_char Z0        = 166;
    static const u_char Z1        = 167;
    static const u_char Z2        = 168;
    static const u_char Z3        = 169;
    static const u_char Z4        = 170;
    static const u_char FontNum0  = 171;
    static const u_char Font1     = 235;
    static const u_char Font2     = 236;
    static const u_char Font3     = 237;
    static const u_char Font4     = 238;
    static const u_char XXX1      = 239;
    static const u_char XXX2      = 240;
    static const u_char XXX3      = 241;
    static const u_char XXX4      = 242;
    static const u_char FontDef1  = 243;
    static const u_char FontDef2  = 244;
    static const u_char FontDef3  = 245;
    static const u_char FontDef4  = 246;
    static const u_char Preamble  = 247;
    static const u_char Postamble = 248;
    static const u_char PostPost  = 249;
    static const u_char StartRefl = 250;
    static const u_char EndRefl   = 251;
    static const u_char Trailer   = 223;
#endif

    BPositionIO *DVIFile;
    char        *Name;
    int         OffsetX;
    int         OffsetY;
    u_int       NumPages;
    u_long      *PageOffset;
    FontTable   Fonts;

  public:
    void         (*DisplayError)(const char *str);
    DrawSettings Settings;
    double       TPicConvert;
    double       DimConvert;
    long         Magnification;
    u_int        UnshrunkPageWidth;
    u_int        UnshrunkPageHeight;
    u_int        PageWidth;
    u_int        PageHeight;

  private:
    void ChangeFont(DrawInfo *di, u_long n);
    void Special   (DrawInfo *di, long len);
    void DrawPart  (DrawInfo *di);

    static void SetEmptyChar(DrawInfo *di, wchar cmd, wchar c);
    static void SetNoChar   (DrawInfo *di, wchar cmd, wchar c);
    static void SetChar     (DrawInfo *di, wchar cmd, wchar c);
    static void SetVFChar   (DrawInfo *di, wchar cmd, wchar c);
    static void DrawRule    (DrawInfo *di, long w, long h);

  public:
    DVI(BPositionIO *File, DrawSettings *prefs, void (*DspError)(const char *) = NULL);
    ~DVI();

    bool Reload();
    void SetDspInfo(DisplayInfo *dsp);
    void Draw(BView *vw, u_int PageNo);
    void DrawMagnify(BView *vw, u_int PageNo);
    int  MagStepValue(float &mag);

    bool Ok()
    {
      return Fonts.Ok() && DVIFile != NULL && PageOffset != NULL;
    }

    friend class DVIView;
    friend class FontTable;
    friend class Font;
};

// information needed during the drawing of a page

class DrawInfo
{
  public:
    typedef stack<FrameData, deque<FrameData, allocator<FrameData> >, allocator<FrameData> > FrameStack;

    DVI         *Document;
    BView       *vw;          // view to draw into
    FrameData   Data;         // current position
    DisplayInfo *DspInfo;
    double      DimConvert;
    int         DrawDir;      // drawing direction: 1: left-to-right; -1: right-to-left
    Font        *CurFont;     // current font
    FontTable   *VirtTable;
    Font        *Virtual;
    wchar       MaxChar;
    SetCharProc SetChar;
    FrameStack  Frames;
    FrameData   *ScanFrame;
    BPositionIO *File;        // DVI file
    u_char      *BufferPos;   // for buffered I/O
    u_char      *BufferEnd;

    static PSInterface *PSIface;

    DrawInfo():
      Frames(allocator<FrameData>()),
      CurFont(NULL),
      VirtTable(NULL),
      Virtual(NULL),
      BufferPos(NULL),
      BufferEnd(NULL)
    {}

    void   InitPSIface();
    uint32 ReadInt (ssize_t Size);
    int32  ReadSInt(ssize_t Size);
    void   ReadString(char *Buffer, size_t len);
    void   Skip(off_t off);

    long PixelConv(long x)
    {
      return Document->Settings.PixelConv(x);
    }
};

PSInterface* InitPSInterface(DrawInfo *di);

#endif
