/*
 *	$Id$
 */

#include  "ml_xim.h"

#include  <stdio.h>		/* sprintf */
#include  <string.h>		/* strcmp */
#include  <kiklib/kik_debug.h>
#include  <kiklib/kik_str.h>	/* strdup */
#include  <kiklib/kik_conf_io.h>
#include  <kiklib/kik_map.h>

#include  "ml_xic.h"		/* refering mutually */
#include  "ml_locale.h"


#if  0
#define  __DEBUG
#endif

#define  MAX_XIMS_SAME_TIME  5


/* --- static variables --- */

static ml_xim_t  xims[MAX_XIMS_SAME_TIME] ;
static u_int  num_of_xims ;


/* --- static functions --- */

static ml_xim_t *
search_xim(
	char *  xmod
	)
{
	int  counter ;
	
	for( counter = 0 ; counter < num_of_xims ; counter ++)
	{
		if( strcasecmp( xims[counter].xmod , xmod) == 0)
		{
			return  &xims[counter] ;
		}
	}

	return  NULL ;
}

static int
close_xim(
	ml_xim_t *  xim
	)
{
	XCloseIM( xim->im) ;

	free( xim->xmod) ;
	free( xim->locale) ;
	(*xim->parser->delete)( xim->parser) ;

#ifdef  DEBUG
	kik_warn_printf( KIK_DEBUG_TAG " xim closed.\n") ;
#endif
	
	return  1 ;
}

static void
xim_server_destroyed(
	XIM  im ,
	XPointer  data1 ,
	XPointer  data2
	)
{
	int  counter ;
	
	for( counter = 0 ; counter < num_of_xims ; counter ++)
	{
		if( xims[counter].im == im)
		{
		#ifdef  DEBUG
			kik_warn_printf( KIK_DEBUG_TAG
				" %s xim server destroyed.\n" , xims[counter].xmod) ;
		#endif

			while( xims[counter].num_of_xic_wins)
			{
				/*
				 * ml_xic_deactivate() calls ml_xic_destroyed() internally.
				 * so num_of_xic_wins member is automatically decreased.
				 */
				if( ! ml_xic_deactivate( xims[counter].xic_wins[0]))
				{
					xims[counter].num_of_xic_wins -- ;
				}
			}
			
			free( xims[counter].xmod) ;
			free( xims[counter].locale) ;
			(*xims[counter].parser->delete)( xims[counter].parser) ;
			
			xims[counter] = xims[--num_of_xims] ;

			return  ;
		}
	}
}
	
static ml_xim_t *
open_xim(
	Display *  display ,
	char *  xmod
	)
{
	XIM  im ;
	int  counter ;

	if( num_of_xims == MAX_XIMS_SAME_TIME)
	{
		counter = 0 ;
		
		while( 1)
		{
			if( counter == num_of_xims)
			{
				return  NULL ;
			}
			else if( xims[counter].num_of_xic_wins == 0)
			{
				close_xim( &xims[counter]) ;

				xims[counter] = xims[-- num_of_xims] ;

				break ;
			}
			else
			{
				counter ++ ;
			}
		}
	}
	
	if( ( im = XOpenIM( display , NULL , NULL , NULL)))
	{
		ml_xim_t *  xim ;
		XIMCallback  callback = { NULL , xim_server_destroyed } ;

		xim = &xims[num_of_xims] ;

		memset( xim , 0 , sizeof( ml_xim_t)) ;

		xim->im = im ;
		xim->num_of_xic_wins = 0 ;

		if( ( xim->encoding = ml_get_encoding( ml_get_codeset())) == ML_UNKNOWN_ENCODING)
		{
			goto  error ;
		}
		
		if( ( xim->parser = ml_parser_new( xim->encoding)) == NULL)
		{
			goto  error ;
		}

		if( ( xim->xmod = strdup( xmod)) == NULL)
		{
			goto  error ;
		}

		if( ( xim->locale = strdup( ml_get_locale())) == NULL)
		{
			goto  error ;
		}

		XSetIMValues( im , XNDestroyCallback , &callback , NULL) ;

		num_of_xims ++ ;

		return  xim ;

	error:
		if( xim->parser)
		{
			(*xim->parser->delete)( xim->parser) ;
		}

		if( xim->xmod)
		{
			free( xim->xmod) ;
		}

		if( xim->locale)
		{
			free( xim->locale) ;
		}

		XCloseIM( xim->im) ;
	}

	return  NULL ;
}

