////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//                                                                                                                //
// $Id: log.h,v 2.1 1998/07/09 13:36:48 achim Exp $
//                                                                                                                //
// BeDVI                                                                                                          //
// by Achim Blumensath                                                                                            //
// blume@corona.oche.de                                                                                           //
//                                                                                                                //
// This program is free software! It may be distributed according to the GNU Public License (see COPYING).        //
//                                                                                                                //
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

#ifndef LOG_H
#define LOG_H

enum // LogLevel
{
  LogLevel_None,
  LogLevel_Fatal,
  LogLevel_Error,
  LogLevel_Warn,
  LogLevel_Info,
  LogLevel_Debug
};

extern int log_level;

void log_open(char *name, int level);
void log_fatal(char *format,...);
void log_error(char *format,...);
void log_warn(char *format,...);
void log_info(char *format,...);
void log_debug(char *format,...);

#endif
