/*
 *	$Id$
 */

#include  "x_xic.h"

#include  <X11/Xutil.h>		/* X{mb|utf8}LookupString */
#include  <kiklib/kik_debug.h>
#include  <kiklib/kik_str.h>	/* kik_str_alloca_dup */
#include  <kiklib/kik_mem.h>	/* malloc */
#include  <kiklib/kik_locale.h>	/* kik_get_locale */
#include  <mkf/mkf_utf8_parser.h>

#include  "x_xim.h"		/* refering mutually */


#define  HAS_XIM_LISTENER(win,function) \
	((win)->xim_listener && (win)->xim_listener->function)


/* --- static variables --- */

static mkf_parser_t *  utf8_parser ;


/* --- static functions --- */

static void
get_rect(
	x_window_t *  win ,
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
	x_window_t *  win ,
	XPoint *  spot
	)
{
	int  x ;
	int  y ;

	if( ! HAS_XIM_LISTENER(win,get_spot) ||
		win->xim_listener->get_spot( win->xim_listener->self , &x , &y) == 0)
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
	x_window_t *  win
	)
{
	char *  cur_locale ;
	XFontSet  fontset ;

	cur_locale = kik_str_alloca_dup( kik_get_locale()) ;
	if( kik_locale_init( x_get_xim_locale( win)))
	{
		if( ! HAS_XIM_LISTENER(win,get_fontset))
		{
			return  NULL ;
		}
		
		fontset = (*win->xim_listener->get_fontset)( win->xim_listener->self) ;
		
		/* restoring */
		kik_locale_init( cur_locale) ;
	}
	else
	{
		fontset = NULL ;
	}

	return  fontset ;
}

static int
destroy_xic(
	x_window_t *  win
	)
{
	if( ! win->xic)
	{
		return  0 ;
	}
	
	XDestroyIC( win->xic->ic) ;

	if( win->xic->fontset)
	{
		XFreeFontSet( win->disp->display , win->xic->fontset) ;
	}
	
	free( win->xic) ;
	win->xic = NULL ;

	return  1 ;
}

static int
create_xic(
	x_window_t *  win
	)
{
	XIMStyle  selected_style ;
	XVaNestedList  preedit_attr ;
	XRectangle  rect ;
	XPoint  spot ;
	XFontSet  fontset ;
	XIC  xic ;
	long  xim_ev_mask ;

	if( win->xic)
	{
		/* already created */
		
		return  0 ;
	}

	if( ( selected_style = x_xim_get_style( win)) == 0)
	{
		return  0 ;
	}

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

		if( ! ( fontset = load_fontset( win)))
		{
			return  0 ;
		}

		if( ( preedit_attr = XVaCreateNestedList( 0 , XNArea , &rect , XNSpotLocation, &spot,
			XNForeground , (*win->xim_listener->get_fg_color)( win->xim_listener->self) ,
			XNBackground , (*win->xim_listener->get_bg_color)( win->xim_listener->self) ,
			XNFontSet , fontset , NULL)) == NULL)
		{
		#ifdef  DEBUG
			kik_warn_printf( KIK_DEBUG_TAG " XVaCreateNestedList() failed.\n") ;
		#endif

			XFreeFontSet( win->disp->display , fontset) ;

			return  0 ;
		}

		if( ( xic = x_xim_create_ic( win , selected_style , preedit_attr)) == NULL)
		{
		#ifdef  DEBUG
			kik_warn_printf( KIK_DEBUG_TAG " XCreateIC() failed\n") ;
		#endif

			XFree( preedit_attr) ;
			XFreeFontSet( win->disp->display , fontset) ;

			return  0 ;
		}
		
		XFree( preedit_attr) ;
	}
	else
	{
		/*
		 * root style
		 */

		if( ( xic = x_xim_create_ic( win , selected_style , NULL)) == NULL)
		{
		#ifdef  DEBUG
			kik_warn_printf( KIK_DEBUG_TAG " XCreateIC() failed\n") ;
		#endif

			return  0 ;
		}
		
		fontset = NULL ;
	}

	if( ( win->xic = malloc( sizeof( x_xic_t))) == NULL)
	{
	#ifdef  DEBUG
		kik_warn_printf( KIK_DEBUG_TAG " malloc() failed.\n") ;
	#endif
	
		if( fontset)
		{
			XFreeFontSet( win->disp->display , fontset) ;
		}

		return  0 ;
	}

	win->xic->ic = xic ;
	win->xic->fontset = fontset ;
	win->xic->style = selected_style ;

	xim_ev_mask = 0 ;

	XGetICValues( win->xic->ic , XNFilterEvents , &xim_ev_mask , NULL) ;
	x_window_add_event_mask( win , xim_ev_mask) ;

#ifdef  DEBUG
	kik_warn_printf( KIK_DEBUG_TAG " XIC activated.\n") ;
#endif

	return  1 ;
}


/* --- global functions --- */

int
x_xic_activate(
	x_window_t *  win ,
	char *  xim_name ,
	char *  xim_locale
	)
{
	if( win->xic)
	{
		/* already activated */
		
		return  0 ;
	}
	
	return  x_add_xim_listener( win , xim_name , xim_locale) ;
}

int
x_xic_deactivate(
	x_window_t *  win
	)
{
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
		x_window_remove_event_mask( win , xim_ev_mask) ;
	}
