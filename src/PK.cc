////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//                                                                                                                //
// $Id: PK.cc,v 2.3 1998/08/20 11:16:22 achim Exp $
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

#include <string.h>
#include <stdio.h>
#include <Debug.h>
#include "BeDVI.h"
#include "TeXFont.h"
#include "log.h"

static const u_char PK_CmdStart = 240;
static const u_char PK_X1       = 240;
static const u_char PK_X2       = 241;
static const u_char PK_X3       = 242;
static const u_char PK_X4       = 243;
static const u_char PK_Y        = 244;
static const u_char PK_Post     = 245;
static const u_char PK_NOP      = 246;

// local class which contains data shared by the pk-parsing functions

class PKInfo
{
  private:
    Font  *f;
    int   FlagByte;
    u_int InputByte;
    int   BitPos;
    int   DynF;
    int   RepeatCount;

  public:
    PKInfo(Font *fnt): f(fnt), FlagByte(0), InputByte(0), BitPos(0), DynF(0), RepeatCount(0) {}

    bool ReadIndex(DVI *doc);
    void ReadChar(Font *f, wchar c);

  private:
    int  GetNybble();
    int  GetPackedNum();
    bool SkipSpecials();
};

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//                                                                                                                //
// void PKInfo::ReadChar(Font *f, wchar c)                                                                       //
//                                                                                                                //
// Reads a character from a font-file.                                                                            //
//                                                                                                                //
// Font  *f                             font                                                                      //
// wchar c                              character                                                                 //
//                                                                                                                //
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

