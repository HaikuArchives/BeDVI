////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//                                                                                                                //
// $Id: FontList.h,v 2.4 1999/07/25 13:24:21 achim Exp $
//                                                                                                                //
// BeDVI                                                                                                          //
// by Achim Blumensath                                                                                            //
// blume@corona.oche.de                                                                                           //
//                                                                                                                //
// This program is free software! It may be distributed according to the GNU Public License (see COPYING).        //
//                                                                                                                //
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

#ifndef FONTLIST_H
#define FONTLIST_H

#include <KernelKit.h>
#include <list.h>

#ifndef DEFINES_H
#include "defines.h"
#endif

class BPositionIO;
class DrawSettings;
class DVI;
class Font;

class FontList
{
  private:
    typedef list<Font *, allocator<Font *> > FontList_t;

    sem_id     ListLock;
    FontList_t Fonts;

  public:
    FontList();
    ~FontList();

    Font *LoadFont(const DVI *doc, const DrawSettings *Settings, const char *Name, float Size, long ChkSum,
                   int MagStep, double DimConvert);
    void FreeFont(Font *f);
    void FreeAll();
    void FreeUnusedFonts();
    void FlushShrinkedGlyphes();

    bool Ok() const
    {
      return ListLock >= B_OK;
    }
};

class FontTable
{
  private:
    FontList Fonts;
    sem_id   TableSem;
    Font     **Table;
    ulong    TableLen;

  public:
    FontTable();
    ~FontTable();

    Font *LoadFont(const DVI *doc, const DrawSettings *Settings, BPositionIO *File, Font *VirtualParent, uchar Command);
    bool Resize(ulong len);
    void FreeFonts();

    void FreeUnusedFonts();
    void FlushShrinkedGlyphes();

    ulong TableLength() const
    {
      return TableLen;
    }

    Font *operator [] (int no) const
    {
      return Table[no];
    }

    bool Ok() const
    {
      return (TableSem >= B_OK) && (Table != NULL) && Fonts.Ok();
    }
};

#endif
