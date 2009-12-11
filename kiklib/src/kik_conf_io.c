/*
 *	$Id$
 */

#include  "kik_conf_io.h"

#include  <stdio.h>	/* sprintf */
#include  <string.h>	/* strlen */
#include  <stdlib.h>	/* getenv */
#include  <sys/stat.h>	/* stat */
#include  <errno.h>

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

	if( sysconfdir == NULL)
	{
		return  NULL ;
	}
	
	if( ( rcpath = malloc( strlen( sysconfdir) + 1 + strlen( rcfile) + 1)) == NULL)
	{
		return  NULL ;
	}

#ifdef  USE_WIN32API
	sprintf( rcpath , "%s\\%s" , sysconfdir , rcfile) ;
#else
	sprintf( rcpath , "%s/%s" , sysconfdir , rcfile) ;
#endif

	return  rcpath ;
}

char *
kik_get_user_rc_path(
	char *  rcfile
	)
{
	char *  homedir ;
	char *  dotrcpath ;

#ifdef  DEBUG
	if( ( homedir = getenv( "CONF_DIR")))
	{
		kik_msg_printf("using %s as an user config dir.\n", homedir);
		/* conf path is overridden */ ;
	}else
#endif

#ifdef  USE_WIN32API
	if( ( homedir = getenv( "HOMEPATH")))
	{
		struct stat s ;

		/* HOMEPATH is corrupt if built with MSYS-DTK 1.0.1. */
		if( stat( homedir , &s) == 0)
		{
			/* Enough for "%s/.%s" */
			if( ( dotrcpath = malloc( strlen( homedir) + 1 + strlen( rcfile) + 1)))
			{
				sprintf( dotrcpath , "%s\\%s" , homedir , rcfile) ;

				return  dotrcpath ;
			}
		}
	}
#endif

	if( ( homedir = getenv( "HOME")) == NULL)
	{
		return  NULL ;
	}

	/* Enough for "%s/.%s" */
	if( ( dotrcpath = malloc( strlen( homedir) + 2 + strlen( rcfile) + 1)) == NULL)
	{
		return  NULL ;
	}

#ifdef  USE_WIN32API
	sprintf( dotrcpath , "%s\\.%s" , homedir , rcfile) ;
#else
	sprintf( dotrcpath , "%s/.%s" , homedir , rcfile) ;
#endif

	return  dotrcpath ;
}

kik_conf_write_t *
kik_conf_write_open(
	char *  name
	)
{
	kik_conf_write_t *  conf ;
	kik_file_t *  from ;
	char *  p ;

	if( ( conf = malloc( sizeof( kik_conf_write_t))) == NULL)
	{
		return  conf ;
	}
	
	if( ( conf->lines = malloc( sizeof( char *) * 128)) == NULL)
	{
		free( conf) ;
		
		return  NULL ;
	}

	conf->num = 0 ;
	conf->scale = 1 ;

	from = kik_file_open( name , "r") ;
	if( from)
	{
		while( 1)
		{
			char *  line ;
			size_t  len ;

			if( conf->num >= conf->scale * 128)
			{
				void *  p ;

				if( ( p = realloc( conf->lines ,
					sizeof( char *) * 128 * (++ conf->scale))) == NULL)
				{
					goto  error ;
				}

				conf->lines = p ;
			}

			if( ( line = kik_file_get_line( from , &len)) == NULL)
			{
				break ;
			}

			line[len - 1] = '\0' ;
			conf->lines[conf->num++] = strdup( line) ;
		}

		kik_file_close( from) ;
	}

	/*
	 * Prepare directory for creating a configuration file.
	 */
	p = name + 1 ;
	while( *p)
	{
		if( *p == '/'
		#ifdef  USE_WIN32API
			|| *p == '\\'
		#endif
			)
		{
			struct stat  s ;
			char  c ;

			c = *p ;	/* save */
			
			*p = '\0' ;
			if( stat( name , &s) != 0)
			{
				if( errno == ENOENT)
				{
					if( mkdir( name , 0755))  goto  error ;
				}
				else
				{
					goto  error ;
				}
			}
			
			*p = c ;	/* restore */
		}

		p ++ ;
	}
	
	if( ( conf->to = fopen( name , "w")) == NULL)
	{
		goto  error ;
	}

	kik_file_lock( fileno( conf->to)) ;

	return  conf ;

error:
	{
		int  count ;

		for( count = 0 ; count < conf->num ; count ++)
		{
			free( conf->lines[count]) ;
		}
	}
	
	free( conf->lines) ;
	free( conf) ;

	return  NULL ;
}

int
kik_conf_io_write(
	kik_conf_write_t *  conf ,
	char *  key ,
	char *  val
	)
{
	int  count ;
	char *  p ;
		
	if( key == NULL)
	{
		return  0 ;
	}

	if( val == NULL)
	{
		val = "\0" ;
	}

	for( count = 0 ; count < conf->num ; count ++)
	{
		if( *conf->lines[count] == '#')
		{
			continue ;
		}

		p = conf->lines[count] ;

		while( *p == ' ' || *p == '\t')
		{
			p ++ ;
		}

		if( strncmp( p , key , strlen(key)) != 0)
		{
			continue ;
		}

		if( ( p = malloc( strlen( key) + strlen( val) + 4)) == NULL)
		{
			continue ;
		}
		sprintf( p , "%s = %s" , key , val) ;

		free( conf->lines[count]) ;
		conf->lines[count] = p ;

		return  1 ;
	}

	if( conf->num + 1 >= conf->scale * 128)
	{
		void *  p ;
		
		if( ( p = realloc( conf->lines , sizeof( char *) * 128 * (++ conf->scale))) == NULL)
		{
			return  0 ;
		}

		conf->lines = p ;
	}

	if( ( p = malloc( strlen( key) + strlen( val) + 4)) == NULL)
	{
		return  0 ;
	}
	sprintf( p , "%s = %s" , key , val) ;

	conf->lines[conf->num ++] = p ;
	
	return  1 ;
}

int
kik_conf_write_close(
	kik_conf_write_t *  conf
	)
{
	int  count ;

	for( count = 0 ; count < conf->num ; count ++)
	{
		fprintf( conf->to , "%s\n" , conf->lines[count]) ;
		free( conf->lines[count]) ;
	}

	kik_file_unlock( fileno( conf->to)) ;

	fclose( conf->to) ;

	free( conf->lines) ;
	free( conf) ;

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
