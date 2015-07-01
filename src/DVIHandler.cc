////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//                                                                                                                //
// $Id: DVIHandler.cc,v 2.2 1998/07/09 13:36:27 achim Exp $
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

#define kDPIExtension     "DVI/dpi"
#define kShrinkExtension  "DVI/shrink"
#define kAAExtension      "DVI/antialiasing"
#define kBorderExtension  "DVI/borderline"

char translatorName[]  = "BeDVI";
long translatorVersion = 200;
char translatorInfo[]  = "BeDVI " VERSION " (" __DATE__ ")";

translation_format inputFormats[] =
{
  {'DVI ',              B_TRANSLATOR_BITMAP, 0.5, 0.7, "application/x-dvi", "DVI Format"      },
  {B_TRANSLATOR_BITMAP, B_TRANSLATOR_BITMAP, 0.5, 0.7, "image/x-be-bitmap", "Be Bitmap Format"},
  {0,                   0,                   0.0, 0.0, NULL,                NULL              }
};

translation_format outputFormats[] =
{
  {'DVI ',              B_TRANSLATOR_BITMAP, 0.5, 0.7, "application/x-dvi", "DVI Format"      },
  {B_TRANSLATOR_BITMAP, B_TRANSLATOR_BITMAP, 0.5, 0.7, "image/x-be-bitmap", "Be Bitmap Format"},
  {0,                   0,                   0.0, 0.0, NULL,                NULL              }
};

