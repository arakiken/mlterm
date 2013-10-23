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

static char *  message ;
static int  gui_is_win32 ;


/* --- static functions --- */

static char *
load_challenge(void)
{
	FILE *  fp ;
	char *  homedir ;
	char  path[256] ;
	static char  challenge[64] ;

	if( ! ( homedir = getenv("HOME")))
	{
		return  "" ;
	}

	snprintf( path , sizeof(path) , "%s/.mlterm/challenge" , homedir) ;

	if( ( fp = fopen( path , "r")))
	{
		size_t  len ;

		if( ( len = fread( challenge , 1 , sizeof(challenge) - 2 , fp)) > 0)
		{
			challenge[len++] = ';' ;
			challenge[len] = '\0' ;
		}

		fclose( fp) ;
	}

	return  challenge ;
}

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
	char *  key ,
	mc_io_t  io
	)
{
#define RET_SIZE 1024
	int  count ;
	char  ret[RET_SIZE] ;
	char  c ;
	char *p;

	printf( "\x1b]%d;%s\x07" , io , key) ;
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
mc_exec(
	char *  cmd
	)
{
#ifdef __DEBUG
	kik_debug_printf( KIK_DEBUG_TAG " %s\n" , cmd) ;
#endif

	printf("\x1b]%d;%s\x07" , mc_io_exec , cmd);
	fflush( stdout) ;

	return  1 ;
}

int
mc_set_str_value(
	char *  key ,
	char *  value
	)
{
	if (value == NULL) return 0;

#ifdef __DEBUG
	kik_debug_printf( KIK_DEBUG_TAG " %s=%s\n" , key , value) ;
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
	kik_debug_printf( KIK_DEBUG_TAG " %s=%s\n" , key , value) ;
#endif

	return append_value(key, (flag_val ? "true" : "false"));
}

int
mc_flush(mc_io_t  io)
{
	char *  chal = "" ;

	if( message == NULL)
	{
		return  1 ;
	}

	if( io == mc_io_set_save)
	{
		chal = load_challenge() ;
	}

	printf( "\x1b]%d;%s%s\x07" , io , chal , message);
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
	
	if( ( value = get_value( key , mc_io_get)) == NULL)
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

	if( ( value = get_value( key , mc_io_get)) == NULL)
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

int
mc_gui_is_win32(void)
{
	char *  value ;
	
	if( gui_is_win32 != 0)
	{
		return  gui_is_win32 == 1 ;
	}

	if( ( value = get_value( "gui" , mc_io_get)) && strcmp( value , "win32") == 0)
	{
		gui_is_win32 = 1 ;
	}
	else
	{
		gui_is_win32 = -1 ;
	}

	free( value) ;

	return  gui_is_win32 == 1 ;
}

int
mc_set_font_name(
	mc_io_t  io ,
	char *  file ,
	char *  font_size ,
	char *  cs ,
	char *  font_name
	)
{
	char *  chal = "" ;

	if( io == mc_io_set_save_font)
	{
		chal = load_challenge() ;
	}

	printf( "\x1b]%d;%s%s:%s=%s,%s\x07" , io , chal , file , cs , font_size , font_name) ;
	fflush( NULL) ;

	return  1 ;
}

char *
mc_get_font_name(
	char *  file ,
	char *  font_size ,
	char *  cs
	)
{
	size_t  len ;
	char *  value ;
	char *  key ;

	len = strlen(cs) + strlen(font_size) + 2 ;
	if( file)
	{
		len += (strlen(file) + 1) ;
	}
	
	if( ( key = alloca( len)) == NULL)
	{
		return  strdup( "error") ;
	}

	sprintf( key , "%s%s%s,%s" ,
		file ? file : "" ,
		file ? ":" : "" ,
		cs , font_size) ;
	
	if( ( value = get_value( key , mc_io_get_font)) == NULL)
	{
		return  strdup( "error") ;
	}
	else
	{
		return  value ;
	}
}
