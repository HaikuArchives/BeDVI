////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//                                                                                                                //
// $Id: DVI.cc,v 2.6 1999/07/25 13:24:20 achim Exp $
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
#include "DVI-DrawPage.h"
#include "FontList.h"
#include "TeXFont.h"
#include "log.h"

static const int NoMagStep = -29999;
static const int NoBuild   =  29999;


/* DisplayInfo ****************************************************************************************************/


////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//                                                                                                                //
// static const uint         _NumModes;                                                                           //
// static const char * const _ModeNames[];                                                                        //
// static const uint         _ModeDPI[];                                                                          //
//                                                                                                                //
// default modes.                                                                                                 //
//                                                                                                                //
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

static const uint   _NumModes             = 3;
static const char * _ModeNames[_NumModes] = {"nextscrn", "cx", "ljfour"};
static const uint   _ModeDPI[_NumModes]   = {100, 300, 600};

uint        DisplayInfo::NumModes    = _NumModes;
const char  **DisplayInfo::ModeNames = &_ModeNames[0];
uint        *DisplayInfo::ModeDPI    = (uint *)&_ModeDPI[0];

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
// void DisplayInfo::SetModes(uint num, const char **names, uint *dpi)                                            //
//                                                                                                                //
// sets the list of supported resolutions. The arrays aren't copied and must therefor be preserved until the      //
// the object is deleted or this function is called with other arrays.                                            //
//                                                                                                                //
// uint        num                      number of modes                                                           //
// const char  **names                  array of mode names                                                       //
// uint        *dpi                     array of resolutions                                                      //
//                                                                                                                //
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

