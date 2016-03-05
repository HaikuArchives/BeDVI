////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//                                                                                                                //
// $Id: Support.cc,v 2.1 1998/10/04 14:25:13 achim Exp $
//                                                                                                                //
// BeDVI                                                                                                          //
// by Achim Blumensath                                                                                            //
// blume@corona.oche.de                                                                                           //
//                                                                                                                //
// This program is free software! It may be distributed according to the GNU Public License (see COPYING).        //
//                                                                                                                //
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

#include <StorageKit.h>
#include "Support.h"

sem_id kpse_sem = B_ERROR;  // kpathsearch library isn't thread safe

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//                                                                                                                //
// uint32 ReadInt(BPositionIO *f, ssize_t Size)                                                                   //
//                                                                                                                //
// Reads an integer of `Size' bytes from `f'.                                                                     //
//                                                                                                                //
// BPositionIO *f                       file                                                                      //
// ssize_t     Size                     size of the integer in bytes                                              //
//                                                                                                                //
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

uint32 ReadInt(BPositionIO *f, ssize_t Size)
{
  uint32 x = 0;
  uint8  c;

  while (Size--)
  {
    f->Read(&c, 1);
    x = (x << 8) | c;
  }
  return x;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//                                                                                                                //
// int32 ReadSInt(BPositionIO *f, ssize_t Size)                                                                   //
//                                                                                                                //
// Reads a signed integer of `Size' bytes from `f'.                                                               //
//                                                                                                                //
// BPositionIO *f                       file                                                                      //
// ssize_t     Size                     size of the integer in bytes                                              //
//                                                                                                                //
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

int32 ReadSInt(BPositionIO *f, ssize_t Size)
{
  int32 x;
  int8  c;

  f->Read(&c, 1);
  x = c;

  while (--Size)
  {
    f->Read(&c, 1);
    x = (x << 8) | (uint8)c;
  }

  return x;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//                                                                                                                //
// bool InitKpseSem()                                                                                             //
//                                                                                                                //
// initializes `kpse_sem'.                                                                                        //
//                                                                                                                //
// Result:                              `true' if successful otherwise `false'                                    //
//                                                                                                                //
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

bool InitKpseSem()
{
  return (kpse_sem = create_sem(1, "kpse semaphore")) >= B_OK;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//                                                                                                                //
// void FreeKpseSem()                                                                                             //
//                                                                                                                //
// frees `kpse_sem'.                                                                                              //
//                                                                                                                //
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

void FreeKpseSem()
{
  if (kpse_sem >= B_OK)
  {
    acquire_sem(kpse_sem);
    delete_sem(kpse_sem);
  }
}
