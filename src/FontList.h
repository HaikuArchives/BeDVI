////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//                                                                                                                //
// $Id: FontList.h,v 2.1 1998/05/02 09:42:08 achim Exp $
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

    Font *LoadFont(DVI *doc, const char *Name, float Size, long ChkSum, int MagStepVal, double DimConvert);
    void FreeFont(Font *f);
    void FreeAll();
    void FreeUnusedFonts();
    void FlushShrinkedGlyphes();

    bool Ok()
    {
      return ListLock >= B_NO_ERROR;
    }
};

class FontTable
{
  private:
    FontList Fonts;
    sem_id   TableSem;
    Font     **Table;
    u_long   TableLen;

  public:
    FontTable();
    ~FontTable();

    Font *LoadFont(DVI *doc, BPositionIO *File, u_char Command);
    bool Resize(u_long len);
    void FreeFonts();

    void FreeUnusedFonts();
    void FlushShrinkedGlyphes();

    u_long TableLength()
    {
      return TableLen;
    }

    Font *operator [] (int no)
    {
      return Table[no];
    }

    bool Ok()
    {
      return (TableSem >= B_NO_ERROR) && (Table != NULL) && Fonts.Ok();
    }
};

#endif
