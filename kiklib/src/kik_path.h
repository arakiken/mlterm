/*
 *	$Id$
 */

#ifndef   __KIK_PATH_H__
#define   __KIK_PATH_H__


#include  <ctype.h>	/* isalpha */

#include  "kik_types.h"
#include  "kik_def.h"


/* PathIsRelative() is not used to avoid link shlwapi.lib */
#define  IS_RELATIVE_PATH_DOS(path) \
	(isalpha((path)[0]) ? ( (path)[1] != ':' || (path)[2] != '\\') : \
	                      ( (path)[0] != '\\'))
#define  IS_RELATIVE_PATH_UNIX(path)  (*(path) != '/')

#ifdef  USE_WIN32API
#define  IS_RELATIVE_PATH(path)  IS_RELATIVE_PATH_DOS(path)
#else
#define  IS_RELATIVE_PATH(path)  IS_RELATIVE_PATH_UNIX(path)
#endif


/* XXX win32 basename() works strangely if cp932 characters are pssed. */
#if  defined(HAVE_BASENAME) && ! defined(USE_WIN32API)

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

#if  defined(__CYGWIN__) || defined(__MSYS__)
#include  <sys/cygwin.h>
#ifdef  __CYGWIN__
#define  cygwin_conv_to_win32_path( path , winpath) \
	cygwin_conv_path( CCP_POSIX_TO_WIN_A , path , winpath , sizeof(winpath))
#define  cygwin_conv_to_posix_path( path , winpath) \
	cygwin_conv_path( CCP_WIN_A_TO_POSIX , path , winpath , sizeof(winpath)) ;
#endif
#endif


#endif
