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


#ifndef  REMOVE_FUNCS_MLTERM_UNUSE

int  kik_path_cleanname( char *  cleaned_path , size_t  size , const char *  path) ;

#endif


int  kik_parse_uri( char **  proto , char **  user , char **  host , char **  port ,
		char **  path , char **  aux , char *  seq) ;

char *  kik_get_home_dir(void) ;


#endif
