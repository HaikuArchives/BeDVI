////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//                                                                                                                //
// $Id: TeXFont.cc,v 2.6 1999/07/22 13:36:45 achim Exp $
//                                                                                                                //
// BeDVI                                                                                                          //
// by Achim Blumensath                                                                                            //
// blume@corona.oche.de                                                                                           //
//                                                                                                                //
// This program is free software! It may be distributed according to the GNU Public License (see COPYING).        //
// Some of the code in this file is based on xdvi. The original copyright follows:                                //
//                                                                                                                //
// Copyright (c) 1994 Paul Vojta.  All rights reserved.                                                           //
//                                                                                                                //
// Redistribution and use in source and binary forms, with or without                                             //
// modification, are permitted provided that the following conditions                                             //
// are met:                                                                                                       //
// 1. Redistributions of source code must retain the above copyright                                              //
//    notice, this list of conditions and the following disclaimer.                                               //
// 2. Redistributions in binary form must reproduce the above copyright                                           //
//    notice, this list of conditions and the following disclaimer in the                                         //
//    documentation and/or other materials provided with the distribution.                                        //
//                                                                                                                //
// THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND                                         //
// ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE                                          //
// IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE                                     //
// ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE                                        //
// FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL                                     //
// DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS                                        //
// OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)                                          //
// HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT                                     //
// LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY                                      //
// OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF                                         //
// SUCH DAMAGE.                                                                                                   //
//                                                                                                                //
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

#include <InterfaceKit.h>
#include "defines.h"
#include <stdio.h>
#include <string.h>
#include <syslog.h>
#include <Debug.h>

extern "C"
{
  #define string _string
  #include "kpathsea/c-auto.h"
  #include "kpathsea/tex-glyph.h"
  #undef string
}

#include "BeDVI.h"
#include "DVI-View.h"
#include "DVI-DrawPage.h"
#include "TeXFont.h"
#include "log.h"

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//                                                                                                                //
// Font::Font(const DVI *doc, const DrawSettings *Settings, const char *name, float size, long chksum,            //
//            int magstep, double dimconvert)                                                                     //
//                                                                                                                //
// Initializes a Font.                                                                                            //
//                                                                                                                //
// const DVI          *doc              document the font appears in                                              //
// const DrawSettings *Settings         settings
// const char         *name             name of the font                                                          //
// float              size              size of the font                                                          //
// long               chksum            checksum                                                                  //
// int                magstep           magnification                                                             //
// double             dimconvert        factor to convert dimensions                                              //
//                                                                                                                //
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

