////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//                                                                                                                //
// $Id: GF.cc,v 2.3 1998/08/20 11:16:13 achim Exp $
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

static const u_char GF_Paint0    = 0;
static const u_char GF_Paint1    = 64;
static const u_char GF_Paint2    = 65;
static const u_char GF_Paint3    = 66;
static const u_char GF_BOC       = 67;
static const u_char GF_BOC1      = 68;
static const u_char GF_EOC       = 69;
static const u_char GF_Skip0     = 70;
static const u_char GF_Skip1     = 71;
static const u_char GF_Skip2     = 72;
static const u_char GF_Skip3     = 73;
static const u_char GF_NewRow0   = 74;
static const u_char GF_NewRowMax = 238;
static const u_char GF_XXX1      = 239;
static const u_char GF_XXX2      = 240;
static const u_char GF_XXX3      = 241;
static const u_char GF_XXX4      = 242;
static const u_char GF_YYY       = 243;
static const u_char GF_NOP       = 244;
static const u_char GF_CharLoc   = 245;
static const u_char GF_CharLoc0  = 246;
static const u_char GF_Post      = 248;
static const u_char GF_PostPost  = 249;
static const u_char GF_Trailer   = 223;

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
  Glyph      *g;
  u_char     Cmd;
  int        MinM, MaxM;
  int        MinN, MaxN;
  BitmapUnit *cp;
  BitmapUnit *BaseP;
  BitmapUnit *MaxP;
  int        UnitsWide;
  bool       PaintSwitch;
  bool       NewRow;
  int        Count;
  int        WordWeight;
  u_long     Width, Height;

  try
  {
    g = &f->Glyphs[c];

    while(true)
    {
      switch(Cmd = ReadInt(f->File, 1))
      {
        case GF_XXX1:
        case GF_XXX2:
        case GF_XXX3:
        case GF_XXX4:
          f->File->Seek(ReadInt(f->File, Cmd - GF_XXX1 + 1), SEEK_CUR);
          continue;

        case GF_YYY:
          f->File->Seek(4, SEEK_CUR);
          continue;

        case GF_BOC:
          f->File->Seek(8, SEEK_CUR);

          MinM = ReadSInt(f->File, 4);
          MaxM = ReadSInt(f->File, 4);
          MinN = ReadSInt(f->File, 4);
          MaxN = ReadSInt(f->File, 4);

          g->Ux = -MinM;
          g->Uy =  MaxN;

          Width  = MaxM - MinM + 1;
          Height = MaxN - MinN + 1;
          break;

        case GF_BOC1:
          f->File->Seek(1, SEEK_CUR);

          Width = ReadInt(f->File, 1);
          g->Ux = Width - ReadInt(f->File, 1);

          Width++;

          Height = ReadInt(f->File, 1) + 1;
          g->Uy  = ReadInt(f->File, 1);
          break;

        default:
          return;
      }
      break;
    }
    PaintSwitch = 0;

    if(!(g->UBitMap = new BBitmap(
                            BRect(0.0, 0.0,
                                  (float)((Width + BITS_PER_UNIT - 1) & ~(BITS_PER_UNIT - 1)) - 1.0,
                                  (float)Height - 1.0),
                            B_MONOCHROME_1_BIT)))
      return;

    BaseP      = (BitmapUnit *)g->UBitMap->Bits();
    cp         = BaseP;
    UnitsWide  = g->UBitMap->BytesPerRow() / (BITS_PER_UNIT / 8);
    MaxP       = BMU_ADD(BaseP, g->UBitMap->BitsLength());
    NewRow     = false;
    WordWeight = BITS_PER_UNIT;

    bzero(g->UBitMap->Bits(), g->UBitMap->BitsLength());

    while(true)
    {
      Count = -1;

      Cmd = ReadInt(f->File, 1);

      if(Cmd < 64)
        Count = Cmd;
      else if(Cmd > GF_NewRow0 && Cmd <= GF_NewRowMax)
      {
        Count       = Cmd - GF_NewRow0;
        PaintSwitch = 0;
        NewRow      = true;
      }
      else
        switch(Cmd)
        {
          case GF_Paint1:
          case GF_Paint2:
          case GF_Paint3:
            Count = ReadInt(f->File, Cmd - GF_Paint1 + 1);
            break;

          case GF_EOC:
            return;

          case GF_Skip1:
          case GF_Skip2:
          case GF_Skip3:
            BaseP += ReadInt(f->File, Cmd - GF_Skip0) * UnitsWide;

          case GF_Skip0:
            NewRow      = true;
            PaintSwitch = 0;
            break;

          case GF_XXX1:
          case GF_XXX2:
          case GF_XXX3:
          case GF_XXX4:
            f->File->Seek(ReadInt(f->File, Cmd - GF_XXX1 + 1), SEEK_CUR);
            break;

          case GF_YYY:
            f->File->Seek(4, SEEK_CUR);
            break;

          case GF_NOP:
            break;

          default:
            return;
        }
      if(NewRow)
      {
        BaseP     += UnitsWide;
        cp         = BaseP;
        WordWeight = BITS_PER_UNIT;
        NewRow     = false;
      }
      if(Count >= 0)
      {
        while(Count)
          if(Count <= WordWeight)
          {
#ifndef MSB_FIRST
            if(PaintSwitch)
              *cp |= BitMasks[Count] << (BITS_PER_UNIT - WordWeight);
#endif
            WordWeight -= Count;

#ifdef MSB_FIRST
            if(PaintSwitch)
              *cp |= BitMasks[Count] << WordWeight;
#endif
            break;
          }
          else
          {
            if(PaintSwitch)
#ifdef MSB_FIRST
              *cp |= BitMasks[WordWeight];
#else
              *cp |= BitMasks[WordWeight] << (BITS_PER_UNIT - WordWeight);
#endif
            cp++;
            Count     -= WordWeight;
            WordWeight = BITS_PER_UNIT;
          }
        PaintSwitch = 1 - PaintSwitch;
      }
    }
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
// bool ReadGFIndex(DVI *doc, Font *f)                                                                            //
//                                                                                                                //
// Reads general information from a font-file.                                                                    //
//                                                                                                                //
// DVI  *doc                            document the font appears in                                              //
// Font *f                              font                                                                      //
//                                                                                                                //
// Result:                              `true' if successful, otherwise `false'                                   //
//                                                                                                                //
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

bool ReadGFIndex(DVI * /* doc */, Font *f)
{
  int32  hppp, vppp;
  u_char c, Cmd;
  Glyph  *g;
  long   CheckSum;

  f->File->Seek(-4, SEEK_END);

  while(ReadInt(f->File, 4) != ((u_long)GF_Trailer << 24 | GF_Trailer << 16 | GF_Trailer << 8 | GF_Trailer))
    f->File->Seek(-5, SEEK_CUR);

  f->File->Seek(-5, SEEK_CUR);

  for(c = ReadInt(f->File, 1); c == GF_Trailer; f->File->Seek(-2, SEEK_CUR))
    ;

  if(c != GF_ID)
    return false;

  f->File->Seek(-6, SEEK_CUR);

  if(ReadInt(f->File, 1) != GF_PostPost)
    return false;

  f->File->Seek(ReadSInt(f->File, 4), SEEK_SET);

  if(ReadInt(f->File, 1) != GF_Post)
    return false;

  f->File->Seek(8, SEEK_CUR);

  CheckSum = ReadInt(f->File, 4);

  hppp = ReadSInt(f->File, 4);
  vppp = ReadSInt(f->File, 4);

  f->File->Seek(16, SEEK_CUR);

  if(!(f->Glyphs = new Glyph[256]))
    return false;

  while((Cmd = ReadInt(f->File, 1)) != GF_PostPost)
  {
    long Addr;

    c = ReadInt(f->File, 1);

    g = &f->Glyphs[c];

    switch(Cmd)
    {
      case GF_CharLoc:
        f->File->Seek(8, SEEK_CUR);
        break;

      case GF_CharLoc0:
        f->File->Seek(4, SEEK_CUR);
        break;

      default:
        return false;
    }
    g->Advance = f->DimConvert * ReadSInt(f->File, 4);

    if((Addr = ReadInt(f->File, 4)) != -1)
      g->Addr = Addr;
  }

  f->ReadChar = ::ReadChar;

  return true;
}
