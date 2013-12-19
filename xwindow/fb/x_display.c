/*
 *	$Id$
 */

#include  "x_display.h"

#include  <stdio.h>	/* printf */
#include  <unistd.h>	/* STDIN_FILENO */
#include  <fcntl.h>	/* open */
#include  <sys/mman.h>	/* mmap */
#include  <sys/ioctl.h>	/* ioctl */
#include  <string.h>	/* memset/memcpy */
#include  <termios.h>

#include  <kiklib/kik_debug.h>
#include  <kiklib/kik_privilege.h>	/* kik_priv_change_e(u|g)id */
#include  <kiklib/kik_unistd.h>		/* kik_getuid */
#include  <kiklib/kik_file.h>

#include  <ml_color.h>

#include  "../x_window.h"
#include  "x_virtual_kbd.h"


#define  DISP_IS_INITED   (_disp.display)
#define  MOUSE_IS_INITED  (_mouse.fd != -1)
#define  CMAP_IS_INITED   (_display.cmap)

/* Because ppb is 2, 4 or 8, "% ppb" can be replaced by "& ppb" */
#define  MOD_PPB(i,ppb)  ((i) & ((ppb) - 1))

/*
 * If the most significant bit stores the pixel at the right side of the screen,
 * define VRAMBIT_MSBRIGHT.
 */
#if  0
#define VRAMBIT_MSBRIGHT
#endif

#if  0
#define ENABLE_2_4_PPB
#endif

#ifdef  ENABLE_2_4_PPB

#ifdef  VRAMBIT_MSBRIGHT
#define FB_SHIFT(ppb,bpp,idx)	(MOD_PPB(idx,ppb) * (bpp))
#define FB_SHIFT_0(ppb,bpp)	(0)
#define FB_SHIFT_NEXT(shift,bpp)	((shift) += (bpp))
#else
#define FB_SHIFT(ppb,bpp,idx)	(((ppb) - MOD_PPB(idx,ppb) - 1) * (bpp))
#define FB_SHIFT_0(ppb,bpp)	(((ppb) - 1) * (bpp))
#define FB_SHIFT_NEXT(shift,bpp)	((shift) -= (bpp))
#endif

#define FB_MASK(ppb)		((2 << (8 / (ppb) - 1)) - 1)

#else	/* ENABLE_2_4_PPB */

#ifdef  VRAMBIT_MSBRIGHT
#define FB_SHIFT(ppb,bpp,idx)	MOD_PPB(idx,ppb)
#define FB_SHIFT_0(ppb,bpp)	(0)
#define FB_SHIFT_NEXT(shift,bpp)	((shift) += 1)
#else
#define FB_SHIFT(ppb,bpp,idx)	(7 - MOD_PPB(idx,ppb))
#define FB_SHIFT_0(ppb,bpp)	(7)
#define FB_SHIFT_NEXT(shift,bpp)	((shift) -= 1)
#endif

#define FB_MASK(ppb)		(1)

#endif	/* ENABLE_2_4_PPB */

#define FB_WIDTH_BYTES(display,x,width) \
	( (width) * (display)->bytes_per_pixel / (display)->pixels_per_byte + \
		(MOD_PPB(x,(display)->pixels_per_byte) > 0 ? 1 : 0) + \
		(MOD_PPB((x) + (width),(display)->pixels_per_byte) > 0 ? 1 : 0))

#define  BG_MAGIC	0xff	/* for 1,2,4 bpp */

/* Enable doube buffering on 1, 2 or 4 bpp */
#if  1
#define  ENABLE_DOUBLE_BUFFER
#endif

/* Parameters of cursor_shape */
#define  CURSOR_WIDTH   7
#define  CURSOR_X_OFF   -3
#define  CURSOR_HEIGHT  15
#define  CURSOR_Y_OFF   -7


/* Note that this structure could be casted to Display */
typedef struct
{
	int  fd ;	/* Same as Display */

	int  x ;
	int  y ;

	int  button_state ;

	struct
	{
		/* x/y is left/top of the cursor. */
		int  x ;
		int  y ;
		int  width ;
		int  height ;

		/* x and y offset of cursor_shape */
		int  x_off ;
		int  y_off ;

		int  is_drawn ;

	} cursor ;

	u_char  saved_image[sizeof(u_int32_t) * CURSOR_WIDTH * CURSOR_HEIGHT] ;
	int  hidden_region_saved ;

} Mouse ;


/* --- static variables --- */

static Display  _display ;
static x_display_t  _disp ;
static Mouse  _mouse ;
static x_display_t  _disp_mouse ;
static x_display_t *  opened_disps[] = { &_disp , &_disp_mouse } ;
static int  use_ansi_colors = 1 ;

static struct termios  orig_tm ;

static char *  cursor_shape =
{
	"#######"
	"#*****#"
	"###*###"
	"  #*#  "
	"  #*#  "
	"  #*#  "
	"  #*#  "
	"  #*#  "
	"  #*#  "
	"  #*#  "
	"  #*#  "
	"  #*#  "
	"###*###"
	"#*****#"
	"#######"
} ;


/* --- static functions --- */

/* _disp.roots[1] is ignored. */
static inline x_window_t *
get_window(
	int  x ,	/* X in display */
	int  y		/* Y in display */
	)
{
	x_window_t *  win ;
	u_int  count ;

	for( count = 0 ; count < _disp.roots[0]->num_of_children ; count++)
	{
		if( ( win = _disp.roots[0]->children[count])->is_mapped)
		{
			if( win->x <= x && x < win->x + ACTUAL_WIDTH(win) &&
			    win->y <= y && y < win->y + ACTUAL_HEIGHT(win))
			{
				return  win ;
			}
		}
	}

	return  _disp.roots[0] ;
}

static inline u_char *
get_fb(
	int  x ,
	int  y
	)
{
	return  _display.fb + (_display.yoffset + y) * _display.line_length +
			(_display.xoffset + x) * _display.bytes_per_pixel
			/ _display.pixels_per_byte ;
}

