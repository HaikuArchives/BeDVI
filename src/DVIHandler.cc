////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//                                                                                                                //
// $Id: DVIHandler.cc,v 2.4 1999/07/22 13:36:40 achim Exp $
//                                                                                                                //
// DVIHandler                                                                                                     //
// by Achim Blumensath                                                                                            //
// blume@corona.oche.de                                                                                           //
//                                                                                                                //
// This program is free software! It may be distributed according to the GNU Public License (see COPYING).        //
//                                                                                                                //
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

#include <AppDefs.h>
#include <Bitmap.h>
#include <InterfaceKit.h>
#include <TranslatorAddOn.h>
#include <TranslatorFormats.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <syslog.h>
#include "BeDVI.h"
#include "DVI.h"

extern "C"
{
  #define string _string
  #include "kpathsea/c-auto.h"
  #include "kpathsea/progname.h"
  #include "kpathsea/proginit.h"
  #include "kpathsea/tex-file.h"
  #undef string
}

// private extensions

#define kModeExtension    "DVI/mode"
#define kDPIExtension     "DVI/dpi"
#define kShrinkExtension  "DVI/shrink"
#define kAAExtension      "DVI/antialiasing"

class HandlerSettings
{
  public:
    sem_id Lock;
    char   *Mode;
    int32  PixelsPerInch;
    int32  PageNo;
    int16  ShrinkFactor;
    bool   AntiAliasing;

    HandlerSettings();
    ~HandlerSettings();
};

static HandlerSettings settings;

char translatorName[]  = "BeDVI";
long translatorVersion = 210;
char translatorInfo[]  = "BeDVI " VERSION " (" __DATE__ ")";

translation_format inputFormats[] =
{
  {'DVI ',              B_TRANSLATOR_BITMAP, 0.5, 0.7, "application/x-dvi", "DVI Format"      },
  {0,                   0,                   0.0, 0.0, NULL,                NULL              }
};

translation_format outputFormats[] =
{
  {'DVI ',              B_TRANSLATOR_BITMAP, 0.5, 0.7, "application/x-dvi", "DVI Format"      },
  {B_TRANSLATOR_BITMAP, B_TRANSLATOR_BITMAP, 0.5, 0.7, "image/x-be-bitmap", "Be Bitmap Format"},
  {0,                   0,                   0.0, 0.0, NULL,                NULL              }
};

HandlerSettings::HandlerSettings():
  Lock(B_ERROR),
  Mode(NULL),
  PixelsPerInch(600),
  ShrinkFactor(6),
  PageNo(1),
  AntiAliasing(true)
{
  PREFHandle PrefHandle = NULL;
  PREFData   PrefData;
  ssize_t    Size;
  uint32     Type;
  void       *p;

  if ((Lock = create_sem(0, "settings lock")) < B_OK)
    return;

  try
  {
    // get settings

    if (PREFInit("x-vnd.blume-BeDVI", &PrefHandle) >= B_OK)
    {
      if (PREFLoadSet(PrefHandle, "TranslatorSettings", true, &PrefData) >= B_OK)
      {
        // use my own settings

        if (PREFGetData(PrefData, "mode",         &p, &Size, &Type) >= B_OK && Type == B_STRING_TYPE)
        {
          Mode = new char [strlen((char *)p) + 1];
          strcpy(Mode, (char *)p);
        }
        if (PREFGetData(PrefData, "dpi",          &p, &Size, &Type) >= B_OK && Type == B_INT32_TYPE)
          PixelsPerInch = *(int32 *)p;
        if (PREFGetData(PrefData, "page",         &p, &Size, &Type) >= B_OK && Type == B_INT32_TYPE)
          PageNo        = *(int32 *)p;
        if (PREFGetData(PrefData, "shrink",       &p, &Size, &Type) >= B_OK && Type == B_INT16_TYPE)
          ShrinkFactor  = *(int16 *)p;
        if (PREFGetData(PrefData, "antialiasing", &p, &Size, &Type) >= B_OK && Type == B_BOOL_TYPE)
          AntiAliasing  = *(bool *)p;

        PREFDisposeSet(&PrefData);
      }
      PREFShutdown(PrefHandle);
    }

    if (!Mode)
    {
      Mode = new char[10];

      if (PixelsPerInch < 300)
        strcpy(Mode, "nextscrn");

      else if (PixelsPerInch < 600)
        strcpy(Mode, "cx");

      else
        strcpy(Mode, "ljfour");
    }
  }
  catch(...)
  {
  }

  release_sem(Lock);
}

