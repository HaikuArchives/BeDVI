////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//                                                                                                                //
// $Id: squeeze.c,v 2.1 1998/07/09 13:36:48 achim Exp $
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
// NOTE:                                                                                                          //
// This routine is adapted from the squeeze.c that comes with dvips;                                              //
// it bears the message:                                                                                          //
//   This software is Copyright 1988 by Radical Eye Software.                                                     //
// Used with permission.                                                                                          //
//                                                                                                                //
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

/*
 *   This routine squeezes a PostScript file down to its
 *   minimum.  We parse and then output it.
 *   Adapted for xdvi 1/94.  Writes a C program that contains the PS file
 *   as a constant string.
 */
#include <stdio.h>
#include <kpathsea/c-auto.h>
#include <kpathsea/c-memstr.h>
#define LINELENGTH (72)
#define BUFLENGTH (1000)
#undef putchar
#define putchar(a) (void)putc(a, out) ;
FILE *in, *out ;
static int linepos = 0 ;
static int lastspecial = 1 ;
static int stringlen = 0;
/*
 *   This next routine writes out a `special' character.  In this case,
 *   we simply put it out, since any special character terminates the
 *   preceding token.
 */
void specialout(c)
char c ;
{
   if (linepos + 1 > LINELENGTH) {
      (void)fputs("\\n\\\n", out);
      stringlen += linepos + 1;
      linepos = 0 ;
   }
   putchar(c) ;
   linepos++ ;
   lastspecial = 1 ;
}
void strout(s)
char *s ;
{
   if (linepos + strlen(s) > LINELENGTH) {
      (void)fputs("\\n\\\n", out);
      stringlen += linepos + 1;
      linepos = 0 ;
   }
   linepos += strlen(s) ;
   while (*s != 0)
      putchar(*s++) ;
   lastspecial = 1 ;
}
void cmdout(s)
char *s ;
{
   int l ;

   l = strlen(s) ;
   if (linepos + l + 1 > LINELENGTH) {
      (void)fputs("\\n\\\n", out);
      stringlen += linepos + 1;
      linepos = 0 ;
      lastspecial = 1 ;
   }
   if (! lastspecial) {
      putchar(' ') ;
      linepos++ ;
   }
   while (*s != 0) {
      putchar(*s++) ;
   }
   linepos += l ;
   lastspecial = 0 ;
}
char buf[BUFLENGTH] ;
#ifndef VMS
void
#endif
main(argc, argv)
int argc ;
char *argv[] ;
{
   int c ;
   char *b ;
   char seeking ;
   extern void exit() ;

   if (argc > 3 || (in=(argc < 2 ? stdin : fopen(argv[1], "r")))==NULL ||
                    (out=(argc < 3 ? stdout : fopen(argv[2], "w")))==NULL) {
      (void)fprintf(stderr, "Usage:  squeeze [infile [outfile]]\n") ;
      exit(1) ;
   }
   (void)fputs("/*\n\
 *   DO NOT EDIT THIS FILE!\n\
 *   It was created by squeeze.c from another file (see the Makefile).\n\
 */\n\n\
static const char psheader[] = \"\\\n", out);
   while (1) {
      c = getc(in) ;
      if (c==EOF)
         break ;
      if (c=='%') {
         while ((c=getc(in))!='\n') ;
      }
      if (c <= ' ')
         continue ;
      switch (c) {
case '{' :
case '}' :
case '[' :
case ']' :
         specialout(c) ;
         break ;
case '<' :
case '(' :
         if (c=='(')
            seeking = ')' ;
         else
            seeking = '>' ;
         b = buf ;
         *b++ = c ;
         do {
            c = getc(in) ;
            if (b > buf + BUFLENGTH-2) {
               (void)fprintf(stderr, "Overran buffer seeking %c", seeking) ;
               exit(1) ;
            }
            *b++ = c ;
            if (c=='\\')
               *b++ = getc(in) ;
         } while (c != seeking) ;
         *b++ = 0 ;
         strout(buf) ;
         break ;
default:
         b = buf ;
         while ((c>='A'&&c<='Z')||(c>='a'&&c<='z')||
                (c>='0'&&c<='9')||(c=='/')||(c=='@')||
                (c=='!')||(c=='"')||(c=='&')||(c=='*')||(c==':')||
                (c==',')||(c==';')||(c=='?')||(c=='^')||(c=='~')||
                (c=='-')||(c=='.')||(c=='#')||(c=='|')||(c=='_')||
                (c=='=')||(c=='$')||(c=='+')) {
            *b++ = c ;
            c = getc(in) ;
         }
         if (b == buf) {
            (void)fprintf(stderr, "Oops!  Missed a case: %c.\n", c) ;
            exit(1) ;
         }
         *b++ = 0 ;
         (void)ungetc(c, in) ;
         cmdout(buf) ;
      }
   }
   (void)fputs("\\n\";\n", out);
   exit(0) ;
   /*NOTREACHED*/
}