static void
put_image_124bpp(
	int  x ,
	int  y ,
	u_char *  image ,
	size_t  size ,
	int  write_back_fb ,
	int  need_fb_pixel
	)
{
	u_int  ppb ;
	u_int  bpp ;
	u_char *  new_image ;
	u_char *  new_image_p ;
	u_char *  fb ;
	int  shift ;
	size_t  count ;
	int  plane ;

	ppb = _display.pixels_per_byte ;
	bpp = 8 / ppb ;

	fb = get_fb( x , y) ;

#ifdef  ENABLE_DOUBLE_BUFFER
	if( write_back_fb)
	{
		new_image = _display.back_fb + (fb - _display.fb) ;
	}
	else
#endif
	{
		/* + 2 is for surplus */
		if( ! ( new_image = alloca( size / ppb + 2)))
		{
			return ;
		}
	}

#ifdef  ENABLE_2_4_PPB
#define  PLANE(image)  (image)
#else
#define  PLANE(image)  (((image) >> plane) & 0x1)
#endif

	plane = 0 ;
	while( 1)
	{
		new_image_p = new_image ;

		if( need_fb_pixel && memchr( image , BG_MAGIC , size))
		{
		#ifdef  ENABLE_DOUBLE_BUFFER
			if( ! write_back_fb)
		#endif
			{
				memcpy( new_image ,
				#ifdef  ENABLE_DOUBLE_BUFFER
					_display.back_fb + (fb - _display.fb) ,
				#else
					fb ,
				#endif
					size / ppb + 2) ;
			}

			shift = FB_SHIFT(ppb,bpp,x) ;

			if( _disp.depth > 1)
			{
				for( count = 0 ; count < size ; count++)
				{
					if( image[count] != BG_MAGIC)
					{
						(*new_image_p) =
						  ((*new_image_p) & ~(_display.mask << shift)) |
						   (PLANE(image[count]) << shift) ;
					}

				#ifdef  VRAMBIT_MSBRIGHT
					if( FB_SHIFT_NEXT(shift,bpp) >= 8)
				#else
					if( FB_SHIFT_NEXT(shift,bpp) < 0)
				#endif
					{
						new_image_p ++ ;
						shift = _display.shift_0 ;
					}
				}
			}
			else
			{
				for( count = 0 ; count < size ; count++)
				{
					if( image[count] != BG_MAGIC)
					{
						(*new_image_p) =
						  ((*new_image_p) & ~(_display.mask << shift)) |
						   (image[count] << shift) ;
					}

				#ifdef  VRAMBIT_MSBRIGHT
					if( FB_SHIFT_NEXT(shift,bpp) >= 8)
				#else
					if( FB_SHIFT_NEXT(shift,bpp) < 0)
				#endif
					{
						new_image_p ++ ;
						shift = _display.shift_0 ;
					}
				}
			}

			if( shift != _display.shift_0)
			{
				new_image_p ++ ;
			}
		}
		else
		{
			int  surplus ;
			u_char  pixel ;

			shift = _display.shift_0 ;
			count = 0 ;
			pixel = 0 ;

			if( ( surplus = MOD_PPB(x,ppb)) > 0)
			{
				u_char  fb_pixel ;

			#ifdef  ENABLE_DOUBLE_BUFFER
				fb_pixel = _display.back_fb[fb - _display.fb] ;
			#else
				fb_pixel = fb[0] ;
			#endif

				for( ; surplus > 0 ; surplus --)
				{
					pixel |= (fb_pixel & (_display.mask << shift)) ;

					FB_SHIFT_NEXT(shift,bpp) ;
				}
			}
			else
			{
				if( _disp.depth > 1)
				{
					goto  round_number_1 ;
				}
				else
				{
					goto  round_number_2 ;
				}
			}

			if( _disp.depth > 1)
			{
				do
				{
					pixel |= (PLANE(image[count++]) << shift) ;

				#ifdef  VRAMBIT_MSBRIGHT
					if( FB_SHIFT_NEXT(shift,bpp) >= 8)
				#else
					if( FB_SHIFT_NEXT(shift,bpp) < 0)
				#endif
					{
						*(new_image_p ++) = pixel ;
						pixel = 0 ;
						shift = _display.shift_0 ;

					round_number_1:
						if( ppb == 8)
						{
							for( ; count + 7 < size ; count += 8)
							{
								*(new_image_p++) =
								#ifdef  VRAMBIT_MSBRIGHT
								  PLANE(image[count]) |
								  (PLANE(image[count + 1]) << 1) |
								  (PLANE(image[count + 2]) << 2) |
								  (PLANE(image[count + 3]) << 3) |
								  (PLANE(image[count + 4]) << 4) |
								  (PLANE(image[count + 5]) << 5) |
								  (PLANE(image[count + 6]) << 6) |
								  (PLANE(image[count + 7]) << 7) ;
								#else
								  (PLANE(image[count]) << 7) |
								  (PLANE(image[count + 1]) << 6) |
								  (PLANE(image[count + 2]) << 5) |
								  (PLANE(image[count + 3]) << 4) |
								  (PLANE(image[count + 4]) << 3) |
								  (PLANE(image[count + 5]) << 2) |
								  (PLANE(image[count + 6]) << 1) |
								  PLANE(image[count + 7]) ;
								#endif
							}
						}
					}
				}
				while( count < size) ;
			}
			else
			{
				do
				{
					pixel |= (image[count++] << shift) ;

				#ifdef  VRAMBIT_MSBRIGHT
					if( FB_SHIFT_NEXT(shift,bpp) >= 8)
				#else
					if( FB_SHIFT_NEXT(shift,bpp) < 0)
				#endif
					{
						*(new_image_p ++) = pixel ;
						pixel = 0 ;
						shift = _display.shift_0 ;

					round_number_2:
						if( ppb == 8)
						{
							for( ; count + 7 < size ; count += 8)
							{
								*(new_image_p++) =
								#ifdef  VRAMBIT_MSBRIGHT
									 image[count] |
									 (image[count + 1] << 1) |
									 (image[count + 2] << 2) |
									 (image[count + 3] << 3) |
									 (image[count + 4] << 4) |
									 (image[count + 5] << 5) |
									 (image[count + 6] << 6) |
									 (image[count + 7] << 7) ;
								#else
									 (image[count] << 7) |
									 (image[count + 1] << 6) |
									 (image[count + 2] << 5) |
									 (image[count + 3] << 4) |
									 (image[count + 4] << 3) |
									 (image[count + 5] << 2) |
									 (image[count + 6] << 1) |
									 image[count + 7] ;
								#endif
							}
						}
					}
				}
				while( count < size) ;
			}

			if( shift != _display.shift_0)
			{
				u_char  fb_pixel ;

			#ifdef  ENABLE_DOUBLE_BUFFER
				fb_pixel = _display.back_fb[
						get_fb( x + size , y) - _display.fb +
						_display.plane_len * plane] ;
			#else
				fb_pixel = get_fb( x + size , y)[_display.plane_len * plane] ;
			#endif

				do
				{
					pixel |= (fb_pixel & (_display.mask << shift)) ;
				}
			#ifdef  VRAMBIT_MSBRIGHT
				while( FB_SHIFT_NEXT(shift,bpp) < 8) ;
			#else
				while( FB_SHIFT_NEXT(shift,bpp) >= 0) ;
			#endif

				*(new_image_p ++) = pixel ;
			}
		}

		memcpy( fb , new_image , new_image_p - new_image) ;

		if( ++plane < _disp.depth)
		{
			fb += _display.plane_len ;
		#ifdef  ENABLE_DOUBLE_BUFFER
			if( write_back_fb)
			{
				new_image += _display.plane_len ;
			}
		#endif
		}
		else
		{
			break ;
		}
	}
}

