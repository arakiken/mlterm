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

kik_file_t *  kik_file_open( char *  file_path , char *  mode) ;

int  kik_file_close( kik_file_t *  file) ;

char *  kik_file_get_line( kik_file_t *  from , size_t *  len) ;
	
 
#endif
