/*
 *	$Id$
 */

#include  "ml_xic.h"

#include  <X11/Xutil.h>		/* X{mb|utf8}LookupString */
#include  <kiklib/kik_debug.h>
#include  <kiklib/kik_str.h>	/* kik_str_alloca_dup */
#include  <kiklib/kik_mem.h>	/* malloc */
#include  <kiklib/kik_locale.h>	/* kik_get_locale */
#include  <mkf/mkf_utf8_parser.h>

#include  "ml_window_intern.h"	/* {FG|BG}_COLOR_PIXEL */
#include  "ml_xim.h"		/* refering mutually */


#define  HAS_XIM_LISTENER(win,function) \
	((win)->xim_listener && (win)->xim_listener->function)


/* --- static variables --- */

static mkf_parser_t *  utf8_parser ;


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
	ml_window_t *  win
	)
{
	char *  cur_locale ;
	XFontSet  fontset ;

	cur_locale = kik_str_alloca_dup( kik_get_locale()) ;
	if( kik_locale_init( ml_get_xim_locale( win)))
	{
		if( ! HAS_XIM_LISTENER(win,get_fontset))
		{
			return  None ;
		}
		
		fontset = (*win->xim_listener->get_fontset)( win->xim_listener->self) ;
		
		/* restoring */
		kik_locale_init( cur_locale) ;
	}
	else
	{
		fontset = None ;
	}

	return  fontset ;
}

static int
destroy_xic(
	ml_window_t *  win
	)
{
	if( ! win->xic)
	{
		return  0 ;
	}
	
	XDestroyIC( win->xic->ic) ;

	if( win->xic->fontset)
	{
		XFreeFontSet( win->display , win->xic->fontset) ;
	}
	
	free( win->xic) ;
	win->xic = NULL ;

	return  1 ;
}

static int
create_xic(
	ml_window_t *  win
	)
{
	XIMStyle  selected_style ;
	XVaNestedList  preedit_attr ;
	XRectangle  rect ;
	XPoint  spot ;
	XFontSet  fontset ;
	XIC  xic ;
	int  xim_ev_mask ;

	if( win->xic)
	{
		/* already created */
		
		return  0 ;
	}

	if( ( selected_style = ml_xim_get_style( win)) == 0)
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

		if( ( fontset = load_fontset( win)) == 0)
		{
			return  0 ;
		}

		if( ( preedit_attr = XVaCreateNestedList( 0 , XNArea , &rect , XNSpotLocation, &spot,
			XNForeground , UNFADE_FG_COLOR_PIXEL(win) ,
			XNBackground , UNFADE_BG_COLOR_PIXEL(win) ,
			XNFontSet , fontset , NULL)) == NULL)
		{
		#ifdef  DEBUG
			kik_warn_printf( KIK_DEBUG_TAG " XVaCreateNestedList() failed.\n") ;
		#endif

			XFreeFontSet( win->display , fontset) ;

			return  0 ;
		}

		if( ( xic = ml_xim_create_ic( win , selected_style , preedit_attr)) == NULL)
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

		if( ( xic = ml_xim_create_ic( win , selected_style , NULL)) == NULL)
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

	win->xic->ic = xic ;
	win->xic->fontset = fontset ;
	win->xic->style = selected_style ;

	xim_ev_mask = 0 ;

	XGetICValues( win->xic->ic , XNFilterEvents , &xim_ev_mask , NULL) ;
	ml_window_add_event_mask( win , xim_ev_mask) ;

#ifdef  DEBUG
	kik_warn_printf( KIK_DEBUG_TAG " XIC activated.\n") ;
#endif

	return  1 ;
}


/* --- global functions --- */

int
ml_xic_activate(
	ml_window_t *  win ,
	char *  xim_name ,
	char *  xim_locale
	)
{
	if( win->xic)
	{
		/* already activated */
		
		return  0 ;
	}
	
	return  ml_add_xim_listener( win , xim_name , xim_locale) ;
}

int
ml_xic_deactivate(
	ml_window_t *  win
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
		ml_window_remove_event_mask( win , xim_ev_mask) ;
	}
#endif
	
	destroy_xic( win) ;
	
	if( ! ml_remove_xim_listener( win))
	{
	#ifdef  DEBUG
		kik_warn_printf( KIK_DEBUG_TAG " ml_remove_xim_listener() failed.\n") ;
	#endif
	}

#ifdef  DEBUG
	kik_warn_printf( KIK_DEBUG_TAG " XIC deactivated.\n") ;
#endif

	return  1 ;
}

char *
ml_xic_get_xim_name(
	ml_window_t *  win
	)
{
	return  ml_get_xim_name( win) ;
}

int
ml_xic_fg_color_changed(
	ml_window_t *  win
	)
{
	XVaNestedList  preedit_attr ;

	if( win->xic == NULL || ! (win->xic->style & XIMPreeditPosition))
	{
		return  0 ;
	}

	if( ( preedit_attr = XVaCreateNestedList( 0 , XNForeground , UNFADE_FG_COLOR_PIXEL(win) , NULL))
		== NULL)
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

	if( win->xic == NULL || ! (win->xic->style & XIMPreeditPosition))
	{
		return  0 ;
	}

	if( ( preedit_attr = XVaCreateNestedList( 0 , XNBackground , UNFADE_BG_COLOR_PIXEL(win) , NULL))
		== NULL)
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
ml_xic_set_spot(
	ml_window_t *  win
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
ml_xic_get_utf8_str(
	ml_window_t *  win ,
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

/*
 * ml_xim.c <-> ml_xic.c communication functions
 */

int
ml_xim_activated(
	ml_window_t *  win
	)
{
	return  create_xic( win) ;
}

int
ml_xim_destroyed(
	ml_window_t *  win
	)
{
	return  destroy_xic( win) ;
}