static void
update_mouse_cursor_state(void)
{
	if( -CURSOR_X_OFF > _mouse.x)
	{
		_mouse.cursor.x = 0 ;
		_mouse.cursor.x_off = -CURSOR_X_OFF - _mouse.x ;
	}
	else
	{
		_mouse.cursor.x = _mouse.x + CURSOR_X_OFF ;
		_mouse.cursor.x_off = 0 ;
	}

	_mouse.cursor.width = CURSOR_WIDTH - _mouse.cursor.x_off ;
	if( _mouse.cursor.x + _mouse.cursor.width > _display.width)
	{
		_mouse.cursor.width -=
			(_mouse.cursor.x + _mouse.cursor.width - _display.width) ;
	}

	if( -CURSOR_Y_OFF > _mouse.y)
	{
		_mouse.cursor.y = 0 ;
		_mouse.cursor.y_off = -CURSOR_Y_OFF - _mouse.y ;
	}
	else
	{
		_mouse.cursor.y = _mouse.y + CURSOR_Y_OFF ;
		_mouse.cursor.y_off = 0 ;
	}

	_mouse.cursor.height = CURSOR_HEIGHT - _mouse.cursor.y_off ;
	if( _mouse.cursor.y + _mouse.cursor.height > _display.height)
	{
		_mouse.cursor.height -=
			(_mouse.cursor.y + _mouse.cursor.height - _display.height) ;
	}
}

static void
restore_hidden_region(void)
{
	u_char *  fb ;

	if( ! _mouse.cursor.is_drawn)
	{
		return ;
	}

	/* Set 0 before window_exposed is called. */
	_mouse.cursor.is_drawn = 0 ;

	if( _mouse.hidden_region_saved)
	{
		size_t  width ;
		u_int  count ;

		fb = get_fb(_mouse.cursor.x , _mouse.cursor.y) ;
		width = FB_WIDTH_BYTES(&_display, _mouse.cursor.x, _mouse.cursor.width) ;

		for( count = 0 ; count < _mouse.cursor.height ; count++)
		{
			memcpy( fb , _mouse.saved_image + count * width , width) ;
			fb += _display.line_length ;
		}
	}
	else
	{
		x_window_t *  win ;

		win = get_window( _mouse.x , _mouse.y) ;

		if( win->window_exposed)
		{
			(*win->window_exposed)( win ,
				_mouse.cursor.x - win->x - win->hmargin ,
				_mouse.cursor.y - win->y - win->vmargin ,
				_mouse.cursor.width ,
				_mouse.cursor.height) ;
		}
	}
}

static void
save_hidden_region(void)
{
	u_char *  fb ;
	size_t  width ;
	u_int  count ;

	fb = get_fb( _mouse.cursor.x , _mouse.cursor.y) ;
	width = FB_WIDTH_BYTES(&_display, _mouse.cursor.x, _mouse.cursor.width) ;

	for( count = 0 ; count < _mouse.cursor.height ; count++)
	{
		memcpy( _mouse.saved_image + count * width , fb , width) ;
		fb += _display.line_length ;
	}

	_mouse.hidden_region_saved = 1 ;
}

static void
draw_mouse_cursor_line(
	int  y
	)
{
	u_char *  fb ;
	x_window_t *  win ;
	char *  shape ;
	u_char  image[CURSOR_WIDTH * sizeof(u_int32_t)] ;
	int  x ;

	fb = get_fb( _mouse.cursor.x , _mouse.cursor.y + y) ;

	win = get_window( _mouse.x , _mouse.y) ;
	shape = cursor_shape +
		((_mouse.cursor.y_off + y) * CURSOR_WIDTH + _mouse.cursor.x_off) ;

	for( x = 0 ; x < _mouse.cursor.width ; x++)
	{
		if( shape[x] == '*')
		{
			switch( _display.bytes_per_pixel)
			{
			case  1:
				image[x] = win->fg_color.pixel ;
				break ;

			case  2:
				((u_int16_t*)image)[x] = win->fg_color.pixel ;
				break ;

			/* case  4: */
			default:
				((u_int32_t*)image)[x] = win->fg_color.pixel ;
				break ;
			}
		}
		else if( shape[x] == '#')
		{
			switch( _display.bytes_per_pixel)
			{
			case  1:
				image[x] = win->bg_color.pixel ;
				break ;

			case  2:
				((u_int16_t*)image)[x] = win->bg_color.pixel ;
				break ;

			/* case  4: */
			default:
				((u_int32_t*)image)[x] = win->bg_color.pixel ;
				break ;
			}
		}
		else
		{
			switch( _display.bytes_per_pixel)
			{
			case  1:
				image[x] = x_display_get_pixel(
						_mouse.cursor.x + x ,
						_mouse.cursor.y + y) ;
				break ;

			case  2:
				((u_int16_t*)image)[x] = TOINT16(fb+2*x) ;
				break ;

			/* case  4: */
			default:
				((u_int32_t*)image)[x] = TOINT32(fb+4*x) ;
				break ;
			}
		}
	}

	if( _display.pixels_per_byte > 1)
	{
		put_image_124bpp(
			_mouse.cursor.x , _mouse.cursor.y + y ,
			image , _mouse.cursor.width , 0 , 1) ;
	}
	else
	{
		memcpy( fb , image , _mouse.cursor.width * _display.bytes_per_pixel) ;
	}
}

static void
draw_mouse_cursor(void)
{
	int  y ;

	for( y = 0 ; y < _mouse.cursor.height ; y++)
	{
		draw_mouse_cursor_line( y) ;
	}

	_mouse.cursor.is_drawn = 1 ;
}

static void
expose_window(
	x_window_t *  win ,
	int  x ,
	int  y ,
	u_int  width ,
	u_int  height
	)
{
	if( x < win->x + win->hmargin || y < win->y + win->vmargin ||
	    x - win->x + width > win->hmargin + win->width ||
	    y - win->y + height > win->vmargin + win->height)
	{
		x_window_clear_margin_area( win) ;
	}

	if( win->window_exposed)
	{
		(*win->window_exposed)( win , x - win->x - win->hmargin ,
			y - win->y - win->vmargin , width , height) ;
	}
}

