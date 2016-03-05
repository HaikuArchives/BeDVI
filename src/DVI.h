////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//                                                                                                                //
// $Id: DVI.h,v 2.5 1999/07/22 13:36:39 achim Exp $
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
#include <deque>
#include <stack>
#if defined (__MWERKS__)
#include <string>
#endif

#ifndef DEFINES_H
#include "defines.h"
#endif
#ifndef FONTLIST_H
#include "FontList.h"
#endif

class BPositionIO;
class BView;
class DrawPage;
class DVIView;
class Font;

// resolution information

class DisplayInfo
{
  public:
    static uint       NumModes;
    static const char **ModeNames;
    static uint       *ModeDPI;

    const char *Mode;
    uint       PixelsPerInch;

    DisplayInfo();

    void SetModes(uint num, const char **names, uint *dpi);
    void SetResolution(uint dpi);

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
    ushort      ShrinkFactor;
    bool        AntiAliasing;
    bool        BorderLine;
    bool        StringFound;
    const char  *SearchString;

    DrawSettings():
      ShrinkFactor(3),
      AntiAliasing(true),
      BorderLine(false),
      StringFound(false),
      SearchString(NULL)
    {}

    DrawSettings &operator = (const DrawSettings &ds)
    {
      DspInfo          = ds.DspInfo;
      ShrinkFactor     = ds.ShrinkFactor;
      AntiAliasing     = ds.AntiAliasing;
      BorderLine       = ds.BorderLine;
      StringFound      = ds.StringFound;
      SearchString     = ds.SearchString;

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

// dvi document

class DVI
{
  public:
    enum
    {
      SetChar0  =   0,
      Set1      = 128,
      Set2      = 129,
      SetRule   = 132,
      Put1      = 133,
      Put2      = 134,
      PutRule   = 137,
      NOP       = 138,
      BeginOP   = 139,
      EndOP     = 140,
      Push      = 141,
      Pop       = 142,
      Right1    = 143,
      Right2    = 144,
      Right3    = 145,
      Right4    = 146,
      W0        = 147,
      W1        = 148,
      W2        = 149,
      W3        = 150,
      W4        = 151,
      X0        = 152,
      X1        = 153,
      X2        = 154,
      X3        = 155,
      X4        = 156,
      Down1     = 157,
      Down2     = 158,
      Down3     = 159,
      Down4     = 160,
      Y0        = 161,
      Y1        = 162,
      Y2        = 163,
      Y3        = 164,
      Y4        = 165,
      Z0        = 166,
      Z1        = 167,
      Z2        = 168,
      Z3        = 169,
      Z4        = 170,
      FontNum0  = 171,
      Font1     = 235,
      Font2     = 236,
      Font3     = 237,
      Font4     = 238,
      XXX1      = 239,
      XXX2      = 240,
      XXX3      = 241,
      XXX4      = 242,
      FontDef1  = 243,
      FontDef2  = 244,
      FontDef3  = 245,
      FontDef4  = 246,
      Preamble  = 247,
      Postamble = 248,
      PostPost  = 249,
      StartRefl = 250,
      EndRefl   = 251,
      Trailer   = 223
    };

  private:
    BPositionIO *DVIFile;
    char        *Name;
    int         OffsetX;
    int         OffsetY;
    uint        NumPages;
    ulong       *PageOffset;
    FontTable   Fonts;

  public:
    void         (*DisplayError)(const char *str);
    string       Directory;
    double       TPicConvert;
    double       DimConvert;
    long         Magnification;
    uint         UnshrunkPageWidth;
    uint         UnshrunkPageHeight;
    uint         PageWidth;
    uint         PageHeight;

  public:
    DVI(BPositionIO *File, DrawSettings *Settings, void (*DspError)(const char *) = NULL);
    ~DVI();

    bool Reload(DrawSettings *Settings);
    void Draw(BView *vw, DrawSettings *Settings, uint PageNo);
    int  MagStepValue(int PixelsPerInch, float &mag) const;

    bool Ok() const
    {
      return Fonts.Ok() && DVIFile != NULL && PageOffset != NULL;
    }

  friend class DVIView;
  friend class DrawPage;
};

#endif
