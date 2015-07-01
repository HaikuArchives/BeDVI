////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//                                                                                                                //
// $Id: Support.h,v 2.0 1997/10/12 10:21:37 achim Exp $
//                                                                                                                //
// BeDVI                                                                                                          //
// by Achim Blumensath                                                                                            //
// blume@corona.oche.de                                                                                           //
//                                                                                                                //
// This program is free software! It may be distributed according to the GNU Public License (see COPYING).        //
//                                                                                                                //
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

#include <sys/types.h>
#include <OS.h>
#include "defines.h"

class BPositionIO;

extern sem_id kpse_sem;

uint32 ReadInt (BPositionIO *f, ssize_t Size);
int32  ReadSInt(BPositionIO *f, ssize_t Size);
bool   InitKpseSem();
void   FreeKpseSem();