static void
expose_display(
	int  x ,
	int  y ,
	u_int  width ,
	u_int  height
	)
{
	x_window_t *  win1 ;
	x_window_t *  win2 ;

	win1 = get_window( x , y) ;
	expose_window( win1 , x , y , width , height) ;

	/*
	 * XXX
	 * x_im_{status|candidate}_screen can exceed display width or height,
	 * because x_im_{status|candidate}_screen_new() shows screen at
	 * non-adjusted position.
	 */

	if( x + width > _disp.width)
	{
		width = _disp.width - x ;
	}

	if( y + height > _disp.height)
	{
		height = _disp.height - y ;
	}

	if( ( win2 = get_window( x + width - 1 , y + height - 1)) != win1)
	{
		expose_window( win2 , x , y , width , height) ;
	}
}

static void
receive_event_for_multi_roots(
	XEvent *  xev
	)
{
	int  redraw_im_win ;

	if( ( redraw_im_win = x_display_check_visibility_of_im_window()))
	{
		/* Stop drawing input method window */
		_disp.roots[1]->is_mapped = 0 ;
	}

	x_window_receive_event( _disp.roots[0] , xev) ;

	if( redraw_im_win && _disp.num_of_roots == 2)
	{
		/* Restart drawing input method window */
		_disp.roots[1]->is_mapped = 1 ;
	}

	if( x_display_check_visibility_of_im_window())
	{
		expose_window( _disp.roots[1] , _disp.roots[1]->x , _disp.roots[1]->y ,
				ACTUAL_WIDTH(_disp.roots[1]) , ACTUAL_HEIGHT(_disp.roots[1])) ;
	}
}

/*
 * Return value
 *  0: button is outside the virtual kbd area.
 *  1: button is inside the virtual kbd area. (bev is converted to kev.)
 */
static int
check_virtual_kbd(
	XButtonEvent *  bev
	)
{
	XKeyEvent  kev ;
	int  ret ;

	if( ! ( ret = x_is_virtual_kbd_event( &_disp , bev)))
	{
		return  0 ;
	}

	if( ret > 0)
	{
		/* don't draw mouse cursor in x_virtual_kbd_read() */
		restore_hidden_region() ;

		ret = x_virtual_kbd_read( &kev , bev) ;

		save_hidden_region() ;
		draw_mouse_cursor() ;

		if( ret == 1)
		{
			receive_event_for_multi_roots( &kev) ;
		}
	}

	return  1 ;
}


/* --- platform dependent stuff --- */

#if  defined(__FreeBSD__) || defined(__NetBSD__) || defined(__OpenBSD__)
#define  CMAP_SIZE(cmap)            ((cmap)->count)
#define  BYTE_COLOR_TO_WORD(color)  (color)
#define  WORD_COLOR_TO_BYTE(color)  (color)
#else	/* Linux */
#define  CMAP_SIZE(cmap)            ((cmap)->len)
#define  BYTE_COLOR_TO_WORD(color)  ((color) << 8 | (color))
#define  WORD_COLOR_TO_BYTE(color)  ((color) & 0xff)
#endif

static int  cmap_init(void) ;
static void  cmap_final(void) ;
static int  get_active_console(void) ;
static int  receive_stdin_key_event(void) ;

#if  defined(__FreeBSD__)
#include  "x_display_freebsd.c"
#elif  defined(USE_GRF)
#include  "x_display_x68kgrf.c"
#elif  defined(__NetBSD__) || defined(__OpenBSD__)
#include  "x_display_wscons.c"
#else	/* Linux */
#include  "x_display_linux.c"
#endif

#ifndef  __FreeBSD__

static int
get_active_console(void)
{
	struct vt_stat  st ;

	if( ioctl( STDIN_FILENO , VT_GETSTATE , &st) == -1)
	{
		return  -1 ;
	}

	return  st.v_active ;
}

static int
receive_stdin_key_event(void)
{
	u_char  buf[6] ;
	ssize_t  len ;

	while( ( len = read( _display.fd , buf , sizeof(buf) - 1)) > 0)
	{
		static struct
		{
			char *  str ;
			KeySym  ksym ;

		} table[] =
		{
			{ "[2~" , XK_Insert } ,
			{ "[3~" , XK_Delete } ,
			{ "[5~" , XK_Prior } ,
			{ "[6~" , XK_Next } ,
			{ "[A" , XK_Up } ,
			{ "[B" , XK_Down } ,
			{ "[C" , XK_Right } ,
			{ "[D" , XK_Left } ,
		#if  defined(__NetBSD__) || defined(__OpenBSD__)
			{ "[8~" , XK_End } ,
			{ "[7~" , XK_Home } ,
		#else
			{ "[F" , XK_End } ,
			{ "[H" , XK_Home } ,
		#endif
		#if  defined(__FreeBSD__)
			{ "OP" , XK_F1 } ,
			{ "OQ" , XK_F2 } ,
			{ "OR" , XK_F3 } ,
			{ "OS" , XK_F4 } ,
			{ "[15~" , XK_F5 } ,
		#elif  defined(__NetBSD__) || defined(__OpenBSD__)
			{ "[11~" , XK_F1 } ,
			{ "[12~" , XK_F2 } ,
			{ "[13~" , XK_F3 } ,
			{ "[14~" , XK_F4 } ,
			{ "[15~" , XK_F5 } ,
		#else
			{ "[[A" , XK_F1 } ,
			{ "[[B" , XK_F2 } ,
			{ "[[C" , XK_F3 } ,
			{ "[[D" , XK_F4 } ,
			{ "[[E" , XK_F5 } ,
		#endif
			{ "[17~" , XK_F6 } ,
			{ "[18~" , XK_F7 } ,
			{ "[19~" , XK_F8 } ,
			{ "[20~" , XK_F9 } ,
			{ "[21~" , XK_F10 } ,
			{ "[23~" , XK_F11 } ,
			{ "[24~" , XK_F12 } ,
		} ;

		size_t  count ;
		XKeyEvent  xev ;

		xev.type = KeyPress ;
		xev.state = get_key_state() ;
		xev.ksym = 0 ;
		xev.keycode = 0 ;

		if( buf[0] == '\x1b' && len > 1)
		{
			buf[len] = '\0' ;

			for( count = 0 ; count < sizeof(table) / sizeof(table[0]) ;
			     count++)
			{
				if( strcmp( buf + 1 , table[count].str) == 0)
				{
					xev.ksym = table[count].ksym ;

					break ;
				}
			}

			/* XXX */
		#ifdef  __FreeBSD__
			if( xev.ksym == 0 && len == 3 && buf[1] == '[')
			{
				if( 'Y' <= buf[2] && buf[2] <= 'Z')
				{
					xev.ksym = XK_F1 + (buf[2] - 'Y') ;
					xev.state = ShiftMask ;
				}
				else if( 'a' <= buf[2] && buf[2] <= 'j')
				{
					xev.ksym = XK_F3 + (buf[2] - 'a') ;
					xev.state = ShiftMask ;
				}
				else if( 'k' <= buf[2] && buf[2] <= 'v')
				{
					xev.ksym = XK_F1 + (buf[2] - 'k') ;
					xev.state = ControlMask ;
				}
				else if( 'w' <= buf[2] && buf[2] <= 'z')
				{
					xev.ksym = XK_F1 + (buf[2] - 'w') ;
					xev.state = ControlMask|ShiftMask ;
				}
				else if( buf[2] == '@')
				{
					xev.ksym = XK_F5 ;
					xev.state = ControlMask|ShiftMask ;
				}
				else if( '[' <= buf[2] && buf[2] <= '\`')
				{
					xev.ksym = XK_F6 + (buf[2] - '[') ;
					xev.state = ControlMask|ShiftMask ;
				}
				else if( buf[2] == '{')
				{
					xev.ksym = XK_F12 ;
					xev.state = ControlMask|ShiftMask ;
				}
			}
		#endif
		}

		if( xev.ksym)
		{
			receive_event_for_multi_roots( &xev) ;
		}
		else
		{
			for( count = 0 ; count < len ; count++)
			{
				xev.ksym = buf[count] ;

				if( (u_int)xev.ksym <= 0x1f)
				{
					if( xev.ksym == '\0')
					{
						/* CTL+' ' instead of CTL+@ */
						xev.ksym = ' ' ;
					}
					else if( 0x01 <= xev.ksym &&
						 xev.ksym <= 0x1a)
					{
						/*
						 * Lower case alphabets instead of
						 * upper ones.
						 */
						xev.ksym = xev.ksym + 0x60 ;
					}
					else
					{
						xev.ksym = xev.ksym + 0x40 ;
					}

					xev.state = ControlMask ;
				}

				receive_event_for_multi_roots( &xev) ;
			}
		}
	}

	return  1 ;
}

