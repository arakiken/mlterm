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
#define PLANE(image)  (image)

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
#define PLANE(image)  (((image) >> plane) & 0x1)

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

/* Parameters of mouse cursor */
#define  MAX_CURSOR_SHAPE_WIDTH  15
#define  CURSOR_SHAPE_SIZE  (7*15*sizeof(u_int32_t))


/*
 * Note that this structure could be casted to Display.
 *
 * x, y, width and height are on the physical display.
 * (rotate_display is not considered.)
 */
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

	u_char  saved_image[CURSOR_SHAPE_SIZE] ;
	int  hidden_region_saved ;

} Mouse ;


/* --- static variables --- */

static Display  _display ;
static x_display_t  _disp ;
static Mouse  _mouse ;
static x_display_t  _disp_mouse ;
static x_display_t *  opened_disps[] = { &_disp , &_disp_mouse } ;
static int  use_ansi_colors = 1 ;
static int  rotate_display = 0 ;

static struct termios  orig_tm ;

static const char  cursor_shape_normal[] =
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
	"#######" ;

static const char  cursor_shape_rotate[] =
	"###         ###"
	"#*#         #*#"
	"#*###########*#"
	"#*************#"
	"#*###########*#"
	"#*#         #*#"
	"###         ###" ;

static struct cursor_shape
{
	const char *  shape ;
	u_int  width ;
	u_int  height ;
	int  x_off ;
	int  y_off ;

} cursor_shape =
{
	cursor_shape_normal ,
	 7 ,	/* width */
	15 ,	/* height */
	-3 ,	/* x_off */
	-7	/* y_off */
} ;


/* --- static functions --- */

