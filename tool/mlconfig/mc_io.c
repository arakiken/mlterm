/*
 *	$Id$
 */

#include  "mc_io.h"

#include  <stdio.h>
#include  <unistd.h>		/* STDIN_FILENO */
#include  <kiklib/kik_str.h>	/* strdup */
#include  <kiklib/kik_debug.h>
#include  <kiklib/kik_mem.h>	/* malloc */


/* --- static variables --- */

char *  save_str ;
char *  set_str ;


/* --- static functions --- */

static int
append_value(
	char **  str ,	/* save_str or set_str */
	char *  key ,
	char *  value
	)
{
	if( *str == NULL)
	{
		if( ( *str = malloc( strlen( key) + 1 + strlen( value) + 1)) == NULL)
		{
			return  0 ;
		}

		sprintf( *str , "%s=%s" , key , value) ;
	}
	else
	{
		void *  p ;
		
		if( ( p = realloc( *str , strlen( *str) + 1 + strlen( key) + 1 + strlen( value) + 1))
				== NULL)
		{
			return  0 ;
		}

		*str = p ;
		
		sprintf( *str , "%s;%s=%s" , *str , key , value) ;
	}

	return  1 ;
}

static char *
get_value(
	char *  key
	)
{
	int  count ;
	char  ret[1024] ;
	char  c ;

	printf( "\x1b]5381;%s\x07" , key) ;
	fflush( stdout) ;

	for( count = 0 ; count < 1024 ; count ++)
	{
		if( read( STDIN_FILENO , &c , 1) == 1)
		{
			if( c != '\n')
			{
				ret[count] = c ;
			}
			else
			{
				ret[count] = '\0' ;

				if( count < 2 + strlen( key) || strcmp( ret , "#error") == 0)
				{
					return  NULL ;
				}

				/* #key=value */
				return  strdup( ret + 2 + strlen( key)) ;
			}
		}
		else
		{
		#ifdef  DEBUG
			kik_debug_printf( KIK_DEBUG_TAG " %s return from mlterm is illegal.\n" , key) ;
		#endif
		
			return  NULL ;
		}
	}

	return  NULL ;
}


/* --- global functions --- */

int
mc_set_str_value(
	char *  key ,
	char *  value ,
	int  save
	)
{
	if( value == NULL)
	{
		return  0 ;
	}

#ifdef __DEBUG
	kik_debug_printf( "%s=%s\n" , key , value) ;
#endif

	if( save)
	{
		if( strcmp( key , "encoding") == 0)
		{
			key = "ENCODING" ;
		}
		
		return  append_value( &save_str , key , value) ;
	}
	else
	{
		return  append_value( &set_str , key , value) ;
	}
}

int
mc_set_flag_value(
	char *  key ,
	int  flag_val ,
	int  save
	)
{
	char *  value ;

	if( flag_val)
	{
		value = "true" ;
	}
	else
	{
		value = "false" ;
	}

#ifdef __DEBUG
	kik_debug_printf( "%s=%s\n" , key , value) ;
#endif

	if( save)
	{
		return  append_value( &save_str , key , value) ;
	}
	else
	{
		return  append_value( &set_str , key , value) ;
	}
}

int
mc_flush(void)
{
	if( save_str)
	{
		printf( "\x1b]5382;%s\x07" , save_str) ;

	#if  0
		fprintf( stderr , "%s\n" , save_str) ;
	#endif
	
		free( save_str) ;
		save_str = NULL ;
	}
	
	if( set_str)
	{
		printf( "\x1b]5379;%s\x07" , set_str) ;

	#if  0
		fprintf( stderr , "%s\n" , set_str) ;
	#endif

		free( set_str) ;
		set_str = NULL ;
	}

	fflush( stdout) ;

	return  1 ;
}

char *
mc_get_str_value(
	char *  key
	)
{
	char *  value ;
	
	if( ( value = get_value( key)) == NULL)
	{
		return  strdup( "error") ;
	}
	else
	{
		return  value ;
	}
}

int
mc_get_flag_value(
	char *  key
	)
{
	char *  value ;

	if( ( value = get_value( key)) == NULL)
	{
		return  0 ;
	}

	if( strcmp( value , "true") == 0)
	{
		free( value) ;
		
		return  1 ;
	}
	else
	{
		free( value) ;
		
		return  0 ;
	}
}
