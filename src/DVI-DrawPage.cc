////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//                                                                                                                //
// $Id: DVI-DrawPage.cc,v 2.3 1999/07/25 13:24:18 achim Exp $
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

#include <stdio.h>
#include "DVI-DrawPage.h"
#include "TeXFont.h"
#include "log.h"

PSInterface *DrawPage::PSIface = NULL;

DrawPage::DrawPage(const DrawSettings &set):
  Settings(set),
  Frames(),
  SearchState(this, set.SearchString),
  CurFont(NULL),
  VirtTable(NULL),
  Virtual(NULL),
  BufferPos(NULL),
  BufferEnd(NULL)
{}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//                                                                                                                //
// void DrawPage::ChangeFont(ulong n)                                                                             //
//                                                                                                                //
// Changes the current font.                                                                                      //
//                                                                                                                //
// ulong n                              number of the new font                                                    //
//                                                                                                                //
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

void DrawPage::ChangeFont(ulong n)
{
  if (n < Document->Fonts.TableLength())
    if ((CurFont = Document->Fonts[n]) != NULL)
    {
      MaxChar = CurFont->MaxChar;
      SetChar = CurFont->SetChar;
    }
    else
    {
      MaxChar = ~0;
      SetChar = SetEmptyChar;
    }
  else
  {
    log_warn("non-existent font!");

    throw(invalid_argument("non-existent font"));
  }
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//                                                                                                                //
// void DrawPage::DrawPart()                                                                                      //
//                                                                                                                //
// Draws a part of a document.                                                                                    //
//                                                                                                                //
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

void DrawPage::DrawPart()
{
  DrawPage NewDP(Settings);
  DrawPage *OldDP, *dp;
  int      ReflCount;
  uchar    c;

  dp = this;

  while (!dp->EndOfFile())
  {
    c = dp->ReadInt(1);

    if (c <= (uchar)(DVI::SetChar0 + 127))
      (*dp->SetChar)(dp, c, c);

    else if (DVI::FontNum0 <= c && c <= (wchar)(DVI::FontNum0 + 63))
      dp->ChangeFont((ulong)(c - DVI::FontNum0));

    else
    {
      int32 a, b;

      switch (c)
      {
        case DVI::Set1:
        case DVI::Put1:
          (*dp->SetChar)(dp, c, dp->ReadInt(1));
          break;

        case DVI::Set2:
        case DVI::Put2:
          (*dp->SetChar)(dp, c, dp->ReadInt(2));
          break;

        case DVI::SetRule:
          a = dp->ReadSInt(4) * dp->DimConvert;
          b = dp->ReadSInt(4) * dp->DimConvert;

          if (a > 0 && b > 0 && !dp->ScanFrame)
            dp->DrawRule(dp->Settings.ToPixel(b), dp->Settings.ToPixel(a));

          dp->Data.Horiz += dp->DrawDir * b;
          break;

        case DVI::PutRule:
          a = dp->ReadSInt(4) * dp->DimConvert;
          b = dp->ReadSInt(4) * dp->DimConvert;

          if (a > 0 && b > 0 && !dp->ScanFrame)
            dp->DrawRule(dp->Settings.ToPixel(b), dp->Settings.ToPixel(a));
          break;

        case DVI::NOP:
          break;

        case DVI::BeginOP:
          dp->Skip(44);

          dp->Data.Horiz  = dp->Settings.DspInfo.PixelsPerInch << 16;
          dp->Data.Vert   = dp->Settings.DspInfo.PixelsPerInch << 16;
          dp->Data.PixelV = dp->Settings.PixelConv(dp->Data.Vert);
          dp->Data.w      = 0;
          dp->Data.x      = 0;
          dp->Data.y      = 0;
          dp->Data.z      = 0;

          break;

        case DVI::EndOP:
          if (!dp->Frames.empty())
          {
            log_warn("stack not empty at Pop!");
            throw(runtime_error("stack not empty at EOP"));
          }
          if (dp->PSIface)
            dp->PSIface->EndPage();
          return;

        case DVI::Push:
          dp->Frames.push(dp->Data);
          break;

        case DVI::Pop:
          if (dp->Frames.empty())
          {
            log_warn("stack empty at EOP!");
            throw(runtime_error("stack empty at Pop"));
          }

          dp->Data = dp->Frames.top();
          dp->Frames.pop();
          break;

        case DVI::StartRefl:
          if (!dp->ScanFrame)
          {
            NewDP.Document     = dp->Document;
            NewDP.vw           = dp->vw;
            NewDP.Settings     = dp->Settings;
            NewDP.Data         = dp->Data;
            NewDP.TPicConvert  = dp->TPicConvert;
            NewDP.DimConvert   = dp->DimConvert;
            NewDP.DrawDir      = dp->DrawDir;
            NewDP.CurFont      = dp->CurFont;
            NewDP.VirtTable    = dp->VirtTable;
            NewDP.Virtual      = dp->Virtual;
            NewDP.MaxChar      = dp->MaxChar;
            NewDP.SetChar      = dp->SetChar;
            NewDP.BufferPos    = dp->BufferPos;
            NewDP.BufferEnd    = dp->BufferEnd;
            NewDP.ScanFrame    = dp->ScanFrame;
            NewDP.Frames       = dp->Frames;
            NewDP.SearchState  = dp->SearchState;

            OldDP = dp;
            dp    = &NewDP;

            dp->ScanFrame = &dp->Frames.top();
            ReflCount     = 0;
          }
          else
          {
            if (dp->ScanFrame = &dp->Frames.top())
              ReflCount++;
          }
          break;

        case DVI::EndRefl:
          if (dp->ScanFrame)
          {
            if (dp->ScanFrame == &dp->Frames.top() && --ReflCount < 0)
            {
              dp->ScanFrame = NULL;

              dp->Frames.push(dp->Data);

              OldDP->Frames      = dp->Frames;
              OldDP->SearchState = dp->SearchState;

              dp = OldDP;                  // this restores the old file position!!!

              dp->Data.Horiz  = dp->Frames.top().Horiz;
              dp->Data.Vert   = dp->Frames.top().Vert;
              dp->Data.PixelV = dp->Frames.top().PixelV;
              dp->DrawDir     = -dp->DrawDir;
            }
          }
          else
          {
            dp->DrawDir = -dp->DrawDir;

            if (dp->Frames.empty())
            {
              log_warn("stack empty at Pop!");
              throw(runtime_error("stack empty at Pop"));
            }

            dp->Data = dp->Frames.top();
            dp->Frames.pop();
          }
          break;

        case DVI::Right1:
        case DVI::Right2:
        case DVI::Right3:
        case DVI::Right4:
          dp->Data.Horiz += dp->DrawDir * dp->ReadSInt(c - DVI::Right1 + 1) * dp->DimConvert;
          break;

        case DVI::W1:
        case DVI::W2:
        case DVI::W3:
        case DVI::W4:
          dp->Data.w      = dp->ReadSInt(c - DVI::W0) * dp->DimConvert;
        case DVI::W0:
          dp->Data.Horiz += dp->DrawDir * dp->Data.w;
          break;

        case DVI::X1:
        case DVI::X2:
        case DVI::X3:
        case DVI::X4:
          dp->Data.x      = dp->ReadSInt(c - DVI::X0) * dp->DimConvert;
        case DVI::X0:
          dp->Data.Horiz += dp->DrawDir * dp->Data.x;
          break;

        case DVI::Down1:
        case DVI::Down2:
        case DVI::Down3:
        case DVI::Down4:
          dp->Data.Vert  += dp->ReadSInt(c - DVI::Down1 + 1) * dp->DimConvert;
          dp->Data.PixelV = dp->Settings.PixelConv(dp->Data.Vert);
          break;

        case DVI::Y1:
        case DVI::Y2:
        case DVI::Y3:
        case DVI::Y4:
          dp->Data.y      = dp->ReadSInt(c - DVI::Y0) * dp->DimConvert;
        case DVI::Y0:
          dp->Data.Vert  += dp->Data.y;
          dp->Data.PixelV = dp->Settings.PixelConv(dp->Data.Vert);
          break;

        case DVI::Z1:
        case DVI::Z2:
        case DVI::Z3:
        case DVI::Z4:
          dp->Data.z      = dp->ReadSInt(c - DVI::Z0) * dp->DimConvert;
        case DVI::Z0:
          dp->Data.Vert  += dp->Data.z;
          dp->Data.PixelV = dp->Settings.PixelConv(dp->Data.Vert);
          break;

        case DVI::Font1:
        case DVI::Font2:
        case DVI::Font3:
        case DVI::Font4:
          dp->ChangeFont(dp->ReadInt(c - DVI::Font1 + 1));
          break;

        case DVI::XXX1:
        case DVI::XXX2:
        case DVI::XXX3:
        case DVI::XXX4:
          a = dp->ReadInt(c - DVI::XXX1 + 1);

          if (a > 0)
            dp->Special(a);
          break;

        case DVI::FontDef1:
        case DVI::FontDef2:
        case DVI::FontDef3:
        case DVI::FontDef4:
          dp->Skip(13 + c - DVI::FontDef1);
          dp->Skip(dp->ReadInt(1) + dp->ReadInt(1));
          break;

        default:
          log_warn("invalid operand: 0x%02x!\n", (uint)c);

          throw(runtime_error("invalid operand"));
          break;
      }
    }
  }
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//                                                                                                                //
// void DrawPage::SetEmptyChar(DrawPage *dp, wchar cmd, wchar c)                                                  //
//                                                                                                                //
// Does nothing.                                                                                                  //
//                                                                                                                //
// DrawPage *dp                         drawing information                                                       //
// wchar    cmd                         command to draw the char (either Put1 or Set1)                            //
// wchar    c                           character to be drawn                                                     //
//                                                                                                                //
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

void DrawPage::SetEmptyChar(DrawPage * /* dp */, wchar /* cmd */, wchar /* c */)
{
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//                                                                                                                //
// void DrawPage::SetNoChar(DrawPage *dp, wchar cmd, wchar c)                                                     //
//                                                                                                                //
// Draws a character if currently no font is selected.                                                            //
//                                                                                                                //
// DrawPage *dp                         drawing information                                                       //
// wchar    cmd                         command to draw the char (either Put1 or Set1)                            //
// wchar    c                           character to be drawn                                                     //
//                                                                                                                //
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

void DrawPage::SetNoChar(DrawPage *dp, wchar cmd, wchar c)
{
  if (dp->Virtual)
  {
    dp->CurFont = dp->Virtual->FirstFont;

    if (dp->CurFont != NULL)
    {
       dp->SetChar = dp->CurFont->SetChar;

       (*dp->SetChar)(dp, cmd, c);
    }
  }
  else
    throw(invalid_argument("no font set"));
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//                                                                                                                //
// void DrawPage::SetVFChar(DrawPage *dp, wchar cmd, wchar c)                                                     //
//                                                                                                                //
// Draws a character if currently a virtual font is selected.                                                     //
//                                                                                                                //
// DrawPage *dp                         drawing information                                                       //
// wchar    cmd                         command to draw the char (either Put1 or Set1)                            //
// wchar    c                           character to be drawn                                                     //
//                                                                                                                //
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

void DrawPage::SetVFChar(DrawPage *dp, wchar cmd, wchar c)
{
  Macro        *m;
  DrawPage     NewDP(dp->Settings);
  long         horiz;
  static uchar ch;

  if ((m = &dp->CurFont->Macros[c])->Position == NULL)
  {
    m->Position = &ch;
    m->End      = &ch;
    return;
  }

  horiz = dp->Data.Horiz;

  if (dp->DrawDir < 0)
    dp->Data.Horiz -= m->Advance;

  if (!dp->ScanFrame)
  {
    NewDP.Document     = dp->Document;
    NewDP.vw           = dp->vw;
    NewDP.Settings     = dp->Settings;
    NewDP.Data         = dp->Data;
    NewDP.Data.w       = 0;
    NewDP.Data.x       = 0;
    NewDP.Data.y       = 0;
    NewDP.Data.z       = 0;
    NewDP.TPicConvert  = dp->TPicConvert;
    NewDP.DimConvert   = dp->CurFont->DimConvert;
    NewDP.DrawDir      = 1;
    NewDP.CurFont      = NULL;
    NewDP.VirtTable    = &dp->CurFont->VFTable;
    NewDP.Virtual      = dp->CurFont;
    NewDP.MaxChar      = dp->MaxChar;
    NewDP.SetChar      = dp->SetNoChar;
    NewDP.BufferPos    = m->Position;
    NewDP.BufferEnd    = m->End;
    NewDP.ScanFrame    = dp->ScanFrame;
    NewDP.Frames       = dp->Frames;
    NewDP.ScanFrame    = NULL;
    NewDP.SearchState  = dp->SearchState;

    NewDP.DrawPart();

    dp->Frames      = NewDP.Frames;
    dp->SearchState = NewDP.SearchState;
  }
  if (cmd == DVI::Put1 || cmd == DVI::Put2)
    dp->Data.Horiz = horiz;
  else
    if (dp->DrawDir > 0)
      dp->Data.Horiz += m->Advance;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//                                                                                                                //
// void DrawPage::SetNormalChar(DrawPage *dp, wchar cmd, wchar c)                                                 //
//                                                                                                                //
// Draws a character if currently a normal font is selected.                                                      //
//                                                                                                                //
// DrawPage *dp                         drawing information                                                       //
// wchar    cmd                         command to draw the char (either Put1 or Set1)                            //
// wchar    c                           character to be drawn                                                     //
//                                                                                                                //
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

void DrawPage::SetNormalChar(DrawPage *dp, wchar cmd, wchar c)
{
  Glyph *g;
  long  horiz;
  float x, y;

  if (c > dp->MaxChar)
  {
    log_info("character out of range: %d", (int)c);
    return;
  }

  g = &dp->CurFont->Glyphs[c];

  if (g->UBitMap == NULL)
    dp->CurFont->ReadChar(dp->CurFont, c);

  if (g->UBitMap == NULL)
    return;

  horiz = dp->Data.Horiz;

  if (dp->DrawDir < 0)
    dp->Data.Horiz -= g->Advance;

  if (!dp->ScanFrame)
  {
    if (dp->Settings.ShrinkFactor == 1)
    {
      x = dp->Settings.PixelConv(dp->Data.Horiz) - g->Ux;
      y = dp->Data.PixelV                        - g->Uy;

      dp->vw->DrawBitmapAsync(g->UBitMap, BPoint(x, y));

      if (dp->Settings.SearchString != NULL)
      {
//        BRect r(g->UBitMap->Bounds());
//        r.OffsetBy(x, y);

        BRect r(x, y, x + g->UWidth, y + g->UHeight);

        dp->SearchState.MatchChar(c, r);
      }
    }
    else
    {
      g->Shrink(dp->Settings.ShrinkFactor, dp->Settings.AntiAliasing);

      x = dp->Settings.PixelConv(dp->Data.Horiz) - g->Sx;
      y = dp->Data.PixelV                        - g->Sy;

      dp->vw->DrawBitmapAsync(g->SBitMap, BPoint(x, y));

      if (dp->Settings.SearchString != NULL)
      {
//        BRect r(g->SBitMap->Bounds());
//        r.OffsetBy(x, y);

        BRect r(x, y, x + g->SWidth, y + g->SHeight);

        dp->SearchState.MatchChar(c, r);
      }
    }
  }
  if (cmd == DVI::Put1 || cmd == DVI::Put2)
    dp->Data.Horiz = horiz;
  else
    if (dp->DrawDir > 0)
      dp->Data.Horiz += g->Advance;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//                                                                                                                //
// void DrawPage::DrawRule(long w, long h)                                                                        //
//                                                                                                                //
// Draws a rule.                                                                                                  //
//                                                                                                                //
// long w, h                            size of the rule                                                          //
//                                                                                                                //
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

void DrawPage::DrawRule(long w, long h)
{
  BRect r(0.0, 0.0, (w > 0 ? w : 1) - 1, (h > 0 ? h : 1) - 1);

  r.OffsetTo((float)(Settings.PixelConv(Data.Horiz) - (DrawDir < 0 ? w - 1 : 0)),
             (float)(Data.PixelV - h + 1));

  vw->SetHighColor(0, 0, 0, 255);
  vw->FillRect(r, B_SOLID_HIGH);
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//                                                                                                                //
// void DrawPage::InitPSIface()                                                                                   //
//                                                                                                                //
// Initializes the interface to GhostScript.                                                                      //
//                                                                                                                //
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

void DrawPage::InitPSIface()
{
  if (!PSIface)
    PSIface = InitPSInterface(this);
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//                                                                                                                //
// uint32 DrawPage::ReadInt(ssize_t Size)                                                                         //
//                                                                                                                //
// reads an integer of `Size' bytes.                                                                              //
//                                                                                                                //
// ssize_t Size                         size of the integer in bytes                                              //
//                                                                                                                //
// Result:                              read integer                                                              //
//                                                                                                                //
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

uint32 DrawPage::ReadInt(ssize_t Size)
{
  uint32 x = 0;
  uint8  c;

  while (Size--)
  {
    if (BufferPos == NULL)
      File->Read(&c, 1);
    else
      c = *BufferPos++;

    x = (x << 8) | c;
  }
  return x;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//                                                                                                                //
// int32 DrawPage::ReadSInt(ssize_t Size)                                                                         //
//                                                                                                                //
// reads a signed integer of `Size' bytes.                                                                        //
//                                                                                                                //
// ssize_t Size                         size of the integer in bytes                                              //
//                                                                                                                //
// Result:                              read integer                                                              //
//                                                                                                                //
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

int32 DrawPage::ReadSInt(ssize_t Size)
{
  int32 x;
  int8  c;

  if (BufferPos == NULL)
    File->Read(&c, 1);
  else
    c = *BufferPos++;

  x = c;

  while (--Size)
  {
    if (BufferPos == NULL)
      File->Read(&c, 1);
    else
      c = *BufferPos++;

    x = (x << 8) | (uint8)c;
  }

  return x;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//                                                                                                                //
// void DrawPage::ReadString(char *Buffer, size_t len)                                                            //
//                                                                                                                //
// reads a string of `len' bytes to `Buffer'.                                                                     //
//                                                                                                                //
// char   *Buffer                       buffer the string is stored in                                            //
// size_t len                           length of the string in bytes                                             //
//                                                                                                                //
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

void DrawPage::ReadString(char *Buffer, size_t len)
{
  if (BufferPos == NULL)
  {
    File->Read(Buffer, len);
    Buffer[len] = 0;
  }
  else
  {
    memcpy(Buffer, BufferPos, len);
    Buffer[len] = 0;
    BufferPos += len;
  }
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//                                                                                                                //
// void DrawPage::Skip(off_t off)                                                                                 //
//                                                                                                                //
// skips the specified number of bytes.                                                                           //
//                                                                                                                //
// off_t off                            number of bytes to skip                                                   //
//                                                                                                                //
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

void DrawPage::Skip(off_t off)
{
  if (BufferPos == NULL)
    File->Seek(off, SEEK_CUR);
  else
    BufferPos += off;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//                                                                                                                //
// bool DrawPage::EndOfFile()                                                                                     //
//                                                                                                                //
// tests if the end of the file/buffer is reached.                                                                //
//                                                                                                                //
// Result:                              `true' if the end is reached, `false' otherwise                           //
//                                                                                                                //
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

bool DrawPage::EndOfFile()
{
  if (BufferPos == NULL)
    return true;
  else
    return BufferPos >= BufferEnd;
}


/* DrawPage::SearchStatus *****************************************************************************************/


////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//                                                                                                                //
// DrawPage::SearchStatus::SearchStatus(DrawPage *draw, const char *str)                                          //
//                                                                                                                //
// initializes the SearchStatus.                                                                                  //
//                                                                                                                //
// DrawPage   *draw                     corresponding DrawPage structure                                          //
// const char *str                      string to search for                                                      //
//                                                                                                                //
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

DrawPage::SearchStatus::SearchStatus(DrawPage *draw, const char *str):
  dp(draw),
  SearchString(NULL),
  Offset(NULL),
  Rects(NULL),
  CurPosition(0),
  Found(false)
{
  int i, j;

  if (str == NULL)
  {
    SearchString = NULL;
    Offset       = NULL;
    Rects        = NULL;
    Length       = 0;
    return;
  }

  Length = strlen(str);

  SearchString = new char[Length + 1];
  Offset       = new ssize_t[Length + 1];
  Rects        = new BRect[Length];

  strcpy(SearchString, str);

  // init offset-array for Knuth-Morris-Pratt search

  Offset[0] = -1;

  for (i = 0, j = -1; i < Length; i++, j++)
  {
    while (j >= 0 && SearchString[i] != SearchString[j])
      j = Offset[j];

    Offset[i] = j;
  }
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//                                                                                                                //
// DrawPage::SearchStatus::~SearchStatus()                                                                        //
//                                                                                                                //
// delete the SearchStatus.                                                                                       //
//                                                                                                                //
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

DrawPage::SearchStatus::~SearchStatus()
{
  delete [] SearchString;
  delete [] Offset;
  delete [] Rects;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//                                                                                                                //
// DrawPage::SearchStatus &DrawPage::SearchStatus::operator =(const DrawPage::SearchStatus &s)                    //
//                                                                                                                //
// copies one SearchStatus into the other.                                                                        //
//                                                                                                                //
// const DrawPage::SearchStatus &s      SearchStatus to copy                                                      //
//                                                                                                                //
// Result:                              *this                                                                     //
//                                                                                                                //
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

DrawPage::SearchStatus &DrawPage::SearchStatus::operator =(const DrawPage::SearchStatus &s)
{
  delete [] SearchString;
  delete [] Offset;
  delete [] Rects;

  dp           = s.dp;
  Length       = s.Length;
  CurPosition  = s.CurPosition;
  Found        = s.Found;

  if (Length == 0)
  {
    SearchString = NULL;
    Offset       = NULL;
    Rects        = NULL;
    Length       = 0;
  }
  else
  {
    SearchString = new char[Length + 1];
    Offset       = new ssize_t[Length + 1];
    Rects        = new BRect[Length];

    strcpy(SearchString, s.SearchString);
    memcpy(Offset,       s.Offset,       (Length + 1) * sizeof(ssize_t));
    memcpy(Rects,        s.Rects,        Length       * sizeof(BRect));
  }
  return *this;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//                                                                                                                //
// void DrawPage::SearchStatus::MatchChar(char c, const BRect &rect)                                              //
//                                                                                                                //
// updates the search-status, when the character `c' is read.                                                     //
//                                                                                                                //
// char        c                        character read                                                            //
// const BRect &rect                    position of the character                                                 //
//                                                                                                                //
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

void DrawPage::SearchStatus::MatchChar(char c, const BRect &rect)
{
  if (Length == 0)
    return;

  while (CurPosition >= 0 && c != SearchString[CurPosition])
    CurPosition = Offset[CurPosition];

  if (CurPosition >= 0)
    Rects[CurPosition] = rect;

  if (++CurPosition == Length)
  {
    Found = true;
    DrawRects();
    CurPosition = 0;
  }
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//                                                                                                                //
// void DrawPage::SearchStatus::DrawRects()                                                                       //
//                                                                                                                //
// highlights the found text.                                                                                     //
//                                                                                                                //
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

void DrawPage::SearchStatus::DrawRects()
{
  int i;

  if (dp->Settings.AntiAliasing && dp->Settings.ShrinkFactor > 1)
  {
    // we are in mode B_OP_MIN

    dp->vw->SetHighColor(200, 200, 0, 255);

    for (i = 0; i < Length; i++)
      dp->vw->FillRect(Rects[i], B_SOLID_HIGH);
  }
  else
  {
    // we are in mode B_OP_OVER

    dp->vw->SetHighColor(200, 0, 0, 255);

    for (i = 0; i < Length; i++)
      dp->vw->StrokeLine(Rects[i].LeftBottom(), Rects[i].RightBottom(), B_SOLID_HIGH);

    dp->vw->SetHighColor(0, 0, 0, 255);
  }
}
