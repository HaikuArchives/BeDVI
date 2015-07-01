////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//                                                                                                                //
// $Id: log.cc,v 2.1 1998/07/09 13:36:47 achim Exp $
//                                                                                                                //
// BeDVI                                                                                                          //
// by Achim Blumensath                                                                                            //
// blume@corona.oche.de                                                                                           //
//                                                                                                                //
// This program is free software! It may be distributed according to the GNU Public License (see COPYING).        //
//                                                                                                                //
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

#include <syslog.h>
#include <stdarg.h>
#include <stdio.h>
#include "log.h"

int log_level = LogLevel_Error;

void log_open(char *name, int level)
{
  openlog_team(name, LOG_THID, LOG_USER);

  log_level = level;
}

void log_fatal(char *format,...)
{
  char buffer[256];
  va_list args;

  if(log_level < LogLevel_Fatal)
    return;

  va_start(args, format);

  vsprintf(buffer, format, args);

  log_team(LOG_THID|LOG_USER|LOG_PERROR|LOG_CRIT, "fatal: %s", buffer);
}

void log_error(char *format,...)
{
  char buffer[256];
  va_list args;

  if(log_level < LogLevel_Error)
    return;

  va_start(args, format);

  vsprintf(buffer, format, args);

  log_team(LOG_THID|LOG_USER|LOG_PERROR|LOG_ERR, "error: %s", buffer);
}

void log_warn(char *format,...)
{
  char buffer[256];
  va_list args;

  if(log_level < LogLevel_Warn)
    return;

  va_start(args, format);

  vsprintf(buffer, format, args);

  log_team(LOG_THID|LOG_USER|LOG_WARNING, "warning: %s", buffer);
}

void log_info(char *format,...)
{
  char buffer[256];
  va_list args;

  if(log_level < LogLevel_Info)
    return;

  va_start(args, format);

  vsprintf(buffer, format, args);

  log_team(LOG_THID|LOG_USER|LOG_INFO, "info: %s", buffer);
}

void log_debug(char *format,...)
{
  char buffer[256];
  va_list args;

  if(log_level < LogLevel_Debug)
    return;

  va_start(args, format);

  vsprintf(buffer, format, args);

  log_team(LOG_THID|LOG_USER|LOG_DEBUG, "debug: %s", buffer);
}
