////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//                                                                                                                //
// $Id: defines.h,v 2.3 1999/07/22 13:36:46 achim Exp $
//                                                                                                                //
// BeDVI                                                                                                          //
// by Achim Blumensath                                                                                            //
// blume@corona.oche.de                                                                                           //
//                                                                                                                //
// This program is free software! It may be distributed according to the GNU Public License (see COPYING).        //
//                                                                                                                //
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

#ifndef DEFINES_H
#define DEFINES_H

#ifndef OS_H
#include <kernel/OS.h>
#endif
#include <exception>
#include <stdexcept>

#ifndef NULL
#include <null.h>
#endif

// types

typedef uint16 wchar;

// compiler

#ifdef __MWERKS__

#define __STDC__ 1

#endif

extern "C"
{
  #define string _string
  #include "kpathsea/c-auto.h"
  #include "kpathsea/types.h"
  #undef string
}

#if defined (__INTEL__)
#include <memory>
#endif

// bitmap macros

#if defined (__POWERPC__) || defined (__MC68K__)

  typedef uint32 BitmapUnit;

  #define BITS_PER_UNIT 32   // 8 * sizeof(BitmapUnit)
  #define MSB_FIRST

#elif defined (__INTEL__)

  typedef uint8 BitmapUnit;

  #define BITS_PER_UNIT 8    // 8 * sizeof(BitmapUnit)
  #define MSB_FIRST

#else

  #error "Unknown processor!"

#endif

#define BMU_ADD(ptr, off) ((BitmapUnit *) ((char *)(ptr) + (off)))
#define BMU_SUB(ptr, off) ((BitmapUnit *) ((char *)(ptr) - (off)))

#endif
