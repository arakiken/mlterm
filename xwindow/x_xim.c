/*
 *	$Id$
 */

#include  "x_xim.h"

#include  <stdio.h>		/* sprintf */
#include  <string.h>		/* strcmp/memset */
#include  <kiklib/kik_debug.h>
#include  <kiklib/kik_str.h>	/* strdup */
#include  <kiklib/kik_locale.h>	/* kik_locale_init/kik_get_locale/kik_get_codeset */
#include  <kiklib/kik_mem.h>	/* alloca/realloc */

#include  "x_xic.h"		/* refering mutually */


#if  0
#define  __DEBUG
#endif

#define  MAX_XIMS_SAME_TIME  5


/* --- static variables --- */

static int  use_xim ;
static char *  default_xim_name ;	/* this can be NULL */

static x_xim_t  xims[MAX_XIMS_SAME_TIME] ;
static u_int  num_of_xims ;


/* --- static functions --- */

/* refered in xim_server_destroyed */
static void  xim_server_instantiated( Display *  display , XPointer  client_data , XPointer  call_data) ;


static int
close_xim(
	x_xim_t *  xim
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

	free( xim->xic_wins) ;
	
	return  1 ;
}

static int
invoke_xim_destroyed(
	x_xim_t *  xim
	)
{
	int  count ;

	for( count = 0 ; count < xim->num_of_xic_wins ; count ++)
	{
		x_xim_destroyed( xims->xic_wins[count]) ;
	}

	return  1 ;
}

static void
xim_server_destroyed(
	XIM  im ,
	XPointer  data1 ,
	XPointer  data2
	)
{
	int  count ;

	for( count = 0 ; count < num_of_xims ; count ++)
	{
		if( xims[count].im == im)
		{
		#ifdef  DEBUG
			kik_warn_printf( KIK_DEBUG_TAG
				" %s xim(with %d xic) server destroyed.\n" ,
				xims[count].name , xims[count].num_of_xic_wins) ;
		#endif

			invoke_xim_destroyed( &xims[count]) ;

			xims[count].im = NULL ;

			break ;
		}
	}

	/*
	 * XXX
	 * XRegisterIMInstantiateCallback of sunos/openwin seems buggy.
	 */
#if  ! defined(sun) && ! defined(__sun__) && ! defined(__sun)
	/* it is necessary to reset callback */
	XRegisterIMInstantiateCallback( XDisplayOfIM( im) , NULL , NULL , NULL ,
		xim_server_instantiated , NULL) ;
#endif
}

