/*
 *	$Id$
 */

#include  "ml_xim.h"

#include  <stdio.h>		/* sprintf */
#include  <string.h>		/* strcmp/memset */
#include  <kiklib/kik_debug.h>
#include  <kiklib/kik_str.h>	/* strdup */
#include  <kiklib/kik_conf_io.h>
#include  <kiklib/kik_map.h>
#include  <kiklib/kik_locale.h>	/* kik_locale_init/kik_get_locale/kik_get_codeset */
#include  <kiklib/kik_mem.h>	/* alloca */

#include  "ml_xic.h"		/* refering mutually */


#if  0
#define  __DEBUG
#endif

#define  MAX_XIMS_SAME_TIME  5


/* --- static variables --- */

static Display *  xim_display ;
static char *  default_xim_name ;

static ml_xim_t  xims[MAX_XIMS_SAME_TIME] ;
static u_int  num_of_xims ;


/* --- static functions --- */

/* refered in xim_closed */
static void  xim_server_instantiated( Display *  display , XPointer  client_data , XPointer  call_data) ;


static int
close_xim(
	ml_xim_t *  xim
	)
{
	if( xim->im)
	{
		XCloseIM( xim->im) ;
	}

	if( xim->parser)
	{
		(*xim->parser->delete)( xim->parser) ;
	}
		
	free( xim->name) ;
	free( xim->locale) ;
	
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
			int  _counter ;
			
		#ifdef  DEBUG
			kik_warn_printf( KIK_DEBUG_TAG
				" %s xim(with %d xic) server destroyed.\n" ,
				xims[counter].name , xims[counter].num_of_xic_wins) ;
		#endif

			for( _counter = 0 ; _counter < xims[counter].num_of_xic_wins ; _counter ++)
			{
				ml_xim_destroyed( xims[counter].xic_wins[_counter]) ;
			}

			xims[counter].im = NULL ;

			break ;
		}
	}

	/* it is necessary to reset callback */
	XRegisterIMInstantiateCallback( xim_display , NULL , NULL , NULL ,
		xim_server_instantiated , NULL) ;
}

static int
open_xim(
	ml_xim_t *  xim
	)
{
	char *  xmod ;
	char *  cur_locale ;
	int  result ;

	/* 4 is the length of "@im=" */
	if( ( xmod = alloca( 4 + strlen( xim->name) + 1)) == NULL)
	{
		return  0 ;
	}
	
	sprintf( xmod , "@im=%s" , xim->name) ;

	cur_locale = kik_get_locale() ;

	if( strcmp( xim->locale , cur_locale) == 0)
	{
		/* the same locale as current */
		cur_locale = NULL ;
	}
	else
	{
		cur_locale = strdup( cur_locale) ;

		if( ! kik_locale_init( xim->locale))
		{
			/* setlocale() failed. restoring */

			kik_locale_init( cur_locale) ;
			free( cur_locale) ;

			return  0 ;
		}
	}

	XSetLocaleModifiers(xmod) ;

	result = 0 ;

	if( xmod && *xmod && ( xim->im = XOpenIM( xim_display , NULL , NULL , NULL)))
	{
		XIMCallback  callback = { NULL , xim_server_destroyed } ;

		if( ( xim->encoding = ml_get_encoding( kik_get_codeset())) == ML_UNKNOWN_ENCODING ||
			( xim->parser = ml_parser_new( xim->encoding)) == NULL)
		{
			XCloseIM( xim->im) ;
		}
		else
		{
			XSetIMValues( xim->im , XNDestroyCallback , &callback , NULL) ;

			/* succeeded */
			result = 1 ;
		}
	}
	
	if( cur_locale)
	{
		/* restoring */
		kik_locale_init( cur_locale) ;
		free( cur_locale) ;
	}
	
	return  result ;
}

static int
activate_xim(
	ml_xim_t *  xim
	)
{
	int  counter ;

	if( ! xim->im && ! open_xim( xim))
	{
		return  0 ;
	}
	
	for( counter = 0 ; counter < xim->num_of_xic_wins ; counter ++)
	{
		ml_xim_activated( xim->xic_wins[counter]) ;
	}

	return  1 ;
}

static void
xim_server_instantiated(
	Display *  display ,
	XPointer  client_data ,
	XPointer  call_data
	)
{
	int  counter ;

#ifdef  DEBUG
	kik_warn_printf( KIK_DEBUG_TAG " new xim server is instantiated.\n") ;
#endif

	for( counter = 0 ; counter < num_of_xims ; counter ++)
	{
		activate_xim( &xims[counter]) ;
	}
}

static ml_xim_t *
search_xim(
	char *  xim_name
	)
{
	int  counter ;

	for( counter = 0 ; counter < num_of_xims ; counter ++)
	{
		if( strcmp( xims[counter].name , xim_name) == 0)
		{
			return  &xims[counter] ;
		}
	}

	return  NULL ;
}