status_t Identify(BPositionIO *inSource, const translation_format * /* inFormat */, BMessage * /* ioExtension */,
                  translator_info *outInfo, uint32 outType)
{
  uint32   GuessedType = 0;
  status_t err;

  if(outType != 0 && outType != B_TRANSLATOR_BITMAP && outType != 'DVI ')
    return B_NO_TRANSLATOR;

  // check header

  union {
    u_char           dvi[sizeof(TranslatorBitmap)];
    TranslatorBitmap BitMap;
  } Buffer;

  err = inSource->Read(&Buffer, sizeof(Buffer));

  if(err != sizeof(Buffer))
    if(err < B_OK)
      return err;
    else
      return B_NO_TRANSLATOR;

  if(Buffer.BitMap.magic == B_TRANSLATOR_BITMAP)
    GuessedType = B_TRANSLATOR_BITMAP;

  else if(Buffer.dvi[0] == Preamble && Buffer.dvi[1] == 2)
    GuessedType = 'DVI ';

  else
    return B_NO_TRANSLATOR;

  if(GuessedType == B_TRANSLATOR_BITMAP)
  {
    int multi = 0;

    // check format

    switch(Buffer.BitMap.colors)
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
    if(Buffer.BitMap.bounds.right  <= Buffer.BitMap.bounds.left ||
       Buffer.BitMap.bounds.bottom <= Buffer.BitMap.bounds.top)
      return B_NO_TRANSLATOR;

    if(Buffer.BitMap.rowBytes < Buffer.BitMap.bounds.IntegerWidth() * multi)
      return B_NO_TRANSLATOR;

    if(Buffer.BitMap.dataSize < Buffer.BitMap.rowBytes * Buffer.BitMap.bounds.IntegerHeight())
      return B_NO_TRANSLATOR;

    // set up outInfo

    outInfo->type       = inputFormats[1].type;
    outInfo->translator = 0;
    outInfo->group      = inputFormats[1].group;
    outInfo->quality    = inputFormats[1].quality;
    outInfo->capability = inputFormats[1].capability;

    sprintf(outInfo->name, "%s, type %d", inputFormats[1].name, Buffer.BitMap.colors);
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

  if(!buffer)
  {
    buffer = temp;
    block = 1024;
  }

  while(!done)
  {
    ssize_t bytes_read;

    if((err = bytes_read = input->Read(buffer, block)) <= 0)
      break;

    done = (bytes_read < block);

    if((err = output->Write(buffer, bytes_read)) < 0)
      break;
  }
  if(buffer != temp)
    free(buffer);

  return err >= 0 ? B_OK : err;
}

static status_t WriteBitMap(BPositionIO *input, BPositionIO *output, BMessage *ioExtension)
{
  DVI              *Document     = NULL;
  DrawSettings     prefs;
  TranslatorBitmap BitMap;
  PREFHandle       PrefHandle    = NULL;
  PREFData         PrefData;
  BView            *BufferView   = NULL;
  BBitmap          *BufferBitMap = NULL;
  BRect            DocumentBounds;
  status_t         err;
  uint32           PixelsPerInch = 300;
  uint32           PageNo        = 1;
  ssize_t          Size;
  uint32           Type;
  void             *p;
  int32            i_value;
  bool             b_value;

  try
  {
    // get settings

    if(PREFInit("x-vnd.blume-BeDVI", &PrefHandle) >= B_OK)                      // use BeDVI's settings
    {
      if(PREFLoadSet(PrefHandle, "settings", true, &PrefData) >= B_OK)
      {
        if(PREFGetData(PrefData, "antialiasing", &p, &Size, &Type) >= B_OK && Type == B_BOOL_TYPE)
          prefs.AntiAliasing = *(bool *)p;
        if(PREFGetData(PrefData, "borderline",   &p, &Size, &Type) >= B_OK && Type == B_BOOL_TYPE)
          prefs.BorderLine   = *(bool *)p;
        if(PREFGetData(PrefData, "dpi",          &p, &Size, &Type) >= B_OK && Type == B_INT32_TYPE)
          PixelsPerInch      = *(int32 *)p;
        if(PREFGetData(PrefData, "shrink",       &p, &Size, &Type) >= B_OK && Type == B_INT16_TYPE)
          prefs.ShrinkFactor = *(int16 *)p;

        PREFDisposeSet(&PrefData);
      }
      PREFShutdown(PrefHandle);
    }

    if(ioExtension)
    {
      if(ioExtension->FindInt32(kFrameExtension,  &i_value) == B_OK)
        PageNo             = i_value;
      if(ioExtension->FindInt32(kDPIExtension,    &i_value) == B_OK)
        PixelsPerInch      = i_value;
      if(ioExtension->FindInt32(kShrinkExtension, &i_value) == B_OK)
        prefs.ShrinkFactor = i_value;
      if(ioExtension->FindBool (kAAExtension,     &b_value) == B_OK)
        prefs.AntiAliasing = b_value;
      if(ioExtension->FindBool (kBorderExtension, &b_value) == B_OK)
        prefs.BorderLine   = b_value;
    }

    prefs.DspInfo.SetResolution(PixelsPerInch);

    // init kpathsea

    if(!InitKpseSem())
      return B_ERROR;

    acquire_sem(kpse_sem);
    kpse_set_program_name("/boot/home/config/add-ons/Translators/DVIHandler", NULL);
    kpse_init_prog("BEDVI", prefs.DspInfo.PixelsPerInch, prefs.DspInfo.Mode, "cmr10");
    kpse_set_program_enabled(kpse_pk_format,        1, kpse_src_compile);
    kpse_set_program_enabled(kpse_any_glyph_format, 1, kpse_src_compile);
    release_sem(kpse_sem);

    // draw page in bitmap

    if(!(Document = new DVI(input, &prefs)))
    {
      FreeKpseSem();
      return B_NO_MEMORY;
    }

    DocumentBounds = BRect(0, 0, Document->PageWidth - 1, Document->PageHeight - 1);

    if(!(BufferView = new BView(DocumentBounds, NULL, 0, 0)))
    {
      delete Document;
      FreeKpseSem();
      return B_NO_MEMORY;
    }

    if(!(BufferBitMap = new BBitmap(DocumentBounds, B_COLOR_8_BIT, true)))
    {
      delete BufferView;
      delete Document;
      FreeKpseSem();
      return B_NO_MEMORY;
    }

    BufferBitMap->AddChild(BufferView);

    if(!BufferView->Window()->Lock())
    {
      delete BufferBitMap;
      delete Document;
      FreeKpseSem();
      return B_ERROR;
    }

    Document->Draw(BufferView, PageNo);

    BufferView->Window()->Unlock();

    FreeKpseSem();

    BitMap.magic    = B_TRANSLATOR_BITMAP;
    BitMap.bounds   = DocumentBounds;
    BitMap.rowBytes = BufferBitMap->BytesPerRow();
    BitMap.dataSize = BitMap.rowBytes * (BitMap.bounds.IntegerHeight() + 1);
    BitMap.colors   = B_COLOR_8_BIT;

    if((err = output->Write(&BitMap, sizeof(BitMap))) < B_OK)
    {
      delete BufferBitMap;
      delete Document;
      return err;
    }

    err = output->Write(BufferBitMap->Bits(), BitMap.dataSize);

    delete BufferBitMap;
    delete Document;

    return err < B_OK ? err : B_OK;
  }
  catch(...)
  {
    if(BufferBitMap)
      delete BufferBitMap;
    if(Document)
      delete Document;
    return B_NO_MEMORY;
  }
}

status_t Translate(BPositionIO *inSource, const translator_info *inInfo, BMessage *ioExtension, uint32 outFormat,
                   BPositionIO *outDestination)
{
  if(outFormat == inInfo->type)
    return CopyLoop(inSource, outDestination);

  else if(outFormat == B_TRANSLATOR_BITMAP && inInfo->type == 'DVI ')
    return WriteBitMap(inSource, outDestination, ioExtension);

  return B_NO_TRANSLATOR;
}

status_t GetConfigMessage(BMessage *ioExtension)
{
  ioExtension->AddInt32(kFrameExtension,  1);
  ioExtension->AddInt32(kDPIExtension,    300);
  ioExtension->AddInt32(kShrinkExtension, 3);
  ioExtension->AddBool (kAAExtension,     true);
  ioExtension->AddBool (kBorderExtension, false);

  return B_OK;
}

int main()
{
  exit(0);
}
