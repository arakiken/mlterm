/*
 *	update: <2001/11/24(23:56:58)>
 *	$Id$
 */

#include  "ml_xic.h"

#include  <X11/Xutil.h>		/* XLookupString */
#include  <kiklib/kik_debug.h>
#include  <kiklib/kik_str.h>	/* kik_str_alloca_dup */

#include  "ml_window_intern.h"
#include  "ml_xim.h"		/* refering mutually */
#include  "ml_locale.h"


/* --- static functions --- */

static void
get_rect(
	ml_window_t *  win ,
	XRectangle *  rect
	)
{
	rect->x = 0 ;
	rect->y = 0 ;
	rect->width = ACTUAL_WIDTH(win) ;
	rect->height = ACTUAL_HEIGHT(win) ;
}

static int
get_spot(
	ml_window_t *  win ,
	XPoint *  spot
	)
{
	int  x ;
	int  y ;

	if( win->xim_listener->get_spot( win->xim_listener->self , &x , &y) == 0)
	{
	#ifdef  DEBUG
		kik_warn_printf( KIK_DEBUG_TAG " xim_listener->get_spot() failed.\n") ;
	#endif

		return  0 ;
	}

	spot->x = x + win->margin ;
	spot->y = y /* + win->margin */ ;

	return  1 ;
}

static XFontSet
load_fontset(
	ml_window_t *  win ,
	ml_xim_t *  xim
	)
{
	char *  cur_locale ;
	XFontSet  fontset ;
	
	cur_locale = kik_str_alloca_dup( ml_get_locale()) ;
	if( ml_locale_init( xim->locale))
	{
		fontset = (*win->xim_listener->get_fontset)( win->xim_listener->self) ;
		
		/* restoring */
		ml_locale_init( cur_locale) ;
	}
	else
	{
		fontset = 0 ;
	}

	return  fontset ;
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
	
#ifdef  DEBUG
	kik_warn_printf( KIK_DEBUG_TAG " style not found.\n") ;
#endif

	return  0 ;
}

static int
xic_create(
	ml_window_t *  win ,
	ml_xim_t *  xim
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
	XVaNestedList  preedit_attr ;
	XRectangle  rect ;
	XPoint  spot ;
	XFontSet  fontset ;
	XIC  xic ;

	if( XGetIMValues( xim->im , XNQueryInputStyle , &xim_styles , NULL) || ! xim_styles)
	{
		return  0 ;
	}
	
	if( ! ( selected_style = search_xim_style( xim_styles , over_the_spot_styles ,
			sizeof( over_the_spot_styles) / sizeof( over_the_spot_styles[0]))) &&
		! ( selected_style = search_xim_style( xim_styles , root_styles ,
			sizeof( root_styles) / sizeof( root_styles[0]))))
	{
		XFree( xim_styles) ;

		return  0 ;
	}

	XFree( xim_styles) ;

	if( selected_style & XIMPreeditPosition)
	{
		/*
		 * over the spot style.
		 */
		 
		get_rect( win , &rect) ;
		if( get_spot( win , &spot) == 0)
		{
			/* forcibly set ... */
			spot.x = 0 ;
			spot.y = 0 ;
		}

		if( ( fontset = load_fontset( win , xim)) == 0)
		{
			return  0 ;
		}

		if( ( preedit_attr = XVaCreateNestedList( 0 , XNArea , &rect , XNSpotLocation, &spot,
			XNForeground , FG_COLOR_PIXEL(win) , XNBackground , BG_COLOR_PIXEL(win) ,
			XNFontSet , fontset , NULL)) == NULL)
		{
		#ifdef  DEBUG
			kik_warn_printf( KIK_DEBUG_TAG " XVaCreateNestedList() failed.\n") ;
		#endif

			XFreeFontSet( win->display , fontset) ;

			return  0 ;
		}

		if( ( xic = XCreateIC( xim->im , XNInputStyle , selected_style ,
				XNClientWindow , win->my_window , XNFocusWindow , win->my_window ,
				XNPreeditAttributes  , preedit_attr , NULL)) == NULL)
		{
		#ifdef  DEBUG
			kik_warn_printf( KIK_DEBUG_TAG " XCreateIC() failed\n") ;
		#endif

			XFree( preedit_attr) ;
			XFreeFontSet( win->display , fontset) ;

			return  0 ;
		}
		
		XFree( preedit_attr) ;
	}
	else
	{
		/*
		 * root style
		 */
		 
		if( ( xic = XCreateIC( xim->im , XNInputStyle , selected_style ,
				XNClientWindow , win->my_window , XNFocusWindow , win->my_window ,
				NULL)) == NULL)
		{
		#ifdef  DEBUG
			kik_warn_printf( KIK_DEBUG_TAG " XCreateIC() failed\n") ;
		#endif

			return  0 ;
		}
		
		fontset = NULL ;
	}
	
	if( ( win->xic = malloc( sizeof( ml_xic_t))) == NULL)
	{
	#ifdef  DEBUG
		kik_warn_printf( KIK_DEBUG_TAG " malloc() failed.\n") ;
	#endif
	
		if( fontset)
		{
			XFreeFontSet( win->display , fontset) ;
		}

		return  0 ;
	}

	win->xic->xim = xim ;
	win->xic->ic = xic ;
	win->xic->fontset = fontset ;
	win->xic->style = selected_style ;

#ifdef  __DEBUG
	kik_debug_printf( KIK_DEBUG_TAG " XIM was successfully opened.\n") ;
#endif

	return  1 ;
}