HandlerSettings::~HandlerSettings()
{
  PREFHandle PrefHandle = NULL;
  PREFData   PrefData;

  acquire_sem(Lock);

  delete_sem(Lock);

  if (PREFInit("x-vnd.blume-BeDVI", &PrefHandle) < B_OK)
    return;

  if (PREFLoadSet(PrefHandle, "TranslatorSettings", true, &PrefData) >= B_OK)
  {
    PREFSetData(PrefData, "mode",         Mode,           strlen(Mode) + 1, B_STRING_TYPE);
    PREFSetData(PrefData, "dpi",          &PixelsPerInch, sizeof(int32),    B_INT32_TYPE);
    PREFSetData(PrefData, "page",         &PageNo,        sizeof(int32),    B_INT32_TYPE);
    PREFSetData(PrefData, "shrink",       &ShrinkFactor,  sizeof(int16),    B_INT16_TYPE);
    PREFSetData(PrefData, "antialiasing", &AntiAliasing,  sizeof(bool),     B_BOOL_TYPE);

    PREFSaveSet(PrefData);
    PREFDisposeSet(&PrefData);
  }
  PREFShutdown(PrefHandle);
}

status_t Identify(BPositionIO *inSource, const translation_format * /* inFormat */, BMessage * /* ioExtension */,
                  translator_info *outInfo, uint32 outType)
{
  uint32   GuessedType;
  status_t err;

  if (outType != 0 && outType != B_TRANSLATOR_BITMAP && outType != 'DVI ')
    return B_NO_TRANSLATOR;

  // check header

  TranslatorBitmap BitMap;

  err = inSource->Read(&BitMap, sizeof(BitMap));

  if (err != sizeof(BitMap))
    if (err < B_OK)
      return err;
    else
      return B_NO_TRANSLATOR;

  GuessedType = 0;

  if (((uchar *)&BitMap)[0] == DVI::Preamble && ((uchar *)&BitMap)[1] == 2)
    GuessedType = 'DVI ';

  swap_data (B_INT32_TYPE, &BitMap.magic,    sizeof(uint32),      B_SWAP_BENDIAN_TO_HOST);
  swap_data (B_RECT_TYPE,  &BitMap.bounds,   sizeof(BRect),       B_SWAP_BENDIAN_TO_HOST);
  swap_data (B_INT32_TYPE, &BitMap.rowBytes, sizeof(uint32),      B_SWAP_BENDIAN_TO_HOST);
  swap_data (B_INT32_TYPE, &BitMap.colors,   sizeof(color_space), B_SWAP_BENDIAN_TO_HOST);
  swap_data (B_INT32_TYPE, &BitMap.dataSize, sizeof(uint32),      B_SWAP_BENDIAN_TO_HOST);

  if (BitMap.magic == B_TRANSLATOR_BITMAP)
    GuessedType = B_TRANSLATOR_BITMAP;

  if (GuessedType == 0)
    return B_NO_TRANSLATOR;

  if (GuessedType == B_TRANSLATOR_BITMAP)
  {
    int multi = 0;

    // check format

    switch (BitMap.colors)
    {
      case B_RGB_32_BIT:
      case B_BIG_RGB_32_BIT:
        multi = 4;
        break;

      case B_COLOR_8_BIT:
        multi = 1;
        break;

      default:
        return B_NO_TRANSLATOR;
    }
    if (BitMap.bounds.right  <= BitMap.bounds.left ||
        BitMap.bounds.bottom <= BitMap.bounds.top)
      return B_NO_TRANSLATOR;

    if (BitMap.rowBytes < BitMap.bounds.IntegerWidth() * multi)
      return B_NO_TRANSLATOR;

    if (BitMap.dataSize < BitMap.rowBytes * BitMap.bounds.IntegerHeight())
      return B_NO_TRANSLATOR;

    // set up outInfo

    outInfo->type       = inputFormats[1].type;
    outInfo->translator = 0;
    outInfo->group      = inputFormats[1].group;
    outInfo->quality    = inputFormats[1].quality;
    outInfo->capability = inputFormats[1].capability;

    sprintf(outInfo->name, "%s, type %d", inputFormats[1].name, BitMap.colors);
    strcpy(outInfo->MIME, inputFormats[1].MIME);
  }
  else
  {
    outInfo->type       = inputFormats[0].type;
    outInfo->translator = 0;
    outInfo->group      = inputFormats[0].group;
    outInfo->quality    = inputFormats[0].quality;
    outInfo->capability = inputFormats[0].capability;
    sprintf(outInfo->name, "%s", inputFormats[0].name);
    strcpy(outInfo->MIME, inputFormats[0].MIME);
  }
  return B_OK;
}

