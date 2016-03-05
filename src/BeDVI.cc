////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//                                                                                                                //
// $Id: BeDVI.cc,v 2.6 1999/07/22 13:36:35 achim Exp $
//                                                                                                                //
// BeDVI                                                                                                          //
// by Achim Blumensath                                                                                            //
// blume@corona.oche.de                                                                                           //
//                                                                                                                //
// This program is free software! It may be distributed according to the GNU Public License (see COPYING).        //
//                                                                                                                //
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

#include <InterfaceKit.h>
#include <StorageKit.h>
#include <NodeInfo.h>
#include <Path.h>
#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <ctype.h>
#include <Debug.h>
#include "defines.h"

#ifdef PROFILING
#include <libprof.h>
#endif

extern "C"
{
  #define string _string
  #include "kpathsea/c-auto.h"
  #include "kpathsea/progname.h"
  #include "kpathsea/proginit.h"
  #include "kpathsea/tex-file.h"
  #undef string
}

#include "BeDVI.h"
#include "DVI-View.h"
#include "FontList.h"
#include "log.h"

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//                                                                                                                //
// ViewApplication::ViewApplication()                                                                             //
//                                                                                                                //
// initializes the display and global data (see the BeBook).                                                      //
//                                                                                                                //
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

ViewApplication::ViewApplication():
  BApplication("application/x-vnd.blume-BeDVI"),
  PrefHandle(NULL),
  MenuDefs(DefaultMenu),
  WindowBounds(90.0, 30.0, 630.0, 470.0),
  SearchWinBounds(120.0, 130.0, 250.0, 150.0),
  MeasureWinPos(10.0, 30.0),
  WinListLock(B_ERROR),
  ActiveWindow(NULL),
  MeasureWin(NULL),
  SearchWin(NULL),
  OpenPanel(NULL),
  SleepPointerNestCnt(0),
  NoDocLoaded(true),
  MeasureWinOpen(false)
{
  app_info   AppInfo;
  BEntry     AppFileEntry;
  BPath      AppPath;
  BFile      AppFile;
  BResources AppResource;
  PREFData   PrefData;
  uint       PixelsPerInch = 300;
  const void *p;
  ssize_t    Size;
  size_t     len;
  uint32     Type;
  char       Buffer[1024];

  if ((WinListLock = create_sem(1, "window list lock")) < B_OK)
    throw(runtime_error("can't create semaphore"));

  if (PREFInit("x-vnd.blume-BeDVI", &PrefHandle) < B_OK)
    PrefHandle = NULL;

  if (GetAppInfo(&AppInfo)             != B_OK ||
      AppFileEntry.SetTo(&AppInfo.ref) != B_OK ||
      AppFileEntry.GetPath(&AppPath)   != B_OK)
    throw(runtime_error("can't get binary path"));

  acquire_sem(kpse_sem);
  kpse_set_program_name((const char *)AppPath.Path(), (const char *)"BeDVI");  // set default name
  release_sem(kpse_sem);

  getcwd(Buffer, 1024);                            // there apparently is no other way to get the current directory!
  get_ref_for_path(Buffer, &PanelDirectory);

  // load menu resource

  if (AppFile.SetTo(&AppFileEntry, B_READ_ONLY) == B_OK &&
      AppResource.SetTo(&AppFile)               == B_OK &&
      (p = AppResource.FindResource(B_STRING_TYPE, "BeDVI:MenuData", &len)))
    ReadMenuDef((char *)p, len);

  // load preferences

  if (PrefHandle && PREFLoadSet(PrefHandle, "settings", true, &PrefData) >= B_OK)
  {
    if (PREFGetData(PrefData, "win x1",       &p, &Size, &Type) >= B_OK && Type == B_FLOAT_TYPE)
      WindowBounds.left      = *(float *)p;
    if (PREFGetData(PrefData, "win y1",       &p, &Size, &Type) >= B_OK && Type == B_FLOAT_TYPE)
      WindowBounds.top       = *(float *)p;
    if (PREFGetData(PrefData, "win x2",       &p, &Size, &Type) >= B_OK && Type == B_FLOAT_TYPE)
      WindowBounds.right     = *(float *)p;
    if (PREFGetData(PrefData, "win y2",       &p, &Size, &Type) >= B_OK && Type == B_FLOAT_TYPE)
      WindowBounds.bottom    = *(float *)p;
    if (PREFGetData(PrefData, "swin x1",      &p, &Size, &Type) >= B_OK && Type == B_FLOAT_TYPE)
      SearchWinBounds.left   = *(float *)p;
    if (PREFGetData(PrefData, "swin y1",      &p, &Size, &Type) >= B_OK && Type == B_FLOAT_TYPE)
      SearchWinBounds.top    = *(float *)p;
    if (PREFGetData(PrefData, "swin x2",      &p, &Size, &Type) >= B_OK && Type == B_FLOAT_TYPE)
      SearchWinBounds.right  = *(float *)p;
    if (PREFGetData(PrefData, "swin y2",      &p, &Size, &Type) >= B_OK && Type == B_FLOAT_TYPE)
      SearchWinBounds.bottom = *(float *)p;
    if (PREFGetData(PrefData, "mwin x",       &p, &Size, &Type) >= B_OK && Type == B_FLOAT_TYPE)
      MeasureWinPos.x        = *(float *)p;
    if (PREFGetData(PrefData, "mwin y",       &p, &Size, &Type) >= B_OK && Type == B_FLOAT_TYPE)
      MeasureWinPos.y        = *(float *)p;
    if (PREFGetData(PrefData, "dpi",          &p, &Size, &Type) >= B_OK && Type == B_INT32_TYPE)
      PixelsPerInch          = *(int32 *)p;
    if (PREFGetData(PrefData, "shrink",       &p, &Size, &Type) >= B_OK && Type == B_INT16_TYPE)
      Settings.ShrinkFactor  = *(int16 *)p;
    if (PREFGetData(PrefData, "antialiasing", &p, &Size, &Type) >= B_OK && Type == B_BOOL_TYPE)
      Settings.AntiAliasing  = *(bool *)p;
    if (PREFGetData(PrefData, "borderline",   &p, &Size, &Type) >= B_OK && Type == B_BOOL_TYPE)
      Settings.BorderLine    = *(bool *)p;
    if (PREFGetData(PrefData, "measure",      &p, &Size, &Type) >= B_OK && Type == B_BOOL_TYPE)
      MeasureWinOpen         = *(bool *)p;

    PREFDisposeSet(&PrefData);
  }

  SetResolution(PixelsPerInch);

  // init display

  if (!OpenWindow())
    throw(runtime_error("can't open window"));

  if (MeasureWinOpen)
    MeasureWin = OpenMeasureWindow(MeasureWinPos);
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//                                                                                                                //
// ViewApplication::~ViewApplication()                                                                            //
//                                                                                                                //
// saves current settings in executable resources (see the BeBook).                                               //
//                                                                                                                //
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

ViewApplication::~ViewApplication()
{
  PREFData PrefData;

  delete OpenPanel;

  if (PrefHandle)
  {
    if (PREFLoadSet(PrefHandle, "settings", true, &PrefData) >= B_OK)
    {
      PREFSetData(PrefData, "win x1",       &WindowBounds.left,              sizeof(float), B_FLOAT_TYPE);
      PREFSetData(PrefData, "win y1",       &WindowBounds.top,               sizeof(float), B_FLOAT_TYPE);
      PREFSetData(PrefData, "win x2",       &WindowBounds.right,             sizeof(float), B_FLOAT_TYPE);
      PREFSetData(PrefData, "win y2",       &WindowBounds.bottom,            sizeof(float), B_FLOAT_TYPE);
      PREFSetData(PrefData, "swin x1",      &SearchWinBounds.left,           sizeof(float), B_FLOAT_TYPE);
      PREFSetData(PrefData, "swin y1",      &SearchWinBounds.top,            sizeof(float), B_FLOAT_TYPE);
      PREFSetData(PrefData, "swin x2",      &SearchWinBounds.right,          sizeof(float), B_FLOAT_TYPE);
      PREFSetData(PrefData, "swin y2",      &SearchWinBounds.bottom,         sizeof(float), B_FLOAT_TYPE);
      PREFSetData(PrefData, "mwin x",       &MeasureWinPos.x,                sizeof(float), B_FLOAT_TYPE);
      PREFSetData(PrefData, "mwin y",       &MeasureWinPos.y,                sizeof(float), B_FLOAT_TYPE);
      PREFSetData(PrefData, "dpi",          &Settings.DspInfo.PixelsPerInch, sizeof(int32), B_INT32_TYPE);
      PREFSetData(PrefData, "shrink",       &Settings.ShrinkFactor,          sizeof(int16), B_INT16_TYPE);
      PREFSetData(PrefData, "antialiasing", &Settings.AntiAliasing,          sizeof(bool),  B_BOOL_TYPE);
      PREFSetData(PrefData, "borderline",   &Settings.BorderLine,            sizeof(bool),  B_BOOL_TYPE);
      PREFSetData(PrefData, "measure",      &MeasureWinOpen,                 sizeof(bool),  B_BOOL_TYPE);

      PREFSaveSet(PrefData);
      PREFDisposeSet(&PrefData);
    }

    PREFShutdown(PrefHandle);
  }

  if (WinListLock >= B_OK)
    delete_sem(WinListLock);
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//                                                                                                                //
// void ViewApplication::ArgvReceived(int32 argc, char **argv)                                                    //
//                                                                                                                //
// Parses command line arguments (see the BeBook).                                                                //
//                                                                                                                //
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

void ViewApplication::ArgvReceived(int32 argc, char **argv)
{
  int i;

  acquire_sem(kpse_sem);
  kpse_set_program_name(argv[0], "BeDVI");
  release_sem(kpse_sem);

  for (i = 1; i < argc; i++)
  {
    if (argv[i][0] == '-')
    {
      switch (argv[i][1])
      {
        case 'v':                // set log level
        {
          int lev = log_level;

          sscanf(&argv[i][2], "%d", &lev);

          log_level = lev;
          break;
        }
        default:
          fprintf(stderr, "USAGE: BeDVI [-v<log-level>] [<files> ...]\n");
          break;
      }
    }
    else
    {
      entry_ref file;
      BEntry    e;

      if (get_ref_for_path(argv[i], &file) != B_OK)
      {
        log_error("Can't open file!");
        continue;
      }

      if (e.SetTo(&file) != B_OK || !e.Exists())
      {
        char *str;

        // append ".dvi" to filename and try again

        if (!(str = (char *)malloc(strlen(argv[i]) + 5)))
        {
          log_error("Can't open file!");
          continue;
        }

        strcpy(str, argv[i]);
        strcat(str, ".dvi");

        if (get_ref_for_path(str, &file) != B_OK)
        {
          free(str);
          log_error("Can't open file!");
          continue;
        }
        free(str);

        if (e.SetTo(&file) != B_OK || !e.Exists())
        {
          log_error("Can't open file!");
          continue;
        }
      }

      if (NoDocLoaded)
      {
        BMessage msg(MsgLoadFile);

        if (msg.AddRef("refs", &file) == B_OK)
          ActiveWindow->PostMessage(&msg);

        // In case there are several files given on the command line, this has to be set here in order to avoid a
        // race condition. The window will set it to late. (I know, it's ugly.)

        NoDocLoaded = false;
      }
      else
      {
        BMessage msg(MsgOpenFile);

        if (msg.AddRef("refs", &file) == B_OK)
          PostMessage(&msg);
      }
    }
  }
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//                                                                                                                //
// void ViewApplication::RefsReceived(BMessage *msg)                                                              //
//                                                                                                                //
// Loads a file (see the BeBook).                                                                                 //
//                                                                                                                //
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

void ViewApplication::RefsReceived(BMessage *msg)
{
  entry_ref file;

  if (NoDocLoaded)
  {
    // load the file into the empty window

    msg->what = MsgLoadFile;

    ActiveWindow->PostMessage(msg);
  }
  else
  {
    // open new window

    msg->what = MsgOpenFile;

    MessageReceived(msg);
  }
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//                                                                                                                //
// bool ViewApplication::QuitRequested()                                                                          //
//                                                                                                                //
// prepares for shutdown (see the BeBook).                                                                        //
//                                                                                                                //
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

bool ViewApplication::QuitRequested()
{
  if (OpenPanel)
  {
    delete OpenPanel;                                      // necessary to prevent deadlock when closing the window
    OpenPanel = NULL;
  }

  return inherited::QuitRequested();
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//                                                                                                                //
// void ViewApplication::AboutRequested()                                                                         //
//                                                                                                                //
// Displays the About Requester (see the BeBook).                                                                 //
//                                                                                                                //
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

void ViewApplication::AboutRequested()
{
  try
  {
    (new BAlert("About",
                "BeDVI " VERSION " ("__DATE__")\n\n"
                "by Achim Blumensath\n\n"
                "Special thanks to Sander Stoks\n\n"
                "This program is free software!\n"
                "Consult the GNU Public License for more information.",
                "Ok", NULL, NULL, B_WIDTH_AS_USUAL, B_INFO_ALERT))->Go();
  }
  catch(...)
  {
    log_warn("unknown exception!");
    log_debug("at %s:%d", __FILE__, __LINE__);
  }
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//                                                                                                                //
// void ViewApplication::MessageReceived(BMessage *msg)                                                           //
//                                                                                                                //
// Loads the selected file if the FilePanel was closed (see the BeBook).                                          //
//                                                                                                                //
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

void ViewApplication::MessageReceived(BMessage *msg)
{
  switch (msg->what)
  {
    case MsgOpenPanel:
      msg->what = MsgOpenFile;

      LaunchFilePanel(msg, NULL);
      break;

    case MsgLoadPanel:
    case MsgSavePanel:
    {
      void *win;

      if (msg->FindPointer("Window", &win) != B_OK)
      {
        log_warn("invalid message received!");
        break;
      }

      if (msg->what == MsgLoadPanel)
        msg->what = MsgLoadFile;
      else
        msg->what = MsgSavePage;

      LaunchFilePanel(msg, (BWindow *)win);

      break;
    }
    case MsgOpenFile:
    {
      BWindow *win;

      // open new window

      if (NoDocLoaded)
        win = ActiveWindow;                  // take empty window

      else
        if (!(win = OpenWindow()))           // open new one
        {
          DisplayError("Not enough memory!");
          return;
        }

      // redirect message to window

      msg->what = MsgLoadFile;
      win->PostMessage(msg);
      break;
    }

    case MsgPoint:
      if (MeasureWin)
        MeasureWin->PostMessage(msg);
      break;

    case MsgMeasureWin:
      MeasureWinOpen = !MeasureWinOpen;

      if (MeasureWinOpen)
      {
        if (!MeasureWin)
          MeasureWin = OpenMeasureWindow(MeasureWinPos);
      }
      else
      {
        if (MeasureWin)
        {
          MeasureWin->Lock();
          MeasureWin->Quit();
          MeasureWin = NULL;
        }
      }

      for (int i = CountWindows(); i > 0; i--)
        WindowAt(i - 1)->PostMessage(MsgRedraw);
      break;

    case MsgSearchWin:
    {
      void *win;

      if (SearchWin)
        if (SearchWin->Lock())
          SearchWin->Quit();

      if (msg->FindPointer("Window", &win) == B_OK)
        SearchWin = OpenSearchWindow(SearchWinBounds, (BWindow *)win);
      else
        log_warn("invalid message received!");

      break;
    }
    case MsgNextWindow:
    {
      BWindow *win;

      if (msg->FindPointer("Window", (void **)&win) < B_OK)
      {
        log_warn("invalid message received!");
        break;
      }

      if (win = NextDocWindow(win))
        win->Activate();
      break;
    }
    case MsgPrevWindow:
    {
      BWindow *win;

      if (msg->FindPointer("Window", (void **)&win) < B_OK)
      {
        log_warn("invalid message received!");
        break;
      }

      if (win = PrevDocWindow(win))
        win->Activate();
      break;
    }
    default:
      inherited::MessageReceived(msg);
      break;
  }
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//                                                                                                                //
// static bool ParseLine(const char *line, const char *end, const char **Fields, int NumFields)                   //
//                                                                                                                //
// splits a string of 'NumFields' fields separated by ',' into its parts and stores them in 'Fields'.             //
//                                                                                                                //
// const char *line                     pointer to the beginning of the string                                    //
// const char *end                      pointer to the character behind the last character of the string          //
// const char **Fields                  array of 2 * 'NumFields' pointers which contain after calling 'ParseLine' //
//                                      the beginning and end of the fields                                       //
// int        NumFields                 number of fields                                                          //
//                                                                                                                //
// Result:                              'true' if successful, otherwise 'false'                                   //
//                                                                                                                //
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

static bool ParseLine(const char *line, const char *end, const char **Fields, int NumFields)
{
  const char *p;
  int        i;

  for (i = NumFields - 1; end >= line && i >= 0; i--)
  {
    // get end of current field

    p = end - 1;

    while (p >= line && isspace(*p))
      p--;

    Fields[2 * i + 1] = p;

    // find last comma in `line'

    while (p >= line && *p != ',')
      p--;

    end = p++;

    // get begin of current field

    while (p < Fields[2 * i + 1] && isspace(*p))
      p++;

    Fields[2 * i] = p;

    while (Fields[2 * i + 1] > p && isspace(*Fields[2 * i + 1]))
      Fields[2 * i + 1]--;

    Fields[2 * i + 1]++;
  }

  // Did we find `NumFields' fields?

  if (end < line && i < 0)
    return true;
  else
    return false;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//                                                                                                                //
// const int                              ViewApplication::NumMenus                                               //
// const int                      * const ViewApplication::NumSubMenus                                            //
// const bool                     * const ViewApplication::RadioModeMenu                                          //
// const ViewApplication::MenuDef * const ViewApplication::DefaultMenu                                            //
//                                                                                                                //
// definition of the BeDVI menus.                                                                                 //
//                                                                                                                //
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

const int          ViewApplication::NumMenus                 = 5;

static const int   _NumSubMenus[ViewApplication::NumMenus]   = {6, 3, 7, 3, 8};
static const bool  _RadioModeMenu[ViewApplication::NumMenus] = {false, false, false, true, true};

const int  * const ViewApplication::NumSubMenus              = &_NumSubMenus[0];
const bool * const ViewApplication::RadioModeMenu            = &_RadioModeMenu[0];

static const ViewApplication::MenuDef _DefaultMenu[] =
{
  {"File",            0,  true,  0                },
  {"About…",         '?', false, B_ABOUT_REQUESTED},
  {"Open…",          'O', false, MsgOpenPanel     },
  {"Load…",          'L', true,  MsgLoadPanel     },
  {"Reload",         'R', true,  MsgReload        },
  {"Save Page…",      0,  true,  MsgSavePanel     },
//  {"Print Page",     'P', true,  MsgPrintPage     },
  {"Quit",           'Q', false, B_QUIT_REQUESTED },
  {"Display",         0,  true,  0                },
  {"Anti Aliasing",  'A', true,  MsgAntiAliasing  },
  {"Border Line",    'B', true,  MsgBorderLine    },
  {"Measure Window", 'M', false, MsgMeasureWin    },
  {"Move",            0,  true,  0                },
  {"Search",         'F', true,  MsgSearchWin     },
  {"First Page",     '<', true,  MsgFirst         },
  {"Last Page",      '>', true,  MsgLast          },
  {"Next Page",      '+', true,  MsgNext          },
  {"Prev Page",      '-', true,  MsgPrev          },
  {"Next Window",    '*', false, MsgNextWindow    },
  {"Prev Window",    '_', false, MsgPrevWindow    },
  {"Resolution",      0,  true,  0                },
  {"600 dpi",         0,  true,  600              },
  {"300 dpi",         0,  true,  300              },
  {"100 dpi",         0,  true,  100              },
  {"Magnification",   0,  true,  0                },
  {"Shrink to 1/1",  '1', true,  1                },
  {"Shrink to 1/2",  '2', true,  2                },
  {"Shrink to 1/3",  '3', true,  3                },
  {"Shrink to 1/4",  '4', true,  4                },
  {"Shrink to 1/5",  '5', true,  5                },
  {"Shrink to 1/6",  '6', true,  6                },
  {"Shrink to 1/9",  '7', true,  9                },
  {"Shrink to 1/12", '8', true,  12               }
};
const ViewApplication::MenuDef * const ViewApplication::DefaultMenu = &_DefaultMenu[0];

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//                                                                                                                //
// bool ViewApplication::ReadMenuDef(char *def, size_t len)                                                       //
//                                                                                                                //
// parses a menu definition and initializes the `Menu...' members of the application object.                      //
//                                                                                                                //
// char   *def                          pointer to the definition string                                          //
// size_t len                           length of the definition string                                           //
//                                                                                                                //
// Result:                              `true' if successful, otherwise `false'                                   //
//                                                                                                                //
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

bool ViewApplication::ReadMenuDef(char *def, size_t len)
{
  MenuDef *NewMenuDef = NULL;
  char    **ModeName  = NULL;
  uint    *ModeDPI    = NULL;

  try
  {
    char *line;
    int  num;
    int  ResMenuIndex;
    int  MagMenuIndex;
    char *Fields[8];
    char *p;
    int  i;

    // get number of all menus (including submenus) and the indexes of the resolution and magnification menus

    for (i = 0, num = NumMenus; i < NumMenus; i++)
      num += NumSubMenus[i];

    for (i = 0, ResMenuIndex = NumMenus - 2; i < NumMenus - 2; i++)
      ResMenuIndex += NumSubMenus[i];

    MagMenuIndex = ResMenuIndex + NumSubMenus[NumMenus - 2] + 1;

    NewMenuDef = new MenuDef[num];
    ModeName   = new char * [NumSubMenus[NumMenus - 2]];
    ModeDPI    = new uint   [NumSubMenus[NumMenus - 2]];

    memcpy(NewMenuDef, DefaultMenu, num * sizeof(*DefaultMenu));

    // parse menu definition

    for (i = 0, line = def; line < def + len && i < num; i++)
    {
      // skip comments and whitespace

      while (*line == '#' || isspace(*line))
      {
        while (isspace(*line))
          if (++line >= def + len)
            throw(runtime_error("parse error at menu definition"));

        if (*line == '#')
          while (*line++ != '\n')
            if (line >= def + len)
              throw(runtime_error("parse error at menu definition"));
      }

      // parse line

      p = line;

      while (line < def + len && *line != '\n')
        line++;

      if (i <= ResMenuIndex || i == MagMenuIndex)
      {
        if (!(ParseLine(p, line, Fields, 2)))
          throw(runtime_error("parse error at menu definition"));
      }
      else if (i < MagMenuIndex)
      {
        if (!(ParseLine(p, line, Fields, 4)))
          throw(runtime_error("parse error at menu definition"));
      }
      else
      {
        if (!(ParseLine(p, line, Fields, 3)))
          throw(runtime_error("parse error at menu definition"));
      }

      line++;

      NewMenuDef[i].Name = Fields[0];
      *Fields[1]         = 0;

      if (Fields[2] < Fields[3])
        NewMenuDef[i].Shortcut = Fields[2][0];
      else
        NewMenuDef[i].Shortcut = 0;

      if (i > ResMenuIndex && i < MagMenuIndex)
      {
        // read mode name and dpi

        *Fields[5] = 0;
        *Fields[7] = 0;

        if (sscanf(Fields[6], "%u", &ModeDPI[i - ResMenuIndex - 1]) != 1)
          throw(runtime_error("parse error at menu definition"));

        if (ModeDPI[i - ResMenuIndex - 1] < 50)
          ModeDPI[i - ResMenuIndex - 1] = 50;

        ModeName[i - ResMenuIndex - 1] = Fields[4];
        NewMenuDef[i].Message          = ModeDPI[i - ResMenuIndex - 1];
      }
      else if (i > MagMenuIndex)
      {
        // read magnification

        *Fields[5] = 0;

        if (sscanf(Fields[4], "%u", &NewMenuDef[i].Message) != 1)
          throw(runtime_error("parse error at menu definition"));

        if (NewMenuDef[i].Message > 15)
          NewMenuDef[i].Message = 15;
      }
    }

    MenuDefs = NewMenuDef;
    Settings.DspInfo.SetModes(NumSubMenus[NumMenus - 2], ModeName, ModeDPI);
    return true;
  }
  catch(const exception &e)
  {
    log_warn("%s!", e.what());
    log_debug("at %s:%d", __FILE__, __LINE__);

    delete [] NewMenuDef;
    delete [] ModeName;
    delete [] ModeDPI;

    return false;
  }
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//                                                                                                                //
// void ViewApplication::LaunchFilePanel(const BMessage *msg, BWindow *win)                                       //
//                                                                                                                //
// opens the FilePanel.                                                                                           //
//                                                                                                                //
// const BMessage *msg                  message which should be sent to the target                                //
// BWindow        *win                  window the file should be loaded into or 'NULL' if a new window should be //
//                                      created.                                                                  //
//                                                                                                                //
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

void ViewApplication::LaunchFilePanel(const BMessage *msg, BWindow *win)
{
  try
  {
    if (OpenPanel)
      CloseFilePanel();

    if (msg->what != MsgSavePage)
      OpenPanel = new BFilePanel(B_OPEN_PANEL, NULL, &PanelDirectory, B_FILE_NODE, false, NULL);
    else
      OpenPanel = new BFilePanel(B_SAVE_PANEL, NULL, &PanelDirectory, B_FILE_NODE, false, NULL);

    OpenPanel->SetMessage((BMessage *)msg);    // cast away `const'

    if (win)
      OpenPanel->SetTarget(BMessenger(win));   // load messages are handled by the window
    else
      OpenPanel->SetTarget(BMessenger(this));  // open messages are handled by the application

    switch (msg->what)
    {
      case MsgOpenFile:
        OpenPanel->SetButtonLabel(B_DEFAULT_BUTTON, "Open");
        OpenPanel->Window()->SetTitle("BeDVI: Open");
        break;

      case MsgLoadFile:
        OpenPanel->SetButtonLabel(B_DEFAULT_BUTTON, "Load");
        OpenPanel->Window()->SetTitle("BeDVI: Load");
        break;

      case MsgSavePage:
        OpenPanel->SetButtonLabel(B_DEFAULT_BUTTON, "Save");
        OpenPanel->Window()->SetTitle("BeDVI: Save Page");
        break;
    }

    OpenPanel->Show();
  }
  catch(...)
  {
    log_warn("unknown exception!");
    log_debug("at %s:%d", __FILE__, __LINE__);
  }
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//                                                                                                                //
// void ViewApplication::CloseFilePanel()                                                                         //
//                                                                                                                //
// stores current settings of the FilePanel.                                                                      //
//                                                                                                                //
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

void ViewApplication::CloseFilePanel()
{
  if (OpenPanel)
  {
    OpenPanel->GetPanelDirectory(&PanelDirectory);
    delete OpenPanel;
    OpenPanel = NULL;
  }
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//                                                                                                                //
// void ViewApplication::DisplayError(char *msg)                                                                  //
//                                                                                                                //
// Displays an Error Requester.                                                                                   //
//                                                                                                                //
// char *msg                            message to be displayed                                                   //
//                                                                                                                //
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

void ViewApplication::DisplayError(const char *msg)
{
  try
  {
    (new BAlert("BeDVI", msg, "Ok", NULL, NULL, B_WIDTH_AS_USUAL, B_STOP_ALERT))->Go();
  }
  catch(...)
  {
    log_warn("unknown exception!");
    log_debug("at %s:%d", __FILE__, __LINE__);
  }
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//                                                                                                                //
// void ViewApplication::SetResolution(uint PixelsPerInch)                                                        //
//                                                                                                                //
// Sets the resolution of the display.                                                                            //
//                                                                                                                //
// uint PixelsPerInch                   dpi value                                                                 //
//                                                                                                                //
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

void ViewApplication::SetResolution(uint PixelsPerInch)
{
  int i;

  Settings.DspInfo.SetResolution(PixelsPerInch);

  // mode may have changed

  acquire_sem(kpse_sem);

  // clear paths, otherwise kpathseach wouldn't update them

  for (i = 0; i < kpse_last_format; i++)
  {
    if (i != kpse_cnf_format && i != kpse_db_format)
      if (kpse_format_info[i].path != NULL)
      {
        free((char *)kpse_format_info[i].path);     // cast away `const'
        kpse_format_info[i].path = NULL;
      }
  }

  kpse_init_prog("BeDVI", Settings.DspInfo.PixelsPerInch, Settings.DspInfo.Mode, "cmr10");
  kpse_set_program_enabled(kpse_pk_format,        1, kpse_src_compile);
  kpse_set_program_enabled(kpse_any_glyph_format, 1, kpse_src_compile);
  release_sem(kpse_sem);

  for (i = CountWindows(); i > 0; i--)
    WindowAt(i - 1)->PostMessage(MsgResChanged);
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//                                                                                                                //
// static const uint16 SleepCursorImage[]                                                                         //
//                                                                                                                //
// Image of clock cursor.                                                                                         //
//                                                                                                                //
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

static const uint16 SleepCursorImage[] =
{
  // 16x16 points, depth 1, hot spot at (9, 7)

  0x1001, 0x0907,

  // image data

  0x03c0, 0x07c0, 0x0280, 0x07e0, 0x1838, 0x201c, 0x4026, 0x4046,
  0x8083, 0x8103, 0x8003, 0x4006, 0x4006, 0x200c, 0x1838, 0x07e0,

  // mask data

  0x07c0, 0x07c0, 0x0380, 0x07e0, 0x1ff8, 0x3ffc, 0x7ffe, 0x7ffe,
  0xffff, 0xffff, 0xffff, 0x7ffe, 0x7ffe, 0x3ffc, 0x1ff8, 0x07e0
};

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//                                                                                                                //
// void SetSleepCursor()                                                                                          //
//                                                                                                                //
// Sets the cursor image to a sleeping-cloud.                                                                     //
//                                                                                                                //
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

void ViewApplication::SetSleepCursor()
{
  if (atomic_add(&SleepPointerNestCnt, 1) < 1)
    SetCursor(SleepCursorImage);
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//                                                                                                                //
// void SetNormalCursor()                                                                                         //
//                                                                                                                //
// Sets the cursor image to the normal Be-hand.                                                                   //
//                                                                                                                //
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

void ViewApplication::SetNormalCursor()
{
  if (atomic_add(&SleepPointerNestCnt, -1) <= 1)
    SetCursor(B_HAND_CURSOR);
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//                                                                                                                //
// void ViewApplication::AddDocWindow(BWindow *win)                                                               //
//                                                                                                                //
// Adds the window `win' to the list of document windows.                                                         //
//                                                                                                                //
// BWindow *win                         the window to add                                                         //
//                                                                                                                //
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

void ViewApplication::AddDocWindow(BWindow *win)
{
  if (acquire_sem(WinListLock) != B_OK)
    throw(runtime_error("can't acquire semaphore"));

  DocumentWindows.push_back(win);

  release_sem(WinListLock);
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//                                                                                                                //
// void ViewApplication::RemoveDocWindow(BWindow *win)                                                            //
//                                                                                                                //
// Removes the window `win' from the list of document windows.                                                    //
//                                                                                                                //
// BWindow *win                         the window to remove                                                      //
//                                                                                                                //
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

void ViewApplication::RemoveDocWindow(BWindow *win)
{
  if (acquire_sem(WinListLock) != B_OK)
    throw(runtime_error("can't acquire semaphore"));

  DocumentWindows.remove(win);

  release_sem(WinListLock);
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//                                                                                                                //
// BWindow *ViewApplication::NextDocWindow(BWindow *win)                                                          //
//                                                                                                                //
// Returns the next document window in the list.                                                                  //
//                                                                                                                //
// BWindow *win                         window                                                                    //
//                                                                                                                //
// Result:                              the next window                                                           //
//                                                                                                                //
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

BWindow *ViewApplication::NextDocWindow(BWindow *win)
{
  WindowList::iterator i, last;

  if (acquire_sem(WinListLock) != B_OK)
    throw(runtime_error("can't acquire semaphore"));

  for (i = DocumentWindows.begin(), last = DocumentWindows.end(); i != last; i++)
    if (*i == win)
      break;

  if (*i == win)
  {
    if (++i == last)
      i = DocumentWindows.begin();

    win = *i;
  }
  else
    win = NULL;

  release_sem(WinListLock);

  return win;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//                                                                                                                //
// BWindow *ViewApplication::PrevDocWindow(BWindow *win)                                                          //
//                                                                                                                //
// Returns the previous document window in the list.                                                              //
//                                                                                                                //
// BWindow *win                         window                                                                    //
//                                                                                                                //
// Result:                              the previous window                                                       //
//                                                                                                                //
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

BWindow *ViewApplication::PrevDocWindow(BWindow *win)
{
  WindowList::iterator i, last;

  if (acquire_sem(WinListLock) != B_OK)
    throw(runtime_error("can't acquire semaphore"));

  for (i = DocumentWindows.begin(), last = DocumentWindows.end(); i != last; i++)
    if (*i == win)
      break;

  if (*i == win)
  {
    if (i == DocumentWindows.begin())
      i = DocumentWindows.end();

    i--;

    win = *i;
  }
  else
    win = NULL;

  release_sem(WinListLock);

  return win;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//                                                                                                                //
// int ViewApplication::NumDocWindows()                                                                           //
//                                                                                                                //
// Returns the number of document windows in the list.                                                            //
//                                                                                                                //
// Result:                              number of windows                                                         //
//                                                                                                                //
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

int ViewApplication::NumDocWindows()
{
  int num;

  if (acquire_sem(WinListLock) != B_OK)
    throw(runtime_error("can't acquire semaphore"));

  num  = DocumentWindows.size();

  release_sem(WinListLock);

  return num;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//                                                                                                                //
// int main()                                                                                                     //
//                                                                                                                //
// main function.                                                                                                 //
//                                                                                                                //
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

int main()
{
  log_open("BeDVI", LogLevel_Error);

#ifdef PROFILING
  PROFILE_INIT(200);
#endif

  try
  {
    log_info("BeDVI "VERSION" ("__DATE__")");

    if (!InitKpseSem())
    {
      log_fatal("Can't create semaphore!");
      exit(1);
    }

    auto_ptr<ViewApplication> ViewApp(new ViewApplication());

    ViewApp->Run();

    FreeKpseSem();
  }
  catch(const exception &e)
  {
    log_fatal("%s!", e.what());
    log_debug("at %s:%d", __FILE__, __LINE__);
    FreeKpseSem();
    exit(1);
  }
  catch(...)
  {
    log_fatal("unknown exception!");
    log_debug("at %s:%d", __FILE__, __LINE__);
    FreeKpseSem();
    exit(1);
  }

#ifdef PROFILING
  PROFILE_DUMP("BeDVI.dump");
#endif

  exit(0);
}
