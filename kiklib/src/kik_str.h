/*
 *	update: <2001/11/14(06:47:04)>
 *	$Id$
 */

#ifndef  __KIK_STR_H__
#define  __KIK_STR_H__


#include  "kik_types.h"		/* size_t */
#include  "kik_config.h"
#include  "kik_mem.h"		/* alloca */


#ifdef  HAVE_STRSEP

#include  <string.h>

#define  kik_str_sep( strp , delim)  strsep( strp , delim)

#else

#define  kik_str_sep( strp , delim)  __kik_str_sep( strp , delim)

char *  __kik_str_sep( char **  strp , const char *  delim) ;

#endif


#ifdef  HAVE_BASENAME

#include  <libgen.h>

#define  kik_basename( path)  basename( path)

#else

#define  kik_basename( path)  __kik_basename( path)

char *  __kik_basename( char *  path) ;

#endif


#ifdef  KIK_DEBUG

#define  strdup( str)  kik_str_dup( str , __FILE__ , __LINE__ , __FUNCTION__)

#else

#include  <string.h>		/* strdup */

#endif

char *  kik_str_dup( const char *  str , const char *  file , int  line , const char *  func) ;


#define  kik_str_alloca_dup( src) __kik_str_copy( alloca( strlen(src) + 1) , (src) )

char *  __kik_str_copy( char *  dst , char *  src) ;


size_t  kik_str_tabify( u_char *  dst , size_t  dst_len ,
	u_char *  src , size_t  src_len , size_t  tab_len) ;

char *  kik_str_chop_spaces( char *  str) ;

int  kik_str_n_to_int( int *  i , char *  s , size_t  n) ;

int  kik_str_to_int( int *  i , char *  s) ;


#endif