static status_t CopyLoop(BPositionIO *input, BPositionIO *output)
{
  size_t   block   = 65536L;
  void     *buffer = malloc(block);
  char     temp[1024];
  bool     done    = false;
  status_t err     = B_OK;

  if (!buffer)
  {
    buffer = temp;
    block = 1024;
  }

  while (!done)
  {
    ssize_t bytes_read;

    if ((err = bytes_read = input->Read(buffer, block)) <= 0)
      break;

    done = (bytes_read < block);

    if ((err = output->Write(buffer, bytes_read)) < 0)
      break;
  }
  if (buffer != temp)
    free(buffer);

  return err >= 0 ? B_OK : err;
}

static status_t WriteBitMap(BPositionIO *input, BPositionIO *output, BMessage *ioExtension)
{
  DVI              *Document     = NULL;
  DrawSettings     Settings;
  TranslatorBitmap BitMap;
  BView            *BufferView   = NULL;
  BBitmap          *BufferBitMap = NULL;
  BRect            DocumentBounds;
  status_t         err;
  char             *s_value;
  int32            i_value;
  bool             b_value;

  char  *Mode;
  int32 PixelsPerInch;
  int32 PageNo;
  int16 ShrinkFactor;
  bool  AntiAliasing;

  try
  {
    // get settings

    acquire_sem(settings.Lock);

    Mode = new char[strlen(settings.Mode) + 1];
    strcpy(Mode, settings.Mode);

    PixelsPerInch = settings.PixelsPerInch;
    PageNo        = settings.PageNo;
    ShrinkFactor  = settings.ShrinkFactor;
    AntiAliasing  = settings.AntiAliasing;

    release_sem(settings.Lock);
  }
  catch(...)
  {
    release_sem(settings.Lock);
    return B_NO_MEMORY;
  }

  try
  {
    if (ioExtension)
    {
      if (ioExtension->FindInt32(B_TRANSLATOR_EXT_FRAME, &i_value) == B_OK)
        PageNo        = i_value;
      if (ioExtension->FindString(kModeExtension,  &s_value) == B_OK)
      {
        delete [] Mode;
        Mode = new char [strlen(s_value) + 1];
        strcpy(Mode, s_value);
      }
      if (ioExtension->FindInt32(kDPIExtension,    &i_value) == B_OK)
        PixelsPerInch = i_value;
      if (ioExtension->FindInt32(kShrinkExtension, &i_value) == B_OK)
        ShrinkFactor  = i_value;
      if (ioExtension->FindBool (kAAExtension,     &b_value) == B_OK)
        AntiAliasing  = b_value;
    }

    // init kpathsea

    if (!InitKpseSem())
      return B_ERROR;

    acquire_sem(kpse_sem);
    kpse_set_program_name("/boot/home/config/add-ons/Translators/DVIHandler", NULL);
    kpse_init_prog("BEDVI", PixelsPerInch, Mode, "cmr10");
    kpse_set_program_enabled(kpse_pk_format,        1, kpse_src_compile);
    kpse_set_program_enabled(kpse_any_glyph_format, 1, kpse_src_compile);
    release_sem(kpse_sem);

    // draw page in bitmap

    Settings.DspInfo.Mode          = Mode;
    Settings.DspInfo.PixelsPerInch = PixelsPerInch;
    Settings.ShrinkFactor          = ShrinkFactor;
    Settings.AntiAliasing          = AntiAliasing;

    if (!(Document = new DVI(input, &Settings)))
    {
      FreeKpseSem();
      return B_NO_MEMORY;
    }

    DocumentBounds = BRect(0, 0, Document->PageWidth - 1, Document->PageHeight - 1);

    if (!(BufferView = new BView(DocumentBounds, NULL, 0, 0)))
    {
      delete Document;
      FreeKpseSem();
      return B_NO_MEMORY;
    }

    if (!(BufferBitMap = new BBitmap(DocumentBounds, B_COLOR_8_BIT, true)))
    {
      delete BufferView;
      delete Document;
      FreeKpseSem();
      return B_NO_MEMORY;
    }

    BufferBitMap->AddChild(BufferView);

    if (!BufferView->LockLooper())
    {
      delete BufferBitMap;
      delete Document;
      FreeKpseSem();
      return B_ERROR;
    }

    Document->Draw(BufferView, &Settings, PageNo);

    BufferView->UnlockLooper();

    FreeKpseSem();

    BitMap.magic    = B_TRANSLATOR_BITMAP;
    BitMap.bounds   = DocumentBounds;
    BitMap.rowBytes = BufferBitMap->BytesPerRow();
    BitMap.dataSize = BitMap.rowBytes * (BitMap.bounds.IntegerHeight() + 1);
    BitMap.colors   = B_COLOR_8_BIT;

    size_t size     = BitMap.dataSize;        // save size, it may be swapped below

    swap_data(B_INT32_TYPE, &BitMap.magic,    sizeof(uint32),      B_SWAP_HOST_TO_BENDIAN);
    swap_data(B_RECT_TYPE,  &BitMap.bounds,   sizeof(BRect),       B_SWAP_HOST_TO_BENDIAN);
    swap_data(B_INT32_TYPE, &BitMap.rowBytes, sizeof(uint32),      B_SWAP_HOST_TO_BENDIAN);
    swap_data(B_INT32_TYPE, &BitMap.colors,   sizeof(color_space), B_SWAP_HOST_TO_BENDIAN);
    swap_data(B_INT32_TYPE, &BitMap.dataSize, sizeof(uint32),      B_SWAP_HOST_TO_BENDIAN);

    if ((err = output->Write(&BitMap, sizeof(BitMap))) < B_OK)
    {
      delete BufferBitMap;
      delete Document;
      return err;
    }

    err = output->Write(BufferBitMap->Bits(), size);

    delete BufferBitMap;
    delete Document;

    return err < B_OK ? err : B_OK;
  }
  catch(...)
  {
    delete BufferBitMap;
    delete Document;
    return B_NO_MEMORY;
  }
}

