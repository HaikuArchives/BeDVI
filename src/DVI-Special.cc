////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//                                                                                                                //
// $Id: DVI-Special.cc,v 2.6 1999/08/02 12:56:22 achim Exp $
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
#include <math.h>
#include "defines.h"

extern "C"
{
#define string _string
#include <kpathsea/c-auto.h>
#include <kpathsea/tex-file.h>
#undef string
}

#include "DVI-DrawPage.h"
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
static uint      BBoxWidth;
static uint      BBoxHeight;
static int       BBoxVOffset;

inline int ConvX(DrawPage *dp, int x)
{
  return (dp->TPicConvert * x / dp->Settings.ShrinkFactor) + dp->Settings.PixelConv(dp->Data.Horiz);
}

inline int ConvY(DrawPage *dp, int y)
{
  return (dp->TPicConvert * y / dp->Settings.ShrinkFactor) + dp->Data.PixelV;
}

static void SetPenSize(DrawPage *dp, char *cmd)
{
  int Size;

  if (sscanf(cmd, " %d ", &Size) != 1)
    return;

  PenSize = (2 * Size * (dp->Settings.DspInfo.PixelsPerInch / dp->Settings.ShrinkFactor) + 1000) / 2000;

  if (PenSize < 1)
    PenSize = 1;
  else if (PenSize > MaxPenSize)
    PenSize = MaxPenSize;
}

static void FlushPath(DrawPage *dp)
{
  const rgb_color black = {0, 0, 0, 255};
  int i;

  if (PathLen)
  {
    dp->vw->BeginLineArray(PathLen);

    for (i = 1; i < PathLen; i++)
      dp->vw->AddLine(BPoint(ConvX(dp, Points[i].x),     ConvY(dp, Points[i].y)),
                      BPoint(ConvX(dp, Points[i + 1].x), ConvY(dp, Points[i + 1].y)),
                      black);

    dp->vw->EndLineArray();
  }
  PathLen = 0;
}

static void FlushDashed(DrawPage *dp, char *cmd, bool dotted)
{
  const rgb_color black = {0, 0, 0, 255};

  int    i;
  int    NumDots;
  int    lx0, ly0, lx1, ly1;
  int    cx0, cy0, cx1, cy1;
  float  InchesPerDash;
  double d, SpaceSize, a, b, dx, dy, MilliPerDash;

  if (sscanf(cmd, " %f ", &InchesPerDash) != 1)
    return;

  if (PathLen <= 1 || InchesPerDash <= 0.0)
    return;

  MilliPerDash = InchesPerDash * 1000.0;

  lx0 = Points[1].x;
  ly0 = Points[1].y;
  lx1 = Points[2].x;
  ly1 = Points[2].y;

  dx = lx1 - lx0;
  dy = ly1 - ly0;

  if (dotted)
  {
    NumDots = sqrt(dx*dx + dy*dy) / MilliPerDash + 0.5;

    if (NumDots == 0)
      NumDots = 1;

    dp->vw->BeginLineArray(NumDots + 1);

    for (i = 0; i <= NumDots; i++)
    {
      a = (float) i / NumDots;

      cx0 = lx0 + a * dx + 0.5;
      cy0 = ly0 + a * dy + 0.5;

      dp->vw->AddLine(BPoint(ConvX(dp, cx0), ConvY(dp, cy0)),
                      BPoint(ConvX(dp, cx0), ConvY(dp, cy0)),
                      black);
    }
    dp->vw->EndLineArray();
  }
  else
  {
    d = sqrt(dx*dx + dy*dy);

    NumDots = d / (2.0 * MilliPerDash) + 1.0;

    dp->vw->BeginLineArray(NumDots + 1);

    if (NumDots <= 1)
      dp->vw->AddLine(BPoint(ConvX(dp, lx0), ConvY(dp, ly0)),
                      BPoint(ConvX(dp, lx1), ConvY(dp, ly1)),
                      black);
    else
    {
      SpaceSize = (d - NumDots * MilliPerDash) / (NumDots - 1);

      for (i = 0; i < NumDots - 1; i++)
      {
        a   = i * (MilliPerDash + SpaceSize) / d;
        b   = a + MilliPerDash / d;

        cx0 = lx0 + a * dx + 0.5;
        cy0 = ly0 + a * dy + 0.5;
        cx1 = lx0 + b * dx + 0.5;
        cy1 = ly0 + b * dy + 0.5;

        dp->vw->AddLine(BPoint(ConvX(dp, cx0), ConvY(dp, cy0)),
                        BPoint(ConvX(dp, cx1), ConvY(dp, cy1)),
                        black);

        b += SpaceSize / d;
      }
      cx0 = lx0 + b * dx + 0.5;
      cy0 = ly0 + b * dy + 0.5;

      dp->vw->AddLine(BPoint(ConvX(dp, cx0), ConvY(dp, cy0)),
                      BPoint(ConvX(dp, lx1), ConvY(dp, ly1)),
                      black);
    }
    dp->vw->EndLineArray();
  }

  PathLen = 0;
}