#endif
	
	destroy_xic( win) ;
	
	if( ! x_remove_xim_listener( win))
	{
	#ifdef  DEBUG
		kik_warn_printf( KIK_DEBUG_TAG " x_remove_xim_listener() failed.\n") ;
	#endif
	}

#ifdef  DEBUG
	kik_warn_printf( KIK_DEBUG_TAG " XIC deactivated.\n") ;
#endif

	return  1 ;
}

char *
x_xic_get_xim_name(
	x_window_t *  win
	)
{
	return  x_get_xim_name( win) ;
}

char *
x_xic_get_default_xim_name(void)
{
	return  x_get_default_xim_name() ;
}

int
x_xic_fg_color_changed(
	x_window_t *  win
	)
{
	XVaNestedList  preedit_attr ;

	if( win->xic == NULL || ! (win->xic->style & XIMPreeditPosition))
	{
		return  0 ;
	}

	if( ( preedit_attr = XVaCreateNestedList( 0 , XNForeground ,
		(*win->xim_listener->get_fg_color)( win->xim_listener->self) , NULL)) == NULL)
	{
		return  0 ;
	}

	XSetICValues( win->xic->ic , XNPreeditAttributes , preedit_attr , NULL) ;

	XFree( preedit_attr) ;
	
	return  1 ;
}

int
x_xic_bg_color_changed(
	x_window_t *  win
	)
{
	XVaNestedList  preedit_attr ;

	if( win->xic == NULL || ! (win->xic->style & XIMPreeditPosition))
	{
		return  0 ;
	}

	if( ( preedit_attr = XVaCreateNestedList( 0 , XNBackground ,
		(*win->xim_listener->get_bg_color)( win->xim_listener->self) , NULL)) == NULL)
	{
		return  0 ;
	}

	XSetICValues( win->xic->ic , XNPreeditAttributes , preedit_attr , NULL) ;

	XFree( preedit_attr) ;
	
	return  1 ;
}

int
x_xic_font_set_changed(
	x_window_t *  win
	)
{
	XVaNestedList  preedit_attr ;
	XFontSet  fontset ;

	if( win->xic == NULL || ! (win->xic->style & XIMPreeditPosition))
	{
		return  0 ;
	}

	if( ! ( fontset = load_fontset( win)))
	{
		return  0 ;
	}
	
	if( ( preedit_attr = XVaCreateNestedList( 0 , XNFontSet , fontset , NULL)) == NULL)
	{
		XFreeFontSet( win->disp->display , fontset) ;
		
		return  0 ;
	}

	XSetICValues( win->xic->ic , XNPreeditAttributes , preedit_attr , NULL) ;

	XFree( preedit_attr) ;
	
	XFreeFontSet( win->disp->display , win->xic->fontset) ;
	win->xic->fontset = fontset ;
	
	return  1 ;
}

int
x_xic_resized(
	x_window_t *  win
	)
{
	XVaNestedList  preedit_attr ;
	XRectangle  rect ;
	XPoint  spot ;

	if( win->xic == NULL || ! (win->xic->style & XIMPreeditPosition))
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
x_xic_set_spot(
	x_window_t *  win
	)
{
	XVaNestedList  preedit_attr ;
	XPoint  spot ;

	if( win->xic == NULL || ! (win->xic->style & XIMPreeditPosition))
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
x_xic_get_str(
	x_window_t *  win ,
	u_char *  seq ,
	size_t  seq_len ,
	mkf_parser_t **  parser ,
	KeySym *  keysym ,
	XKeyEvent *  event
	)
{
	Status  stat ;
	size_t  len ;

	if( win->xic == NULL)
	{
		return  0 ;
	}

	if( ( len = XmbLookupString( win->xic->ic , event , seq , seq_len , keysym , &stat)) == 0)
	{
		return  0 ;
	}

	if( IS_ENCODING_BASED_ON_ISO2022(win->xim->encoding) && *seq < 0x20)
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
		*parser = win->xim->parser ;
	}

	return  len ;
}

size_t
x_xic_get_utf8_str(
	x_window_t *  win ,
	u_char *  seq ,
	size_t  seq_len ,
	mkf_parser_t **  parser ,
	KeySym *  keysym ,
	XKeyEvent *  event
	)
{
#ifdef  HAVE_XUTF8_LOOKUP_STRING
	Status  stat ;
	size_t  len ;

	if( win->xic == NULL)
	{
		return  0 ;
	}
	
	if( ( len = Xutf8LookupString( win->xic->ic , event , seq , seq_len , keysym , &stat)) == 0)
	{
		return  0 ;
	}
	
	if( ! utf8_parser)
	{
		utf8_parser = mkf_utf8_parser_new() ;
	}

	*parser = utf8_parser ;

	return  len ;
#else
	return  0 ;
#endif
}

int
x_xic_set_focus(
	x_window_t *  win
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
x_xic_unset_focus(
	x_window_t *  win
	)
{
	if( ! win->xic)
	{
		return  0 ;
	}

	XUnsetICFocus( win->xic->ic) ;

	return  1 ;
}

/*
 * x_xim.c <-> x_xic.c communication functions
 */

int
x_xim_activated(
	x_window_t *  win
	)
{
	return  create_xic( win) ;
}

int
x_xim_destroyed(
	x_window_t *  win
	)
{
	return  destroy_xic( win) ;
}
