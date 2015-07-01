////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//                                                                                                                //
// $Id: DVI-Special.cc,v 2.2 1998/07/09 13:36:20 achim Exp $
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
#include <ctype.h>
#include <string.h>
#include <Debug.h>
#include "defines.h"

extern "C"
{
#define string _string
#include <kpathsea/c-auto.h>
#include <kpathsea/tex-file.h>
#undef string
}

#include "DVI.h"
#include "log.h"


/* drawing functions **********************************************************************************************/

static const int MaxPoints  = 300;
static const int MaxPenSize = 7;

static struct { int x, y; } Points[MaxPoints];

static int       PathLen = 0;
static int       PenSize = 1;
static bool      Whiten  = false;
static bool      Blacken = false;
static bool      Shade   = false;
static bool      BBoxValid;
static u_int     BBoxWidth;
static u_int     BBoxHeight;
static int       BBoxVOffset;

inline int ConvX(DrawInfo *di, int x)
{
  return (di->Document->TPicConvert * x / di->Document->Settings.ShrinkFactor) + di->PixelConv(di->Data.Horiz);
}

inline int ConvY(DrawInfo *di, int y)
{
  return (di->Document->TPicConvert * y / di->Document->Settings.ShrinkFactor) + di->Data.PixelV;
}

static void SetPenSize(DrawInfo *di, char *cmd)
{
  int Size;

  if(sscanf(cmd, " %d ", &Size) != 1)
    return;

  PenSize = (2 * Size * (di->DspInfo->PixelsPerInch / di->Document->Settings.ShrinkFactor) + 1000) / 2000;

  if(PenSize < 1)
    PenSize = 1;
  else if(PenSize > MaxPenSize)
    PenSize = MaxPenSize;
}

static void FlushPath(DrawInfo *di)
{
  const rgb_color black = {0, 0, 0, 255};
  int i;

  if(PathLen)
  {
    di->vw->BeginLineArray(PathLen);

    for(i = 1; i < PathLen; i++)
      di->vw->AddLine(BPoint(ConvX(di, Points[i].x),     ConvY(di, Points[i].y)),
                      BPoint(ConvX(di, Points[i + 1].x), ConvY(di, Points[i + 1].y)),
                      black);

    di->vw->EndLineArray();
  }
  PathLen = 0;
}

static void FlushDashed(DrawInfo *di, char *cmd, bool dotted)
{
  const rgb_color black = {0, 0, 0, 255};

  int    i;
  int    NumDots;
  int    lx0, ly0, lx1, ly1;
  int    cx0, cy0, cx1, cy1;
  float  InchesPerDash;
  double d, SpaceSize, a, b, dx, dy, MilliPerDash;

  if(sscanf(cmd, " %f ", &InchesPerDash) != 1)
    return;

  if(PathLen <= 1 || InchesPerDash <= 0.0)
    return;

  MilliPerDash = InchesPerDash * 1000.0;

  lx0 = Points[1].x;
  ly0 = Points[1].y;
  lx1 = Points[2].x;
  ly1 = Points[2].y;

  dx = lx1 - lx0;
  dy = ly1 - ly0;

  if(dotted)
  {
    NumDots = sqrt(dx*dx + dy*dy) / MilliPerDash + 0.5;

    if(NumDots == 0)
      NumDots = 1;

    di->vw->BeginLineArray(NumDots + 1);

    for(i = 0; i <= NumDots; i++)
    {
      a = (float) i / NumDots;

      cx0 = lx0 + a * dx + 0.5;
      cy0 = ly0 + a * dy + 0.5;

      di->vw->AddLine(BPoint(ConvX(di, cx0), ConvY(di, cy0)),
                      BPoint(ConvX(di, cx0), ConvY(di, cy0)),
                      black);
    }
    di->vw->EndLineArray();
  }
  else
  {
    d = sqrt(dx*dx + dy*dy);

    NumDots = d / (2.0 * MilliPerDash) + 1.0;

    di->vw->BeginLineArray(NumDots + 1);

    if(NumDots <= 1)
      di->vw->AddLine(BPoint(ConvX(di, lx0), ConvY(di, ly0)),
                      BPoint(ConvX(di, lx1), ConvY(di, ly1)),
                      black);
    else
    {
      SpaceSize = (d - NumDots * MilliPerDash) / (NumDots - 1);

      for(i = 0; i < NumDots - 1; i++)
      {
        a   = i * (MilliPerDash + SpaceSize) / d;
        b   = a + MilliPerDash / d;

        cx0 = lx0 + a * dx + 0.5;
        cy0 = ly0 + a * dy + 0.5;
        cx1 = lx0 + b * dx + 0.5;
        cy1 = ly0 + b * dy + 0.5;

        di->vw->AddLine(BPoint(ConvX(di, cx0), ConvY(di, cy0)),
                        BPoint(ConvX(di, cx1), ConvY(di, cy1)),
                        black);

        b += SpaceSize / d;
      }
      cx0 = lx0 + b * dx + 0.5;
      cy0 = ly0 + b * dy + 0.5;

      di->vw->AddLine(BPoint(ConvX(di, cx0), ConvY(di, cy0)),
                      BPoint(ConvX(di, lx1), ConvY(di, ly1)),
                      black);
    }
    di->vw->EndLineArray();
  }

  PathLen = 0;
}