#endif	/* __FreeBSD__ */

static fb_cmap_t *
cmap_new(
	int  num_of_colors
	)
{
	fb_cmap_t *  cmap ;

	if( ! ( cmap = malloc( sizeof(*cmap) + sizeof(*(cmap->red)) * num_of_colors * 3)))
	{
		return  NULL ;
	}

#if  defined(__FreeBSD__)
	cmap->index = 0 ;
	cmap->transparent = NULL ;
#elif  defined(__NetBSD__) || defined(__OpenBSD__)
	cmap->index = 0 ;
#else
	cmap->start = 0 ;
	cmap->transp = NULL ;
#endif
	CMAP_SIZE(cmap) = num_of_colors ;
	cmap->red = cmap + 1 ;
	cmap->green = cmap->red + num_of_colors ;
	cmap->blue = cmap->green + num_of_colors ;

	return  cmap ;
}

static int
cmap_init(void)
{
	int  num_of_colors ;
	ml_color_t  color ;
	u_int8_t  r ;
	u_int8_t  g ;
	u_int8_t  b ;
	static u_char rgb_1bpp[] =
	{
		0x00 , 0x00 , 0x00 ,
		0xff , 0xff , 0xff
	} ;
	static u_char rgb_2bpp[] =
	{
		0x00 , 0x00 , 0x00 ,
		0x55 , 0x55 , 0x55 ,
		0xaa , 0xaa , 0xaa ,
		0xff , 0xff , 0xff
	} ;
	u_char *  rgb_tbl ;

	num_of_colors = (2 << (_disp.depth - 1)) ;

	if( ! _display.cmap)
	{
	#ifndef  USE_GRF
		if( num_of_colors > 2)
		{
			/*
			 * Not get the current cmap because num_of_colors == 2 doesn't
			 * conform the actual depth (1,2,4).
			 */
			if( ! ( _display.cmap_orig = cmap_new( num_of_colors)))
			{
				return  0 ;
			}

			ioctl( _display.fb_fd , FBIOGETCMAP , _display.cmap_orig) ;
		}
	#else
		if( ( _display.cmap_orig = malloc( sizeof(((fb_reg_t*)_display.fb)->gpal))))
		{
			memcpy( _display.cmap_orig , ((fb_reg_t*)_display.fb)->gpal ,
				sizeof(((fb_reg_t*)_display.fb)->gpal)) ;
		}
	#endif

		if( ! ( _display.cmap = cmap_new( num_of_colors)))
		{
			free( _display.cmap_orig) ;

			return  0 ;
		}
	}

	if( num_of_colors == 2)
	{
		rgb_tbl = rgb_1bpp ;
	}
	else if( num_of_colors == 4)
	{
		rgb_tbl = rgb_2bpp ;
	}
	else
	{
		rgb_tbl = NULL ;
	}

	for( color = 0 ; color < num_of_colors ; color ++)
	{
		if( rgb_tbl)
		{
			r = rgb_tbl[color * 3] ;
			g = rgb_tbl[color * 3 + 1] ;
			b = rgb_tbl[color * 3 + 2] ;
		}
		else
		{
			ml_get_color_rgba( color , &r , &g , &b , NULL) ;
		}

		_display.cmap->red[color] = BYTE_COLOR_TO_WORD(r) ;
		_display.cmap->green[color] = BYTE_COLOR_TO_WORD(g) ;
		_display.cmap->blue[color] = BYTE_COLOR_TO_WORD(b) ;
	}

#ifndef  USE_GRF
	ioctl( _display.fb_fd , FBIOPUTCMAP , _display.cmap) ;
#else
	{
		u_int  count ;

		for( count = 0 ; count < CMAP_SIZE(_display.cmap) ; count++)
		{
			((fb_reg_t*)_display.fb)->gpal[count] =
				(_display.cmap->red[count] >> 3) << 6 |
				(_display.cmap->green[count] >> 3) << 11 |
				(_display.cmap->blue[count] >> 3) << 1 ;
		}
	}
#endif

	_display.prev_pixel = 0xff000000 ;
	_display.prev_closest_pixel = 0xff000000 ;

	return  1 ;
}

static void
cmap_final(void)
{
	if( _display.cmap_orig)
	{
	#ifndef  USE_GRF
		ioctl( _display.fb_fd , FBIOPUTCMAP , _display.cmap_orig) ;
	#else
		memcpy( ((fb_reg_t*)_display.fb)->gpal , _display.cmap_orig ,
			sizeof(((fb_reg_t*)_display.fb)->gpal)) ;
	#endif

		free( _display.cmap_orig) ;
	}

	free( _display.cmap) ;
}


/* --- global functions --- */

