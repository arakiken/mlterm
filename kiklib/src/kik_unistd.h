/*
 *	$Id$
 */

#ifndef  __KIK_UNISTD_H__
#define  __KIK_UNISTD_H__


#include  "kik_config.h"
#include  "kik_types.h"


#ifdef  HAVE_USLEEP

#include  <unistd.h>

#define  kik_usleep( microsec)  usleep( microsec)

#else

#define  kik_usleep( microsec)  __kik_usleep( microsec)

int  __kik_usleep( u_int  microseconds) ;

#endif


#ifdef  HAVE_UNSETENV

#include  <stdlib.h>

#define  kik_unsetenv( name)  unsetenv( name)

#else

#define  kik_unsetenv( name)  __kik_unsetenv( name)

void  __kik_unsetenv( char *  name) ;

#endif


#endif
