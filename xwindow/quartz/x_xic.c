/*
 *	$Id$
 */

#include  "../x_xic.h"

#include  <string.h>
#include  <kiklib/kik_debug.h>
#include  <kiklib/kik_mem.h>	/* malloc */


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

	if( ( win->xic->parser = ml_parser_new( ML_UTF8)) == NULL)
	{
		free( win->xic) ;
		win->xic = NULL ;

		return  0 ;
	}

	x_xic_font_set_changed( win) ;

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
	return  0 ;
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
	if( ( *keysym = event->keysym) >= 0xf700)
	{
		*parser = NULL ;

		return  0 ;
	}
	else
	{
		size_t  len = strlen( event->utf8) ;

		if( len <= seq_len)
		{
			memcpy( seq , event->utf8 , seq_len) ;
			*parser = win->xic->parser ;
		}

		return  len ;
	}
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
	return  0 ;
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
	return  0 ;
}

int
x_xic_switch_mode(
	x_window_t *  win
	)
{
	return  0 ;
}

#if  0

/*
 * x_xim.c <-> x_xic.c communication functions
 * Not necessary in fb.
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