x_display_t *
x_display_open(
	char *  disp_name ,
	u_int  depth
	)
{
	if( ! DISP_IS_INITED)
	{
		if( ! open_display( depth))
		{
			return  NULL ;
		}

	#ifdef  DEBUG
		kik_debug_printf( KIK_DEBUG_TAG " Framebuffer: "
			"len %d line_len %d xoff %d yoff %d depth %db/%dB w %d h %d\n" ,
			_display.smem_len ,
			_display.line_length ,
			_display.xoffset ,
			_display.yoffset ,
			_disp.depth ,
			_display.bytes_per_pixel ,
			_display.width ,
			_display.height) ;
	#endif

		fcntl( STDIN_FILENO , F_SETFL ,
			fcntl( STDIN_FILENO , F_GETFL , 0) | O_NONBLOCK) ;

		/* Hide the cursor of default console. */
		write( STDIN_FILENO , "\x1b[?25l" , 6) ;
	}

	return  &_disp ;
}

int
x_display_close(
	x_display_t *  disp
	)
{
	if( disp == &_disp)
	{
		return  x_display_close_all() ;
	}
	else
	{
		return  0 ;
	}
}

int
x_display_close_all(void)
{
	if( DISP_IS_INITED)
	{
		x_virtual_kbd_hide() ;

		if( MOUSE_IS_INITED)
		{
			close( _mouse.fd) ;
		}

	#ifdef  ENABLE_DOUBLE_BUFFER
		free( _display.back_fb) ;
	#endif

		write( STDIN_FILENO , "\x1b[?25h" , 6) ;
		tcsetattr( STDIN_FILENO , TCSAFLUSH , &orig_tm) ;

	#if  defined(__FreeBSD__)
		ioctl( STDIN_FILENO , KDSKBMODE , K_XLATE) ;
	#elif  defined(__NetBSD__)
	#ifdef  USE_GRF
		close_grf0() ;
		setup_reg( (fb_reg_t*)_display.fb , &orig_reg) ;
	#else
		ioctl( STDIN_FILENO , WSDISPLAYIO_SMODE , &orig_console_mode) ;
		x_event_source_remove_fd( -10) ;
	#endif
	#elif  defined(__OpenBSD__)
		ioctl( STDIN_FILENO , WSDISPLAYIO_SMODE , &orig_console_mode) ;
	#else
		set_use_console_backscroll( 1) ;
	#endif

		if( CMAP_IS_INITED)
		{
			cmap_final() ;
		}

		if( _display.fd != STDIN_FILENO)
		{
			close( _display.fd) ;
		}

		munmap( _display.fb , _display.smem_len) ;
		close( _display.fb_fd) ;

		free( _disp.roots) ;

		/* DISP_IS_INITED is false from here. */
		_disp.display = NULL ;
	}

	return  1 ;
}

x_display_t **
x_get_opened_displays(
	u_int *  num
	)
{
	if( ! DISP_IS_INITED)
	{
		*num = 0 ;

		return  NULL ;
	}

	if( MOUSE_IS_INITED)
	{
		*num = 2 ;
	}
	else
	{
		*num = 1 ;
	}

	return  opened_disps ;
}

int
x_display_fd(
	x_display_t *  disp
	)
{
	return  disp->display->fd ;
}

int
x_display_show_root(
	x_display_t *  disp ,
	x_window_t *  root ,
	int  x ,
	int  y ,
	int  hint ,
	char *  app_name ,
	Window  parent_window	/* Ignored */
	)
{
	void *  p ;

	if( ( p = realloc( disp->roots , sizeof( x_window_t*) * (disp->num_of_roots + 1))) == NULL)
	{
	#ifdef  DEBUG
		kik_warn_printf( KIK_DEBUG_TAG " realloc failed.\n") ;
	#endif

		return  0 ;
	}

	disp->roots = p ;

	root->disp = disp ;
	root->parent = NULL ;
	root->parent_window = disp->my_window ;
	root->gc = disp->gc ;
	root->x = x ;
	root->y = y ;

	if( app_name)
	{
		root->app_name = app_name ;
	}

	disp->roots[disp->num_of_roots++] = root ;

	/* Cursor is drawn internally by calling x_display_put_image(). */
	if( ! x_window_show( root , hint))
	{
		return  0 ;
	}

	if( MOUSE_IS_INITED)
	{
		update_mouse_cursor_state() ;
		save_hidden_region() ;
		draw_mouse_cursor() ;
	}

	return  1 ;
}

int
x_display_remove_root(
	x_display_t *  disp ,
	x_window_t *  root
	)
{
	u_int  count ;

	for( count = 0 ; count < disp->num_of_roots ; count ++)
	{
		if( disp->roots[count] == root)
		{
			/* XXX x_window_unmap resize all windows internally. */
		#if  0
			x_window_unmap( root) ;
		#endif
			x_window_final( root) ;

			disp->num_of_roots -- ;

			if( count == disp->num_of_roots)
			{
				disp->roots[count] = NULL ;
			}
			else
			{
				disp->roots[count] = disp->roots[disp->num_of_roots] ;
			}

			return  1 ;
		}
	}

	return  0 ;
}

void
x_display_idling(
	x_display_t *  disp
	)
{
	u_int  count ;

	for( count = 0 ; count < disp->num_of_roots ; count ++)
	{
		x_window_idling( disp->roots[count]) ;
	}
}

int
x_display_receive_next_event(
	x_display_t *  disp
	)
{
	if( disp == &_disp_mouse)
	{
		return  receive_mouse_event() ;
	}
	else
	{
		return  receive_key_event() ;
	}
}


/*
 * Folloing functions called from x_window.c
 */

int
x_display_own_selection(
	x_display_t *  disp ,
	x_window_t *  win
	)
{
	return  0 ;
}

int
x_display_clear_selection(
	x_display_t *  disp ,
	x_window_t *  win
	)
{
	return  0 ;
}



XModifierKeymap *
x_display_get_modifier_mapping(
	x_display_t *  disp
	)
{
	return  disp->modmap.map ;
}

void
x_display_update_modifier_mapping(
	x_display_t *  disp ,
	u_int  serial
	)
{
	/* dummy */
}

XID
x_display_get_group_leader(
	x_display_t *  disp
	)
{
	return  None ;
}

int
x_display_reset_cmap(void)
{
	_display.prev_pixel = 0xff000000 ;
	_display.prev_closest_pixel = 0xff000000 ;

	return  _display.cmap && cmap_init()
	#ifdef  USE_GRF
		&& gpal_init( ((fb_reg_t*)_display.fb)->gpal)
	#endif
		;
}

void
x_display_set_use_ansi_colors(
	int  use
	)
{
	use_ansi_colors = use ;
}