static void AddPath(char *cmd)
{
  int PathX, PathY;

  if (++PathLen >= MaxPoints)
     return;

  if (sscanf(cmd, " %d %d ", &PathX, &PathY) != 2)
     return;

  Points[PathLen].x = PathX;
  Points[PathLen].y = PathY;
}

static void Arc(DrawPage *dp, char *cmd, bool invis)
{
  int    CenterX, CenterY;
  int    RadX, RadY;
  float  StartAngle, EndAngle;

  if (sscanf(cmd, " %d %d %d %d %f %f ", &CenterX, &CenterY, &RadX, &RadY, &StartAngle, &EndAngle) != 6)
    return;

  if (invis)
    return;

  StartAngle *= 180 / PI;
  EndAngle   *= 180 / PI;

  dp->vw->StrokeArc(BPoint(ConvX(dp, CenterX), ConvY(dp, CenterY)),
                    ConvX(dp, RadX) - ConvX(dp, 0),
                    ConvY(dp, RadY) - ConvY(dp, 0),
                    StartAngle,
                    EndAngle - StartAngle);
}

static inline int dist(int x0, int y0, int x1, int y1)
{
  return abs(x0 - x1) + abs(y0 - y1);
}

static void FlushSpline(DrawPage *dp)
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

  for (i = 0; i < N - 1; i++)
  {
    Steps = (dist(Points[i].x,   Points[i].y,   Points[i+1].x, Points[i+1].y) +
		dist(Points[i+1].x, Points[i+1].y, Points[i+2].x, Points[i+2].y)) / 80;

    dp->vw->BeginLineArray(Steps);

    for (j = 0; j < Steps; j++)
    {
      w = (j * 1000 + 500) / Steps;
      t1 = w * w / 20;
      w -= 500;
      t2 = (750000 - w * w) / 10;
      w -= 500;
      t3 = w * w / 20;

      xp = (t1 * Points[i+2].x + t2 * Points[i+1].x + t3 * Points[i].x + 50000) / 100000;
      yp = (t1 * Points[i+2].y + t2 * Points[i+1].y + t3 * Points[i].y + 50000) / 100000;

      if (LastValid)
        dp->vw->AddLine(BPoint(ConvX(dp, LastX), ConvY(dp, LastY)),
                        BPoint(ConvX(dp, xp),    ConvY(dp, yp)),
                        black);
      LastX     = xp;
      LastY     = yp;
      LastValid = true;
    }
    dp->vw->EndLineArray();
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

static string FindFigFile(const string &Directory, const char *FileName, int &decompress)
{
  string Name;
  char   *found;

  if (*FileName == '`')
  {
    Name.assign(FileName + 1);

    decompress = 1;
  }
  else if (found = kpse_find_pict(FileName))
  {
    Name.assign(found);

    free(found);

    decompress = 0;
  }
  else
  {
    Name = Directory + "/" + string(FileName);

    decompress = 0;
  }

  return Name;
}

static char *ReadLine(FILE *f)
{
  char *Line;
  char *p;
  int  Size = 100;

  if (!(Line = (char *)malloc(Size)))
    return NULL;

  p = Line;

  do
  {
    if (p >= Line + Size)
    {
      Size += 100;

      if (!(Line = (char *)realloc(Line, Size)))
        return NULL;
    }
    if (fread(p, sizeof(char), 1, f) != sizeof(char))
      break;
  }
  while (*p++ != '\n');

  p[-1] = 0;

  return Line;
}

static void DrawFile(DrawPage *dp, const char *Name, int Decompress)
{
  FILE *Pipe;
  char *Line;

  if (Decompress)
  {
    if (Pipe = popen(Name, "r"))
    {
      while (Line = ReadLine(Pipe))
      {
        dp->PSIface->DrawRaw(Line);
        free(Line);
      }
      pclose(Pipe);
    }
  }
  else
    dp->PSIface->DrawFile(Name);
}

static void PSfigSpecial(DrawPage *dp, char *cmd)
{
  char	 *FileName;
  string Name;
  int    RawW, RawH;
  int    Decompress;

  dp->InitPSIface();

  if (strncmp(cmd, ":[begin]", 8) == 0)
  {
    cmd += 8;

    BBoxValid = false;

    if (sscanf(cmd, "%d %d\n", &RawW, &RawH) >= 2)
    {
      BBoxValid   = true;
      BBoxWidth   = dp->Settings.PixelConv(RawW * dp->DimConvert);
      BBoxHeight  = dp->Settings.PixelConv(RawH * dp->DimConvert);
      BBoxVOffset = 0;
    }
    dp->PSIface->DrawBegin(dp, dp->Settings.PixelConv(dp->Data.Horiz), dp->Data.PixelV, cmd);
  }
  else if (strncmp(cmd, " plotfile ", 10) == 0)
  {
    cmd += 10;

    while (isspace(*cmd))
      cmd++;

    for (FileName = cmd; !isspace(*cmd); cmd++)
      ;

    *cmd = '\0';

    Name = FindFigFile(dp->Document->Directory, FileName, Decompress);

    if (!Name.empty())
      DrawFile(dp, Name.c_str(), Decompress);
  }
  else if (strncmp(cmd, ":[end]", 6) == 0)
  {
    cmd += 6;

    dp->PSIface->DrawEnd(cmd);

    BBoxValid = false;
  }
  else
  {
    if (*cmd == ':')
      cmd++;

    dp->PSIface->DrawBegin(dp, dp->Settings.PixelConv(dp->Data.Horiz), dp->Data.PixelV, "");
    dp->PSIface->DrawEnd("");
    dp->PSIface->DrawRaw(cmd);
  }
}

static void epsfSpecial(DrawPage *dp, char *cmd)
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
  static uint     BufLen = 0;

  string Name;
  char   *FileName;
  int    Decompress;
  uint   len;
  char   *p;
  char   *q;
  int    Flags = 0;
  double KeyVal[6];

  if (memcmp(cmd, "ile=", 4) != 0)
    return;

  p        = cmd + 4;
  FileName = p;

  if (*p == '\'' || *p == '"')
  {
    do
    {
      p++;
    }
    while (*p != '\0' && *p != *FileName);

    FileName++;
  }
  else
    while (*p != '\0' && *p != ' ' && *p != '\t')
      p++;

  if (*p != '\0')
    *p++ = '\0';

  Name = FindFigFile(dp->Document->Directory, FileName, Decompress);

  while (*p == ' ' || *p == '\t')
    p++;

  len = strlen(p) + NumKeys + 30;

  if (BufLen < len)
  {
    if (BufLen != 0)
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

    while (*p1 != '=' && !isspace(*p1) && *p1 != '\0')
      p1++;

    for (KeyNo = 0;; KeyNo++)
    {
      if (KeyNo >= NumKeys)
        break;

      if (memcmp(p, KeyTab[KeyNo], p1 - p) == 0)
      {
        if (KeyNo >= NumArglessKeys)
        {
          if (*p1 == '=')
            p1++;

          if (KeyNo < NumArglessKeys + 6)
          {
            KeyVal[KeyNo - NumArglessKeys] = atof(p1);

            Flags |= (1 << (KeyNo - NumArglessKeys));
          }
          *q++ = ' ';

          while (!isspace(*p1) && *p1 != '\0')
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

    while (!isspace(*p) && *p != '\0')
      p++;

    while (isspace(*p))
      p++;
  }

  strcpy(q, " @setspecial\n");

  BBoxValid = false;

  if ((Flags & 0x30) == 0x30 || ((Flags & 0x30) && (Flags & 0xf) == 0xf))
  {
    BBoxValid = true;

    BBoxWidth   = 0.1 * ((Flags & 0x10) ? KeyVal[4] :
                  KeyVal[5] * (KeyVal[2] - KeyVal[0]) / (KeyVal[3] - KeyVal[1])) *
                  dp->DimConvert / dp->Settings.ShrinkFactor + 0.5;
    BBoxHeight  = 0.1 * ((Flags & 0x20) ? KeyVal[5] :
                  KeyVal[4] * (KeyVal[3] - KeyVal[1]) / (KeyVal[2] - KeyVal[0])) *
                  dp->DimConvert / dp->Settings.ShrinkFactor + 0.5;
    BBoxVOffset = BBoxHeight;
  }

  if (!Name.empty())
  {
    log_info("loading eps file `%s'", Name.c_str());

    dp->InitPSIface();
    dp->PSIface->DrawBegin(dp, dp->Settings.PixelConv(dp->Data.Horiz), dp->Data.PixelV, Buffer);
    DrawFile(dp, Name.c_str(), Decompress);
    dp->PSIface->DrawEnd(" @endspecial");
    dp->PSIface->EndPage();
  }

  BBoxValid = false;
}

static void QuoteSpecial(DrawPage *dp, char *cmd)
{
  dp->InitPSIface();
  dp->PSIface->DrawBegin(dp, dp->Settings.PixelConv(dp->Data.Horiz), dp->Data.PixelV, "@beginspecial @setspecial ");
  dp->PSIface->DrawRaw(cmd);
  dp->PSIface->DrawEnd(" @endspecial");
}

static void BangSpecial(DrawPage *dp, char *cmd)
{
  dp->InitPSIface();
  dp->PSIface->DrawBegin(dp, dp->Settings.PixelConv(dp->Data.Horiz), dp->Data.PixelV, "@defspecial ");
  dp->PSIface->DrawRaw(cmd);
  dp->PSIface->DrawEnd(" @fedspecial");
}

// used to access the BBox... variables from GhostScript.cc without declaring them global

void DrawBBox(DrawPage *dp)
{
  if (BBoxValid)
  {
    dp->vw->StrokeRect(BRect(dp->Settings.PixelConv(dp->Data.Horiz),
                             dp->Data.PixelV - BBoxVOffset,
                             dp->Settings.PixelConv(dp->Data.Horiz) + BBoxWidth - 1,
                             dp->Data.PixelV - BBoxVOffset + BBoxHeight));
    BBoxValid = false;
  }
}


/* Special ********************************************************************************************************/


////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//                                                                                                                //
// void DrawPage::Special(long len)                                                                               //
//                                                                                                                //
// Processes `special' commands.                                                                                  //
//                                                                                                                //
// long len                             length of the command                                                     //
//                                                                                                                //
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

void DrawPage::Special(long len)
{
  static char *Cmd   = NULL;
  static long CmdLen = -1;
  const int   CommandNameLen = 3;
  char        CommandName[CommandNameLen + 1];
  char        *str;
  char        *p;

  try
  {
    if (CmdLen < len)
    {
      delete [] Cmd;

      if (Cmd = new char[len + 1])
        CmdLen = len;
      else
        CmdLen = -1;
    }
    if (Cmd)
    {
      ReadString(Cmd, len);
      Cmd[len] = 0;
    }
    else
      Skip(len);

    log_debug("special: `%s'\n", Cmd);

    for (str = Cmd; isspace(*str); str++)
      ;

    for (p = CommandName; !isspace(*str) && *str && p < &CommandName[CommandNameLen]; str++, p++)
      *p = *str;

    *p = 0;

    if (strcmp(CommandName, "pn") == 0)
      SetPenSize(this, str);

    else if (strcmp(CommandName, "fp") == 0)
      FlushPath(this);

    else if (strcmp(CommandName, "da") == 0)
      FlushDashed(this, str, false);

    else if (strcmp(CommandName, "dt") == 0)
      FlushDashed(this, str, true);

    else if (strcmp(CommandName, "pa") == 0)
      AddPath(str);

    else if (strcmp(CommandName, "ar") == 0)
      Arc(this, str, false);

    else if (strcmp(CommandName, "ia") == 0)
      Arc(this, str, true);

    else if (strcmp(CommandName, "sp") == 0)
      FlushSpline(this);

    else if (strcmp(CommandName, "sh") == 0)
      ShadeLast();

    else if (strcmp(CommandName, "wh") == 0)
      WhitenLast();

    else if (strcmp(CommandName, "bk") == 0)
      BlackenLast();

    else if (strcmp(CommandName, "ip") == 0)
      PathLen = 0;

    else if (strcmp(CommandName, "ps:") == 0)
      PSfigSpecial(this, str);

    else if (strcmp(CommandName, "PSf") == 0)
      epsfSpecial(this, str);

    else if (strcmp(CommandName, "psf") == 0)
      epsfSpecial(this, str);

    else if (*Cmd == '"')
      QuoteSpecial(this, &Cmd[1]);

    else if (*Cmd == '!')
      BangSpecial(this, &Cmd[1]);
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