status_t Translate(BPositionIO *inSource, const translator_info *inInfo, BMessage *ioExtension, uint32 outFormat,
                   BPositionIO *outDestination)
{
  if (outFormat == inInfo->type)
    return CopyLoop(inSource, outDestination);

  else if (outFormat == B_TRANSLATOR_BITMAP && inInfo->type == 'DVI ')
    return WriteBitMap(inSource, outDestination, ioExtension);

  return B_NO_TRANSLATOR;
}

status_t GetConfigMessage(BMessage *ioExtension)
{
  status_t err;

  if ((err = acquire_sem(settings.Lock)) < B_OK)
    return err;

  ioExtension->RemoveName(B_TRANSLATOR_EXT_FRAME);
  ioExtension->RemoveName(kModeExtension);
  ioExtension->RemoveName(kDPIExtension);
  ioExtension->RemoveName(kShrinkExtension);
  ioExtension->RemoveName(kAAExtension);

  ioExtension->AddInt32(B_TRANSLATOR_EXT_FRAME, settings.PageNo);
  ioExtension->AddString(kModeExtension,        settings.Mode);
  ioExtension->AddInt32(kDPIExtension,          settings.PixelsPerInch);
  ioExtension->AddInt32(kShrinkExtension,       settings.ShrinkFactor);
  ioExtension->AddBool (kAAExtension,           settings.AntiAliasing);

  release_sem(settings.Lock);

  return B_OK;
}