static char *
get_xim_name(
	char *  xmod
	)
{
	char *  p ;
	
	/* 4 is the length of "@im=" */
	if( strlen( xmod) < 4 || ( p = strstr( xmod , "@im=")) == NULL)
	{
		return  NULL ;
	}

	return  p + 4 ;
}


/* --- global functions --- */

int
ml_xim_final(void)
{
	int  counter ;
	
	for( counter = 0 ; counter < num_of_xims ; counter ++)
	{
		close_xim( &xims[counter]) ;
	}

	return  1 ;
}

ml_xim_t *
ml_get_xim(
	Display *  display ,
	char *  xim_name ,
	char *  xim_locale
	)
{
	ml_xim_t *  xim ;
	char *  xmod ;
	char *  cur_locale ;

	if( xim_locale != NULL && strcmp( xim_locale , "C") == 0)
	{
		return  NULL ;
	}

	/* 4 is the length of "@im=" */
	if( *xim_name && ( xmod = alloca( 4 + strlen( xim_name) + 1)) != NULL)
	{
		sprintf( xmod , "@im=%s" , xim_name) ;

		if( xim_locale)
		{
			cur_locale = ml_get_locale() ;

			if( strcmp( xim_locale , cur_locale) == 0)
			{
				/* the same locale as current */
				cur_locale = NULL ;
			}
			else
			{
				cur_locale = strdup( cur_locale) ;
				
				if( ! ml_locale_init( xim_locale))
				{
					/* setlocale() failed. restoring */

					ml_locale_init( cur_locale) ;
					free( cur_locale) ;

					return  NULL ;
				}
			}
			
			XSetLocaleModifiers(xmod) ;
		}
		else
		{
			XSetLocaleModifiers(xmod) ;
			cur_locale = NULL ;
		}
	}
	else
	{
		/* under current locale */
		
		xmod = XSetLocaleModifiers("") ;
		cur_locale = NULL ;
	}

	xim = NULL ;
	
	if( xmod != NULL && *xmod)
	{
		if( ! ( xim = search_xim( xmod)))
		{
			xim = open_xim( display , xmod) ;
		}
	}

	if( cur_locale)
	{
		/* restoring */
		ml_locale_init( cur_locale) ;
		free( cur_locale) ;
	}
	
	if( xim == NULL)
	{
		xmod = "@im=none" ;
		XSetLocaleModifiers(xmod) ;
		
		if( ! ( xim = search_xim( xmod)))
		{
			xim = open_xim( display , xmod) ;
		}
	}

	return  xim ;
}

int
ml_xic_created(
	ml_xim_t *  xim ,
	ml_window_t *  win
	)
{
	if( xim->num_of_xic_wins == MAX_XICS_PER_XIM)
	{
		return  0 ;
	}
	
	xim->xic_wins[xim->num_of_xic_wins ++] = win ;

	return  1 ;
}

int
ml_xic_destroyed(
	ml_xim_t *  xim ,
	ml_window_t *  win
	)
{
	int  counter ;
	
	if( xim->num_of_xic_wins == 0)
	{
		return  0 ;
	}

	for( counter = 0 ; counter < xim->num_of_xic_wins ; counter ++)
	{
		if( xim->xic_wins[counter] == win)
		{
			xim->xic_wins[counter] = xim->xic_wins[-- xim->num_of_xic_wins] ;

			return  1 ;
		}
	}

	return  0 ;
}

char *
ml_get_xim_name(
	ml_xim_t *  xim
	)
{
	return  get_xim_name( xim->xmod) ;
}
