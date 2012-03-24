/*
 *	$Id$
 */

#ifndef  __KIK_UNISTD_H__
#define  __KIK_UNISTD_H__


#include  "kik_def.h"
#include  "kik_types.h"


#ifdef  HAVE_USLEEP

#include  <unistd.h>

#define  kik_usleep( microsec)  usleep( microsec)

#else

#define  kik_usleep( microsec)  __kik_usleep( microsec)

int  __kik_usleep( u_int  microseconds) ;

#endif


#ifdef  USE_WIN32API

#define  kik_setenv( name , value , overwrite)  SetEnvironmentVariable( name , value)

#else

#include  <stdlib.h>

#define  kik_setenv( name , value , overwrite)  setenv( name , value , overwrite)

#endif


#ifdef  HAVE_UNSETENV

#include  <stdlib.h>

#define  kik_unsetenv( name)  unsetenv( name)

#else

#ifdef  USE_WIN32API
#define  kik_unsetenv( name)  SetEnvironmentVariable( name , NULL)
#else
#include  <stdlib.h>
#define  kik_unsetenv( name)  setenv( name , "" , 1) ;
#endif

#endif


#ifdef  HAVE_GETUID

#include  <unistd.h>

#define  kik_getuid  getuid

#else

#define  kik_getuid  __kik_getuid

uid_t  __kik_getuid( void) ;

#endif


#ifdef  HAVE_GETGID

#include  <unistd.h>

#define  kik_getgid  getgid

#else

#define  kik_getgid  __kik_getgid

gid_t  __kik_getgid( void) ;

#endif


#endif