static void AddPath(char *cmd)
{
  int PathX, PathY;

  if(++PathLen >= MaxPoints)
     return;

  if(sscanf(cmd, " %d %d ", &PathX, &PathY) != 2)
     return;

  Points[PathLen].x = PathX;
  Points[PathLen].y = PathY;
}

static void Arc(DrawInfo *di, char *cmd, bool invis)
{
  int    CenterX, CenterY;
  int    RadX, RadY;
  float  StartAngle, EndAngle;

  if(sscanf(cmd, " %d %d %d %d %f %f ", &CenterX, &CenterY, &RadX, &RadY, &StartAngle, &EndAngle) != 6)
    return;

  if(invis)
    return;

  StartAngle *= 180 / PI;
  EndAngle   *= 180 / PI;

  di->vw->StrokeArc(BPoint(ConvX(di, CenterX), ConvY(di, CenterY)),
                    ConvX(di, RadX) - ConvX(di, 0),
                    ConvY(di, RadY) - ConvY(di, 0),
                    StartAngle,
                    EndAngle - StartAngle);
}

static inline int dist(int x0, int y0, int x1, int y1)
{
  return abs(x0 - x1) + abs(y0 - y1);
}

static void FlushSpline(DrawInfo *di)
{
  const rgb_color black = {0, 0, 0, 255};

  int  xp, yp;
  int  N;
  int  LastX, LastY;
  bool LastValid = false;
  int  t1, t2, t3;
  int  Steps;
  int  j;
  int  i, w;

  N = PathLen + 1;

  Points[0] = Points[1];
  Points[N] = Points[N - 1];

  for(i = 0; i < N - 1; i++)
  {
    Steps = (dist(Points[i].x,   Points[i].y,   Points[i+1].x, Points[i+1].y) +
		dist(Points[i+1].x, Points[i+1].y, Points[i+2].x, Points[i+2].y)) / 80;

    di->vw->BeginLineArray(Steps);

    for(j = 0; j < Steps; j++)
    {
      w = (j * 1000 + 500) / Steps;
      t1 = w * w / 20;
      w -= 500;
      t2 = (750000 - w * w) / 10;
      w -= 500;
      t3 = w * w / 20;

      xp = (t1 * Points[i+2].x + t2 * Points[i+1].x + t3 * Points[i].x + 50000) / 100000;
      yp = (t1 * Points[i+2].y + t2 * Points[i+1].y + t3 * Points[i].y + 50000) / 100000;

      if(LastValid)
        di->vw->AddLine(BPoint(ConvX(di, LastX), ConvY(di, LastY)),
                        BPoint(ConvX(di, xp),    ConvY(di, yp)),
                        black);
      LastX     = xp;
      LastY     = yp;
      LastValid = true;
    }
    di->vw->EndLineArray();
  }

  PathLen = 0;
}