class ParamView: public BView
{
  private:
    enum
    {
      MsgChanged = 'chng'
    };

  public:
    BTextControl *Mode;
    BTextControl *DPI;
    BTextControl *Shrink;
    BTextControl *PageNo;
    BCheckBox    *AntiAliasing;

    ParamView();
    virtual ~ParamView();

    virtual void AttachedToWindow();
    virtual void MessageReceived(BMessage *msg);
};

ParamView::ParamView():
  BView(BRect(0, 0, 100, 100), NULL, B_FOLLOW_ALL_SIDES, 0),
  Mode(NULL),
  DPI(NULL),
  Shrink(NULL),
  AntiAliasing(NULL)
{
  BRect r(0, 1, 100, 10);

  Mode         = new BTextControl(r, NULL, "Mode:",   NULL, new BMessage(MsgChanged), B_FOLLOW_TOP | B_FOLLOW_LEFT_RIGHT);
  AddChild(Mode);

  DPI          = new BTextControl(r, NULL, "dpi:",    NULL, new BMessage(MsgChanged), B_FOLLOW_TOP | B_FOLLOW_LEFT_RIGHT);
  AddChild(DPI);

  Shrink       = new BTextControl(r, NULL, "Shrink:", NULL, new BMessage(MsgChanged), B_FOLLOW_TOP | B_FOLLOW_LEFT_RIGHT);
  AddChild(Shrink);

  PageNo       = new BTextControl(r, NULL, "Page:",   NULL, new BMessage(MsgChanged), B_FOLLOW_TOP | B_FOLLOW_LEFT_RIGHT);
  AddChild(PageNo);

  AntiAliasing = new BCheckBox(r,    NULL, "AntiAliasing",  new BMessage(MsgChanged), B_FOLLOW_TOP | B_FOLLOW_LEFT_RIGHT);
  AddChild(AntiAliasing);
}

ParamView::~ParamView()
{
}

