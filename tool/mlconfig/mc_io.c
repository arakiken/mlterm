/*
 *	$Id$
 */

#include  "mc_io.h"

#include  <stdio.h>
#include  <unistd.h>		/* STDIN_FILENO */
#include  <kiklib/kik_str.h>	/* strdup */
#include  <kiklib/kik_debug.h>


/* --- static functions --- */

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
	char *  value
	)
{
	if( value == NULL)
	{
		return  0 ;
	}

	kik_debug_printf( "%s=%s\n" , key , value) ;
	
	printf( "\x1b]5379;%s=%s\x07" , key , value) ;
	fflush( stdout) ;

	return  1 ;
}

int
mc_set_flag_value(
	char *  key ,
	int  value
	)
{
#if  0
	if( value == -1)
	{
		return  0 ;
	}
#endif

	if( value)
	{
		printf( "\x1b]5379;%s=true\x07" , key) ;
	}
	else
	{
		printf( "\x1b]5379;%s=false\x07" , key) ;
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