static void ShadeLast()
{
  Blacken = false;
  Whiten  = false;
  Shade   = true;
}

static void WhitenLast()
{
  Blacken = false;
  Whiten  = true;
  Shade   = false;
}

static void BlackenLast()
{
  Blacken = true;
  Whiten  = false;
  Shade   = false;
}

static char *FindFigFile(char *FileName, int *decompress)
{
  char *Name;
  
  if(*FileName == '`')
  {
    Name = FileName + 1;

    *decompress = 1;
  }
  else
  {
    Name = kpse_find_pict(FileName);

    *decompress = 0;
  }
  
  return Name;
}

static char *ReadLine(FILE *f)
{
  char *Line;
  char *p;
  int  Size = 100;

  if(!(Line = (char *)malloc(Size)))
    return NULL;

  p = Line;

  do
  {
    if(p >= Line + Size)
    {
      Size += 100;

      if(!(Line = (char *)realloc(Line, Size)))
        return NULL;
    }
    if(fread(p, sizeof(char), 1, f) != sizeof(char))
      break;
  }
  while(*p++ != '\n');

  p[-1] = 0;

  return Line;
}

static void DrawFile(DrawInfo *di, char *Name, int Decompress)
{
  FILE *Pipe;
  char *Line;

  if(Decompress)
  {
    if(Pipe = popen(Name, "r"))
    {
      while(Line = ReadLine(Pipe))
      {
        di->PSIface->DrawRaw(Line);
        free(Line);
      }
      pclose(Pipe);
    }
  }
  else
    di->PSIface->DrawFile(Name);
}

static void PSfigSpecial(DrawInfo *di, char *cmd)
{
  char	*FileName;
  char *Name;
  int  RawW, RawH;
  int  Decompress;

  di->InitPSIface();

  if(strncmp(cmd, ":[begin]", 8) == 0)
  {
    cmd += 8;

    BBoxValid = false;

    if(sscanf(cmd, "%d %d\n", &RawW, &RawH) >= 2)
    {
      BBoxValid   = true;
      BBoxWidth   = di->PixelConv(RawW * di->DimConvert);
      BBoxHeight  = di->PixelConv(RawH * di->DimConvert);
      BBoxVOffset = 0;
    }
    di->PSIface->DrawBegin(di, di->PixelConv(di->Data.Horiz), di->Data.PixelV, cmd);
  }
  else if (strncmp(cmd, " plotfile ", 10) == 0)
  {
    cmd += 10;

    while(isspace(*cmd))
      cmd++;

    for(FileName = cmd; !isspace(*cmd); cmd++)
      ;

    *cmd = '\0';

    if(Name = FindFigFile(FileName, &Decompress))
    {
      DrawFile(di, Name, Decompress);

      if(!Decompress && Name != FileName)
        free(Name);
    }
  }
  else if (strncmp(cmd, ":[end]", 6) == 0)
  {
    cmd += 6;

    di->PSIface->DrawEnd(cmd);

    BBoxValid = false;
  }
  else
  {
    if(*cmd == ':')
      cmd++;

    di->PSIface->DrawBegin(di, di->PixelConv(di->Data.Horiz), di->Data.PixelV, "");
    di->PSIface->DrawEnd("");
    di->PSIface->DrawRaw(cmd);
  }
}

