/*
 *	$Id$
 */

#include  "mc_io.h"

#include  <stdio.h>
#include  <kiklib/kik_mem.h>		/* malloc */
#include  <kiklib/kik_unistd.h>		/* STDIN_FILENO */
#include  <kiklib/kik_debug.h>
#include  <kiklib/kik_str.h>

/* --- static variables --- */

char *  message ;


/* --- static functions --- */

static int
append_value(
	char *  key ,
	char *  value
	)
{
	if( message == NULL)
	{
		if( ( message = malloc( strlen( key) + 1 + strlen( value) + 1)) == NULL)
		{
			return  0 ;
		}

		sprintf( message , "%s=%s" , key , value) ;
	}
	else
	{
		void *  p ;
		int len;
		
		len = strlen(message);
		if( ( p = realloc( message , len + 1 + strlen( key) + 1 + strlen( value) + 1))
				== NULL)
		{
			return  0 ;
		}

		message = p ;
		
		sprintf( message + len , ";%s=%s" , key , value) ;
	}

	return  1 ;
}

static char *
get_value(
	char *  key
	)
{
#define RET_SIZE 1024
	int  count ;
	char  ret[RET_SIZE] ;
	char  c ;
	char *p;

	printf( "\x1b]%d;%s\x07" , mc_io_get, key) ;
	fflush( stdout) ;

	for( count = 0 ; count < RET_SIZE ; count ++)
	{
		if( read( STDIN_FILENO , &c , 1) == 1)
		{
			if( c != '\n')
			{
				ret[count] = c ;
			}
			else
			{
				break;
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

	if( count == RET_SIZE) return NULL;

	ret[count] = '\0' ;

	p = strchr(ret, '=');
	if( p == NULL || strcmp( ret , "#error") == 0)
		return  NULL ;

	/* #key=value */
	return  strdup( p + 1) ;

#undef RET_SIZE
}


/* --- global functions --- */

int
mc_set_str_value(
	char *  key ,
	char *  value
	)
{
	if (value == NULL) return 0;

#ifdef __DEBUG
	kik_debug_printf( "%s=%s\n" , key , value) ;
#endif

	return append_value(key, value);
}

int
mc_set_flag_value(
	char *  key ,
	int  flag_val
	)
{
#ifdef __DEBUG
	kik_debug_printf( "%s=%s\n" , key , value) ;
#endif

	return append_value(key, (flag_val ? "true" : "false"));
}

int
mc_flush(mc_io_t  io)
{
	if( message == NULL)
	{
		return  1 ;
	}
	
	printf("\x1b]%d;%s\x07" , io , message);
	fflush( stdout) ;

#if  0
	fprintf( stderr , "%s\n" , message) ;
#endif

	free(message);
	message = NULL;

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