void
x_display_set_cmap(
	u_int32_t *  pixels ,
	u_int  cmap_size
	)
{
	if( ! use_ansi_colors && cmap_size <= 16 && _disp.depth == 4)
	{
		u_int  count ;
		ml_color_t  color ;

		if( cmap_size < 16)
		{
			color = ML_RED ;
		}
		else
		{
			color = ML_BLACK ;
		}

		for( count = 0 ; count < cmap_size ; count++ , color++)
		{
			if( color == ML_WHITE && cmap_size < 15)
			{
				color ++ ;
			}

			_display.cmap->red[color] = (pixels[count] >> 16) & 0xff ;
			_display.cmap->green[color] = (pixels[count] >> 8) & 0xff ;
			_display.cmap->blue[color] = pixels[count] & 0xff ;
		}

		ioctl( _display.fb_fd , FBIOPUTCMAP , _display.cmap) ;
	}
}


u_long
x_display_get_pixel(
	int  x ,
	int  y
	)
{
	u_char *  fb ;
	u_long  pixel ;

	if( _display.pixels_per_byte > 1)
	{
		return  BG_MAGIC ;
	}

	fb = get_fb( x , y) ;

	switch( _display.bytes_per_pixel)
	{
	case 1:
		pixel = *fb ;
		break ;

	case 2:
		pixel = TOINT16(fb) ;
		break ;

	/* case 4: */
	default:
		pixel = TOINT32(fb) ;
	}

	return  pixel ;
}

void
x_display_put_image(
	int  x ,
	int  y ,
	u_char *  image ,
	size_t  size ,
	int  need_fb_pixel
	)
{
	if( _display.pixels_per_byte > 1)
	{
		put_image_124bpp( x , y , image , size , 1 , need_fb_pixel) ;
	}
	else
	{
		memcpy( get_fb( x , y) , image , size) ;
	}

	if( /* MOUSE_IS_INITED && */ _mouse.cursor.is_drawn &&
	    _mouse.cursor.y <= y &&
	    y < _mouse.cursor.y + _mouse.cursor.height)
	{
		if( x <= _mouse.cursor.x + _mouse.cursor.width &&
		    _mouse.cursor.x < x + size)
		{
			_mouse.hidden_region_saved = 0 ;
			draw_mouse_cursor_line( y - _mouse.cursor.y) ;
		}
	}
}

/* Check if bytes_per_pixel == 1 or not by the caller. */
void
x_display_fill_with(
	int  x ,
	int  y ,
	u_int  width ,
	u_int  height ,
	u_int8_t  pixel
	)
{
	u_char *  fb_orig ;
	u_char *  fb ;
	u_int  ppb ;
	u_char *  buf ;
	int  y_off ;

	fb_orig = fb = get_fb( x , y) ;

	if( ( ppb = _display.pixels_per_byte) > 1)
	{
		u_int  bpp ;
		int  plane ;
		u_char *  fb_end ;
		u_char *  buf_end ;
		u_int  surplus ;
		u_int  surplus_end ;
		int  packed_pixel ;
		u_int  count ;
		int  shift ;

		bpp = 8 / ppb ;
		plane = 0 ;
		fb_end = get_fb( x + width , y) ;

	#ifndef ENABLE_DOUBLE_BUFFER
		if( ! ( buf = alloca( fb_end - fb + 1)))
		{
			return ;
		}

		buf_end = buf + (fb_end - fb) ;
	#endif

		while( 1)
		{
		#ifdef  ENABLE_DOUBLE_BUFFER
			fb_end = _display.back_fb + ( fb_end - _display.fb) ;
		#endif

			surplus = MOD_PPB(x,ppb) ;
			surplus_end = MOD_PPB(x+width,ppb) ;

			packed_pixel = 0 ;
			if( pixel)
			{
				if( ppb == 8)
				{
					if( _disp.depth == 1 || PLANE(pixel))
					{
						packed_pixel = 0xff ;
					}
				}
				else
				{
					shift = _display.shift_0 ;

					for( count = 0 ; count < ppb ; count++)
					{
						packed_pixel |= (pixel << shift) ;
						FB_SHIFT_NEXT(shift,bpp) ;
					}
				}
			}

			for( y_off = 0 ; y_off < height ; y_off ++)
			{
				u_char *  buf_p ;
				u_int8_t  pix ;
				size_t  size ;

			#ifdef  ENABLE_DOUBLE_BUFFER
				buf = fb = _display.back_fb + ( fb - _display.fb) ;
				buf_end = fb_end ;
			#endif
				buf_p = buf ;

				shift = _display.shift_0 ;
				count = 0 ;
				pix = 0 ;

				if( surplus > 0)
				{
					for( ; count < surplus ; count++)
					{
						pix |= (fb[0] & (_display.mask << shift)) ;

						FB_SHIFT_NEXT(shift,bpp) ;
					}

					if( buf_p != buf_end)
					{
						if( pixel)
						{
							for( ; count < ppb ; count++)
							{
								pix |= (PLANE(pixel) << shift) ;

								FB_SHIFT_NEXT(shift,bpp) ;
							}
						}

						*(buf_p++) = pix ;

						shift = _display.shift_0 ;
						count = 0 ;
						pix = 0 ;
					}
				}

				if( surplus_end > 0)
				{
					if( pixel)
					{
						for( ; count < surplus_end ; count++)
						{
							pix |= (PLANE(pixel) << shift) ;

							FB_SHIFT_NEXT(shift,bpp) ;
						}
					}
					else
					{
						count = surplus_end ;
						shift = FB_SHIFT(ppb,bpp,surplus_end) ;
					}

					for( ; count < ppb ; count++)
					{
						pix |= (fb_end[0] & (_display.mask << shift)) ;

						FB_SHIFT_NEXT(shift,bpp) ;
					}

					*buf_end = pix ;

					shift = _display.shift_0 ;
					pix = 0 ;

					size = buf_end - buf + 1 ;
				}
				else
				{
					size = buf_end - buf ;
				}

				if( buf_p < buf_end)
				{
					/*
					 * XXX
					 * If ENABLE_DOUBLE_BUFFER is on, it is not necessary
					 * to memset every time because the pointer of buf
					 * points the same address.
					 */
					memset( buf_p , packed_pixel , buf_end - buf_p) ;
				}

			#ifdef  ENABLE_DOUBLE_BUFFER
				fb = _display.fb + ( fb - _display.back_fb) ;
			#endif

				memcpy( fb , buf , size) ;
				fb += _display.line_length ;
				fb_end += _display.line_length ;
			}

			if( ++plane < _disp.depth)
			{
				fb = fb_orig + _display.plane_len * plane ;
				fb_end = fb + (buf_end - buf) ;
			}
			else
			{
				break ;
			}
		}
	}
	else
	{
		if( ! ( buf = alloca( width)))
		{
			return ;
		}

		for( y_off = 0 ; y_off < height ; y_off ++)
		{
			memset( buf , pixel , width) ;
			memcpy( fb , buf , width) ;
			fb += _display.line_length ;
		}
	}
}

