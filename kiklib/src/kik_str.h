/*
 *	$Id$
 */

#ifndef  __KIK_STR_H__
#define  __KIK_STR_H__


#include  <string.h>		/* strlen/strsep/strdup */

#include  "kik_types.h"		/* size_t */
#include  "kik_def.h"
#include  "kik_mem.h"		/* alloca */


#ifdef  HAVE_STRSEP

#define  kik_str_sep( strp , delim)  strsep( strp , delim)

#else

#define  kik_str_sep( strp , delim)  __kik_str_sep( strp , delim)

char *  __kik_str_sep( char **  strp , const char *  delim) ;

#endif


/*
 * cpp doesn't necessarily process variable number of arguments.
 */
int  kik_snprintf( char *  str , size_t  size , const char *  format , ...) ;


#ifdef  KIK_DEBUG

#define  strdup( str)  kik_str_dup( str , __FILE__ , __LINE__ , __FUNCTION__)

#endif

char *  kik_str_dup( const char *  str , const char *  file , int  line , const char *  func) ;


#define  kik_str_alloca_dup( src) __kik_str_copy( alloca( strlen(src) + 1) , (src) )

char *  __kik_str_copy( char *  dst , const char *  src) ;


size_t  kik_str_tabify( u_char *  dst , size_t  dst_len ,
	const u_char *  src , size_t  src_len , size_t  tab_len) ;

char *  kik_str_chop_spaces( char *  str) ;


int  kik_str_n_to_uint( u_int *  i , const char *  s , size_t  n) ;

int  kik_str_n_to_int( int *  i , const char *  s , size_t  n) ;

int  kik_str_to_uint( u_int *  i , const char *  s) ;

int  kik_str_to_int( int *  i , const char *  s) ;

u_int  kik_count_char_in_str( const char *  str , char  ch) ;

int  kik_compare_str( const char *  str1 , const char *  str2) ;

char *  kik_str_replace( const char *  str , const char *  orig , const char *  new) ;


#endif