void PKInfo::ReadChar(Font *f, wchar c)
{
  int        i, j;
  int        n;
  int        RowBitPos;
  bool       PaintSwitch;
  BitmapUnit *RowStart;
  BitmapUnit *cp;
  Glyph      *g;
  int32      FPWidth;
  u_long     Width, Height;
  BitmapUnit Word;
  int        WordWeight;
  int        UnitsWide;
  int        RowsLeft;
  int        HBit;
  int        Count;
 
  g = &f->Glyphs[c];

  if(g->UBitMap)
    return;

  FlagByte    = g->FlagByte;
  DynF        = FlagByte >> 4;
  PaintSwitch = ((FlagByte & 8) != 0);
  FlagByte   &= 0x7;

  f->File->Seek(g->Addr, SEEK_SET);

  if(FlagByte == 7)
    n = 4;
  else if(FlagByte > 3)
    n = 2;
  else
    n = 1;

  if(n != 4)
    FPWidth = ReadInt(f->File, 3);
  else
  {
    FPWidth = (long)ReadSInt(f->File, 4);
    ReadInt(f->File, 4);
  }
  ReadInt(f->File, n);

  Width  = ReadInt(f->File, n);
  Height = ReadInt(f->File, n);

  g->UBitMap = new BBitmap(
                     BRect(0.0, 0.0,
                           (float)((Width + BITS_PER_UNIT - 1) & ~(BITS_PER_UNIT - 1)) - 1.0,
                           (float)Height - 1.0),
                     B_MONOCHROME_1_BIT);

  g->Ux = ReadSInt(f->File, n);
  g->Uy = ReadSInt(f->File, n);

  g->Advance = f->DimConvert * FPWidth;

  if(!g->UBitMap)
    return;

  RowStart = (BitmapUnit *)g->UBitMap->Bits();
  cp       = RowStart;

  UnitsWide = g->UBitMap->BytesPerRow() / (BITS_PER_UNIT / 8);
  BitPos    = -1;

  if(DynF == 14)
  {
    bzero(g->UBitMap->Bits(), g->UBitMap->BitsLength());

    for(i = 0; i < Height; i++)
    {
      cp        = RowStart;
      RowStart += UnitsWide;

#ifdef MSB_FIRST
      RowBitPos = BITS_PER_UNIT;
#else
      RowBitPos = -1;
#endif

      for(j = 0; j < Width; j++)
      {
        if(--BitPos < 0)
        {
          Word   = ReadInt(f->File, 1);
          BitPos = 7;
        }
#ifdef MSB_FIRST
        if(--RowBitPos < 0)
        {
          cp++;
          RowBitPos = BITS_PER_UNIT - 1;
        }
#else
        if(++RowBitPos >= BITS_PER_UNIT)
        {
          cp++;
          RowBitPos = 0;
        }
#endif
        if(Word & (1 << BitPos))
          *cp |= 1 << RowBitPos;
      }
    }
  }
  else
  {
    RowsLeft    = Height;
    HBit        = Width;
    RepeatCount = 0;
    WordWeight  = BITS_PER_UNIT;
    Word        = 0;

    while(RowsLeft > 0)
    {
      Count = GetPackedNum();

      while(Count > 0)
      {
        if(Count < WordWeight && Count < HBit)
        {
#ifndef MSB_FIRST
          if(PaintSwitch)
            Word |= BitMasks[Count] << (BITS_PER_UNIT - WordWeight);
#endif
          HBit       -= Count;
          WordWeight -= Count;

#ifdef MSB_FIRST
          if(PaintSwitch)
            Word |= BitMasks[Count] << WordWeight;
#endif
          Count = 0;
        }
        else if(Count >= HBit && HBit <= WordWeight)
        {
          if(PaintSwitch)
#ifdef MSB_FIRST
            Word |= BitMasks[HBit] << (WordWeight - HBit);
#else
            Word |= BitMasks[HBit] << (BITS_PER_UNIT - WordWeight);
#endif
          *cp       = Word;
          RowStart += UnitsWide;
          cp        = RowStart;

          for(i = RepeatCount * UnitsWide; i > 0; i--)
          {
            *cp = *(cp - UnitsWide);
            cp++;
          }
          RowStart    = cp;
          RowsLeft   -= RepeatCount + 1;
          RepeatCount = 0;
          Word        = 0;
          WordWeight  = BITS_PER_UNIT;
          Count      -= HBit;
          HBit        = Width;
        }
        else
        {
          if(PaintSwitch)
#ifdef MSB_FIRST
            Word |= BitMasks[WordWeight];
#else
            Word |= BitMasks[WordWeight] << (BITS_PER_UNIT - WordWeight);
#endif
          *cp++      = Word;
          Word       = 0;
          Count     -= WordWeight;
          HBit      -= WordWeight;
          WordWeight = BITS_PER_UNIT;
        }
      }
      PaintSwitch = 1 - PaintSwitch;
    }
  }
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//                                                                                                                //
// int PKInfo::GetNybble()                                                                                        //
//                                                                                                                //
// Reads a nybble from a font-file.                                                                               //
//                                                                                                                //
// Result:                              nybble read                                                               //
//                                                                                                                //
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

int PKInfo::GetNybble()
{
  u_int temp;

  if(BitPos < 0)
  {
    InputByte = ReadInt(f->File, 1);
    BitPos    = 4;
  }
  temp    = InputByte >> BitPos;
  BitPos -= 4;

  return temp & 0xf;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//                                                                                                                //
// int PKInfo::GetPackedNum()                                                                                     //
//                                                                                                                //
// Reads a packed number from a font-file.                                                                        //
//                                                                                                                //
// Result:                              number read                                                               //
//                                                                                                                //
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

int PKInfo::GetPackedNum()
{
  int i, j;

  if((i = GetNybble()) == 0)
  {
    for(j = GetNybble(), i++; j == 0; i++)
      j = GetNybble();

    for(; i > 0; i--)
      j = (j << 4) | GetNybble();

    return (j - 15 + ((13 - DynF) << 4) + DynF);
  }
  else
  {
    if(i <= DynF)
      return i;

    if(i < 14)
      return (((i - DynF - 1) << 4) + GetNybble() + DynF + 1);

    if(i == 14)
      RepeatCount = GetPackedNum();
    else
      RepeatCount = 1;

    return GetPackedNum();
  }
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//                                                                                                                //
// bool PKInfo::SkipSpecials()                                                                                    //
//                                                                                                                //
// Skips special commands in a font-file.                                                                         //
//                                                                                                                //
// Result:                              `true' if successful, otherwise `false'                                   //
//                                                                                                                //
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

bool PKInfo::SkipSpecials()
{
  int i, j;

  do
  {
    FlagByte = ReadInt(f->File, 1);

    if(FlagByte >= PK_CmdStart)
    {
      switch(FlagByte)
      {
        case PK_X1:
        case PK_X2:
        case PK_X3:
        case PK_X4:
          for(i = 0, j = PK_CmdStart; j <= FlagByte; j++)
            i = (i << 8) | ReadInt(f->File, 1);

          f->File->Seek(i, SEEK_CUR);
          break;

        case PK_Y:
          ReadInt(f->File, 4);
          break;

        case PK_Post:
        case PK_NOP:
          break;

        default:
          return false;
      }
    }
  }
  while(FlagByte != PK_Post && FlagByte >= PK_CmdStart);

  return true;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//                                                                                                                //
// static void ReadChar(Font *f, wchar c)                                                                         //
//                                                                                                                //
// Reads a character from a font-file.                                                                            //
//                                                                                                                //
// Font  *f                             font                                                                      //
// wchar c                              character                                                                 //
//                                                                                                                //
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

static void ReadChar(Font *f, wchar c)
{
  PKInfo info(f);

  try
  {
    info.ReadChar(f, c);
  }
  catch(const exception &e)
  {
    log_warn("%s!", e.what());
    log_debug("at %s:%d", __FILE__, __LINE__);
  }
  catch(...)
  {
    log_warn("unknown exception!");
    log_debug("at %s:%d", __FILE__, __LINE__);
  }
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//                                                                                                                //
// bool PKInfo::ReadIndex(DVI *doc)                                                                               //
//                                                                                                                //
// Reads general information from a font-file.                                                                    //
//                                                                                                                //
// DVI *doc                             document the font appears in                                              //
//                                                                                                                //
// Result:                              `true' if successful, otherwise `false'                                   //
//                                                                                                                //
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

bool PKInfo::ReadIndex(DVI * /* doc */)
{
  u_long chksum;
  int32  hppp, vppp;

  f->ReadChar = ::ReadChar;

  f->File->Seek(ReadInt(f->File, 1), SEEK_CUR);

  ReadInt(f->File, 4);

  chksum = ReadInt(f->File, 4);

  if(chksum && f->ChkSum && f->ChkSum != chksum)
    log_warn("wrong checksum");

  hppp = (long)ReadSInt(f->File, 4);
  vppp = (long)ReadSInt(f->File, 4);

  f->Glyphs = new Glyph[256];

  while(true)
  {
    u_long BytesLeft;
    int    FlagLowBits;
    u_long c;

    if(!SkipSpecials())
      return false;

    if(FlagByte == PK_Post)
      return true;

    FlagLowBits = FlagByte & 0x7;

    if(FlagLowBits == 7)
    {
      BytesLeft = ReadInt(f->File, 4);
      c         = ReadInt(f->File, 4);
    }
    else if(FlagLowBits > 3)
    {
      BytesLeft = ((FlagLowBits - 4) << 16) + ReadInt(f->File, 2);
      c         = ReadInt(f->File, 1);
    }
    else
    {
      BytesLeft = (FlagLowBits << 8) + ReadInt(f->File, 1);
      c         = ReadInt(f->File, 1);
    }
    f->Glyphs[c].Addr     = f->File->Seek(0, SEEK_CUR);
    f->Glyphs[c].FlagByte = FlagByte;

    f->File->Seek(BytesLeft, SEEK_CUR);
  }
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//                                                                                                                //
// bool ReadPKIndex(DVI *doc, Font *f)                                                                            //
//                                                                                                                //
// Reads general information from a font-file.                                                                    //
//                                                                                                                //
// DVI  *doc                            document the font appears in                                              //
// Font *f                              font                                                                      //
//                                                                                                                //
// Result:                              `true' if successful, otherwise `false'                                   //
//                                                                                                                //
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

bool ReadPKIndex(DVI *doc, Font *f)
{
  PKInfo info(f);

  return info.ReadIndex(doc);
}