static void epsfSpecial(DrawInfo *di, char *cmd)
{
  static const char *KeyTab[] =
  {
    "clip", 
    "llx",
    "lly",
    "urx",
    "ury",
    "rwi",
    "rhi",
    "hsize",
    "vsize",
    "hoffset",
    "voffset",
    "hscale",
    "vscale",
    "angle"
  };

  static const int NumKeys        = sizeof(KeyTab) / sizeof(char *);
  static const int NumArglessKeys = 1;

  static char     *Buffer;
  static u_int    BufLen = 0;

  char   *FileName, *Name;
  int    Decompress;
  u_int  len;
  char   *p;
  char   *q;
  int    Flags = 0;
  double KeyVal[6];

  if(memcmp(cmd, "ile=", 4) != 0)
    return;

  p        = cmd + 4;
  FileName = p;

  if(*p == '\'' || *p == '"')
  {
    do
    {
      p++;
    }
    while(*p != '\0' && *p != *FileName);

    FileName++;
  }
  else
    while(*p != '\0' && *p != ' ' && *p != '\t')
      p++;

  if(*p != '\0')
    *p++ = '\0';

  Name = FindFigFile(FileName, &Decompress);

  while(*p == ' ' || *p == '\t')
    p++;

  len = strlen(p) + NumKeys + 30;

  if(BufLen < len)
  {
    if(BufLen != 0)
      delete [] Buffer;

    BufLen = len;
    try
    {
      Buffer = new char[BufLen];
    }
    catch(...)
    {
      Buffer = NULL;
      return;
    }
  }

  strcpy(Buffer, "@beginspecial");
  q = Buffer + strlen(Buffer);

  while (*p != '\0')
  {
    char *p1 = p;
    int  KeyNo;

    while(*p1 != '=' && !isspace(*p1) && *p1 != '\0')
      p1++;

    for(KeyNo = 0;; KeyNo++)
    {
      if(KeyNo >= NumKeys)
        break;

      if(memcmp(p, KeyTab[KeyNo], p1 - p) == 0)
      {
        if(KeyNo >= NumArglessKeys)
        {
          if(*p1 == '=')
            p1++;

          if(KeyNo < NumArglessKeys + 6)
          {
            KeyVal[KeyNo - NumArglessKeys] = atof(p1);

            Flags |= (1 << (KeyNo - NumArglessKeys));
          }
          *q++ = ' ';

          while(!isspace(*p1) && *p1 != '\0')
            *q++ = *p1++;
        }
        *q++ = ' ';
        *q++ = '@';

        strcpy(q, KeyTab[KeyNo]);
        q += strlen(q);
        break;
      }
    }
    p = p1;

    while(!isspace(*p) && *p != '\0')
      p++;

    while(isspace(*p))
      p++;
  }

  strcpy(q, " @setspecial\n");

  BBoxValid = false;

  if((Flags & 0x30) == 0x30 || ((Flags & 0x30) && (Flags & 0xf) == 0xf))
  {
    BBoxValid = true;

    BBoxWidth   = 0.1 * ((Flags & 0x10) ? KeyVal[4] :
                  KeyVal[5] * (KeyVal[2] - KeyVal[0]) / (KeyVal[3] - KeyVal[1])) *
                  di->DimConvert / di->Document->Settings.ShrinkFactor + 0.5;
    BBoxHeight  = 0.1 * ((Flags & 0x20) ? KeyVal[5] :
                  KeyVal[4] * (KeyVal[3] - KeyVal[1]) / (KeyVal[2] - KeyVal[0])) *
                  di->DimConvert / di->Document->Settings.ShrinkFactor + 0.5;
    BBoxVOffset = BBoxHeight;
  }

  if(Name)
  {
    log_info("loading eps file `%s'", Name);

    di->InitPSIface();
    di->PSIface->DrawBegin(di, di->PixelConv(di->Data.Horiz), di->Data.PixelV, Buffer);
    DrawFile(di, Name, Decompress);
    di->PSIface->DrawEnd(" @endspecial");

    if(!Decompress && Name != FileName)
      free(Name);
  }

  BBoxValid = false;
}

