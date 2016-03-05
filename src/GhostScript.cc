////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//                                                                                                                //
// $Id: GhostScript.cc,v 2.5 1999/07/22 13:36:42 achim Exp $
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
#include <string.h>
#include "DVI-DrawPage.h"
#include "log.h"

#ifndef NO_GHOSTSCRIPT

typedef void *HWND;

extern "C"
{
  #include "gsdll.h"
  void gsdll_draw(unsigned char *device, BView *view, BRect dest, BRect src);
}

#include "PSHeader.h"

#endif  // !NO_GHOSTSCRIPT

// Interface classes

void DrawBBox(DrawPage *dp);

class EmptyPSInterface: public PSInterface
{
  public:
    virtual void DrawBegin(const DrawPage *dp, int, int, const char *)
    {
      DrawBBox(dp);
    }

    virtual void DrawRaw(const char *)  {}
    virtual void DrawFile(const char *) {}
    virtual void DrawEnd(const char *)  {}
    virtual void EndPage()              {}
};

static EmptyPSInterface EmptyIface;

#ifndef NO_GHOSTSCRIPT

class GSInterface: public PSInterface
{
  private:
    uint  PageWidth;      // size of the current page
    uint  PageHeight;
    int   Magnification;  // magnification currently in use
    int   Shrink;         // shrink factor currently in use

    uchar *Device;        // GhostScript device
    BView *vw;

    bool  Initialized;    // GhostScript interface initialized
    bool  Active;         // if we've started a page yet

  public:
    GSInterface():
      PageWidth(0),
      PageHeight(0),
      Magnification(-1),
      Shrink(-1),
      Device(NULL),
      vw(NULL),
      Initialized(false),
      Active(false)
    {}

    bool Init(DrawPage *dp);

    virtual void DrawBegin(const DrawPage *dp, int, int, const char *);
    virtual void DrawRaw(const char *);
    virtual void DrawFile(const char *);
    virtual void DrawEnd(const char *);
    virtual void EndPage();

  private:
    static int  CallBack(int message, char *str, ulong count);
    static void Send(const char *cmd, size_t len);

  friend PSInterface* InitPSInterface(DrawPage *dp);
};

static GSInterface GSIface;

#endif  // !NO_GHOSTSCRIPT

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//                                                                                                                //
// PSInterface* InitPSInterface(DVI *doc)                                                                         //
//                                                                                                                //
// initializes the GhostScript interface.                                                                         //
//                                                                                                                //
// DrawPage *dp                         drawing information                                                       //
//                                                                                                                //
// Result:                              pointer to the interface, mustn't be deleted!                             //
//                                                                                                                //
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

PSInterface* InitPSInterface(DrawPage *dp)
{
#ifndef NO_GHOSTSCRIPT

  if (GSIface.Initialized || GSIface.Init(dp))
    return &GSIface;
  else
    return &EmptyIface;

#else  // !NO_GHOSTSCRIPT

  return &EmptyIface;

#endif  // !NO_GHOSTSCRIPT
}

#ifndef NO_GHOSTSCRIPT

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//                                                                                                                //
// bool GSInterface::Init(DVI *doc)                                                                               //
//                                                                                                                //
// initializes the GhostScript interface.                                                                         //
//                                                                                                                //
// DVI *doc                             document                                                                  //
//                                                                                                                //
// Result:                              `true' if successful, otherwise `false'                                   //
//                                                                                                                //
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

