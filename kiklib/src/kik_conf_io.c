/*
 *	update: <2001/11/26(10:13:39)>
 *	$Id$
 */

#include  "kik_conf_io.h"

#include  <stdio.h>	/* sprintf */
#include  <string.h>	/* strlen */
#include  <stdlib.h>	/* getenv */

#include  "kik_str.h"	/* kik_str_sep/kik_str_chop_spaces */
#include  "kik_mem.h"	/* malloc */


/* --- static variables --- */

static char *  sysconfdir ;


/* --- global functions --- */

int
kik_set_sys_conf_dir(
	char *  dir
	)
{
	sysconfdir = dir ;

	return  1 ;
}

char *
kik_get_sys_rc_path(
	char *  rcfile
	)
{
	char *  rcpath ;
	
	if( ( rcpath = malloc( strlen( sysconfdir) + 1 + strlen( rcfile) + 1)) == NULL)
	{
		return  NULL ;
	}

	sprintf( rcpath , "%s/%s" , sysconfdir , rcfile) ;

	return  rcpath ;
}

char *
kik_get_user_rc_path(
	char *  rcfile
	)
{
	char *  homedir ;
	char *  dotrcpath ;
	
	if( ( homedir = getenv( "HOME")) == NULL)
	{
		return  NULL ;
	}
	
	if( ( dotrcpath = malloc( strlen( homedir) + 2 + strlen( rcfile) + 1)) == NULL)
	{
		return  NULL ;
	}

	sprintf( dotrcpath , "%s/.%s" , homedir , rcfile) ;

	return  dotrcpath ;
}

int
kik_conf_io_write(
	FILE *  to ,
	char *  key ,
	char *  val
	)
{
	fprintf( to , "%s=%s\n" , key , val) ;

	return  1 ;
}

int
kik_conf_io_read(
	kik_file_t *  from ,
	char **  key ,
	char **  val
	)
{
	char *  line ;
	size_t  len ;
	
	while( 1)
	{
		if( ( line = kik_file_get_line( from , &len)) == NULL)
		{
			return  0 ;
		}
		
		if( *line == '#' || *line == '\n')
		{
			/* comment out or empty line. */

			continue ;
		}
		
		line[ len - 1] = '\0' ;

		/*
		 * finding key
		 */
		 
		while( *line == ' ' || *line == '\t')
		{
			line ++ ;
		}

		if( ( *key = kik_str_sep( &line , "=")) == NULL ||
			line == NULL)
		{
			/* not a conf line */
			
			continue ;
		}

		*key = kik_str_chop_spaces( *key) ;

		/*
		 * finding value
		 */

		while( *line == ' ' || *line == '\t')
		{
			line ++ ;
		}
		
		*val = kik_str_chop_spaces( line) ;

		return  1 ;
	}
}
