/*
 *	$Id$
 */

#include  "x_xic.h"

#include  <kiklib/kik_debug.h>
#include  <kiklib/kik_str.h>	/* kik_str_alloca_dup */
#include  <kiklib/kik_mem.h>	/* malloc */
#include  <kiklib/kik_locale.h>	/* kik_get_locale */

#ifdef  UTF16_IME_CHAR
#include  <mkf/mkf_utf16_parser.h>
#endif


#define  HAS_XIM_LISTENER(win,function) \
	((win)->xim_listener && (win)->xim_listener->function)


/* --- static functions --- */

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

	if( ( win->xic = malloc( sizeof( x_xic_t))) == NULL)
	{
		return  0 ;
	}

#ifndef  UTF16_IME_CHAR
	if( ( win->xic->parser = ml_parser_new(
					ml_get_char_encoding( kik_get_codeset_win32()))) == NULL)
#else
	/* UTF16LE => UTF16BE in x_xic_get_str. */
	if( ( win->xic->parser = mkf_utf16_parser_new()) == NULL)
#endif
	{
		return  0 ;
	}

	win->xic->ic = ImmGetContext( win->my_window) ;
	win->xic->prev_keydown_wparam = 0 ;

	return  1 ;
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

	ImmReleaseContext( win->my_window, win->xic->ic) ;
	(*win->xic->parser->delete)( win->xic->parser) ;
	free( win->xic) ;
	win->xic = NULL ;

	return  1 ;
}

char *
x_xic_get_xim_name(
	x_window_t *  win
	)
{
#ifdef  USE_WIN32GUI
	return  "" ;
#else
	return  x_get_xim_name( win) ;
#endif
}

char *
x_xic_get_default_xim_name(void)
{
#ifdef  USE_WIN32GUI
	return  "" ;
#else
	return  x_get_default_xim_name() ;
#endif
}

int
x_xic_fg_color_changed(
	x_window_t *  win
	)
{
#ifndef  USE_WIN32GUI
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
#endif

	return  1 ;
}

int
x_xic_bg_color_changed(
	x_window_t *  win
	)
{
#ifndef  USE_WIN32GUI
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
#endif

	return  1 ;
}

int
x_xic_font_set_changed(
	x_window_t *  win
	)
{
#ifndef  USE_WIN32GUI
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
#endif

	return  1 ;
}

int
x_xic_resized(
	x_window_t *  win
	)
{
#ifndef  USE_WIN32GUI
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
#endif

	return  1 ;
}

int
x_xic_set_spot(
	x_window_t *  win
	)
{
	XPoint  spot ;
	COMPOSITIONFORM  cf ;

	if( win->xic == NULL)
	{
		return  0 ;
	}
	
	if( get_spot( win, &spot) == 0)
	{
		return  0 ;
	}

	MapWindowPoints( win->my_window, x_get_root_window( win)->my_window, &spot, 1) ;

	cf.ptCurrentPos = spot ;
	cf.dwStyle = CFS_POINT ;

	return  ImmSetCompositionWindow( win->xic->ic, &cf) ;
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
	size_t  len ;

	if( seq_len == 0 || event->ch == 0)
	{
		goto  zero_return ;
	}

#ifndef  UTF16_IME_CHAR
	len = 1 ;
	if( event->ch > 0xff)
	{
		*(seq++) = (char)((event->ch >> 8) & 0xff) ;

		if( seq_len == 1)
		{
			goto  zero_return ;
		}

		len ++ ;
	}

	*seq = (char)(event->ch & 0xff) ;
#else
	if( seq_len == 1)
	{
		goto  zero_return ;
	}

	*(seq++) = (char)((event->ch >> 8) & 0xff) ;
	*seq = (char)(event->ch & 0xff) ;
	len = 2 ;
#endif

	*parser = win->xic->parser ;
	*keysym = win->xic->prev_keydown_wparam ;
	win->xic->prev_keydown_wparam = 0 ;

	return  len ;

zero_return:
	*parser = NULL ;
	*keysym = win->xic->prev_keydown_wparam ;
	win->xic->prev_keydown_wparam = 0 ;

	return  0 ;
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
	return  0 ;
}

int
x_xic_filter_event(
	x_window_t *  win,	/* Should be root window. */
	XEvent *  event
	)
{
	int  count ;

	if( event->msg != WM_KEYDOWN)
	{
		return  0 ;
	}

	for( count = 0 ; count < win->num_of_children ; count++)
	{
		x_xic_filter_event( win->children[count], event) ;
	}
	
	if( ! win->xic)
	{
		return  0 ;
	}
	
	win->xic->prev_keydown_wparam = event->wparam ;
	
	return  1 ;
}

int
x_xic_set_focus(
	x_window_t *  win
	)
{
	return  1 ;
}

int
x_xic_unset_focus(
	x_window_t *  win
	)
{
	return  1 ;
}

/*
 * x_xim.c <-> x_xic.c communication functions
 * Not necessary in win32.
 */

int
x_xim_activated(
	x_window_t *  win
	)
{
	return  1 ;
}

int
x_xim_destroyed(
	x_window_t *  win
	)
{
	return  1 ;
}