bool GSInterface::Init(DrawPage *dp)
{
  static const int  GSargc = 9;
  static const char *GSargv[GSargc + 1] =
  {
    "gs",
    NULL,
    NULL,
    "-sDEVICE=beos",
    "-dDEVICEXRESOLUTION=72",
    "-dDEVICEYRESOLUTION=72",
    "-dNOPAUSE",
    "-dSAFER",
    "-q",
    NULL
  };

  char Buffer[100];

  sprintf(Buffer,      "-dDEVICEWIDTH=%d",  dp->Document->PageWidth);
  sprintf(Buffer + 50, "-dDEVICEHEIGHT=%d", dp->Document->PageHeight);
  GSargv[1] = Buffer;
  GSargv[2] = Buffer + 50;

  vw = dp->vw;

  log_info("running: %s %s %s %s %s %s %s %s",
    GSargv[0], GSargv[1], GSargv[2], GSargv[3], GSargv[4], GSargv[5], GSargv[6], GSargv[7]);

  if (gsdll_init(CallBack, NULL, GSargc, (char **)GSargv) != 0)
  {
    log_warn("can't initialize GhostScript!");
    return false;
  }

  Active        = false;
  Initialized   = true;
  Magnification = -1;
  Shrink        = -1;

  return true;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//                                                                                                                //
// void GSInterface::DrawBegin(DrawPage *dp, int xul, int yul, char *cmd)                                         //
//                                                                                                                //
// initiates a drawing session.                                                                                   //
//                                                                                                                //
// DrawPage *dp                         drawing information                                                       //
// int      xul, yul                    upper left corner                                                         //
// char     *cmd                        PostScript commands                                                       //
//                                                                                                                //
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

void GSInterface::DrawBegin(const DrawPage *dp, int xul, int yul, const char *cmd)
{
  // loop:
  //   if first character is 'H'
  //     then read command and execute it
  //     else clear stack, read command and execute it

  static const char str1[] = "\
/xdvi$run {{currentfile cvx execute} stopped pop} def \
/xdvi$dslen countdictstack def \
{currentfile read not {exit} if 72 eq \
    {xdvi$run} \
    {xdvi$run \
      clear countdictstack xdvi$dslen sub {end} repeat } \
  ifelse \
  {(%%xdvimark) currentfile =string {readline} stopped \
    {clear $error /newerror false put} {pop eq {exit} if} ifelse }loop \
  flushpage \
}loop\nH";

  static const char str2[] = "[1 0 0 -1 0 0] concat\nstop\n%%xdvimark\n";

  static const char str3[] = " TeXDict begin\n";

  char Buffer[512];

  log_debug("GSDrawBegin(%s)", cmd);

  if (!Initialized)
    Init(dp);

  gsdll_execute_begin();

  Send(str1,     sizeof(str1) - 1);
  Send(psheader, sizeof(psheader) - 1);
  Send(str2,     sizeof(str2) - 1);

  if (!Active)
  {
    if (dp->Document->PageWidth > PageWidth || dp->Document->PageHeight > PageHeight)
    {
      PageWidth  = dp->Document->PageWidth;
      PageHeight = dp->Document->PageHeight;

      sprintf(Buffer,
              "H mark /HWSize [%d %d] /ImagingBBox [0 0 %d %d] "
              "currentdevice putdeviceprops pop\n"
              "initgraphics [1 0 0 -1 0 0] concat "
              "stop\n%%%%xdvimark\n",
              PageWidth, PageHeight, PageWidth, PageHeight);
      Send(Buffer, strlen(Buffer));
    }
    if (dp->Document->Magnification != Magnification)
    {
      Magnification = dp->Document->Magnification;
      sprintf(Buffer, "H TeXDict begin /DVImag %d 1000 div def end stop\n%%%%xdvimark\n", Magnification);
      Send(Buffer, strlen(Buffer));
    }
    if (dp->Settings.ShrinkFactor != Shrink)
    {
      Shrink = dp->Settings.ShrinkFactor;
      sprintf(Buffer, "H TeXDict begin %d %d div dup /Resolution X /VResolution X end stop\n%%%%xdvimark\n",
              dp->Settings.DspInfo.PixelsPerInch, Shrink);
      Send(Buffer, strlen(Buffer));
    }
    Send(str3, sizeof(str3) - 1);

    Active = true;
  }

  sprintf(Buffer, "0 %d translate %d %d moveto\n", -PageHeight, xul, yul);
  Send(Buffer, strlen(Buffer));
  Send(cmd, strlen(cmd));
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//                                                                                                                //
// void GSInterface::DrawRaw(const char *cmd)                                                                     //
//                                                                                                                //
// sends PostScript commands to GhostScript.                                                                      //
//                                                                                                                //
// const char *cmd                      PostScript commands                                                       //
//                                                                                                                //
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

void GSInterface::DrawRaw(const char *cmd)
{
  if (!Active)
    return;

  Send(cmd, strlen(cmd));
  Send("\n", 1);
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//                                                                                                                //
// void GSInterface::DrawFile(const char *cmd)                                                                    //
//                                                                                                                //
// executes a PostScript file.                                                                                    //
//                                                                                                                //
// const char *cmd                      file name                                                                 //
//                                                                                                                //
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

void GSInterface::DrawFile(const char *cmd)
{
  char Buffer[PATH_MAX + 7];

  if (!Active)
    return;

  sprintf(Buffer, "(%s)run\n", cmd);
  Send(Buffer, strlen(Buffer));
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//                                                                                                                //
// void GSInterface::DrawEnd(const char *cmd)                                                                     //
//                                                                                                                //
// end a drawing session.                                                                                         //
//                                                                                                                //
// const char *cmd                      PostScript commands                                                       //
//                                                                                                                //
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

void GSInterface::DrawEnd(const char *cmd)
{
  if (!Active)
    return;

  Send(cmd, strlen(cmd));
  Send("\n", 1);
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//                                                                                                                //
// void GSInterface::EndPage()                                                                                    //
//                                                                                                                //
// draws the output of GhostScript and terminates the connection.                                                 //
//                                                                                                                //
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

void GSInterface::EndPage()
{
  static const char str[] = "stop\n%%xdvimark\n";

  if (Active)
  {
    Send(str, sizeof(str) - 1);

    Active = false;

    gsdll_draw(Device, vw, vw->Bounds(), vw->Bounds());
    vw->Sync();

    gsdll_execute_end();
    gsdll_exit();

    Initialized = false;
  }
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//                                                                                                                //
// int GSInterface::CallBack(int message, char *str, ulong count)                                                 //
//                                                                                                                //
// callback hook invoked by GhostScript.                                                                          //
//                                                                                                                //
// int   message                        type of message                                                           //
// char  *str                           parameter of message                                                      //
// ulong count                          length of parameter                                                       //
//                                                                                                                //
// Result:                              type specific                                                             //
//                                                                                                                //
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

int GSInterface::CallBack(int message, char *str, ulong count)
{
  log_debug("gs callback: %d 0x%08x %d", message, str, count);

  switch (message)
  {
    case GSDLL_STDIN:
      return 0; 

    case GSDLL_STDOUT:
      return count;

    case GSDLL_DEVICE:
      if (count)
        GSIface.Device = (unsigned char *)str;
      break;

    case GSDLL_SYNC:
    case GSDLL_PAGE:
      if (count)
      {
        gsdll_draw(GSIface.Device, GSIface.vw, GSIface.vw->Bounds(), GSIface.vw->Bounds());
        GSIface.vw->Sync();
      }
      break;

    case GSDLL_SIZE:
      break;

    case GSDLL_POLL:
      return 0;

    default:
      break;
  }
  return 0;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//                                                                                                                //
// void GSInterface::Send(const char *cmd, size_t len)                                                            //
//                                                                                                                //
// sends data to GhostScript.                                                                                     //
//                                                                                                                //
// const char *cmd                      data                                                                      //
// size_t     len                       length of data                                                            //
//                                                                                                                //
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

void GSInterface::Send(const char *cmd, size_t len)
{
  if (len < 180)
    log_debug("sending to gs: %s", cmd);
  else
    log_debug("sending to gs: ...");

  gsdll_execute_cont(cmd, len);
}

#endif  // !NO_GHOSTSCRIPT