void DisplayInfo::SetModes(uint num, const char **names, uint *dpi)
{
  int        i, j;
  uint       OldDPI;
  const char *OldName;

  // sort modes (InsertionSort)

  for (i = 1; i < num; i++)
  {
    OldDPI  = dpi[i];
    OldName = names[i];

    for (j = i; j > 0 && dpi[j - 1] >= OldDPI; j--)
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
// void DisplayInfo::SetResolution(uint dpi)                                                                      //
//                                                                                                                //
// Sets the resolution information.                                                                               //
//                                                                                                                //
// uint dpi                             dpi value                                                                 //
//                                                                                                                //
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

void DisplayInfo::SetResolution(uint dpi)
{
  int i;

  // search for mode name

  for (i = NumModes - 1; i > 0; i--)
    if (ModeDPI[i] <= dpi)
      break;

  Mode          = ModeNames[i];
  PixelsPerInch = dpi;
};


/* DVI ************************************************************************************************************/


////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//                                                                                                                //
// DVI::DVI(BPositionIO *File, DrawSettings *Settings, void (*DspError)(const char *) = NULL)                     //
//                                                                                                                //
// Initializes a DVI-Document.                                                                                    //
//                                                                                                                //
// BPositionIO  *File                   file of the document                                                      //
// DrawSettings *Settings               various options                                                           //
// void (*DspError)(const char *)       function to be called when an error occures                               //
//                                                                                                                //
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

DVI::DVI(BPositionIO *File, DrawSettings *Settings, void (*DspError)(const char *)):
  Fonts(),
  DVIFile(File),
  Name(NULL),
  PageOffset(NULL),
  Magnification(1000),
  DimConvert(1.0),
  OffsetX(Settings->DspInfo.PixelsPerInch),
  OffsetY(Settings->DspInfo.PixelsPerInch),
  DisplayError(DspError)
{
  if (!Reload(Settings))
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
// int DVI::MagStepValue(int PixelsPerInch, float &mag) const                                                     //
//                                                                                                                //
// Adjusts a magnification value.                                                                                 //
//                                                                                                                //
// int   PixelsPerInch                  resolution                                                                //
// float &mag                           magnification                                                             //
//                                                                                                                //
// Result:                              adjusted value                                                            //
//                                                                                                                //
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

int DVI::MagStepValue(int PixelsPerInch, float &mag) const
{
  int  Result;
  uint dpi;

  acquire_sem(kpse_sem);
  dpi = kpse_magstep_fix((uint)mag, PixelsPerInch, &Result);
  release_sem(kpse_sem);

  mag = (float)dpi;

  return Result ? Result : NoMagStep;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//                                                                                                                //
// bool DVI::Reload(DrawSettings *Settings)                                                                       //
//                                                                                                                //
// Reads general data of a DVI file.                                                                              //
//                                                                                                                //
// DrawSettings *Settings               settings                                                                  //
//                                                                                                                //
// Result:                              `true' if successful, otherwise `false'                                   //
//                                                                                                                //
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

bool DVI::Reload(DrawSettings *Settings)
{
  static const int BufferLen = 512;

  long    Numerator;
  long    Denominator;
  int     len;
  long    LastPageOffset;
  long    pos;
  uchar   *p;
  uchar   Buffer[BufferLen];
  uchar   x;
  uchar   Command;
  int     i;

  try
  {
    log_info("loading...");

    ASSERT(DVIFile != NULL);

    DVIFile->Seek(0, SEEK_SET);

    if (ReadInt(DVIFile, 1) != Preamble)
    {
      log_error("not a DVI file!");
      return false;
    }
    if ((i = ReadInt(DVIFile, 1)) != 2)
    {
      log_error("wrong DVI version (%d)!", i);
      return false;
    }

    Numerator     = ReadInt(DVIFile, 4);
    Denominator   = ReadInt(DVIFile, 4);
    Magnification = ReadInt(DVIFile, 4);
    DimConvert    = (((double)Numerator * Magnification) / ((double)Denominator * 1000.0));
    DimConvert    = DimConvert * (((long)Settings->DspInfo.PixelsPerInch) << 16) / 254000;
    TPicConvert   = Settings->DspInfo.PixelsPerInch * Magnification / 1000000.0;

    len = ReadInt(DVIFile, 1);

    Name = new char[len + 1];

    DVIFile->Read(Name, len);
    Name[len] = 0;

    pos = DVIFile->Seek(0, SEEK_END);

    if (pos > BufferLen)
      pos -= BufferLen;
    else
      pos = 0;

    DVIFile->Seek(pos, SEEK_SET);

    p = &Buffer[DVIFile->Read(&Buffer, BufferLen)];

    do
    {
      while (p > Buffer && *--p != Trailer)
        ;

      if (p <= Buffer)
      {
        log_error("trailer not found!");
        return false;
      }
    }
    while (p[-1] != Trailer || p[-2] != Trailer || p[-3] != Trailer);

    pos += p - Buffer;

    for (x = *p; x == Trailer; x = ReadInt(DVIFile, 1))
      DVIFile->Seek(--pos, SEEK_SET);

    if (x != 2)
    {
      log_error("file corrupt?");
      return false;
    }

    DVIFile->Seek(pos - 4, SEEK_SET);
    DVIFile->Seek((long)ReadInt(DVIFile, 4), SEEK_SET);

    if (ReadInt(DVIFile, 1) != Postamble)
    {
      log_error("file corrupt?");
      return false;
    }

    LastPageOffset = ReadInt(DVIFile, 4);

    if (ReadInt(DVIFile, 4) != Numerator || ReadInt(DVIFile, 4) != Denominator || ReadInt(DVIFile, 4) != Magnification)
    {
      log_error("file corrupt?");
      return false;
    }

    UnshrunkPageHeight = ((long)((long)ReadInt(DVIFile, 4) * DimConvert) >> 16) + 2 * Settings->DspInfo.PixelsPerInch;
    UnshrunkPageWidth  = ((long)((long)ReadInt(DVIFile, 4) * DimConvert) >> 16) + 2 * Settings->DspInfo.PixelsPerInch;

    ReadInt(DVIFile, 2);

    NumPages = ReadInt(DVIFile, 2);

    Fonts.FreeFonts();
    Fonts.FlushShrinkedGlyphes();

    for (Command = ReadInt(DVIFile, 1); Command >= FontDef1 && Command <= FontDef4; Command = ReadInt(DVIFile, 1))
      if (!Fonts.LoadFont(this, Settings, DVIFile, NULL, Command))
        if (DisplayError)
          (*DisplayError)("Font not found!");

    Fonts.FreeUnusedFonts();

    if (Command != PostPost)
    {
      log_error("file corrupt?");
      return false;
    }

    delete [] PageOffset;

    PageOffset = new ulong[NumPages];

    PageOffset[NumPages - 1] = LastPageOffset;
    DVIFile->Seek(LastPageOffset, SEEK_SET);

    for (i = NumPages - 2; i >= 0; i--)
    {
      DVIFile->Seek(41, SEEK_CUR);
      DVIFile->Seek(PageOffset[i] = ReadInt(DVIFile, 4), SEEK_SET);
    }

    PageWidth  = (UnshrunkPageWidth  + Settings->ShrinkFactor - 1) / Settings->ShrinkFactor + 2;
    PageHeight = (UnshrunkPageHeight + Settings->ShrinkFactor - 1) / Settings->ShrinkFactor + 2;

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
// void DVI::Draw(BView *vw, DrawSettings *Settings, uint PageNo)                                                 //
//                                                                                                                //
// Draws a DVI-Document.                                                                                          //
//                                                                                                                //
// BView        *vw                     view the document should be displayed in                                  //
// DrawSettings *Settings               settings used to draw the page                                            //
// uint         PageNo                  page to be displayed                                                      //
//                                                                                                                //
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

void DVI::Draw(BView *vw, DrawSettings *Settings, uint PageNo)
{
  DrawPage dp(*Settings);
  size_t   BufferLen;

  vw->PushState();

  try
  {
    PageWidth  = (UnshrunkPageWidth  + Settings->ShrinkFactor - 1) / Settings->ShrinkFactor + 2;
    PageHeight = (UnshrunkPageHeight + Settings->ShrinkFactor - 1) / Settings->ShrinkFactor + 2;

    // read page into memory

    if (PageNo < NumPages)                                       // this is a little bit more than the actual page
      BufferLen = PageOffset[PageNo] - PageOffset[PageNo - 1];
    else
    {
      BufferLen =  DVIFile->Seek(0, SEEK_END);
      BufferLen -= PageOffset[PageNo - 1];
    }

    DVIFile->Seek(PageOffset[PageNo - 1], SEEK_SET);

    dp.BufferPos = new uchar[BufferLen];

    DVIFile->Read(dp.BufferPos, BufferLen);

    dp.Document    = this;
    dp.vw          = vw;
    dp.TPicConvert = TPicConvert;
    dp.DimConvert  = DimConvert;
    dp.DrawDir     = 1;
    dp.Virtual     = NULL;
    dp.CurFont     = NULL;
    dp.SetChar     = dp.SetNoChar;
    dp.File        = DVIFile;
    dp.BufferEnd   = dp.BufferPos + BufferLen;
    dp.ScanFrame   = NULL;

    memset(&dp.Data, 0, sizeof(dp.Data));

    // draw it

    vw->SetHighColor( 0,   0,   0, 255);
    vw->SetLowColor(255, 255, 255, 255);

    vw->SetDrawingMode(B_OP_COPY);
    vw->FillRect(vw->Bounds(), B_SOLID_LOW);

    if (dp.Settings.AntiAliasing && dp.Settings.ShrinkFactor > 1)
      vw->SetDrawingMode(B_OP_MIN);
    else
      vw->SetDrawingMode(B_OP_OVER);

    dp.DrawPart();

    delete [] (dp.BufferEnd - BufferLen);
    dp.BufferPos = NULL;
    dp.BufferEnd = NULL;

    // draw border

    if (dp.Settings.BorderLine)
    {
      BRect   r;
      pattern Dash = {0xf0, 0xf0, 0xf0, 0xf0, 0x0f, 0x0f, 0x0f, 0x0f};

      vw->SetDrawingMode(B_OP_COPY);

      r.Set(dp.Settings.DspInfo.PixelsPerInch / dp.Settings.ShrinkFactor,
            dp.Settings.DspInfo.PixelsPerInch / dp.Settings.ShrinkFactor,
            PageWidth  - dp.Settings.DspInfo.PixelsPerInch / dp.Settings.ShrinkFactor,
            PageHeight - dp.Settings.DspInfo.PixelsPerInch / dp.Settings.ShrinkFactor);

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

    if (dp.BufferPos && dp.BufferEnd)
      delete [] (dp.BufferEnd - BufferLen);

    if (DisplayError)
      (*DisplayError)(e.what());
  }

  Settings->StringFound = dp.StringFound();

  vw->PopState();
}