void
x_display_copy_lines(
	int  src_x ,
	int  src_y ,
	int  dst_x ,
	int  dst_y ,
	u_int  width ,
	u_int  height
	)
{
	u_char *  src ;
	u_char *  dst ;
	u_int  copy_len ;
	u_int  count ;
	int  plane ;

	/* XXX cheap implementation. */
	restore_hidden_region() ;

	/* XXX could be different from FB_WIDTH_BYTES(display, dst_x, width) */
	copy_len = FB_WIDTH_BYTES(&_display, src_x, width) ;

	for( plane = 0 ; plane < _disp.depth ; plane++)
	{
		if( src_y <= dst_y)
		{
			src = get_fb( src_x , src_y + height - 1) + _display.plane_len * plane ;
			dst = get_fb( dst_x , dst_y + height - 1) + _display.plane_len * plane ;

		#ifdef  ENABLE_DOUBLE_BUFFER
			if( _display.back_fb)
			{
				u_char *  src_back ;
				u_char *  dst_back ;

				src_back = _display.back_fb + (src - _display.fb) ;
				dst_back = _display.back_fb + (dst - _display.fb) ;

				if( dst_y == src_y)
				{
					for( count = 0 ; count < height ; count++)
					{
						memmove( dst_back , src_back , copy_len) ;
						memcpy( dst , src_back , copy_len) ;
						dst -= _display.line_length ;
						dst_back -= _display.line_length ;
						src_back -= _display.line_length ;
					}
				}
				else
				{
					for( count = 0 ; count < height ; count++)
					{
						memcpy( dst_back , src_back , copy_len) ;
						memcpy( dst , src_back , copy_len) ;
						dst -= _display.line_length ;
						dst_back -= _display.line_length ;
						src_back -= _display.line_length ;
					}
				}
			}
			else
		#endif
			{
				if( src_y == dst_y)
				{
					for( count = 0 ; count < height ; count++)
					{
						memmove( dst , src , copy_len) ;
						dst -= _display.line_length ;
						src -= _display.line_length ;
					}
				}
				else
				{
					for( count = 0 ; count < height ; count++)
					{
						memcpy( dst , src , copy_len) ;
						dst -= _display.line_length ;
						src -= _display.line_length ;
					}
				}
			}
		}
		else
		{
			src = get_fb( src_x , src_y) + _display.plane_len * plane ;
			dst = get_fb( dst_x , dst_y) + _display.plane_len * plane ;

		#ifdef  ENABLE_DOUBLE_BUFFER
			if( _display.back_fb)
			{
				u_char *  src_back ;
				u_char *  dst_back ;

				src_back = _display.back_fb + (src - _display.fb) ;
				dst_back = _display.back_fb + (dst - _display.fb) ;

				for( count = 0 ; count < height ; count++)
				{
					memcpy( dst_back , src_back , copy_len) ;
					memcpy( dst , src_back , copy_len) ;
					dst += _display.line_length ;
					dst_back += _display.line_length ;
					src_back += _display.line_length ;
				}
			}
			else
		#endif
			{
				for( count = 0 ; count < height ; count++)
				{
					memcpy( dst , src , copy_len) ;
					dst += _display.line_length ;
					src += _display.line_length ;
				}
			}
		}
	}
}

/* XXX for input method window */
int
x_display_check_visibility_of_im_window(void)
{
	static struct
	{
		int  saved ;
		int  x ;
		int  y ;
		u_int  width ;
		u_int  height ;

	} im_region ;
	int  redraw_im_win ;

	redraw_im_win = 0 ;

	if( _disp.num_of_roots == 2 && _disp.roots[1]->is_mapped)
	{
		if( im_region.saved)
		{
			if( im_region.x == _disp.roots[1]->x &&
			    im_region.y == _disp.roots[1]->y &&
			    im_region.width == ACTUAL_WIDTH(_disp.roots[1]) &&
			    im_region.height == ACTUAL_HEIGHT(_disp.roots[1]))
			{
				return  0 ;
			}

			expose_display( im_region.x , im_region.y ,
				im_region.width , im_region.height) ;

			redraw_im_win = 1 ;
		}

		im_region.saved = 1 ;
		im_region.x = _disp.roots[1]->x ;
		im_region.y = _disp.roots[1]->y ;
		im_region.width = ACTUAL_WIDTH(_disp.roots[1]) ;
		im_region.height = ACTUAL_HEIGHT(_disp.roots[1]) ;
	}
	else
	{
		if( im_region.saved)
		{
			expose_display( im_region.x , im_region.y ,
				im_region.width , im_region.height) ;
			im_region.saved = 0 ;
		}
	}

	return  redraw_im_win ;
}

/* seek the closest color */
int
x_cmap_get_closest_color(
	u_long *  closest ,
	int  red ,
	int  green ,
	int  blue
	)
{
	u_long  pixel ;
	u_int  color ;
	u_long  min = 0xffffff ;
	u_long  diff ;
	int  diff_r , diff_g , diff_b ;

	if( ! _display.cmap)
	{
		return  0 ;
	}

	/* 0xf8f8f8 ignores least significant 3bits */
	if( (((pixel = (red << 16) | (green << 8) | blue) ^ _display.prev_pixel)
	     & 0xfff8f8f8) == 0)
	{
		*closest = _display.prev_closest_pixel ;

		return  1 ;
	}

	_display.prev_pixel = pixel ;

	for( color = 0 ; color < CMAP_SIZE(_display.cmap) ; color++)
	{
	#ifdef  USE_GRF
		if( grf0_fd != -1 && ! use_tvram_cmap && color == TP_COLOR)
		{
			continue ;
		}
	#endif

		/* lazy color-space conversion */
		diff_r = red - WORD_COLOR_TO_BYTE(_display.cmap->red[color]) ;
		diff_g = green - WORD_COLOR_TO_BYTE(_display.cmap->green[color]) ;
		diff_b = blue - WORD_COLOR_TO_BYTE(_display.cmap->blue[color]) ;
		diff = diff_r * diff_r * 9 + diff_g * diff_g * 30 + diff_b * diff_b ;

		if( diff < min)
		{
			min = diff ;
			*closest = color ;

			/* no one may notice the difference (4[2^3/2]*4*9+4*4*30+4*4) */
			if( diff < 640)
			{
				break ;
			}
		}
	}

	_display.prev_closest_pixel = *closest ;

	return  1 ;
}

int
x_cmap_get_pixel_rgb(
	u_int8_t *  red ,
	u_int8_t *  green ,
	u_int8_t *  blue ,
	u_long  pixel
	)
{
	*red = WORD_COLOR_TO_BYTE(_display.cmap->red[pixel]) ;
	*green = WORD_COLOR_TO_BYTE(_display.cmap->green[pixel]) ;
	*blue = WORD_COLOR_TO_BYTE(_display.cmap->blue[pixel]) ;

	return  1 ;
}