void ParamView::AttachedToWindow()
{
  static const int Spacing = 3;

  BRect bounds = Bounds();
  BRect r;
  float width, w;

  BView::AttachedToWindow();

  // tell controls where to send their messages

  BMessenger messenger(this);

  Mode->SetTarget(messenger);
  DPI->SetTarget(messenger);
  Shrink->SetTarget(messenger);
  PageNo->SetTarget(messenger);
  AntiAliasing->SetTarget(messenger);

  // position controls

  Window()->DisableUpdates();

  width = (bounds.Width() + 1 - 2 * Spacing) / 2;

  Mode->ResizeToPreferred();
  DPI->ResizeToPreferred();
  Shrink->ResizeToPreferred();
  PageNo->ResizeToPreferred();
  AntiAliasing->ResizeToPreferred();

  Mode->MoveTo(Spacing, Spacing);

  r = Mode->Frame();

  Mode->ResizeTo(width - 1, r.Height());

  DPI->MoveTo(width + Spacing, Spacing);
  DPI->ResizeTo(width - 2 * Spacing, r.Height());

  Shrink->MoveTo(Spacing, r.bottom + Spacing);

  r = Shrink->Frame();

  Shrink->ResizeTo(width - 1, r.Height());

  AntiAliasing->MoveTo(Spacing, r.bottom + Spacing);

  r = AntiAliasing->Frame();

  PageNo->MoveTo(Spacing, r.bottom + Spacing);

  r = PageNo->Frame();

  PageNo->ResizeTo(width - 1, r.Height());

  width = StringWidth("Mode: ");
  w     = StringWidth("Shrink: ");

  if (w > width)  width = w;

  w     = StringWidth("Page: ");

  if (w > width)  width = w;

  Mode->SetDivider(width);
  Shrink->SetDivider(width);
  PageNo->SetDivider(width);

  if (acquire_sem(settings.Lock) == B_OK)
  {
    char buffer[16];

    sprintf(buffer, "%ld", settings.PageNo);
    PageNo->SetText(buffer);

    Mode->SetText(settings.Mode);

    sprintf(buffer, "%ld", settings.PixelsPerInch);
    DPI->SetText(buffer);

    sprintf(buffer, "%ld", settings.ShrinkFactor);
    Shrink->SetText(buffer);

    AntiAliasing->SetValue(settings.AntiAliasing);

    release_sem(settings.Lock);
  }

  Window()->EnableUpdates();
}

void ParamView::MessageReceived(BMessage *msg)
{
  if (msg->what != MsgChanged)
  {
    BView::MessageReceived(msg);
    return;
  }

  if (acquire_sem(settings.Lock) == B_OK)
  {
    char *mode;
    long value;

    if (mode = new(nothrow) char [strlen(Mode->Text()) + 1])
    {
      strcpy(mode, Mode->Text());

      delete [] settings.Mode;
      settings.Mode = mode;
    }
    sscanf(PageNo->Text(), "%ld", &value);
    settings.PageNo        = value;
    sscanf(DPI->Text(),    "%ld", &value);
    settings.PixelsPerInch = value;
    sscanf(Shrink->Text(), "%ld", &value);
    settings.ShrinkFactor  = value;

    settings.AntiAliasing = AntiAliasing->Value();

    release_sem(settings.Lock);
  }
}

status_t MakeConfig(BMessage *ioExtension, BView **outView, BRect *outExtent)
{
  try
  {
    auto_ptr<ParamView> vw(new ParamView);

    *outExtent = vw.get()->Bounds();
    *outView   = vw.release();

    if (ioExtension)
    {
      char *str;
      int32 x;
      bool  b;

      if (acquire_sem(settings.Lock) == B_OK)
      {
        if (ioExtension->FindString(kModeExtension, &str) == B_OK)
        {
          char *mode;

          if (mode = new(nothrow) char[strlen(str) + 1])
          {
            strcpy(mode, str);
            delete [] settings.Mode;
            settings.Mode = mode;
          }
        }
        if (ioExtension->FindInt32(B_TRANSLATOR_EXT_FRAME, &x) == B_OK)
          settings.PageNo        = x;
        if (ioExtension->FindInt32(kDPIExtension,          &x) == B_OK)
          settings.PixelsPerInch = x;
        if (ioExtension->FindInt32(kShrinkExtension,       &x) == B_OK)
          settings.ShrinkFactor  = x;
        if (ioExtension->FindBool (kAAExtension,           &b) == B_OK)
          settings.AntiAliasing  = b;

        release_sem(settings.Lock);
      }
    }

    return B_OK;
  }
  catch(...)
  {
    return B_NO_MEMORY;
  }
}

int main()
{
  exit(0);
}
