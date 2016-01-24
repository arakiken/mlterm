/*
 *	$Id$
 */

#include  "../x_window.h"

#include  <X11/Xft/Xft.h>
#include  <kiklib/kik_mem.h>	/* alloca */


#define  x_color_to_xft(xcolor)  _x_color_to_xft( alloca(sizeof(XftColor)) , (xcolor))


/* --- static functions --- */

static XftColor *
_x_color_to_xft(
	XftColor *  xftcolor ,
	x_color_t *  xcolor
	)
{
	xftcolor->pixel = xcolor->pixel ;
	xftcolor->color.red = (xcolor->red << 8) + xcolor->red ;
	xftcolor->color.green = (xcolor->green << 8) + xcolor->green ;
	xftcolor->color.blue = (xcolor->blue << 8) + xcolor->blue ;
	xftcolor->color.alpha = (xcolor->alpha << 8) + xcolor->alpha ;

	return  xftcolor ;
}


/* --- global functions --- */

int
x_window_set_use_xft(
	x_window_t *  win ,
	int  use_xft
	)
{
	if( use_xft)
	{
		if( ( win->xft_draw = XftDrawCreate( win->disp->display , win->my_window ,
					win->disp->visual , win->disp->colormap)))
		{
			return  1 ;
		}
	}
	else
	{
		XftDrawDestroy( win->xft_draw) ;
		win->xft_draw = NULL ;

		return  1 ;
	}

	return  0 ;
}

int
x_window_xft_draw_string8(
	x_window_t *  win ,
	x_font_t *  font ,
	x_color_t *  fg_color ,
	int  x ,
	int  y ,
	u_char *  str ,
	size_t  len
	)
{
	XftColor *  xftcolor ;

	/* Removing trailing spaces. */
	while( 1)
	{
		if( len == 0)
		{
			return  1 ;
		}

		if( *(str + len - 1) == ' ')
		{
			len-- ;
		}
		else
		{
			break ;
		}
	}

	xftcolor = x_color_to_xft( fg_color) ;

	XftDrawString8( win->xft_draw , xftcolor , font->xft_font ,
		x + font->x_off + win->hmargin ,
		y + win->vmargin , str , len) ;

	if( font->double_draw_gap)
	{
		XftDrawString8( win->xft_draw , xftcolor , font->xft_font ,
			x + font->x_off + win->hmargin + font->double_draw_gap ,
			y + win->vmargin , str , len) ;
	}

	return  1 ;
}

int
x_window_xft_draw_string32(
	x_window_t *  win ,
	x_font_t *  font ,
	x_color_t *  fg_color ,
	int  x ,
	int  y ,
	FcChar32 *  str ,
	u_int  len
	)
{
	XftColor *  xftcolor ;

	xftcolor = x_color_to_xft( fg_color) ;

	if( font->use_ot_layout /* && font->ot_font */)
	{
		XftDrawGlyphs( win->xft_draw , xftcolor , font->xft_font ,
			x + font->x_off + win->hmargin ,
			y + win->vmargin , str , len) ;
	}
	else
	{
		XftDrawString32( win->xft_draw , xftcolor , font->xft_font ,
			x + font->x_off + win->hmargin ,
			y + win->vmargin , str , len) ;
	}

	if( font->double_draw_gap)
	{
		if( font->use_ot_layout /* && font->ot_font */)
		{
			XftDrawGlyphs( win->xft_draw , xftcolor , font->xft_font ,
				x + font->x_off + win->hmargin ,
				y + win->vmargin , str , len) ;
		}
		else
		{
			XftDrawString32( win->xft_draw , xftcolor , font->xft_font ,
				x + font->x_off + win->hmargin + font->double_draw_gap ,
				y + win->vmargin , str , len) ;
		}
	}

	return  1 ;
}

void
xft_set_clip(
	x_window_t *  win ,
	int  x ,
	int  y ,
	u_int  width ,
	u_int  height
	)
{
	XRectangle  rect ;

	rect.x = 0 ;
	rect.y = 0 ;
	rect.width = width ;
	rect.height = height ;

	XftDrawSetClipRectangles( win->xft_draw , x , y , &rect , 1) ;
}

void
xft_unset_clip(
	x_window_t *  win
	)
{
	XRectangle  rect ;

	rect.x = 0 ;
	rect.y = 0 ;
	rect.width = ACTUAL_WIDTH(win) ;
	rect.height = ACTUAL_HEIGHT(win) ;

	XftDrawSetClipRectangles( win->xft_draw , 0 , 0 , &rect , 1) ;
}