static void QuoteSpecial(DrawInfo *di, char *cmd)
{
  di->InitPSIface();
  di->PSIface->DrawBegin(di, di->PixelConv(di->Data.Horiz), di->Data.PixelV, "@beginspecial @setspecial ");
  di->PSIface->DrawRaw(cmd);
  di->PSIface->DrawEnd(" @endspecial");
}

static void BangSpecial(DrawInfo *di, char *cmd)
{
  di->InitPSIface();
  di->PSIface->DrawBegin(di, di->PixelConv(di->Data.Horiz), di->Data.PixelV, "@defspecial ");
  di->PSIface->DrawRaw(cmd);
  di->PSIface->DrawEnd(" @fedspecial");
}

// used to access the BBox... variables from GhostScript.cc without declaring them global

void DrawBBox(DrawInfo *di)
{
  if(BBoxValid)
  {
    di->vw->StrokeRect(BRect(di->PixelConv(di->Data.Horiz),
                             di->Data.PixelV - BBoxVOffset,
                             di->PixelConv(di->Data.Horiz) + BBoxWidth - 1,
                             di->Data.PixelV - BBoxVOffset + BBoxHeight));
    BBoxValid = false;
  }
}


/* Special ********************************************************************************************************/


////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//                                                                                                                //
// void DVI::Special(DrawInfo *di, long len)                                                                      //
//                                                                                                                //
// Processes `special' commands.                                                                                  //
//                                                                                                                //
// DrawInfo *di                         drawing information                                                       //
// long len                             length of the command                                                     //
//                                                                                                                //
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

void DVI::Special(DrawInfo *di, long len)
{
  static char *Cmd   = NULL;
  static long CmdLen = -1;
  const int   CommandNameLen = 3;
  char        CommandName[CommandNameLen + 1];
  char        *str;
  char        *p;

  try
  {
    if(CmdLen < len)
    {
      if(Cmd)
        delete [] Cmd;

      if(Cmd = new char[len + 1])
        CmdLen = len;
      else
        CmdLen = -1;
    }
    if(Cmd)
    {
      di->ReadString(Cmd, len);
      Cmd[len] = 0;
    }
    else
      di->Skip(len);

    log_debug("special: `%s'\n", Cmd);

    for(str = Cmd; isspace(*str); str++)
      ;

    for(p = CommandName; !isspace(*str) && *str && p < &CommandName[CommandNameLen]; str++, p++)
      *p = *str;

    *p = 0;

    if(strcmp(CommandName, "pn") == 0)
      SetPenSize(di, str);

    else if(strcmp(CommandName, "fp") == 0)
      FlushPath(di);

    else if(strcmp(CommandName, "da") == 0)
      FlushDashed(di, str, false);

    else if(strcmp(CommandName, "dt") == 0)
      FlushDashed(di, str, true);

    else if(strcmp(CommandName, "pa") == 0)
      AddPath(str);

    else if(strcmp(CommandName, "ar") == 0)
      Arc(di, str, false);

    else if(strcmp(CommandName, "ia") == 0)
      Arc(di, str, true);

    else if(strcmp(CommandName, "sp") == 0)
      FlushSpline(di);

    else if(strcmp(CommandName, "sh") == 0)
      ShadeLast();

    else if(strcmp(CommandName, "wh") == 0)
      WhitenLast();

    else if(strcmp(CommandName, "bk") == 0)
      BlackenLast();

    else if(strcmp(CommandName, "ip") == 0)
      PathLen = 0;

    else if(strcmp(CommandName, "ps:") == 0)
      PSfigSpecial(di, str);

    else if(strcmp(CommandName, "PSf") == 0)
      epsfSpecial(di, str);

    else if(strcmp(CommandName, "psf") == 0)
      epsfSpecial(di, str);

    else if(*Cmd == '"')
      QuoteSpecial(di, &Cmd[1]);

    else if(*Cmd == '!')
      BangSpecial(di, &Cmd[1]);
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
