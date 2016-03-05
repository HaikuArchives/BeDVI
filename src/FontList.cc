////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//                                                                                                                //
// $Id: FontList.cc,v 2.5 1999/07/25 13:24:21 achim Exp $
//                                                                                                                //
// BeDVI                                                                                                          //
// by Achim Blumensath                                                                                            //
// blume@corona.oche.de                                                                                           //
//                                                                                                                //
// This program is free software! It may be distributed according to the GNU Public License (see COPYING).        //
//                                                                                                                //
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

#include <StorageKit.h>
#include <string.h>
#include <syslog.h>
#include <Debug.h>
#include "BeDVI.h"
#include "FontList.h"
#include "TeXFont.h"
#include "log.h"


/* FontList *******************************************************************************************************/


////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//                                                                                                                //
// FontList::FontList()                                                                                           //
//                                                                                                                //
// Initializes a FontList.                                                                                        //
//                                                                                                                //
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

FontList::FontList()
{
  ListLock = create_sem(1, NULL);
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//                                                                                                                //
// FontList::~FontList()                                                                                          //
//                                                                                                                //
// Deletes a FontList.                                                                                            //
//                                                                                                                //
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

FontList::~FontList()
{
  if (ListLock >= B_OK)
  {
    acquire_sem(ListLock);
    delete_sem(ListLock);
  }
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//                                                                                                                //
// Font *FontList::LoadFont(const DVI *doc, const DrawSettings *Settings, const char *Name, float Size,           //
//                          long ChkSum, int MagStep, double DimConvert)                                          //
//                                                                                                                //
// Loads a font and adds it to the list.                                                                          //
//                                                                                                                //
// const DVI          *doc              document the font appears in                                              //
// const DrawSettings *Settings         settings                                                                  //
// const char         *Name             name of the font                                                          //
// float              Size              size of the font                                                          //
// long               ChkSum            checksum                                                                  //
// int                MagStep           magnification                                                             //
// double             DimConvert        factor to convert dimensions                                              //
//                                                                                                                //
// Result:                              pointer to the font or `NULL' if an error occures                         //
//                                                                                                                //
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

Font *FontList::LoadFont(const DVI *doc, const DrawSettings *Settings, const char *Name, float Size, long ChkSum,
                         int MagStep, double DimConvert)
{
  Font *f = NULL;

  log_info("loading font: %s, %f, %lu, %d, %g", Name, Size, ChkSum, MagStep, DimConvert);

  if (acquire_sem(ListLock) < B_OK)
    return NULL;

  try
  {
    FontList_t::iterator i = Fonts.begin();

    for (i = Fonts.begin(); i != Fonts.end(); i++)
      if (strcmp((*i)->Name, Name) == 0 && (int)(Size + 0.5) == (int)((*i)->Size + 0.5))
        break;

    if (i == Fonts.end())
    {
      f = new Font(doc, Settings, Name, Size, ChkSum, MagStep, DimConvert);

      if (!f->Loaded)
        throw(runtime_error("can't load font"));

      Fonts.push_back(f);
    }

    atomic_add(&f->UseCount, 1);
  }
  catch(const exception &e)
  {
    log_warn("%s!", e.what());
    log_debug("at %s:%d", __FILE__, __LINE__);

    delete f;
    f = NULL;
  }
  release_sem(ListLock);
  
  return f;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//                                                                                                                //
// void FontList::FreeFont(Font *f)                                                                               //
//                                                                                                                //
// frees a font.                                                                                                  //
//                                                                                                                //
// Font *f                              font to be freed                                                          //
//                                                                                                                //
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

void FontList::FreeFont(Font *f)
{
  if (atomic_add(&f->UseCount, -1) == 1)
  {
    if (acquire_sem(ListLock) < B_OK)
      return;

    Fonts.remove(f);

    release_sem(ListLock);

    delete f;
  }
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//                                                                                                                //
// void FontList::FreeAll()                                                                                       //
//                                                                                                                //
// Frees all fonts in the list.                                                                                   //
//                                                                                                                //
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

void FontList::FreeAll()
{
  if (acquire_sem(ListLock) < B_OK)
    return;

  Fonts.clear();

  release_sem(ListLock);
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//                                                                                                                //
// void FontList::FreeUnusedFonts()                                                                               //
//                                                                                                                //
// Frees all unused fonts in the list.                                                                            //
//                                                                                                                //
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

void FontList::FreeUnusedFonts()
{
  FontList_t::iterator i;

  if (acquire_sem(ListLock) < B_OK)
    return;

  for (i = Fonts.begin(); i != Fonts.end(); i++)
    if ((*i)->UseCount < 1)
    {
      delete *i;

      Fonts.erase(i);
    }

  release_sem(ListLock);
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//                                                                                                                //
// void FontList::FlushShrinkedGlyphes()                                                                          //
//                                                                                                                //
// Frees all bitmaps of shrinked glyphes which is neccessary if the anti aliasing method is changed.              //
//                                                                                                                //
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

void FontList::FlushShrinkedGlyphes()
{
  FontList_t::iterator i;

  if (acquire_sem(ListLock) < B_OK)
    return;

  for (i = Fonts.begin(); i != Fonts.end(); i++)
    (*i)->FlushShrinkedGlyphes();

  release_sem(ListLock);
}


/* FontTable ******************************************************************************************************/


////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//                                                                                                                //
// FontTable::FontTable()                                                                                         //
//                                                                                                                //
// Initializes a FontTable.                                                                                       //
//                                                                                                                //
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

FontTable::FontTable():
  TableSem(B_ERROR),
  Table(NULL),
  TableLen(0)
{
  if ((TableSem = create_sem(1, NULL)) < B_OK)
    return;

  if (Table = new Font *[16])
  {
    TableLen = 16;
    memset(Table, 0, TableLen * sizeof(Font *));
  }
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//                                                                                                                //
// FontTable::~FontTable()                                                                                        //
//                                                                                                                //
// Deletes a FontTable.                                                                                           //
//                                                                                                                //
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

FontTable::~FontTable()
{
  if (TableSem >= B_OK)
  {
    acquire_sem(TableSem);
    delete_sem(TableSem);
  }

  if (Table)
  {
    FreeFonts();
    delete [] Table;
  }
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//                                                                                                                //
// Font *FontTable::LoadFont(const DVI *doc, const DrawSettings *Settings, BPositionIO *File, uchar Command)      //
//                                                                                                                //
// Loads a font and adds it to the table.                                                                         //
//                                                                                                                //
// const DVI          *doc              document the font appears in                                              //
// const DrawSettings *Settings         settings                                                                  //
// BPositionIO        *File             file which contains the fontname                                          //
// Font               *VirtualParent    virtual font this font belongs to or `NULL'                               //
// uchar              Command           Font-Definition command                                                   //
//                                                                                                                //
// Result:                              pointer to the font or `NULL' if an error occured                         //
//                                                                                                                //
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

Font *FontTable::LoadFont(const DVI *doc, const DrawSettings *Settings, BPositionIO *File, Font *VirtualParent,
                          uchar Command)
{
  Font    *NewFont;
  int     TeXNo;
  long    ChkSum;
  int     Scale;
  int     Design;
  int     MagStep;
  int     len;
  char    *FontName;
  ssize_t ReadSize;
  float   FontSize;
  int     Size;
  double  ScaleDimConvert;

  try
  {
    TeXNo  = ReadInt(File, Command - DVI::FontDef1 + 1);
    ChkSum = ReadInt(File, 4);
    Scale  = ReadInt(File, 4);
    Design = ReadInt(File, 4);

    len    = ReadInt(File, 1);
    len   += ReadInt(File, 1);

    FontName = new char[len + 1];

    ReadSize = len;
    File->Read(FontName, ReadSize);
    FontName[len] = 0;

    if (!VirtualParent)
    {
      FontSize        = 0.001 * Scale / Design * doc->Magnification * Settings->DspInfo.PixelsPerInch;
      ScaleDimConvert = doc->DimConvert;
    }
    else
    {
      FontSize        = 72.27 * 16 * VirtualParent->DimConvert * Scale / Design;
      ScaleDimConvert = VirtualParent->DimConvert;
    }

    MagStep = doc->MagStepValue(Settings->DspInfo.PixelsPerInch, FontSize);
    Size    = FontSize + 0.5;

    if (!(NewFont = Fonts.LoadFont(doc, Settings, FontName, FontSize, ChkSum, MagStep,
                                   Scale * ScaleDimConvert / (double)(1L << 20))))
    {
      delete [] FontName;
      return NULL;
    }

    delete [] FontName;

    if (TeXNo >= TableLen)
      if (!Resize(TeXNo + 8))
        return NULL;

    Table[TeXNo] = NewFont;

    return NewFont;
  }
  catch(const exception &e)
  {
    log_warn("%s!", e.what());
    log_debug("at %s:%d", __FILE__, __LINE__);
    return NULL;
  }
  catch(...)
  {
    log_warn("unknown exception!");
    log_debug("at %s:%d", __FILE__, __LINE__);
    return NULL;
  }
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//                                                                                                                //
// bool FontTable::Resize(ulong len)                                                                              //
//                                                                                                                //
// Resizes the table.                                                                                             //
//                                                                                                                //
// ulong len                            new length of the table                                                   //
//                                                                                                                //
// Result:                              `true' if successful, otherwise `false'                                   //
//                                                                                                                //
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

bool FontTable::Resize(ulong len)
{
  Font **NewTable;

  try
  {
    if (!(NewTable = new Font *[len]))
      return false;

    memcpy(NewTable, Table, min_c(len, TableLen) * sizeof(Font *));

    if (len > TableLen)
      memset(NewTable + TableLen, 0, (len - TableLen) * sizeof(Font *));

    if (acquire_sem(TableSem) != B_OK)
    {
      delete [] NewTable;
      return false;
    }

    delete [] Table;

    Table    = NewTable;
    TableLen = len;

    release_sem(TableSem);

    return true;
  }
  catch(const exception &e)
  {
    log_warn("%s!", e.what());
    log_debug("at %s:%d", __FILE__, __LINE__);
    return false;
  }
  catch(...)
  {
    log_warn("unknown exception!");
    log_debug("at %s:%d", __FILE__, __LINE__);
    return false;
  }
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//                                                                                                                //
// void FontTable::FreeFonts()                                                                                    //
//                                                                                                                //
// Frees all fonts in the table.                                                                                  //
//                                                                                                                //
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

void FontTable::FreeFonts()
{
  int i;

  for (i = TableLen - 1; i >= 0; i--)
    if (Table[i])
    {
      Fonts.FreeFont(Table[i]);
      Table[i] = NULL;
    }
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//                                                                                                                //
// void FontTable::FreeUnusedFonts()                                                                              //
//                                                                                                                //
// Frees all unused fonts in the list.                                                                            //
//                                                                                                                //
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

void FontTable::FreeUnusedFonts()
{
  Fonts.FreeUnusedFonts();
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//                                                                                                                //
// void FontTable::FlushShrinkedGlyphes()                                                                         //
//                                                                                                                //
// Frees all bitmaps of shrinked glyphes which is neccessary if the anti aliasing method is changed.              //
//                                                                                                                //
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

void FontTable::FlushShrinkedGlyphes()
{
  Fonts.FlushShrinkedGlyphes();
}
