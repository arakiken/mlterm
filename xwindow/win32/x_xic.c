/*
 *	$Id$
 */

#include  "../x_xic.h"

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
	return  "" ;
}

char *
x_xic_get_default_xim_name(void)
{
	return  "" ;
}

int
x_xic_fg_color_changed(
	x_window_t *  win
	)
{
	return  0 ;
}

int
x_xic_bg_color_changed(
	x_window_t *  win
	)
{
	return  0 ;
}

int
x_xic_font_set_changed(
	x_window_t *  win
	)
{
	return  0 ;
}

int
x_xic_resized(
	x_window_t *  win
	)
{
	return  0 ;
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

	*keysym = win->xic->prev_keydown_wparam ;
	win->xic->prev_keydown_wparam = 0 ;

	if( seq_len == 0 || event->ch == 0)
	{
		goto  zero_return ;
	}
	else if( event->state & ControlMask)
	{
		if( event->ch == '2' || event->ch == ' ')
		{
			event->ch = 0 ;
		}
		else if( '3' <= event->ch && event->ch <= '7')
		{
			/* '3' => 0x1b  '4' => 0x1c  '5' => 0x1d  '6' => 0x1e  '7' => 0x1f */
			event->ch -= 0x18 ;
		}
		else if( event->ch == '8')
		{
			event->ch = 0x7f ;
		}
		else if( event->ch == '^')
		{
			event->ch = 0x1d ;
		}
		else if( event->ch == '_' || event->ch == '/')
		{
			event->ch = 0x1f ;
		}
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

	/* wparam doesn't tell upper case from lower case. */
	if( 'A' <= event->ch && event->ch <= 'Z')
	{
		/* Upper to Lower case */
		*keysym += 0x20 ;
	}

	*parser = win->xic->parser ;

	return  len ;

zero_return:
	*parser = NULL ;

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
	u_int  count ;

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

int
x_xic_is_active(
	x_window_t *  win
	)
{
	if( win->xic == NULL)
	{
		return  0 ;
	}

	return  ImmGetOpenStatus( win->xic->ic) ;
}

int
x_xic_switch_mode(
	x_window_t *  win
	)
{
	if( win->xic == NULL)
	{
		return  0 ;
	}

	return  ImmSetOpenStatus( win->xic->ic , (ImmGetOpenStatus( win->xic->ic) == FALSE)) ;
}

#if  0

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

#endif
