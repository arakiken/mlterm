/*
 *	$Id$
 */

#include  "ml_termcap.h"

#include  <string.h>		/* strchr */
#include  <X11/keysym.h>
#include  <kiklib/kik_str.h>	/* strdup */
#include  <kiklib/kik_mem.h>	/* free */
#include  <kiklib/kik_debug.h>
#include  <kiklib/kik_file.h>
#include  <kiklib/kik_conf_io.h>


typedef struct  field_table
{
	char *  name ;
	ml_termcap_field_t  field ;
	
} field_table_t ;


/* --- static variables --- */

static field_table_t  field_table[] =
{
	{ "kD" , MLT_DELETE , } ,
	{ "kb" , MLT_BACKSPACE , } ,
} ;


/* --- static functions --- */

static char *
unescape_sequence(
	char *  seq
	)
{
	char *  new_seq ;
	char *  p ;

	if( ( new_seq = malloc( strlen( seq) + 1)) == NULL)
	{
		return  NULL ;
	}

	p = new_seq ;

	while( *seq)
	{
		if( *seq == '\\')
		{
			if( *(++ seq) == '\0')
			{
				break ;
			}
			
			if( *seq == 'E')
			{
				*p = '\x1b' ;
			}
			else
			{
				*p = *seq ;
			}
		}
		else if( *seq == '^')
		{
			if( *(++ seq) == '\0')
			{
				break ;
			}

			if( '@' <= *seq && *seq <= '_')
			{
				*p = *seq - 'A' + 1 ;
			}
			else if( *seq == '?')
			{
				*p = '\x7f' ;
			}
		}
		else
		{
			*p = *seq ;
		}

		p ++ ;
		seq ++ ;
	}

	*p = '\0' ;

	return  new_seq ;
}


/* --- global functions --- */

int
ml_termcap_init(
	ml_termcap_t *  termcap
	)
{
	termcap->fields[MLT_DELETE] = strdup( "\x7f") ;
	termcap->fields[MLT_BACKSPACE] = strdup( "\x08") ;

	return  1 ;
}

int
ml_termcap_final(
	ml_termcap_t *  termcap
	)
{
	int  counter ;

	for( counter = 0 ; counter < MAX_TERMCAP_FIELDS ; counter ++)
	{
		free( termcap->fields[counter]) ;
	}
	
	return  1 ;
}

int
ml_termcap_read_conf(
	ml_termcap_t *  termcap ,
	char *  filename
	)
{
	kik_file_t *  from ;
	char *  key ;
	char *  value ;

	if( ! ( from = kik_file_open( filename , "r")))
	{
	#ifdef  DEBUG
		kik_warn_printf( KIK_DEBUG_TAG " %s couldn't be opened.\n" , filename) ;
	#endif
	
		return  0 ;
	}

	while( kik_conf_io_read( from , &key , &value))
	{
		int  counter ;
		
		for( counter = 0 ; counter < MAX_TERMCAP_FIELDS ; counter ++)
		{
			if( strcmp( key , field_table[counter].name) == 0)
			{
				if( ( value = unescape_sequence( value)))
				{
					free( termcap->fields[ field_table[counter].field]) ;
					termcap->fields[ field_table[counter].field] = value ;
				}

				continue ;
			}
		}
	}

	kik_file_close( from) ;
	
	return  1 ;
}

char *
ml_termcap_get_sequence(
	ml_termcap_t *  termcap ,
	ml_termcap_field_t  field
	)
{
	if( field < MAX_TERMCAP_FIELDS)
	{
		return  termcap->fields[field] ;
	}
	else
	{
		return  NULL ;
	}
}