static int
destroy_xic(
	ml_window_t *  win
	)
{
	XDestroyIC( win->xic->ic) ;

	if( win->xic->fontset)
	{
		XFreeFontSet( win->display , win->xic->fontset) ;
	}
	
	free( win->xic) ;
	win->xic = NULL ;

	return  1 ;
}


/* --- global functions --- */

int
ml_use_xim(
	ml_window_t *  win ,
	ml_xim_event_listener_t *  xim_listener
	)
{
	win->use_xim = 1 ;
	win->xim_listener = xim_listener ;
	
	return  1 ;
}

int
ml_xic_activate(
	ml_window_t *  win ,
	char *  xim_name ,
	char *  xim_locale
	)
{
	ml_xim_t *  xim ;
	int  xim_ev_mask ;

	if( win->xic)
	{
		/* already activated */
		
		return  0 ;
	}
	
	if( strcmp( xim_name , "unused") == 0)
	{
		return  0 ;
	}
	
	if( ( xim = ml_get_xim( win->display , xim_name , xim_locale)) == NULL)
	{
		return  0 ;
	}

	if( ! xic_create( win , xim))
	{
		return  0 ;
	}

	xim_ev_mask = 0 ;

	XGetICValues( win->xic->ic , XNFilterEvents , &xim_ev_mask , NULL) ;
	ml_window_add_event_mask( win , xim_ev_mask) ;

	if( ! ml_xic_created( xim , win))
	{
	#ifdef  DEBUG
		kik_warn_printf( KIK_DEBUG_TAG " ml_xic_created() failed.\n") ;
	#endif
	
		ml_xic_deactivate( win) ;
		
		return  0 ;
	}

	return  1 ;
}

int
ml_xic_deactivate(
	ml_window_t *  win
	)
{
	ml_xim_t *  xim ;
	
	if( win->xic == NULL)
	{
		/* already deactivated */
		
		return  0 ;
	}

#if  0
	{
		/*
		 * this should not be done.
		 */
		int  xim_ev_mask ;
	
		XGetICValues( win->xic->ic , XNFilterEvents , &xim_ev_mask , NULL) ;
		ml_window_remove_event_mask( win , xim_ev_mask) ;
	}
#endif
	
	xim = win->xic->xim ;

	destroy_xic( win) ;
	
	if( ! ml_xic_destroyed( xim , win))
	{
	#ifdef  DEBUG
		kik_warn_printf( KIK_DEBUG_TAG " ml_xic_destroyed() failed.\n") ;
	#endif
	}

	return  0 ;
}

char *
ml_xic_get_xim_name(
	ml_window_t *  win
	)
{
	if( ! win->xic)
	{
		return  "unused" ;
	}
	
	return  ml_get_xim_name( win->xic->xim) ;
}

int
ml_xic_fg_color_changed(
	ml_window_t *  win
	)
{
	XVaNestedList  preedit_attr ;

	if( win->use_xim == 0 || win->xic == NULL || ! (win->xic->style & XIMPreeditPosition))
	{
		return  0 ;
	}

	if( ( preedit_attr = XVaCreateNestedList( 0 , XNForeground , FG_COLOR_PIXEL(win) , NULL)) == NULL)
	{
		return  0 ;
	}

	XSetICValues( win->xic->ic , XNPreeditAttributes , preedit_attr , NULL) ;

	XFree( preedit_attr) ;
	
	return  1 ;
}

int
ml_xic_bg_color_changed(
	ml_window_t *  win
	)
{
	XVaNestedList  preedit_attr ;

	if( win->use_xim == 0 || win->xic == NULL || ! (win->xic->style & XIMPreeditPosition))
	{
		return  0 ;
	}

	if( ( preedit_attr = XVaCreateNestedList( 0 , XNBackground , BG_COLOR_PIXEL(win) , NULL)) == NULL)
	{
		return  0 ;
	}

	XSetICValues( win->xic->ic , XNPreeditAttributes , preedit_attr , NULL) ;

	XFree( preedit_attr) ;
	
	return  1 ;
}