static ml_xim_t *
get_xim(
	char *  xim_name ,
	char *  xim_locale
	)
{
	ml_xim_t *  xim ;
	
	if( ( xim = search_xim( xim_name)) == NULL)
	{
		if( num_of_xims == MAX_XIMS_SAME_TIME)
		{
			int  counter ;

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

		xim = &xims[num_of_xims ++] ;
		memset( xim , 0 , sizeof( ml_xim_t)) ;

		xim->name = strdup( xim_name) ;
		xim->locale = strdup( xim_locale) ;
	}

	return  xim ;
}

static XIMStyle
search_xim_style(
	XIMStyles *  xim_styles ,
	XIMStyle *  supported_styles ,
	u_int  size
	)
{
	int  counter ;
	
	for( counter = 0 ; counter < xim_styles->count_styles ; counter ++)
	{
		int  _counter ;

		for( _counter = 0 ; _counter < size ; _counter ++)
		{
			if( supported_styles[_counter] == xim_styles->supported_styles[counter])
			{
				return  supported_styles[_counter] ;
			}
		}
	}
	
	return  0 ;
}


/* --- global functions --- */

int
ml_xim_init(
	Display *  display
	)
{
	char *  xmod ;
	char *  p ;

	xim_display = display ;

	xmod = XSetLocaleModifiers("") ;

	/* 4 is the length of "@im=" */
	if( strlen( xmod) < 4 || ( p = strstr( xmod , "@im=")) == NULL)
	{
		return  0 ;
	}

	if( ( default_xim_name = strdup( p + 4)) == NULL)
	{
		return  0 ;
	}

	if( ( p = strstr( default_xim_name , "@")))
	{
		/* only the first entry is used , others are ignored. */
		*p = '\0' ;
	}

	XRegisterIMInstantiateCallback( xim_display , NULL , NULL , NULL ,
		xim_server_instantiated , NULL) ;
		
	return  1 ;
}

int
ml_xim_final(void)
{
	int  counter ;
	
	for( counter = 0 ; counter < num_of_xims ; counter ++)
	{
		close_xim( &xims[counter]) ;
	}

	free( default_xim_name) ;

	XUnregisterIMInstantiateCallback( xim_display , NULL , NULL , NULL ,
		xim_server_instantiated , NULL) ;
	
	return  1 ;
}

int
ml_add_xim_listener(
	ml_window_t *  win ,
	char *  xim_name ,
	char *  xim_locale
	)
{
	if( strcmp( xim_locale , "C") == 0 ||
		strcmp( xim_name , "unused") == 0)
	{
		return  0 ;
	}
	
	if( *xim_name == '\0' && win->xim)
	{
		/*
		 * reactivating current xim.
		 */

		return  activate_xim( win->xim) ;
	}
	 
	if( win->xim)
	{
		ml_remove_xim_listener( win) ;
	}

	if( *xim_name == '\0')
	{
		/*
		 * default xim name is used
		 */

		xim_name = default_xim_name ;
	}
	
	if( ( win->xim = get_xim( xim_name , xim_locale)) == NULL)
	{
		return  0 ;
	}
	
	win->xim->xic_wins[ win->xim->num_of_xic_wins ++] = win ;
	
	return  activate_xim( win->xim) ;
}

int
ml_remove_xim_listener(
	ml_window_t *  win
	)
{
	int  counter ;

	if( win->xim->num_of_xic_wins == 0)
	{
		return  0 ;
	}

	for( counter = 0 ; counter < win->xim->num_of_xic_wins ; counter ++)
	{
		if( win->xim->xic_wins[counter] == win)
		{
			win->xim->xic_wins[counter] = win->xim->xic_wins[-- win->xim->num_of_xic_wins] ;
			win->xim = NULL ;

			return  1 ;
		}
	}

	return  0 ;
}

XIC
ml_xim_create_ic(
	ml_window_t *  win ,
	XIMStyle  selected_style ,
	XVaNestedList  preedit_attr
	)
{
	if( preedit_attr)
	{
		return  XCreateIC( win->xim->im , XNClientWindow , win->my_window ,
				XNFocusWindow , win->my_window ,
				XNInputStyle , selected_style ,
				XNPreeditAttributes  , preedit_attr , NULL) ;
	}
	else
	{
		return  XCreateIC( win->xim->im , XNClientWindow , win->my_window ,
				XNFocusWindow , win->my_window ,
				XNInputStyle , selected_style , NULL) ;
	}
}

XIMStyle
ml_xim_get_style(
	ml_window_t *  win
	)
{
	XIMStyle  over_the_spot_styles[] =
	{
		XIMPreeditPosition | XIMStatusNothing ,
		XIMPreeditPosition | XIMStatusNone ,
	} ;

	XIMStyle  root_styles[] =
	{
		XIMPreeditNothing | XIMStatusNothing ,
		XIMPreeditNothing | XIMStatusNone ,
		XIMPreeditNone | XIMStatusNothing ,
		XIMPreeditNone | XIMStatusNone ,
	} ;

	XIMStyle  selected_style ;
	XIMStyles *  xim_styles ;

	if( XGetIMValues( win->xim->im , XNQueryInputStyle , &xim_styles , NULL) || ! xim_styles)
	{
		return  0 ;
	}
	
	if( ! ( selected_style = search_xim_style( xim_styles , over_the_spot_styles ,
			sizeof( over_the_spot_styles) / sizeof( over_the_spot_styles[0]))))
	{
	#ifdef  DEBUG
		kik_warn_printf( KIK_DEBUG_TAG " over the spot style not found.\n") ;
	#endif

		if( ! ( selected_style = search_xim_style( xim_styles , root_styles ,
			sizeof( root_styles) / sizeof( root_styles[0]))))
		{
		#ifdef  DEBUG
			kik_warn_printf( KIK_DEBUG_TAG " root style not found.\n") ;
		#endif
		
			XFree( xim_styles) ;

			return  0 ;
		}
	}

	XFree( xim_styles) ;

	return  selected_style ;
}

char *
ml_get_xim_name(
	ml_window_t *  win
	)
{
	if( win->xim == NULL)
	{
		return  "unused" ;
	}
	
	return  win->xim->name ;
}

char *
ml_get_xim_locale(
	ml_window_t *  win
	)
{
	if( win->xim == NULL)
	{
		return  "" ;
	}

	return  win->xim->locale ;
}
