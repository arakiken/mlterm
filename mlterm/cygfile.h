/*
 *	$Id$
 */

#ifndef  __CYGFILE_H__
#define  __CYGFILE_H__


#define  FILE  void
#define  fopen( path , mode)  cygfopen( path , mode)
#define  fclose( file)  cygfclose( file)
#define  fwrite( ptr , size , nmemb , file)  cygfwrite( ptr , size , nmemb , file)


void *  cygfopen( const char *  path , const char *  mode) ;

int  cygfclose( void *  file) ;

size_t  cygfwrite( const void *  ptr , size_t  size , size_t  nmemb , void *  file) ;


#endif
