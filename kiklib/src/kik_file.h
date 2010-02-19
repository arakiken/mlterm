/*
 *	$Id$
 */

#ifndef  __KIK_FILE_H__
#define  __KIK_FILE_H__


#include  <stdio.h>

#include  "kik_types.h"		/* size_t */

typedef struct  kik_file
{
	FILE *  file ;
	char *  buffer ;
	size_t  buf_size ;

} kik_file_t ;


kik_file_t *  kik_file_new( FILE *  fp) ;

int  kik_file_delete( kik_file_t *  file) ;

kik_file_t *  kik_file_open( const char *  file_path , const char *  mode) ;

int  kik_file_close( kik_file_t *  file) ;

FILE *  kik_fopen_with_mkdir( const char *  file_path , const char *  mode) ;

char *  kik_file_get_line( kik_file_t *  from , size_t *  len) ;

int  kik_file_lock( int  fd) ;

int  kik_file_unlock( int  fd) ;

int  kik_file_set_cloexec( int  fd) ;

int  kik_file_unset_cloexec( int  fd) ;

int  kik_mkdir_for_file( char *  file_path , mode_t  mode) ;


#endif