int
ml_xic_font_set_changed(
	ml_window_t *  win
	)
{
	XVaNestedList  preedit_attr ;
	XFontSet  fontset ;

	if( win->use_xim == 0 || win->xic == NULL || ! (win->xic->style & XIMPreeditPosition))
	{
		return  0 ;
	}

	if( ! ( fontset = load_fontset( win , win->xic->xim)))
	{
		return  0 ;
	}
	
	if( ( preedit_attr = XVaCreateNestedList( 0 , XNFontSet , fontset , NULL)) == NULL)
	{
		XFreeFontSet( win->display , fontset) ;
		
		return  0 ;
	}

	XSetICValues( win->xic->ic , XNPreeditAttributes , preedit_attr , NULL) ;

	XFree( preedit_attr) ;
	
	XFreeFontSet( win->display , win->xic->fontset) ;
	win->xic->fontset = fontset ;
	
	return  1 ;
}

int
ml_xic_resized(
	ml_window_t *  win
	)
{
	XVaNestedList  preedit_attr ;
	XRectangle  rect ;
	XPoint  spot ;

	if( win->use_xim == 0 || win->xic == NULL || ! (win->xic->style & XIMPreeditPosition))
	{
		return  0 ;
	}
	
	get_rect( win , &rect) ;
	if( get_spot( win , &spot) == 0)
	{
		/* forcibly set ...*/
		spot.x = 0 ;
		spot.y = 0 ;
	}
	
	if( ( preedit_attr = XVaCreateNestedList( 0 , XNArea , &rect , XNSpotLocation , &spot , NULL))
		== NULL)
	{
	#ifdef  DEBUG
		kik_warn_printf( KIK_DEBUG_TAG " XvaCreateNestedList() failed.\n") ;
	#endif
	
		return  0 ;
	}
	
	XSetICValues( win->xic->ic , XNPreeditAttributes , preedit_attr , NULL) ;

	XFree( preedit_attr) ;
	
	return  1 ;
}

int
ml_xic_set_spot(
	ml_window_t *  win
	)
{
	XVaNestedList  preedit_attr ;
	XPoint  spot ;

	if( win->use_xim == 0 || win->xic == NULL || ! (win->xic->style & XIMPreeditPosition))
	{
		return  0 ;
	}
	
	if( get_spot( win , &spot) == 0)
	{
		/* XNSpotLocation not changed */
		
		return  0 ;
	}

	if( ( preedit_attr = XVaCreateNestedList( 0 , XNSpotLocation , &spot , NULL)) == NULL)
	{
	#ifdef  DEBUG
		kik_warn_printf( KIK_DEBUG_TAG " XvaCreateNestedList failed.\n") ;
	#endif
	
		return  0 ;
	}
	
	XSetICValues( win->xic->ic , XNPreeditAttributes , preedit_attr , NULL) ;
	
	XFree( preedit_attr) ;

	return  1 ;
}

size_t
ml_xic_get_str(
	ml_window_t *  win ,
	u_char *  seq ,
	size_t  seq_len ,
	mkf_parser_t **  parser ,
	KeySym *  keysym ,
	XKeyEvent *  event
	)
{
	Status  stat ;
	size_t  len ;

	if( win->xic == NULL || win->use_xim == 0)
	{
		return  0 ;
	}

	if( ( len = XmbLookupString( win->xic->ic , event , seq , seq_len , keysym , &stat)) == 0)
	{
		return  0 ;
	}

	if( IS_ENCODING_BASED_ON_ISO2022(win->xic->xim->encoding) && *seq < 0x20)
	{
		/*
		 * XXX hack
		 * control char(except delete char[0x7f]) is received.
		 * in afraid of it being parsed as part of iso2022 sequence ,
		 * *parser is set NULL.
		 */
		*parser = NULL ;
	}
	else
	{
		*parser = win->xic->xim->parser ;
	}

	return  len ;
}

int
ml_xic_set_focus(
	ml_window_t *  win
	)
{
	if( ! win->xic)
	{
		return  0 ;
	}

	XSetICFocus( win->xic->ic) ;

	return  1 ;
}

int
ml_xic_unset_focus(
	ml_window_t *  win
	)
{
	if( ! win->xic)
	{
		return  0 ;
	}

	XUnsetICFocus( win->xic->ic) ;

	return  1 ;
}