static int
open_xim(
	x_xim_t *  xim ,
	Display *  display
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

	result = 0 ;

	if( XSetLocaleModifiers(xmod) && ( xim->im = XOpenIM( display , NULL , NULL , NULL)))
	{
		if( ( xim->encoding = ml_get_char_encoding( kik_get_codeset())) == ML_UNKNOWN_ENCODING ||
			( xim->parser = ml_parser_new( xim->encoding)) == NULL)
		{
			XCloseIM( xim->im) ;
			xim->im = NULL ;
		}
		else
		{
			XIMCallback  callback = { NULL , xim_server_destroyed } ;

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
	x_xim_t *  xim ,
	Display *  display
	)
{
	int  count ;

	if( ! xim->im && ! open_xim( xim , display))
	{
		return  0 ;
	}

	for( count = 0 ; count < xim->num_of_xic_wins ; count ++)
	{
		x_xim_activated( xim->xic_wins[count]) ;
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
	int  count ;

#ifdef  DEBUG
	kik_warn_printf( KIK_DEBUG_TAG " new xim server is instantiated.\n") ;
#endif

	for( count = 0 ; count < num_of_xims ; count ++)
	{
		activate_xim( &xims[count] , display) ;
	}
}

static x_xim_t *
search_xim(
	Display *  display ,
	char *  xim_name
	)
{
	int  count ;

	for( count = 0 ; count < num_of_xims ; count ++)
	{
		if( strcmp( xims[count].name , xim_name) == 0)
		{
			if( ! xims[count].im)
			{
				return  &xims[count] ;
			}
			else if( XDisplayOfIM( xims[count].im) == display)
			{
				return  &xims[count] ;
			}
		}
	}

	return  NULL ;
}

static x_xim_t *
get_xim(
	Display *  display ,
	char *  xim_name ,
	char *  xim_locale
	)
{
	x_xim_t *  xim ;
	
	if( ( xim = search_xim( display , xim_name)) == NULL)
	{
		if( num_of_xims == MAX_XIMS_SAME_TIME)
		{
			int  count ;

			count = 0 ;

			while( 1)
			{
				if( count == num_of_xims)
				{
					return  NULL ;
				}
				else if( xims[count].num_of_xic_wins == 0)
				{
					close_xim( &xims[count]) ;

					xims[count] = xims[-- num_of_xims] ;

					break ;
				}
				else
				{
					count ++ ;
				}
			}
		}

		xim = &xims[num_of_xims ++] ;
		memset( xim , 0 , sizeof( x_xim_t)) ;

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
	int  count ;
	
	for( count = 0 ; count < xim_styles->count_styles ; count ++)
	{
		int  _count ;

		for( _count = 0 ; _count < size ; _count ++)
		{
			if( supported_styles[_count] == xim_styles->supported_styles[count])
			{
				return  supported_styles[_count] ;
			}
		}
	}
	
	return  0 ;
}


/* --- global functions --- */

int
x_xim_init(
	int  _use_xim
	)
{
	char *  xmod ;
	char *  p ;

	if( ! ( use_xim = _use_xim))
	{
		return  0 ;
	}

	xmod = XSetLocaleModifiers("") ;

	/* 4 is the length of "@im=" */
	if( xmod == NULL || strlen( xmod) < 4 || ( p = strstr( xmod , "@im=")) == NULL ||
		( default_xim_name = strdup( p + 4)) == NULL)
	{
	#if  0
		default_xim_name = NULL ;
	#else
		default_xim_name = "none" ;
	#endif
	}
	else
	{
		if( ( p = strstr( default_xim_name , "@")))
		{
			/* only the first entry is used , others are ignored. */
			*p = '\0' ;
		}
	}
	
	return  1 ;
}

int
x_xim_final(void)
{
	int  count ;
	
	if( ! use_xim)
	{
		return  0 ;
	}

	for( count = 0 ; count < num_of_xims ; count ++)
	{
		close_xim( &xims[count]) ;
	}
	
	if( default_xim_name)
	{
		free( default_xim_name) ;
	}

	return  1 ;
}

int
x_xim_display_opened(
	Display *  display
	)
{
	if( ! use_xim)
	{
		return  0 ;
	}

	/*
	 * XXX
	 * XRegisterIMInstantiateCallback of sunos/openwin seems buggy.
	 */
#if  ! defined(sun) && ! defined(__sun__) && ! defined(__sun)
	XRegisterIMInstantiateCallback( display , NULL , NULL , NULL ,
		xim_server_instantiated , NULL) ;
#endif
	
	return  1 ;
}

int
x_xim_display_closed(
	Display *  display
	)
{
	int  count ;

	if( ! use_xim)
	{
		return  0 ;
	}

	count = 0 ;
	while( count < num_of_xims)
	{
		if( xims[count].im && XDisplayOfIM( xims[count].im) == display)
		{
			close_xim( &xims[count]) ;
			xims[count] = xims[-- num_of_xims] ;
		}
		else
		{
			count ++ ;
		}
	}
	
	/*
	 * XXX
	 * XRegisterIMInstantiateCallback of sunos/openwin seems buggy.
	 */
#if  ! defined(sun) && ! defined(__sun__) && ! defined(__sun)
	XUnregisterIMInstantiateCallback( display , NULL , NULL , NULL ,
		xim_server_instantiated , NULL) ;
#endif

	return  1 ;
}

int
x_add_xim_listener(
	x_window_t *  win ,
	char *  xim_name ,
	char *  xim_locale
	)
{
	void *  p ;

	if( ! use_xim)
	{
		return  0 ;
	}
	
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

		return  activate_xim( win->xim , win->display) ;
	}
	 
	if( win->xim)
	{
		x_remove_xim_listener( win) ;
	}

	if( *xim_name == '\0')
	{
		if( default_xim_name == NULL)
		{
			/*
			 * no default xims are set as XMODIFIERS.
			 */
			 
			return  0 ;
		}
		else
		{
			/*
			 * default xim name is used
			 */
			
			xim_name = default_xim_name ;
		}
	}
	
	if( ( win->xim = get_xim( win->display , xim_name , xim_locale)) == NULL)
	{
		return  0 ;
	}

	if( ( p = realloc( win->xim->xic_wins ,
			sizeof( x_window_t*) * (win->xim->num_of_xic_wins + 1))) == NULL)
	{
	#ifdef  DEBUG
		kik_warn_printf( KIK_DEBUG_TAG " realloc failed.\n") ;
	#endif
	
		return  0 ;
	}

	win->xim->xic_wins = p ;
	
	win->xim->xic_wins[ win->xim->num_of_xic_wins ++] = win ;
	
	return  activate_xim( win->xim , win->display) ;
}

int
x_remove_xim_listener(
	x_window_t *  win
	)
{
	int  count ;

	if( win->xim == NULL)
	{
		return  0 ;
	}

	if( win->xim->num_of_xic_wins == 0)
	{
		return  0 ;
	}

	for( count = 0 ; count < win->xim->num_of_xic_wins ; count ++)
	{
		if( win->xim->xic_wins[count] == win)
		{
			win->xim->xic_wins[count] = win->xim->xic_wins[-- win->xim->num_of_xic_wins] ;
			win->xim = NULL ;

			/*
			 * memory area of win->xim->xic_wins is not shrunk.
			 */
			
			return  1 ;
		}
	}

	return  0 ;
}

XIC
x_xim_create_ic(
	x_window_t *  win ,
	XIMStyle  selected_style ,
	XVaNestedList  preedit_attr
	)
{
	if( win->xim == NULL)
	{
		return  0 ;
	}
	
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
x_xim_get_style(
	x_window_t *  win
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
	#if  0
		/*
		 * These styles doesn't support character composing(XK_dead_xxx,XK_Multi_key...).
		 */
		XIMPreeditNothing | XIMStatusNone ,
		XIMPreeditNone | XIMStatusNothing ,
		XIMPreeditNone | XIMStatusNone ,
	#endif
	} ;

	XIMStyle  selected_style ;
	XIMStyles *  xim_styles ;

	if( win->xim == NULL)
	{
		return  0 ;
	}

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
x_get_xim_name(
	x_window_t *  win
	)
{
	if( win->xim == NULL)
	{
		return  "unused" ;
	}
	
	return  win->xim->name ;
}

char *
x_get_xim_locale(
	x_window_t *  win
	)
{
	if( win->xim == NULL)
	{
		return  "" ;
	}

	return  win->xim->locale ;
}

char *
x_get_default_xim_name(void)
{
	if ( ! use_xim)
	{
		return  "disable" ;
	}

	return  default_xim_name ;
}