/*
 * _disp.roots[1] is ignored.
 * x and y are rotated values.
 */
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

		#ifndef  ENABLE_2_4_PPB
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
		#endif
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
			#ifndef  ENABLE_2_4_PPB
				if( _disp.depth > 1)
				{
					goto  round_number_1 ;
				}
				else
			#endif
				{
					goto  round_number_2 ;
				}
			}

		#ifndef  ENABLE_2_4_PPB
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
		#endif
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
						_display.plane_offset[plane]] ;
			#else
				fb_pixel = get_fb( x + size , y)[_display.plane_offset[plane]] ;
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
			size_t  offset ;

			offset = _display.plane_offset[plane] - _display.plane_offset[plane - 1] ;
			fb += offset ;
		#ifdef  ENABLE_DOUBLE_BUFFER
			if( write_back_fb)
			{
				new_image += offset ;
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
rotate_mouse_cursor_shape(void)
{
	int  tmp ;

	cursor_shape.shape = (cursor_shape.shape == cursor_shape_normal) ?
				cursor_shape_rotate : cursor_shape_normal ;

	tmp = cursor_shape.x_off ;
	cursor_shape.x_off = cursor_shape.y_off ;
	cursor_shape.y_off = tmp ;

	tmp = cursor_shape.width ;
	cursor_shape.width = cursor_shape.height ;
	cursor_shape.height = tmp ;
}

static void
update_mouse_cursor_state(void)
{
	if( -cursor_shape.x_off > _mouse.x)
	{
		_mouse.cursor.x = 0 ;
		_mouse.cursor.x_off = -cursor_shape.x_off - _mouse.x ;
	}
	else
	{
		_mouse.cursor.x = _mouse.x + cursor_shape.x_off ;
		_mouse.cursor.x_off = 0 ;
	}

	_mouse.cursor.width = cursor_shape.width - _mouse.cursor.x_off ;
	if( _mouse.cursor.x + _mouse.cursor.width > _display.width)
	{
		_mouse.cursor.width -=
			(_mouse.cursor.x + _mouse.cursor.width - _display.width) ;
	}

	if( -cursor_shape.y_off > _mouse.y)
	{
		_mouse.cursor.y = 0 ;
		_mouse.cursor.y_off = -cursor_shape.y_off - _mouse.y ;
	}
	else
	{
		_mouse.cursor.y = _mouse.y + cursor_shape.y_off ;
		_mouse.cursor.y_off = 0 ;
	}

	_mouse.cursor.height = cursor_shape.height - _mouse.cursor.y_off ;
	if( _mouse.cursor.y + _mouse.cursor.height > _display.height)
	{
		_mouse.cursor.height -=
			(_mouse.cursor.y + _mouse.cursor.height - _display.height) ;
	}
}

static void
restore_hidden_region(void)
{
	if( ! _mouse.cursor.is_drawn)
	{
		return ;
	}

	/* Set 0 before window_exposed is called. */
	_mouse.cursor.is_drawn = 0 ;

	if( _mouse.hidden_region_saved)
	{
		size_t  width ;
		u_char *  saved ;
		int  plane ;
		int  num_of_planes ;
		u_char *  fb ;
		u_int  count ;

		width = FB_WIDTH_BYTES(&_display, _mouse.cursor.x, _mouse.cursor.width) ;
		saved = _mouse.saved_image ;

		if( _display.pixels_per_byte == 8)
		{
			num_of_planes = _disp.depth ;
		}
		else
		{
			num_of_planes = 1 ;
		}

		for( plane = 0 ; plane < num_of_planes ; plane++)
		{
			fb = get_fb(_mouse.cursor.x , _mouse.cursor.y) +
				_display.plane_offset[plane] ;

			for( count = 0 ; count < _mouse.cursor.height ; count++)
			{
				memcpy( fb , saved , width) ;
				saved += width ;
				fb += _display.line_length ;
			}
		}
	}
	else
	{
		x_window_t *  win ;
		int  cursor_x ;
		int  cursor_y ;
		u_int  cursor_height ;
		u_int  cursor_width ;

		if( rotate_display)
		{
			win = get_window( _mouse.y , _display.width - _mouse.x - 1) ;

			if( rotate_display > 0)
			{
				cursor_x = _mouse.cursor.y ;
				cursor_y = _display.width - _mouse.cursor.x - _mouse.cursor.width ;
			}
			else
			{
				cursor_x = _display.height - _mouse.cursor.y -
						_mouse.cursor.height ;
				cursor_y = _mouse.cursor.x ;
			}

			cursor_width = _mouse.cursor.height ;
			cursor_height = _mouse.cursor.width ;
		}
		else
		{
			win = get_window( _mouse.x , _mouse.y) ;

			cursor_x = _mouse.cursor.x ;
			cursor_y = _mouse.cursor.y ;
			cursor_width = _mouse.cursor.width ;
			cursor_height = _mouse.cursor.height ;
		}

		if( win->window_exposed)
		{
			(*win->window_exposed)( win ,
				cursor_x - win->x - win->hmargin ,
				cursor_y - win->y - win->vmargin ,
				cursor_width ,
				cursor_height) ;
		}
	}
}

static void
save_hidden_region(void)
{
	size_t  width ;
	u_char *  saved ;
	int  plane ;
	int  num_of_planes ;
	u_char *  fb ;
	u_int  count ;

	width = FB_WIDTH_BYTES(&_display, _mouse.cursor.x, _mouse.cursor.width) ;
	saved = _mouse.saved_image ;

	if( _display.pixels_per_byte == 8)
	{
		num_of_planes = _disp.depth ;
	}
	else
	{
		num_of_planes = 1 ;
	}

	for( plane = 0 ; plane < num_of_planes ; plane++)
	{
		fb = get_fb( _mouse.cursor.x , _mouse.cursor.y) + _display.plane_offset[plane] ;

		for( count = 0 ; count < _mouse.cursor.height ; count++)
		{
			memcpy( saved , fb , width) ;
			fb += _display.line_length ;
			saved += width ;
		}
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
	u_char  image[MAX_CURSOR_SHAPE_WIDTH * sizeof(u_int32_t)] ;
	int  x ;

	fb = get_fb( _mouse.cursor.x , _mouse.cursor.y + y) ;

	if( rotate_display)
	{
		if( rotate_display > 0)
		{
			win = get_window( _mouse.y , _display.width - _mouse.x - 1) ;
		}
		else
		{
			win = get_window( _display.height - _mouse.y - 1 , _mouse.x) ;
		}
	}
	else
	{
		win = get_window( _mouse.x , _mouse.y) ;
	}

	shape = cursor_shape.shape +
		((_mouse.cursor.y_off + y) * cursor_shape.width + _mouse.cursor.x_off) ;

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
				int  tmp ;
			case  1:
				tmp = rotate_display ;
				rotate_display = 0 ;
				image[x] = x_display_get_pixel(
						_mouse.cursor.x + x ,
						_mouse.cursor.y + y) ;
				rotate_display = tmp ;
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

static int
check_visibility_of_im_window(void)
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

			if( im_region.x < _disp.roots[1]->x ||
			    im_region.y < _disp.roots[1]->y ||
			    im_region.x + im_region.width >
				_disp.roots[1]->x + ACTUAL_WIDTH(_disp.roots[1]) ||
			    im_region.y + im_region.height >
				_disp.roots[1]->y + ACTUAL_HEIGHT(_disp.roots[1]))
			{
				expose_display( im_region.x , im_region.y ,
					im_region.width , im_region.height) ;
				redraw_im_win = 1 ;
			}
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

static void
receive_event_for_multi_roots(
	XEvent *  xev
	)
{
	int  redraw_im_win ;

	if( ( redraw_im_win = check_visibility_of_im_window()))
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
	else if( ! check_visibility_of_im_window())
	{
		return ;
	}

	expose_window( _disp.roots[1] , _disp.roots[1]->x , _disp.roots[1]->y ,
				ACTUAL_WIDTH(_disp.roots[1]) , ACTUAL_HEIGHT(_disp.roots[1])) ;
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

	if( rotate_display || ! ( ret = x_is_virtual_kbd_event( &_disp , bev)))
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

		if( ! ( _display.color_cache = calloc( 1 , sizeof(*_display.color_cache))))
		{
			free( _display.cmap_orig) ;
			free( _display.cmap) ;

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

	/* Same processing as x_display_set_cmap(). */
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
	free( _display.color_cache) ;
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

		if( rotate_display)
		{
			u_int  tmp ;

			if( _display.bytes_per_pixel < 2)
			{
				rotate_display = 0 ;
				rotate_mouse_cursor_shape() ;
			}
			else
			{
				tmp = _disp.width ;
				_disp.width = _disp.height ;
				_disp.height = tmp ;
			}
		}

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
	if( _display.color_cache)
	{
		memset( _display.color_cache , 0 , sizeof(*_display.color_cache)) ;
	}

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
x_display_enable_to_change_cmap(
	int  flag
	)
{
	if( flag)
	{
	#ifdef  USE_GRF
		x68k_set_use_tvram_colors( 1) ;
	#endif

		if( ! use_ansi_colors)
		{
			use_ansi_colors = -1 ;
		}
	}
	else
	{
	#ifdef  USE_GRF
		x68k_set_use_tvram_colors( 0) ;
	#endif

		if( use_ansi_colors == -1)
		{
			use_ansi_colors = 0 ;
		}
	}
}

void
x_display_set_cmap(
	u_int32_t *  pixels ,
	u_int  cmap_size
	)
{
	if(
	#ifdef  USE_GRF
	    ! x68k_set_tvram_cmap( pixels , cmap_size) &&
	#endif
	    use_ansi_colors == -1 && cmap_size <= 16 && _disp.depth == 4)
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

		/* Same processing as cmap_init(). */
	#ifndef  USE_GRF
		ioctl( _display.fb_fd , FBIOPUTCMAP , _display.cmap) ;
	#else
		for( count = 0 ; count < CMAP_SIZE(_display.cmap) ; count++)
		{
			((fb_reg_t*)_display.fb)->gpal[count] =
				(_display.cmap->red[count] >> 3) << 6 |
				(_display.cmap->green[count] >> 3) << 11 |
				(_display.cmap->blue[count] >> 3) << 1 ;
		}

		gpal_init( ((fb_reg_t*)_display.fb)->gpal) ;
	#endif

		if( _display.color_cache)
		{
			memset( _display.color_cache , 0 , sizeof(*_display.color_cache)) ;
		}

		kik_msg_printf( "Palette changed.\n") ;
	}
}

void
x_display_rotate(
	int  rotate	/* 1: clockwise, -1: counterclockwise */
	)
{
	if( rotate == rotate_display)
	{
		return ;
	}

	if( rotate)
	{
		if( DISP_IS_INITED)
		{
			if( _display.bytes_per_pixel < 2)
			{
				/* rotate is available for 16 or more bpp */

				return ;
			}

			/* virual kbd is unavailable on the rotated display. */
			x_virtual_kbd_hide() ;
		}
	}

	if( rotate_display + rotate != 0)
	{
		int  tmp ;

		tmp = _disp.width ;
		_disp.width = _disp.height ;
		_disp.height = tmp ;

		rotate_mouse_cursor_shape() ;

		rotate_display = rotate ;

		if( _disp.num_of_roots > 0)
		{
			x_window_resize_with_margin( _disp.roots[0] ,
				_disp.width , _disp.height , NOTIFY_TO_MYSELF) ;
		}
	}
	else
	{
		/* If rotate_display == -1 rotate == 1 or vice versa, don't swap. */

		rotate_display = rotate ;

		if( _disp.num_of_roots > 0)
		{
			x_window_update_all( _disp.roots[0]) ;
		}
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

	if( rotate_display)
	{
		int  tmp ;

		if( rotate_display > 0)
		{
			tmp = x ;
			x = _disp.height - y - 1 ;
			y = tmp ;
		}
		else
		{
			tmp = x ;
			x = y ;
			y = _disp.width - tmp - 1 ;
		}
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
	else if( ! rotate_display)
	{
		memcpy( get_fb( x , y) , image , size) ;
	}
	else
	{
		/* Display is rotated. */

		u_char *  fb ;
		int  tmp ;
		int  line_length ;
		size_t  count ;

		tmp = x ;
		if( rotate_display > 0)
		{
			x = _disp.height - y - 1 ;
			y = tmp ;
			line_length = _display.line_length ;
		}
		else
		{
			x = y ;
			y = _disp.width - tmp - 1 ;
			line_length = - _display.line_length ;
		}

		fb = get_fb( x , y) ;

		if( _display.bytes_per_pixel == 2)
		{
			size /= 2 ;
			for( count = 0 ; count < size ; count++)
			{
				*((u_int16_t*)fb) = ((u_int16_t*)image)[count] ;
				fb += line_length ;
			}
		}
		else /* if( _display.bytes_per_pixel == 4) */
		{
			size /= 4 ;
			for( count = 0 ; count < size ; count++)
			{
				*((u_int32_t*)fb) = ((u_int32_t*)image)[count] ;
				fb += line_length ;
			}
		}

		if( rotate_display < 0)
		{
			y -= (size - 1) ;
		}

		if( /* MOUSE_IS_INITED && */ _mouse.cursor.is_drawn &&
		    _mouse.cursor.x <= x &&
		    x < _mouse.cursor.x + _mouse.cursor.width)
		{
			if( y <= _mouse.cursor.y + _mouse.cursor.height &&
			    _mouse.cursor.y < y + size)
			{
				_mouse.hidden_region_saved = 0 ;
				/* draw_mouse_cursor() */
			}
		}

		return ;
	}

	if( /* MOUSE_IS_INITED && */ _mouse.cursor.is_drawn &&
	    _mouse.cursor.y <= y &&
	    y < _mouse.cursor.y + _mouse.cursor.height)
	{
		size /= _display.bytes_per_pixel ;

		if( x <= _mouse.cursor.x + _mouse.cursor.width &&
		    _mouse.cursor.x < x + size)
		{
			_mouse.hidden_region_saved = 0 ;
			draw_mouse_cursor_line( y - _mouse.cursor.y) ;
		}
	}
}

/*
 * For 8 or less bpp.
 * Check if bytes_per_pixel == 1 or not by the caller.
 */
void
x_display_fill_with(
	int  x ,
	int  y ,
	u_int  width ,
	u_int  height ,
	u_int8_t  pixel
	)
{
	u_char *  fb ;
	u_int  ppb ;
	u_char *  buf ;
	int  y_off ;

	fb = get_fb( x , y) ;

	if( ( ppb = _display.pixels_per_byte) > 1)
	{
		u_char *  fb_orig ;
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
		fb_orig = fb ;
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
					 * If ENABLE_DOUBLE_BUFFER is off, it is not necessary
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

		#ifndef  ENABLE_2_4_PPB
			if( ++plane < _disp.depth)
			{
				fb = fb_orig + _display.plane_offset[plane] ;
				fb_end = fb + (buf_end - buf) ;
			}
			else
		#endif
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
	int  num_of_planes ;
	int  plane ;

	/* XXX cheap implementation. */
	restore_hidden_region() ;

	if( rotate_display)
	{
		int  tmp ;

		if( rotate_display > 0)
		{
			tmp = src_x ;
			src_x = _disp.height - src_y - height ;
			src_y = tmp ;

			tmp = dst_x ;
			dst_x = _disp.height - dst_y - height ;
			dst_y = tmp ;
		}
		else
		{
			tmp = src_x ;
			src_x = src_y ;
			src_y = _disp.width - tmp - width ;

			tmp = dst_x ;
			dst_x = dst_y ;
			dst_y = _disp.width - tmp - width ;
		}

		tmp = height ;
		height = width ;
		width = tmp ;
	}

	/* XXX could be different from FB_WIDTH_BYTES(display, dst_x, width) */
	copy_len = FB_WIDTH_BYTES(&_display, src_x, width) ;

	if( _display.pixels_per_byte == 8)
	{
		num_of_planes = _disp.depth ;
	}
	else
	{
		num_of_planes = 1 ;
	}

	for( plane = 0 ; plane < num_of_planes ; plane++)
	{
		if( src_y <= dst_y)
		{
			src = get_fb( src_x , src_y + height - 1) + _display.plane_offset[plane] ;
			dst = get_fb( dst_x , dst_y + height - 1) + _display.plane_offset[plane] ;

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
			src = get_fb( src_x , src_y) + _display.plane_offset[plane] ;
			dst = get_fb( dst_x , dst_y) + _display.plane_offset[plane] ;

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
void
x_display_reset_input_method_window(void)
{
#if  0
	if( _disp.num_of_roots == 2 && _disp.roots[1]->is_mapped)
#endif
	{
		check_visibility_of_im_window() ;
		x_window_clear_margin_area( _disp.roots[1]) ;
	}
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
	u_int  segment ;
	u_int  offset ;
	u_int  color ;
	u_long  min = 0xffffff ;
	u_long  diff ;
	int  diff_r , diff_g , diff_b ;

	if( ! _display.cmap)
	{
		return  0 ;
	}

#ifdef  LARGE_CACHE
	segment = 0 ;
	/*
	 * R        G        B
	 * 11111111 11111111 11111111
	 * ^^^^^    ^^^^^    ^^^^
	 */
	offset = ((red << 6) & 0x3e00) | ((green << 1) & 0x1f0) | ((blue >> 4) & 0xf) ;
#else
	/*
	 * R        G        B
	 * 11111111 11111111 11111111
	 * ^        ^        ^
	 */
	segment = ((red >> 5) & 0x4) | ((green >> 6) & 0x2) | ((blue >> 7) & 0x1) ;
	/*
	 * R        G        B
	 * 11111111 11111111 11111111
	 *  ^^^^     ^^^^     ^^^
	 */
	offset = ((red << 4) & 0x780) | (green & 0x78) | ((blue >> 4) & 0x7) ;
#endif

	if( _display.color_cache->segments[offset] == (segment|0x80))
	{
		*closest = _display.color_cache->pixels[offset] ;
	#ifdef  __DEBUG
		kik_debug_printf( "CACHED PIXEL %x <= r%x g%x b%x segment %x offset %x\n" ,
			*closest , red , green , blue , segment , offset) ;
	#endif

		return  1 ;
	}

	for( color = 0 ; color < CMAP_SIZE(_display.cmap) ; color++)
	{
	#ifdef  USE_GRF
		if( grf0_fd != -1 && ! use_tvram_cmap && color == TP_COLOR)
		{
			continue ;
		}
	#endif

		/* lazy color-space conversion */
		diff_r = (red - WORD_COLOR_TO_BYTE(_display.cmap->red[color])) / 8 ;
		diff_g = (green - WORD_COLOR_TO_BYTE(_display.cmap->green[color])) / 8 ;
		diff_b = (blue - WORD_COLOR_TO_BYTE(_display.cmap->blue[color])) / 8 ;
		diff = diff_r * diff_r * 9 + diff_g * diff_g * 30 + diff_b * diff_b ;

		if( diff < min)
		{
			min = diff ;
			*closest = color ;

			/* no one may notice the difference (4[2^3/2]*4*9+4*4*30+4*4)/(8^2) */
			if( diff < 10)
			{
				break ;
			}
		}
	}

	_display.color_cache->pixels[offset] = *closest ;
	_display.color_cache->segments[offset] = (segment|0x80) ;

#ifdef  __DEBUG
	kik_debug_printf( "NEW PIXEL %x <= r%x g%x b%x segment %x offset %x\n" ,
		*closest , red , green , blue , segment , offset) ;
#endif

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