Font::Font(const DVI *doc, const DrawSettings *Settings, const char *name, float size, long chksum, int magstep,
           double dimconvert):
  File(NULL),
  Buffer(NULL),
  Name(NULL),
  Size(size),
  ChkSum(chksum),
  MagStep(magstep),
  DimConvert(dimconvert),
  UseCount(0),
  Loaded(false),
  Virtual(false),
  VFTable()
{
  if (name)
    if (Name = new char[strlen(name) + 1])
      strcpy(Name, name);

  if (!Load(doc, Settings))
    throw(runtime_error("can't load font"));
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//                                                                                                                //
// Font::~Font()                                                                                                  //
//                                                                                                                //
// Deletes a font.                                                                                                //
//                                                                                                                //
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

Font::~Font()
{
  delete File;
  delete [] Buffer;
  delete [] Name;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//                                                                                                                //
// bool Font::Open(char *&FontFound, int &SizeFound)                                                              //
//                                                                                                                //
// Opens the file of a font.                                                                                      //
//                                                                                                                //
// char *&FontFound                     used to return the name of the font found                                 //
// int  &SizeFound                      used to return the size of the font found                                 //
//                                                                                                                //
// Result:                              `true' if successful, otherwise `false'                                   //
//                                                                                                                //
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

bool Font::Open(char *&FontFound, int &SizeFound)
{
  BFile FontFile;
  char  *name;
  off_t FileSize;

  acquire_sem(kpse_sem);

  name = kpse_find_ovf(Name);

  if (!name)
    name = kpse_find_vf(Name);

  if (name)
  {
    SizeFound = Size;
    FontFound = NULL;
  }
  else
  {
    kpse_glyph_file_type FileResult;

    name = kpse_find_glyph(Name, (uint)(Size + 0.5), kpse_any_glyph_format, &FileResult);

    if (name)
    {
      if (FileResult.source == kpse_glyph_source_fallback)
        FontFound = FileResult.name;
      else
        FontFound = NULL;

      SizeFound = FileResult.dpi;
    }
  }
  release_sem(kpse_sem);

  if (!name)
  {
    log_warn("font '%s' not found!", Name);
    return false;
  }

  if (FontFile.SetTo(name, O_RDONLY) != B_OK ||
      FontFile.InitCheck()           != B_OK)
  {
    log_warn("can't open file!");
    return false;
  }

  if (FontFile.GetSize(&FileSize) != B_OK)
  {
    log_warn("can't read file!");
    return false;
  }

  delete File;
  delete [] Buffer;
  File   = NULL;
  Buffer = NULL;

  Buffer = new char[FileSize];

  if (FontFile.Read(Buffer, FileSize) < B_OK)
  {
    delete [] Buffer;
    Buffer = NULL;

    log_warn("can't read file!");
    return false;
  }

  File = new BMemoryIO(Buffer, FileSize);

  return true;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//                                                                                                                //
// void Font::ReallocFont(wchar num) throw(bad_alloc)                                                             //
//                                                                                                                //
// shrinks the arrays containing font data to their minimum size.                                                 //
//                                                                                                                //
// wchar num                            used length of array                                                      //
//                                                                                                                //
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

void Font::ReallocFont(wchar num) throw(bad_alloc)
{
  if (Virtual)
  {
    Macro *m;

    m = new Macro[num];

    if (Macros)
    {
      memcpy(m, Macros, num * sizeof(Macro));

      memset(Macros, 0, num * sizeof(Macro));
      delete [] Macros;
    }
    Macros = m;
  }
  else
  {
    Glyph *g;

    g = new Glyph[num];

    if (Glyphs)
    {
      memcpy(g, Glyphs, num * sizeof(Glyph));

      memset(Glyphs, 0, num * sizeof(Glyphs));
      delete [] Glyphs;
    }
    Glyphs = g;
  }
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//                                                                                                                //
// bool Font::Load(const DVI *doc, const DrawSettings *Settings)                                                  //
//                                                                                                                //
// Loads a font.                                                                                                  //
//                                                                                                                //
// const DVI          *doc              document the font appears in                                              //
// const DrawSettings *Settings         settings                                                                  //
//                                                                                                                //
// Result:                              `true' if successful, otherwise `false'                                   //
//                                                                                                                //
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

bool Font::Load(const DVI *doc, const DrawSettings *Settings)
{
  char *FontFound;
  int  SizeFound;
  uint Type;

  try
  {
    if (!Open(FontFound, SizeFound))
      return false;

    if (FontFound)
    {
      delete [] Name;
      Name = FontFound;
    }

    Size    = SizeFound;
    MaxChar = 255;
    SetChar = DrawPage::SetNormalChar;

    Type = ReadInt(File, 2);

    if (Type == Font::PK_Magic)
    {
      if (!ReadPKIndex(this))
        return false;
    }
    else if (Type == Font::GF_Magic)
    {
      if (!ReadGFIndex(this))
        return false;
    }
    else if (Type == Font::VF_Magic)
    {
      if (!ReadVFIndex(doc, Settings, this))
        return false;

      SetChar = DrawPage::SetVFChar;
    }
    else
    {
      log_warn("unknown font type!");
      return false;
    }

    if (!Virtual)
    {
      while (MaxChar > 0 && Glyphs[MaxChar].Addr == 0)
        MaxChar--;

      if (MaxChar < 255)
        ReallocFont(MaxChar + 1);
    }
/*
    if (Virtual)
      while (MaxChar > 0 && Macros[MaxChar].Position == NULL)
        MaxChar--;
    else
      while (MaxChar > 0 && Glyphs[MaxChar].Addr == 0)
        MaxChar--;

    if (MaxChar < 255)
      ReallocFont(MaxChar + 1);
*/
    Loaded = true;

    return true;
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

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//                                                                                                                //
// void Font::FlushShrinkedGlyphes()                                                                              //
//                                                                                                                //
// Frees all bitmaps of shrinked glyphes which is neccessary if the anti aliasing method is changed.              //
//                                                                                                                //
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

void Font::FlushShrinkedGlyphes()
{
  int i;

  for (i = 0; i < MaxChar; i++)
    if (Glyphs[i].SBitMap)
    {
      delete Glyphs[i].SBitMap;
      Glyphs[i].SBitMap = NULL;
    }
}


/* Glyph **********************************************************************************************************/


uchar *Glyph::ColourTable[MaxShrinkFactor + 1] = {};

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//                                                                                                                //
// Glyph::Glyph()                                                                                                 //
//                                                                                                                //
// Initializes a Glyph.                                                                                           //
//                                                                                                                //
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

Glyph::Glyph():
  Addr(0),
  Advance(0),
  Ux(0), Uy(0), UWidth(0), UHeight(0),
  UBitMap(NULL),
  Sx(0), Sy(0), SWidth(0), SHeight(0),
  SBitMap(NULL)
{
  static int32 TableInitialized = 0;           // record, if `ColourTable' is already initialized

  if (atomic_add(&TableInitialized, 1) < 1)
  {
    // initialize table

    BScreen scr;
    int     Colour;

    for (int Factor = 2; Factor < MaxShrinkFactor; Factor++)
    {
      try
      {
        ColourTable[Factor] = new uchar [Factor * Factor + 1];
      }
      catch(...)
      {
        log_error("not enough memory!");

        ColourTable[Factor] = NULL;
        atomic_add(&TableInitialized, -1);
        return;
      }

      for (int i = 0; i <= Factor * Factor; i++)
      {
        Colour = 255 - (510 * i + Factor * Factor) / (2 * Factor * Factor);
        ColourTable[Factor][i] = scr.IndexForColor(Colour, Colour, Colour);
      }
    }
  }
  else
    atomic_add(&TableInitialized, -1);
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//                                                                                                                //
// Glyph::~Glyph()                                                                                                //
//                                                                                                                //
// Deletes a Glyph.                                                                                               //
//                                                                                                                //
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

Glyph::~Glyph()
{
  delete UBitMap;
  delete SBitMap;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//                                                                                                                //
// bool Glyph::Shrink(int Factor)                                                                                 //
//                                                                                                                //
// shrinks a glyph.                                                                                               //
//                                                                                                                //
// int Factor                           factor the glyph should be shrinked                                       //
//                                                                                                                //
// Result:                              `true' if successful, otherwise `false'                                   //
//                                                                                                                //
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

bool Glyph::Shrink(int Factor, bool AntiAliasing)
{
  if (AntiAliasing)
    return ShrinkGrey(Factor);
  else
    return ShrinkMonochrome(Factor);
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//                                                                                                                //
// bool Glyph::ShrinkMonochrome(int Factor)                                                                       //
//                                                                                                                //
// shrinks a glyph without antialiasing.                                                                          //
//                                                                                                                //
// int Factor                           factor the glyph should be shrinked                                       //
//                                                                                                                //
// Result:                              `true' if successful, otherwise `false'                                   //
//                                                                                                                //
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

bool Glyph::ShrinkMonochrome(int Factor)
{
  BRect      r;
  int        ShrunkBytesWide;
  int        UnshrunkBytesWide;
  int        RowsLeft;
  int        Rows;
  int        ColsLeft;
  int        Cols;
  int        InitCols;
  BitmapUnit *OldPtr;
  BitmapUnit *NewPtr;
  BitmapUnit m;
  BitmapUnit *cp;
  int        MinSample = 0.4 * Factor * Factor;

  if (SBitMap)         // already shrunken?
    return true;

  try
  {
    Sx       = Ux / Factor;
    InitCols = Ux - Sx * Factor;

    if (InitCols <= 0)
      InitCols += Factor;
    else
      Sx++;

    Cols = Uy + 1;
    Sy   = Cols / Factor;
    Rows = Cols - Sy * Factor;

    if (Rows <= 0)
    {
      Rows += Factor;
      Sy--;
    }

    SWidth  = Sx + (UWidth  - Ux   + Factor - 1) / Factor;
    SHeight = Sy + (UHeight - Cols + Factor - 1) / Factor + 1;

    r.left   = 0;
    r.top    = 0;
    r.right  = ((SWidth + BITS_PER_UNIT - 1) & ~(BITS_PER_UNIT - 1)) - 1;
    r.bottom = SHeight - 1;

    if (!(SBitMap = new BBitmap(r, B_MONOCHROME_1_BIT)))
      return false;

    OldPtr = (BitmapUnit *)UBitMap->Bits();
    NewPtr = (BitmapUnit *)SBitMap->Bits();

    ShrunkBytesWide   = SBitMap->BytesPerRow();
    UnshrunkBytesWide = UBitMap->BytesPerRow();
    RowsLeft          = UHeight;

    memset(NewPtr, 0, SBitMap->BitsLength());

    while (RowsLeft)
    {
      if (Rows > RowsLeft)
        Rows = RowsLeft;

      ColsLeft = UWidth;
      cp       = NewPtr;
      Cols     = InitCols;

#ifdef MSB_FIRST
      m = (BitmapUnit) 1 << (BITS_PER_UNIT - 1);
#else
      m = 1;
#endif

      while (ColsLeft)
      {
        if (Cols > ColsLeft)
          Cols = ColsLeft;

        if (Sample(OldPtr, UnshrunkBytesWide, UWidth - ColsLeft, Cols, Rows) >= MinSample)
          *cp |= m;

#ifdef MSB_FIRST
        if (m == 1)
        {
          m = (BitmapUnit) 1 << (BITS_PER_UNIT - 1);
          cp++;
        }
        else
          m >>= 1;
#else
        if (m == (BitmapUnit) 1 << (BITS_PER_UNIT - 1))
        {
          m = 1;
          cp++;
        }
        else
          m <<= 1;
#endif

        ColsLeft -= Cols;
        Cols      = Factor;
      }
      NewPtr   =  BMU_ADD(NewPtr, ShrunkBytesWide);
      OldPtr   =  BMU_ADD(OldPtr, Rows * UnshrunkBytesWide);
      RowsLeft -= Rows;
      Rows     =  Factor;
    }

    Sy = Uy / Factor;

    return true;
  }
  catch(const exception &e)
  {
    log_warn("%s!", e.what());
    log_debug("at %s:%d", __FILE__, __LINE__);

    delete SBitMap;
    SBitMap = NULL;

    return false;
  }
  catch(...)
  {
    log_warn("unknown exception!");
    log_debug("at %s:%d", __FILE__, __LINE__);

    delete SBitMap;
    SBitMap = NULL;

    return false;
  }
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//                                                                                                                //
// bool Glyph::ShrinkGrey(int Factor)                                                                             //
//                                                                                                                //
// shrinks a glyph with antialiasing.                                                                             //
//                                                                                                                //
// int Factor                           factor the glyph should be shrinked                                       //
//                                                                                                                //
// Result:                              `true' if successful, otherwise `false'                                   //
//                                                                                                                //
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

bool Glyph::ShrinkGrey(int Factor)
{
  BRect      r;
  int        ShrunkBytesWide;
  int        UnshrunkBytesWide;
  int        RowsLeft;
  int        Rows;
  int        ColsLeft;
  int        Cols;
  int        InitCols;
  BitmapUnit *OldPtr;
  uint8      *NewPtr;
  uint8      *cp;

  if (SBitMap)         // already shrunken?
    return true;

  try
  {
    Sx       = Ux / Factor;
    InitCols = Ux - Sx * Factor;

    if (InitCols <= 0)
      InitCols += Factor;
    else
      Sx++;

    Cols = Uy + 1;
    Sy   = Cols / Factor;
    Rows = Cols - Sy * Factor;

    if (Rows <= 0)
    {
      Rows += Factor;
      Sy--;
    }

    SWidth  = Sx + (UWidth  - Ux   + Factor - 1) / Factor;
    SHeight = Sy + (UHeight - Cols + Factor - 1) / Factor + 1;

    r.left   = 0;
    r.top    = 0;
    r.right  = SWidth  - 1;
    r.bottom = SHeight - 1;

    if (!(SBitMap = new BBitmap(r, B_COLOR_8_BIT)))      // should be replaced by B_GRAYSCALE_8_BIT
      return false;

    OldPtr = (BitmapUnit *)UBitMap->Bits();
    NewPtr = (uint8 *)SBitMap->Bits();

    ShrunkBytesWide   = SBitMap->BytesPerRow();
    UnshrunkBytesWide = UBitMap->BytesPerRow();
    RowsLeft          = UHeight;

    memset(NewPtr, 255, SBitMap->BitsLength());

    while (RowsLeft)
    {
      if (Rows > RowsLeft)
        Rows = RowsLeft;

      ColsLeft = UWidth;
      Cols     = InitCols;
      cp       = NewPtr;

      while (ColsLeft)
      {
        if (Cols > ColsLeft)
          Cols = ColsLeft;

        *cp++ = ColourTable[Factor][Sample(OldPtr, UnshrunkBytesWide, UWidth - ColsLeft, Cols, Rows)];

        ColsLeft -= Cols;
        Cols      = Factor;
      }
      NewPtr   += ShrunkBytesWide;
      OldPtr   =  BMU_ADD(OldPtr, Rows * UnshrunkBytesWide);
      RowsLeft -= Rows;
      Rows     =  Factor;
    }

    Sy = Uy / Factor;

    return true;
  }
  catch(const exception &e)
  {
    log_warn("%s!", e.what());
    log_debug("at %s:%d", __FILE__, __LINE__);

    delete SBitMap;
    SBitMap = NULL;

    return false;
  }
  catch(...)
  {
    log_warn("unknown exception!");
    log_debug("at %s:%d", __FILE__, __LINE__);

    delete SBitMap;
    SBitMap = NULL;

    return false;
  }
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//                                                                                                                //
// int Glyph::Sample(BitmapUnit *Bits, int BytesPerRow, int BitSkip, int Width, int Height)                       //
//                                                                                                                //
// Counts the number of black pixels in a rectangle which should be shrinked to one pixel.                        //
//                                                                                                                //
// BitmapUnit *Bits                     pointer to the top row of the rectangle in the bitmap                     //
// int        BytesPerRow               bytes per row of the bitmap                                               //
// int        BitSkip                   left edge of the rectangle                                                //
// int        Width                     width of the rectangle                                                    //
// int        Height                    height of the rectangle                                                   //
//                                                                                                                //
// Result:                              number indicating color of the pixel                                      //
//                                                                                                                //
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

int Glyph::Sample(BitmapUnit *Bits, int BytesPerRow, int BitSkip, int Width, int Height)
{
  static const int SampleCount[] =
  {
    0, 1, 1, 2, 1, 2, 2, 3, 1, 2, 2, 3, 2, 3, 3, 4,
    1, 2, 2, 3, 2, 3, 3, 4, 2, 3, 3, 4, 3, 4, 4, 5,
    1, 2, 2, 3, 2, 3, 3, 4, 2, 3, 3, 4, 3, 4, 4, 5,
    2, 3, 3, 4, 3, 4, 4, 5, 3, 4, 4, 5, 4, 5, 5, 6
  };

  BitmapUnit *ptr;
  BitmapUnit *EndPtr;
  BitmapUnit *cp;
  int        BitsLeft;
  int        n;
  int        BitShift;
  int        wid;

  ptr      = Bits + BitSkip / BITS_PER_UNIT;
  EndPtr   = BMU_ADD(Bits, Height * BytesPerRow);
  BitsLeft = Width;
  n        = 0;

#ifdef MSB_FIRST
  BitShift = BITS_PER_UNIT - BitSkip % BITS_PER_UNIT;
#else
  BitShift = BitSkip % BITS_PER_UNIT;
#endif

  while (BitsLeft)
  {
#ifdef MSB_FIRST
    wid = BitShift;
#else
    wid = BITS_PER_UNIT - BitShift;
#endif

    if (wid > BitsLeft)
      wid = BitsLeft;

    if (wid > 6)
      wid = 6;

#ifdef MSB_FIRST
    BitShift -= wid;
#endif

    for (cp = ptr; cp < EndPtr; cp = BMU_ADD(cp, BytesPerRow))
      n += SampleCount[(*cp >> BitShift) & BitMasks[wid]];

#ifdef MSB_FIRST
    if (BitShift == 0)
    {
      BitShift = BITS_PER_UNIT;
      ptr++;
    }
#else
    BitShift += wid;
    if (BitShift == BITS_PER_UNIT)
    {
      BitShift = 0;
      ptr++;
    }
#endif

    BitsLeft -= wid;
  }
  return n;
}
