////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//                                                                                                                //
// $Id: TeXFont.h,v 2.6 1999/07/22 13:36:46 achim Exp $
//                                                                                                                //
// BeDVI                                                                                                          //
// by Achim Blumensath                                                                                            //
// blume@corona.oche.de                                                                                           //
//                                                                                                                //
// This program is free software! It may be distributed according to the GNU Public License (see COPYING).        //
//                                                                                                                //
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

#ifndef FONT_H
#define FONT_H

#ifndef _BITMAP_H
#include <interface/Bitmap.h>
#endif
#ifndef DEFINES_H
#include "defines.h"
#endif
#ifndef LIST_H
#include "list.h"
#endif
#ifndef DVI_DRAWPAGE_H
#include "DVI-DrawPage.h"
#endif
#ifndef FONTLIST_H
#include "FontList.h"
#endif

class Font;

static const uint32 BitMasks[33] =
{
  0x00000000,
  0x00000001, 0x00000003, 0x00000007, 0x0000000f, 
  0x0000001f, 0x0000003f, 0x0000007f, 0x000000ff, 
  0x000001ff, 0x000003ff, 0x000007ff, 0x00000fff, 
  0x00001fff, 0x00003fff, 0x00007fff, 0x0000ffff, 
  0x0001ffff, 0x0003ffff, 0x0007ffff, 0x000fffff, 
  0x001fffff, 0x003fffff, 0x007fffff, 0x00ffffff, 
  0x01ffffff, 0x03ffffff, 0x07ffffff, 0x0fffffff, 
  0x1fffffff, 0x3fffffff, 0x7fffffff, 0xffffffff 
};

typedef void (*ReadCharProc)(Font *, wchar);

class Glyph
{
  public:
    enum
    {
      MaxShrinkFactor = 15
    };

    long    Addr;
    long    Advance;
    short   Ux, Uy, UWidth, UHeight;         // unshrunken
    BBitmap *UBitMap;
    short   Sx, Sy, SWidth, SHeight;         // shrunken
    BBitmap *SBitMap;
    int     FlagByte;

  private:
    static uchar *ColourTable[MaxShrinkFactor + 1];

  public:
    Glyph();
    ~Glyph();

    bool Shrink(int Factor, bool AntiAliasing);

  private:
    bool ShrinkMonochrome(int Factor);
    bool ShrinkGrey(int Factor);
    int  Sample(BitmapUnit *Bits, int BytesPerRow, int BitSkip, int Widht, int Height);
};

class Macro
{
  public:
    uchar *Position;
    uchar *End;
    long  Advance;
    bool  FreeMe;
};

class Font
{
  public:
    enum
    {
      PK_Preamble = 247,
      PK_ID       = 89,
      PK_Magic    = (PK_Preamble << 8) | PK_ID,
      GF_Preamble = 247,
      GF_ID       = 131,
      GF_Magic    = (GF_Preamble << 8) | GF_ID,
      VF_Preamble = 247,
      VF_ID       = 202,
      VF_Magic    = (VF_Preamble << 8) | VF_ID
    };

    BPositionIO  *File;
    long         UseCount;
    char         *Name;      // name of the font
    float        Size;       // size in dpi
    long         ChkSum;     // checksum
    int          MagStep;    // 2*magstepnumber or `NoMagStep'
    double       DimConvert; // size conversion faktor
    wchar        MaxChar;    // largest character code
    uchar        Loaded:1;
    uchar        Virtual:1;
    SetCharProc  SetChar;    // procedure to set a character

    // Raster Fonts

    ReadCharProc ReadChar;
    Glyph        *Glyphs;

    // Virtual Fonts

    FontTable    VFTable;
    Font         *FirstFont;
    Macro        *Macros;

  private:
    char         *Buffer;    // buffer the font file is stored in

  public:
            Font(const DVI *doc, const DrawSettings *Settings, const char *name = NULL, float size = 0.0, long chksum = 0,
                 int magstep = 0, double dimconvert = 0.0);
    virtual ~Font();

    bool Load(const DVI *doc, const DrawSettings *Settings);
    void FlushShrinkedGlyphes();

  private:
    bool Open(char *&FontFound, int &SizeFound);
    void ReallocFont(wchar num) throw(bad_alloc);
};

bool ReadPKIndex(Font *f);
bool ReadGFIndex(Font *f);
bool ReadVFIndex(const DVI *doc, const DrawSettings *Settings, Font *f);

#endif
