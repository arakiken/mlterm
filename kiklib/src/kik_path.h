/*
 *	$Id$
 */

#ifndef   __KIK_PATH_H__
#define   __KIK_PATH_H__


#include  "kik_types.h"
#include  "kik_config.h"


#ifdef  HAVE_BASENAME

#include  <libgen.h>

#define  kik_basename( path)  basename( path)

#else

#define  kik_basename( path)  __kik_basename( path)

char *  __kik_basename( char *  path) ;

#endif


int  kik_path_cleanname( char *  cleaned_path , size_t  size , char *  path) ;


#endif
