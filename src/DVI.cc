////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//                                                                                                                //
// $Id: DVI.cc,v 2.2 1998/07/09 13:36:24 achim Exp $
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
#include <InterfaceKit.h>
#include <StorageKit.h>
#include <stdio.h>
#include <syslog.h>
#include <Debug.h>

#include "defines.h"

extern "C"
{
  #include "kpathsea/c-auto.h"
  #include "kpathsea/magstep.h"
}

#include "BeDVI.h"
#include "DVI-View.h"
#include "DVI.h"
#include "FontList.h"
#include "TeXFont.h"
#include "log.h"

static const int NoMagStep = -29999;
static const int NoBuild   =  29999;


/* DrawInfo *******************************************************************************************************/


PSInterface *DrawInfo::PSIface = NULL;

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//                                                                                                                //
// void DVI::InitPSIface()                                                                                        //
//                                                                                                                //
// Initializes the interface to GhostScript.                                                                      //
//                                                                                                                //
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

void DrawInfo::InitPSIface()
{
  if(!PSIface)
    PSIface = InitPSInterface(this);
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//                                                                                                                //
// uint32 DrawInfo::ReadInt(ssize_t Size)                                                                         //
//                                                                                                                //
// reads an integer of `Size' bytes.                                                                              //
//                                                                                                                //
// ssize_t Size                         size of the integer in bytes                                              //
//                                                                                                                //
// Result:                              read integer                                                              //
//                                                                                                                //
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

uint32 DrawInfo::ReadInt(ssize_t Size)
{
  uint32 x = 0;
  uint8  c;

  while(Size--)
  {
    if(BufferPos == NULL)
      File->Read(&c, 1);
    else
      c = *BufferPos++;

    x = (x << 8) | c;
  }
  return x;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//                                                                                                                //
// int32 DrawInfo::ReadSInt(ssize_t Size)                                                                         //
//                                                                                                                //
// reads a signed integer of `Size' bytes.                                                                        //
//                                                                                                                //
// ssize_t Size                         size of the integer in bytes                                              //
//                                                                                                                //
// Result:                              read integer                                                              //
//                                                                                                                //
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

int32 DrawInfo::ReadSInt(ssize_t Size)
{
  int32 x;
  int8  c;

  if(BufferPos == NULL)
    File->Read(&c, 1);
  else
    c = *BufferPos++;

  x = c;

  while(--Size)
  {
    if(BufferPos == NULL)
      File->Read(&c, 1);
    else
      c = *BufferPos++;

    x = (x << 8) | (uint8)c;
  }

  return x;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//                                                                                                                //
// void DrawInfo::ReadString(char *Buffer, size_t len)                                                            //
//                                                                                                                //
// reads a string of `len' bytes to `Buffer'.                                                                     //
//                                                                                                                //
// char   *Buffer                       buffer the string is stored in                                            //
// size_t len                           length of the string in bytes                                             //
//                                                                                                                //
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

void DrawInfo::ReadString(char *Buffer, size_t len)
{
  if(BufferPos == NULL)
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
// void DrawInfo::Skip(off_t off)                                                                                 //
//                                                                                                                //
// skips the specified number of bytes.                                                                           //
//                                                                                                                //
// off_t off                            number of bytes to skip                                                   //
//                                                                                                                //
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

void DrawInfo::Skip(off_t off)
{
  if(BufferPos == NULL)
    File->Seek(off, SEEK_CUR);
  else
    BufferPos += off;
}


/* DisplayInfo ****************************************************************************************************/


////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//                                                                                                                //
// static const u_int        _NumModes;                                                                           //
// static const char * const _ModeNames[];                                                                        //
// static const u_int        _ModeDPI[];                                                                          //
//                                                                                                                //
// default modes.                                                                                                 //
//                                                                                                                //
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

static const u_int        _NumModes             = 3;
static const char * const _ModeNames[_NumModes] = {"nextscrn", "cx", "ljfour"};
static const u_int        _ModeDPI[_NumModes]   = {100, 300, 600};

u_int       DisplayInfo::NumModes    = _NumModes;
const char  **DisplayInfo::ModeNames = &_ModeNames[0];
u_int       *DisplayInfo::ModeDPI    = (u_int *)&_ModeDPI[0];

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//                                                                                                                //
// DisplayInfo::DisplayInfo()                                                                                     //
//                                                                                                                //
// initializes a DisplayInfo structure.                                                                           //
//                                                                                                                //
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

DisplayInfo::DisplayInfo():
  Mode(_ModeNames[_NumModes / 2]),
  PixelsPerInch(_ModeDPI[_NumModes / 2])
{}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//                                                                                                                //
// void DisplayInfo::SetModes(u_int num, const char **names, u_int *dpi)                                          //
//                                                                                                                //
// sets the list of supported resolutions. The arrays aren't copied and must therefor be preserved until the      //
// the object is deleted or this function is called with other arrays.                                            //
//                                                                                                                //
// u_int       num                      number of modes                                                           //
// const char  **names                  array of mode names                                                       //
// u_int       *dpi                     array of resolutions                                                      //
//                                                                                                                //
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

void DisplayInfo::SetModes(u_int num, const char **names, u_int *dpi)
{
  int        i, j;
  u_int      OldDPI;
  const char *OldName;

  // sort modes (InsertionSort)

  for(i = 1; i < num; i++)
  {
    OldDPI  = dpi[i];
    OldName = names[i];

    for(j = i; j > 0 && dpi[j - 1] >= OldDPI; j--)
    {
      dpi[j]   = dpi[j - 1];
      names[j] = names[j - 1];
    }

    dpi[j]   = OldDPI;
    names[j] = OldName;
  }

  NumModes  = num;
  ModeNames = names;
  ModeDPI   = dpi;

  SetResolution(ModeDPI[num / 2]);
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//                                                                                                                //
// void DisplayInfo::SetResolution(u_int dpi)                                                                     //
//                                                                                                                //
// Sets the resolution information.                                                                               //
//                                                                                                                //
// u_int dpi                            dpi value                                                                 //
//                                                                                                                //
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

void DisplayInfo::SetResolution(u_int dpi)
{
  int i;

  // search for mode name

  for(i = NumModes - 1; i > 0; i--)
    if(ModeDPI[i] <= dpi)
      break;

  Mode          = ModeNames[i];
  PixelsPerInch = dpi;
};


/* DrawSettings ***************************************************************************************************/


////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//                                                                                                                //
// DrawSettings::DrawSettings()                                                                                   //
//                                                                                                                //
// initializes a DrawSettings structure.                                                                          //
//                                                                                                                //
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

DrawSettings::DrawSettings():
  ShrinkFactor(3),
  RealShrinkFactor(3),
  AntiAliasing(true),
  BorderLine(false)
{}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//                                                                                                                //
// void DrawSettings::SetShrinkFactor(u_short sf)                                                                 //
//                                                                                                                //
// Sets the shrink factor.                                                                                        //
//                                                                                                                //
// u_short sf                           factor to shrink document                                                 //
//                                                                                                                //
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

void DrawSettings::SetShrinkFactor(u_short sf)
{
  ShrinkFactor = sf;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//                                                                                                                //
// void DrawSettings::SetMagnifyMode(bool OnOff)                                                                  //
//                                                                                                                //
// Determines if ShrinkFactor should be ignored.                                                                  //
//                                                                                                                //
// bool OnOff                           if `true' `ShrinkFactor' will be ignored                                  //
//                                                                                                                //
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

void DrawSettings::SetMagnifyMode(bool OnOff)
{
  if(OnOff)
  {
    RealShrinkFactor = ShrinkFactor;
    ShrinkFactor     = 1;
  }
  else
    ShrinkFactor = RealShrinkFactor;
}


/* DVI ************************************************************************************************************/


////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//                                                                                                                //
// DVI::DVI(BPositionIO *File, DrawSettings *prefs, void (*DspError)(const char *) = NULL)                        //
//                                                                                                                //
// Initializes a DVI-Document.                                                                                    //
//                                                                                                                //
// BPositionIO  *File                   file of the document                                                      //
// DrawSettings *prefs                  various options                                                           //
// void (*DspError)(const char *)       function to be called when an error occures                               //
//                                                                                                                //
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

DVI::DVI(BPositionIO *File, DrawSettings *prefs, void (*DspError)(const char *)):
  Fonts(),
  DVIFile(File),
  Name(NULL),
  PageOffset(NULL),
  Magnification(1000),
  DimConvert(1.0),
  OffsetX(prefs->DspInfo.PixelsPerInch),
  OffsetY(prefs->DspInfo.PixelsPerInch),
  DisplayError(DspError)
{
  Settings = *prefs;

  if(!Reload())
  {
    delete [] Name;
    delete [] PageOffset;
    delete DVIFile;

    DVIFile    = NULL;
    Name       = NULL;
    PageOffset = NULL;

    return;
  }
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//                                                                                                                //
// DVI::~DVI()                                                                                                    //
//                                                                                                                //
// Deletes a DVI-Document.                                                                                        //
//                                                                                                                //
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

DVI::~DVI()
{
  delete [] Name;
  delete [] PageOffset;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//                                                                                                                //
// int DVI::MagStepValue(float &mag)                                                                              //
//                                                                                                                //
// Adjusts a magnification value.                                                                                 //
//                                                                                                                //
// float &mag                           magnification                                                             //
//                                                                                                                //
// Result:                              adjusted value                                                            //
//                                                                                                                //
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

int DVI::MagStepValue(float &mag)
{
  int   Result;
  u_int dpi;

  acquire_sem(kpse_sem);
  dpi = kpse_magstep_fix((u_int)mag, Settings.DspInfo.PixelsPerInch, &Result);
  release_sem(kpse_sem);

  mag = (float)dpi;

  return Result ? Result : NoMagStep;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//                                                                                                                //
// bool DVI::Reload()                                                                                             //
//                                                                                                                //
// Reads general data of a DVI file.                                                                              //
//                                                                                                                //
// Result:                              `true' if successful, otherwise `false'                                   //
//                                                                                                                //
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

bool DVI::Reload()
{
  static const int BufferLen = 512;

  long    Numerator;
  long    Denominator;
  int     len;
  long    LastPageOffset;
  long    pos;
  u_char  *p;
  u_char  Buffer[BufferLen];
  u_char  x;
  u_char  Command;
  int     i;

  try
  {
    log_info("loading...");

    ASSERT(DVIFile != NULL);

    DVIFile->Seek(0, SEEK_SET);

    if(ReadInt(DVIFile, 1) != Preamble)
    {
      log_error("not a DVI file!");
      return false;
    }
    if((i = ReadInt(DVIFile, 1)) != 2)
    {
      log_error("wrong DVI version (%d)!", i);
      return false;
    }

    Numerator     = ReadInt(DVIFile, 4);
    Denominator   = ReadInt(DVIFile, 4);
    Magnification = ReadInt(DVIFile, 4);
    DimConvert    = (((double)Numerator * Magnification) / ((double)Denominator * 1000.0));
    DimConvert    = DimConvert * (((long)Settings.DspInfo.PixelsPerInch) << 16) / 254000;
    TPicConvert   = Settings.DspInfo.PixelsPerInch * Magnification / 1000000.0;

    len = ReadInt(DVIFile, 1);

    Name = new char[len + 1];

    DVIFile->Read(Name, len);
    Name[len] = 0;

    pos = DVIFile->Seek(0, SEEK_END);

    if(pos > BufferLen)
      pos -= BufferLen;
    else
      pos = 0;

    DVIFile->Seek(pos, SEEK_SET);

    p = &Buffer[DVIFile->Read(&Buffer, BufferLen)];

    do
    {
      while(p > Buffer && *--p != Trailer)
        ;

      if(p <= Buffer)
      {
        log_error("trailer not found!");
        return false;
      }
    }
    while(p[-1] != Trailer || p[-2] != Trailer || p[-3] != Trailer);

    pos += p - Buffer;

    for(x = *p; x == Trailer; x = ReadInt(DVIFile, 1))
      DVIFile->Seek(--pos, SEEK_SET);

    if(x != 2)
    {
      log_error("file corrupt?");
      return false;
    }

    DVIFile->Seek(pos - 4, SEEK_SET);
    DVIFile->Seek((long)ReadInt(DVIFile, 4), SEEK_SET);

    if(ReadInt(DVIFile, 1) != Postamble)
    {
      log_error("file corrupt?");
      return false;
    }

    LastPageOffset = ReadInt(DVIFile, 4);

    if(ReadInt(DVIFile, 4) != Numerator || ReadInt(DVIFile, 4) != Denominator || ReadInt(DVIFile, 4) != Magnification)
    {
      log_error("file corrupt?");
      return false;
    }

    UnshrunkPageHeight = ((long)((long)ReadInt(DVIFile, 4) * DimConvert) >> 16) + 2 * Settings.DspInfo.PixelsPerInch;
    UnshrunkPageWidth  = ((long)((long)ReadInt(DVIFile, 4) * DimConvert) >> 16) + 2 * Settings.DspInfo.PixelsPerInch;

    ReadInt(DVIFile, 2);

    NumPages = ReadInt(DVIFile, 2);

    Fonts.FreeFonts();
    Fonts.FlushShrinkedGlyphes();

    for(Command = ReadInt(DVIFile, 1); Command >= FontDef1 && Command <= FontDef4; Command = ReadInt(DVIFile, 1))
      if(!Fonts.LoadFont(this, DVIFile, Command))
        if(DisplayError)
          (*DisplayError)("Font not found!");

    Fonts.FreeUnusedFonts();

    if(Command != PostPost)
    {
      log_error("file corrupt?");
      return false;
    }

    delete [] PageOffset;

    PageOffset = new u_long[NumPages];

    PageOffset[NumPages - 1] = LastPageOffset;
    DVIFile->Seek(LastPageOffset, SEEK_SET);

    for(i = NumPages - 2; i >= 0; i--)
    {
      DVIFile->Seek(41, SEEK_CUR);
      DVIFile->Seek(PageOffset[i] = ReadInt(DVIFile, 4), SEEK_SET);
    }

    PageWidth  = (UnshrunkPageWidth  + Settings.ShrinkFactor - 1) / Settings.ShrinkFactor + 2;
    PageHeight = (UnshrunkPageHeight + Settings.ShrinkFactor - 1) / Settings.ShrinkFactor + 2;

    return true;
  }
  catch(const exception &e)
  {
    log_error("%s!", e.what());
    log_debug("at %s:%d", __FILE__, __LINE__);
    return false;
  }
  catch(...)
  {
    log_error("unknown exception!");
    log_debug("at %s:%d", __FILE__, __LINE__);
    return false;
  }
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//                                                                                                                //
// void DVI::SetDspInfo(DisplayInfo *dsp)                                                                         //
//                                                                                                                //
// Sets display data.                                                                                             //
//                                                                                                                //
// DisplayInfo *dsp                     display information                                                       //
//                                                                                                                //
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

void DVI::SetDspInfo(DisplayInfo *dsp)
{
  ASSERT(dsp != NULL);

  Settings.DspInfo = *dsp;

  OffsetX = Settings.DspInfo.PixelsPerInch;
  OffsetY = Settings.DspInfo.PixelsPerInch;

  Reload();
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//                                                                                                                //
// void DVI::ChangeFont(u_long n)                                                                                 //
//                                                                                                                //
// Changes the current font.                                                                                      //
//                                                                                                                //
// u_long n                             number of the new font                                                    //
//                                                                                                                //
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

void DVI::ChangeFont(DrawInfo *di, u_long n)
{
  if(n < Fonts.TableLength())
    if((di->CurFont = Fonts[n]) != NULL)
    {
      di->MaxChar = di->CurFont->MaxChar;
      di->SetChar = di->CurFont->SetChar;
    }
    else
    {
      di->MaxChar = ~0;
      di->SetChar = SetEmptyChar;
    }
  else
  {
    log_warn("non-existent font!");

    throw(exception("non-existent font"));
  }
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//                                                                                                                //
// void DVI::Draw(BView *vw, u_int PageNo)                                                                        //
//                                                                                                                //
// Draws a DVI-Document.                                                                                          //
//                                                                                                                //
// BView *vw                            view the document should be displayed in                                  //
// u_int PageNo                         page to be displayed                                                      //
//                                                                                                                //
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

void DVI::Draw(BView *vw, u_int PageNo)
{
  DrawInfo di;
  size_t   BufferLen;

  try
  {
    ASSERT(vw != NULL);

    vw->SetDrawingMode(B_OP_COPY);

    vw->FillRect(vw->Bounds(), B_SOLID_LOW);

    if(Settings.AntiAliasing && Settings.ShrinkFactor > 1)
      vw->SetDrawingMode(B_OP_MIN);
    else
      vw->SetDrawingMode(B_OP_OVER);

    // read page into memory

    if(PageNo < NumPages)                                       // this is a little bit more than the actual page
      BufferLen = PageOffset[PageNo] - PageOffset[PageNo - 1];
    else
    {
      BufferLen =  DVIFile->Seek(0, SEEK_END);
      BufferLen -= PageOffset[PageNo - 1];
    }

    DVIFile->Seek(PageOffset[PageNo - 1], SEEK_SET);

    di.BufferPos = new u_char[BufferLen];

    DVIFile->Read(di.BufferPos, BufferLen);

    di.Document   = this;
    di.vw         = vw;
    di.DspInfo    = &Settings.DspInfo;
    di.DimConvert = DimConvert;
    di.DrawDir    = 1;
    di.Virtual    = NULL;
    di.CurFont    = NULL;
    di.SetChar    = SetNoChar;
    di.File       = DVIFile;
    di.BufferEnd  = di.BufferPos + BufferLen;
    di.ScanFrame  = NULL;

    bzero(&di.Data, sizeof(di.Data));

    DrawPart(&di);

    delete [] (di.BufferEnd - BufferLen);
    di.BufferPos = NULL;
    di.BufferEnd = NULL;

    if(Settings.BorderLine)
    {
      BRect   r;
      pattern Dash = {0xf0, 0xf0, 0xf0, 0xf0, 0x0f, 0x0f, 0x0f, 0x0f};

      vw->SetDrawingMode(B_OP_COPY);

      r.Set(Settings.DspInfo.PixelsPerInch / Settings.ShrinkFactor,
            Settings.DspInfo.PixelsPerInch / Settings.ShrinkFactor,
            PageWidth  - Settings.DspInfo.PixelsPerInch / Settings.ShrinkFactor,
            PageHeight - Settings.DspInfo.PixelsPerInch / Settings.ShrinkFactor);

      vw->StrokeLine(BPoint(r.left,  0.0),      BPoint(r.left,        PageHeight - 1), Dash);
      vw->StrokeLine(BPoint(r.right, 0.0),      BPoint(r.right,       PageHeight - 1), Dash);
      vw->StrokeLine(BPoint(0.0,     r.top),    BPoint(PageWidth - 1, r.top),          Dash);
      vw->StrokeLine(BPoint(0.0,     r.bottom), BPoint(PageWidth - 1, r.bottom),       Dash);
    }

    vw->Sync();
  }
  catch(const exception &e)
  {
    log_warn("%s!", e.what());
    log_debug("at %s:%d", __FILE__, __LINE__);

    if(di.BufferPos && di.BufferEnd)
      delete [] (di.BufferEnd - BufferLen);

    if(DisplayError)
      (*DisplayError)(e.what());
  }
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//                                                                                                                //
// void DVI::DrawMagnify(BView *vw, u_int PageNo)                                                                 //
//                                                                                                                //
// Draws a DVI-Document unshrunken.                                                                               //
//                                                                                                                //
// BView *vw                            view the document should be displayed in                                  //
// u_int PageNo                         page to be displayed                                                      //
//                                                                                                                //
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

void DVI::DrawMagnify(BView *vw, u_int PageNo)
{
  u_int OldPageWidth  = PageWidth;
  u_int OldPageHeight = PageHeight;

  PageWidth  = UnshrunkPageWidth;
  PageHeight = UnshrunkPageHeight;

  Settings.SetMagnifyMode(true);

  Draw(vw, PageNo);

  Settings.SetMagnifyMode(false);

  PageWidth  = OldPageWidth;
  PageHeight = OldPageHeight;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//                                                                                                                //
// void DVI::DrawPart(DrawInfo *di)                                                                               //
//                                                                                                                //
// Draws a part of a document.                                                                                    //
//                                                                                                                //
// DrawInfo *di                         drawing information                                                       //
//                                                                                                                //
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

void DVI::DrawPart(DrawInfo *di)
{
  DrawInfo NewDI;
  DrawInfo *OldDI;
  int      ReflCount;
  u_char   c;

  while(true)
  {
    c = di->ReadInt(1);

    if(c <= (u_char)(SetChar0 + 127))
      (*di->SetChar)(di, c, c);

    else if(FontNum0 <= c && c <= (wchar)(FontNum0 + 63))
      ChangeFont(di, (u_long)(c - FontNum0));

    else
    {
      int32 a, b;

      switch(c)
      {
        case Set1:
        case Put1:
          (*di->SetChar)(di, c, di->ReadInt(1));
          break;

        case Set2:
        case Put2:
          (*di->SetChar)(di, c, di->ReadInt(2));
          break;

        case SetRule:
          a = di->ReadSInt(4) * DimConvert;
          b = di->ReadSInt(4) * DimConvert;

          if(a > 0 && b > 0 && !di->ScanFrame)
            DrawRule(di, Settings.ToPixel(b), Settings.ToPixel(a));

          di->Data.Horiz += di->DrawDir * b;
          break;

        case PutRule:
          a = di->ReadSInt(4) * DimConvert;
          b = di->ReadSInt(4) * DimConvert;

          if(a > 0 && b > 0 && !di->ScanFrame)
            DrawRule(di, Settings.ToPixel(b), Settings.ToPixel(a));
          break;

        case NOP:
          break;

        case BeginOP:
          di->Skip(44);

          di->Data.Horiz  = OffsetX << 16;
          di->Data.Vert   = OffsetY << 16;
          di->Data.PixelV = Settings.PixelConv(di->Data.Vert);
          di->Data.w      = 0;
          di->Data.x      = 0;
          di->Data.y      = 0;
          di->Data.z      = 0;

          break;

        case EndOP:
          if(!di->Frames.empty())
          {
            log_warn("stack not empty at Pop!");
            throw(exception("stack not empty at EOP"));
          }
          if(di->PSIface)
            di->PSIface->EndPage();
          return;

        case Push:
          di->Frames.push(di->Data);
          break;

        case Pop:
          if(di->Frames.empty())
          {
            log_warn("stack empty at EOP!");
            throw(exception("stack empty at Pop"));
          }

          di->Data = di->Frames.top();
          di->Frames.pop();
          break;

        case StartRefl:
          if(!di->ScanFrame)
          {
            NewDI.Document     = di->Document;
            NewDI.vw           = di->vw;
            NewDI.Data         = di->Data;
            NewDI.DspInfo      = di->DspInfo;
            NewDI.DimConvert   = di->DimConvert;
            NewDI.DrawDir      = di->DrawDir;
            NewDI.CurFont      = di->CurFont;
            NewDI.VirtTable    = di->VirtTable;
            NewDI.Virtual      = di->Virtual;
            NewDI.MaxChar      = di->MaxChar;
            NewDI.SetChar      = di->SetChar;
            NewDI.BufferPos    = di->BufferPos;
            NewDI.BufferEnd    = di->BufferEnd;
            NewDI.ScanFrame    = di->ScanFrame;
            NewDI.Frames       = di->Frames;

            OldDI = di;
            di    = &NewDI;

            di->ScanFrame = &di->Frames.top();
            ReflCount     = 0;
          }
          else
          {
            if(di->ScanFrame = &di->Frames.top())
              ReflCount++;
          }
          break;

        case EndRefl:
          if(di->ScanFrame)
          {
            if(di->ScanFrame == &di->Frames.top() && --ReflCount < 0)
            {
              di->ScanFrame = NULL;

              di->Frames.push(di->Data);

              OldDI->Frames = di->Frames;
              di = OldDI;                  // this restores the old file position!!!

              di->Data.Horiz  = di->Frames.top().Horiz;
              di->Data.Vert   = di->Frames.top().Vert;
              di->Data.PixelV = di->Frames.top().PixelV;
              di->DrawDir     = -di->DrawDir;
            }
          }
          else
          {
            di->DrawDir = -di->DrawDir;

            if(di->Frames.empty())
            {
              log_warn("stack empty at Pop!");
              throw(exception("stack empty at Pop"));
            }

            di->Data = di->Frames.top();
            di->Frames.pop();
          }
          break;

        case Right1:
        case Right2:
        case Right3:
        case Right4:
          di->Data.Horiz += di->DrawDir * di->ReadSInt(c - Right1 + 1) * DimConvert;
          break;

        case W1:
        case W2:
        case W3:
        case W4:
          di->Data.w      = di->ReadSInt(c - W0) * DimConvert;
        case W0:
          di->Data.Horiz += di->DrawDir * di->Data.w;
          break;

        case X1:
        case X2:
        case X3:
        case X4:
          di->Data.x      = di->ReadSInt(c - X0) * DimConvert;
        case X0:
          di->Data.Horiz += di->DrawDir * di->Data.x;
          break;

        case Down1:
        case Down2:
        case Down3:
        case Down4:
          di->Data.Vert  += di->ReadSInt(c - Down1 + 1) * DimConvert;
          di->Data.PixelV = Settings.PixelConv(di->Data.Vert);
          break;

        case Y1:
        case Y2:
        case Y3:
        case Y4:
          di->Data.y      = di->ReadSInt(c - Y0) * DimConvert;
        case Y0:
          di->Data.Vert  += di->Data.y;
          di->Data.PixelV = Settings.PixelConv(di->Data.Vert);
          break;

        case Z1:
        case Z2:
        case Z3:
        case Z4:
          di->Data.z      = di->ReadSInt(c - Z0) * DimConvert;
        case Z0:
          di->Data.Vert  += di->Data.z;
          di->Data.PixelV = Settings.PixelConv(di->Data.Vert);
          break;

        case Font1:
        case Font2:
        case Font3:
        case Font4:
          ChangeFont(di, di->ReadInt(c - Font1 + 1));
          break;

        case XXX1:
        case XXX2:
        case XXX3:
        case XXX4:
          a = di->ReadInt(c - XXX1 + 1);

          if(a > 0)
            Special(di, a);
          break;

        case FontDef1:
        case FontDef2:
        case FontDef3:
        case FontDef4:
          di->Skip(13 + c - FontDef1);
          di->Skip(di->ReadInt(1) + di->ReadInt(1));
          break;

        default:
          log_warn("invalid operand: 0x%02x!\n", (u_int)c);

          throw(exception("invalid operand"));
          break;
      }
    }
  }
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//                                                                                                                //
// void DVI::SetEmptyChar(DrawInfo *di, wchar cmd, wchar c)                                                       //
//                                                                                                                //
// Does nothing.                                                                                                  //
//                                                                                                                //
// DrawInfo *di                         drawing information                                                       //
// wchar    cmd                         command to draw the char (either Put1 or Set1)                            //
// wchar    c                           character to be drawn                                                     //
//                                                                                                                //
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

void DVI::SetEmptyChar(DrawInfo * /* di */, wchar /* cmd */, wchar /* c */)
{
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//                                                                                                                //
// void DVI::SetNoChar(DrawInfo *di, wchar cmd, wchar c)                                                          //
//                                                                                                                //
// Draws a character if currently no font is selected.                                                            //
//                                                                                                                //
// DrawInfo *di                         drawing information                                                       //
// wchar    cmd                         command to draw the char (either Put1 or Set1)                            //
// wchar    c                           character to be drawn                                                     //
//                                                                                                                //
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

void DVI::SetNoChar(DrawInfo *di, wchar cmd, wchar c)
{
  if(di->Virtual)
  {
    di->CurFont = di->Virtual->FirstFont;

    if(di->CurFont != NULL)
    {
       di->SetChar = di->CurFont->SetChar;

       (*di->SetChar)(di, cmd, c);
    }
  }
  else
    throw(exception("no font set"));
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//                                                                                                                //
// void DVI::SetVFChar(DrawInfo *di, wchar cmd, wchar c)                                                          //
//                                                                                                                //
// Draws a character if currently a virtual font is selected.                                                     //
//                                                                                                                //
// DrawInfo *di                         drawing information                                                       //
// wchar    cmd                         command to draw the char (either Put1 or Set1)                            //
// wchar    c                           character to be drawn                                                     //
//                                                                                                                //
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

void DVI::SetVFChar(DrawInfo *di, wchar cmd, wchar c)
{
  Macro         *m;
  DrawInfo      NewInfo;
  long          horiz;
  static u_char ch;

  if((m = &di->CurFont->Macros[c])->Position == NULL)
  {
    m->Position = &ch;
    m->End      = &ch;
    return;
  }

  horiz = di->Data.Horiz;

  if(di->DrawDir < 0)
    di->Data.Horiz -= m->Advance;

  if(!di->ScanFrame)
  {
    NewInfo.Document     = di->Document;
    NewInfo.vw           = di->vw;
    NewInfo.Data         = di->Data;
    NewInfo.Data.w       = 0;
    NewInfo.Data.x       = 0;
    NewInfo.Data.y       = 0;
    NewInfo.Data.z       = 0;
    NewInfo.DspInfo      = di->DspInfo;
    NewInfo.DimConvert   = di->DimConvert;
    NewInfo.DrawDir      = di->DrawDir;
    NewInfo.CurFont      = di->CurFont;
    NewInfo.VirtTable    = &di->CurFont->VFTable;
    NewInfo.Virtual      = di->Virtual;
    NewInfo.MaxChar      = di->MaxChar;
    NewInfo.SetChar      = di->SetChar;
    NewInfo.BufferPos    = m->Position;
    NewInfo.BufferEnd    = m->End;
    NewInfo.ScanFrame    = di->ScanFrame;

    NewInfo.Frames       = di->Frames;

    di->Document->DrawPart(&NewInfo);

    di->Frames = NewInfo.Frames;
  }
  if(cmd == Put1 || cmd == Put2)
    di->Data.Horiz = horiz;
  else
    if(di->DrawDir > 0)
      di->Data.Horiz += m->Advance;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//                                                                                                                //
// void DVI::SetChar(DrawInfo *di, wchar cmd, wchar c)                                                            //
//                                                                                                                //
// Draws a character if currently a normal font is selected.                                                      //
//                                                                                                                //
// DrawInfo *di                         drawing information                                                       //
// wchar    cmd                         command to draw the char (either Put1 or Set1)                            //
// wchar    c                           character to be drawn                                                     //
//                                                                                                                //
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

void DVI::SetChar(DrawInfo *di, wchar cmd, wchar c)
{
  Glyph *g;
  long  horiz;

  if(c > di->MaxChar)
  {
    log_info("character out of range: %d", (int)c);
    return;
  }

  g = &di->CurFont->Glyphs[c];

  if(g->UBitMap == NULL)
    di->CurFont->ReadChar(di->CurFont, c);

  if(g->UBitMap == NULL)
    return;

  horiz = di->Data.Horiz;

  if(di->DrawDir < 0)
    di->Data.Horiz -= g->Advance;

  if(!di->ScanFrame)
  {
    if(di->Document->Settings.ShrinkFactor == 1)
      di->vw->DrawBitmapAsync(g->UBitMap, BPoint(di->PixelConv(di->Data.Horiz) - g->Ux, di->Data.PixelV - g->Uy));

    else
    {
      g->Shrink(di->Document->Settings.ShrinkFactor, di->Document->Settings.AntiAliasing);

      di->vw->DrawBitmapAsync(g->SBitMap, BPoint(di->PixelConv(di->Data.Horiz) - g->Sx, di->Data.PixelV - g->Sy));
    }
  }
  if(cmd == Put1 || cmd == Put2)
    di->Data.Horiz = horiz;
  else
    if(di->DrawDir > 0)
      di->Data.Horiz += g->Advance;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//                                                                                                                //
// void DVI::DrawRule(DrawInfo *di, long w, long h)                                                               //
//                                                                                                                //
// Draws a rule.                                                                                                  //
//                                                                                                                //
// DrawInfo *di                         drawing information                                                       //
// long     w, h                        size of the rule                                                          //
//                                                                                                                //
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

void DVI::DrawRule(DrawInfo *di, long w, long h)
{
  BRect r(0.0, 0.0, (w > 0 ? w : 1) - 1, (h > 0 ? h : 1) - 1);

  r.OffsetTo((float)(di->PixelConv(di->Data.Horiz) - (di->DrawDir < 0 ? w - 1 : 0)),
             (float)(di->Data.PixelV - h + 1));
  di->vw->FillRect(r, B_SOLID_HIGH);
}
