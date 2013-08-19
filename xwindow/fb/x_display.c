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

#if  defined(__FreeBSD__)
#include  <sys/consio.h>
#include  <sys/mouse.h>
#include  <sys/time.h>
#elif  defined(__NetBSD__) || defined(__OpenBSD__)
#include  <sys/param.h>				/* MACHINE */
#if  _MACHINE == x68k
#include  <machine/grfioctl.h>
#endif
#include  <dev/wscons/wsdisplay_usl_io.h>	/* VT_GETSTATE */
#include  "../x_event_source.h"
#else
#include  <linux/kd.h>
#include  <linux/keyboard.h>
#include  <linux/vt.h>	/* VT_GETSTATE */
#endif

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

#if  0
#define  READ_CTRL_KEYMAP
#endif

/* Enable doube buffering on 1, 2 or 4 bpp */
#if  1
#define  ENABLE_DOUBLE_BUFFER
#endif

#if  defined(__FreeBSD__)
#define  SYSMOUSE_PACKET_SIZE  8
#elif  defined(__NetBSD__)
#define  KEY_REPEAT_UNIT  25	/* msec (see x_event_source.c) */
#define  DEFAULT_KEY_REPEAT_1  400	/* msec */
#define  DEFAULT_KEY_REPEAT_N  50	/* msec */
#endif

#if  defined(__FreeBSD__) || defined(__NetBSD__) || defined(__OpenBSD__)
#define  CMAP_SIZE(cmap)            ((cmap)->count)
#define  BYTE_COLOR_TO_WORD(color)  (color)
#define  WORD_COLOR_TO_BYTE(color)  (color)
#else	/* Linux */
#define  CMAP_SIZE(cmap)            ((cmap)->len)
#define  BYTE_COLOR_TO_WORD(color)  ((color) << 8 | (color))
#define  WORD_COLOR_TO_BYTE(color)  ((color) & 0xff)
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

#if  _MACHINE == x68k
typedef struct  fb_reg
{
	/* CRT controller */
	struct
	{
		u_short  r00 , r01 , r02 , r03 , r04 , r05 , r06 , r07 ;
		u_short  r08 , r09 , r10 , r11 , r12 , r13 , r14 , r15 ;
		u_short  r16 , r17 , r18 , r19 , r20 , r21 , r22 , r23 ;
		char  pad0[0x450] ;
		u_short  ctrl ;
		char  pad1[0x1b7e] ;

	} crtc ;

	u_short gpal[256] ;	/* graphic palette */
	u_short tpal[256] ;	/* text palette */

	/* video controller */
	struct
	{
		u_short  r0 ;
		char  pad0[0xfe] ;
		u_short  r1 ;
		char  pad1[0xfe] ;
		u_short  r2 ;
		char  pad2[0x19fe] ;

	} videoc ;

	u_short  pad0[0xa000] ;

	/* system port */
	struct
	{
		u_short  r1 , r2 , r3 , r4 ;
		u_short  pad0[2] ;
		u_short  r5 , r6 ;
		u_short  pad[0x1ff0] ;

	} sysport;

} fb_reg_t ;

typedef struct  fb_reg_conf
{
	struct
	{
		u_short  r00 , r01 , r02 , r03 , r04 , r05 , r06 , r07 , r08 , r20 ;

	} crtc ;

	struct
	{
		u_short  r0 , r1 , r2 ;

	} videoc ;

} fb_reg_conf_t ;

#endif


/* --- static variables --- */

static Display  _display ;
static x_display_t  _disp ;
static Mouse  _mouse ;
static x_display_t  _disp_mouse ;
static x_display_t *  opened_disps[] = { &_disp , &_disp_mouse } ;

#if  defined(__FreeBSD__)
static keymap_t  keymap ;
#elif  defined(__NetBSD__) || defined(__OpenBSD__)
static u_int  kbd_type ;
static struct wskbd_map_data  keymap ;
static int  console_id = -1 ;
static int  orig_console_mode = WSDISPLAYIO_MODE_EMUL ;	/* 0 */
static struct wscons_event  prev_key_event ;
u_int  fb_width = 640 ;
u_int  fb_height = 480 ;
u_int  fb_depth = 8 ;
#ifdef  __NetBSD__
static int  wskbd_repeat_wait = (DEFAULT_KEY_REPEAT_1 + KEY_REPEAT_UNIT - 1) / KEY_REPEAT_UNIT ;
int  wskbd_repeat_1 = DEFAULT_KEY_REPEAT_1 ;
int  wskbd_repeat_N = DEFAULT_KEY_REPEAT_N ;
#endif
#if  _MACHINE == x68k
static fb_reg_conf_t  orig_reg ;
#endif
#else	/* Linux */
static int  console_id = -1 ;
#endif

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
	#if  ! defined(_MACHINE) || _MACHINE != x68k
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

	#if  defined(__FreeBSD__) || defined(__NetBSD__) || defined(__OpenBSD__)
		_display.cmap->red[color] = r ;
		_display.cmap->green[color] = g ;
		_display.cmap->blue[color] = b ;
	#else
		_display.cmap->red[color] = BYTE_COLOR_TO_WORD(r) ;
		_display.cmap->green[color] = BYTE_COLOR_TO_WORD(g) ;
		_display.cmap->blue[color] = BYTE_COLOR_TO_WORD(b) ;
	#endif
	}

#if  ! defined(_MACHINE) || _MACHINE != x68k
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
	#if ! defined(_MACHINE) || _MACHINE != x68k
		ioctl( _display.fb_fd , FBIOPUTCMAP , _display.cmap_orig) ;
	#else
		memcpy( ((fb_reg_t*)_display.fb)->gpal , _display.cmap_orig ,
			sizeof(((fb_reg_t*)_display.fb)->gpal)) ;
	#endif

		free( _display.cmap_orig) ;
	}

	free( _display.cmap) ;
}

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
			goto  round_number ;
		}

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

			round_number:
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

		if( shift != _display.shift_0)
		{
			u_char  fb_pixel ;

		#ifdef  ENABLE_DOUBLE_BUFFER
			fb_pixel = _display.back_fb[get_fb( x + size , y) - _display.fb] ;
		#else
			fb_pixel = get_fb( x + size , y)[0] ;
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
				_mouse.cursor.x - win->x - win->margin ,
				_mouse.cursor.y - win->y - win->margin ,
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
	if( x < win->x + win->margin || y < win->y + win->margin ||
	    x - win->x + width > win->margin + win->width ||
	    y - win->y + height > win->margin + win->height)
	{
		x_window_clear_margin_area( win) ;
	}

	if( win->window_exposed)
	{
		(*win->window_exposed)( win , x - win->x , y - win->y , width , height) ;
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

#ifndef  __FreeBSD__

#if  defined(__NetBSD__) || defined(__OpenBSD__)
#define get_key_state()  (0)
#else
static int get_key_state(void) ;
#endif

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
#endif

#if  defined(__FreeBSD__)

static int
open_display(void)
{
	char *  dev ;
	int  vmode ;
	video_info_t  vinfo ;
	video_adapter_info_t  vainfo ;
	video_display_start_t  vstart ;
	struct termios  tm ;

	if( ! ( _display.fb_fd = open( ( dev = getenv("FRAMEBUFFER")) ?
						dev : "/dev/ttyv0" ,
					O_RDWR)))
	{
		kik_msg_printf( "Couldn't open %s.\n" , dev ? dev : "/dev/ttyv0") ;

		return  0 ;
	}

	kik_file_set_cloexec( _display.fb_fd) ;

	ioctl( _display.fb_fd , FBIO_GETMODE , &vmode) ;

	vinfo.vi_mode = vmode ;
	ioctl( _display.fb_fd , FBIO_MODEINFO , &vinfo) ;
	ioctl( _display.fb_fd , FBIO_ADPINFO , &vainfo) ;
	ioctl( _display.fb_fd , FBIO_GETDISPSTART , &vstart) ;

	if( ( _disp.depth = vinfo.vi_depth) < 8)
	{
	#if  0
		/* 1/2/4 bpp is not supported. */
		kik_msg_printf( "%d bpp is not supported.\n" , vinfo.vi_depth) ;

		goto  error ;
	#endif
	}

	if( ( _display.fb = mmap( NULL , (_display.smem_len = vainfo.va_window_size) ,
				PROT_WRITE|PROT_READ , MAP_SHARED , _display.fb_fd , (off_t)0))
				== MAP_FAILED)
	{
		kik_msg_printf( "Retry another mode of resolution and depth.\n") ;

		goto  error ;
	}

	if( ( _display.bytes_per_pixel = (_disp.depth + 7) / 8) == 3)
	{
		_display.bytes_per_pixel = 4 ;
	}

	if( _disp.depth < 15)
	{
		if( _disp.depth < 8)
		{
		#ifdef  ENABLE_2_4_PPB
			_display.pixels_per_byte = 8 / _disp.depth ;
		#else
			/* XXX Forcibly set 1 bpp */
			_display.pixels_per_byte = 8 ;
			_disp.depth = 1 ;
		#endif

			_display.shift_0 = FB_SHIFT_0(_display.pixels_per_byte,_disp.depth) ;
			_display.mask = FB_MASK(_display.pixels_per_byte) ;
		}
		else
		{
			_display.pixels_per_byte = 1 ;
		}

		if( ! cmap_init())
		{
			goto  error ;
		}
	}
	else
	{
		_display.pixels_per_byte = 1 ;
	}

#ifdef  ENABLE_DOUBLE_BUFFER
	if( _display.pixels_per_byte > 1 &&
	    ! ( _display.back_fb = malloc( _display.smem_len)))
	{
		goto  error ;
	}
#endif

	_display.line_length = vainfo.va_line_width ;
	_display.xoffset = vstart.x ;
	_display.yoffset = vstart.y ;

	_display.width = _disp.width = vinfo.vi_width ;
	_display.height = _disp.height = vinfo.vi_height ;

	_display.rgbinfo.r_limit = 8 - vinfo.vi_pixel_fsizes[0] ;
	_display.rgbinfo.g_limit = 8 - vinfo.vi_pixel_fsizes[1] ;
	_display.rgbinfo.b_limit = 8 - vinfo.vi_pixel_fsizes[2] ;
	_display.rgbinfo.r_offset = vinfo.vi_pixel_fields[0] ;
	_display.rgbinfo.g_offset = vinfo.vi_pixel_fields[1] ;
	_display.rgbinfo.b_offset = vinfo.vi_pixel_fields[2] ;

	tcgetattr( STDIN_FILENO , &tm) ;
	orig_tm = tm ;
	tm.c_iflag = tm.c_oflag = 0 ;
	tm.c_cflag &= ~CSIZE ;
	tm.c_cflag |= CS8 ;
	tm.c_lflag &= ~(ECHO|ISIG|IEXTEN|ICANON) ;
	tm.c_cc[VMIN] = 1 ;
	tm.c_cc[VTIME] = 0 ;
	tcsetattr( STDIN_FILENO , TCSAFLUSH , &tm) ;

	ioctl( STDIN_FILENO , GIO_KEYMAP , &keymap) ;
	ioctl( STDIN_FILENO , KDSKBMODE , K_CODE) ;
	ioctl( STDIN_FILENO , KDGKBSTATE , &_display.lock_state) ;

	_display.fd = STDIN_FILENO ;

	_disp.display = &_display ;

	kik_priv_restore_euid() ;
	kik_priv_restore_egid() ;
	_mouse.fd = open( "/dev/sysmouse" , O_RDWR|O_NONBLOCK) ;
	kik_priv_change_euid( kik_getuid()) ;
	kik_priv_change_egid( kik_getgid()) ;

	if( _mouse.fd != -1)
	{
		int  level ;
		mousemode_t  mode ;
		struct mouse_info  info ;

		level = 1 ;
		ioctl( _mouse.fd , MOUSE_SETLEVEL , &level) ;
		ioctl( _mouse.fd , MOUSE_GETMODE , &mode) ;

		if( mode.packetsize != SYSMOUSE_PACKET_SIZE)
		{
		#ifdef  DEBUG
			kik_debug_printf( KIK_DEBUG_TAG " Failed to open /dev/sysmouse.\n") ;
		#endif

			close( _mouse.fd) ;
			_mouse.fd = -1 ;
		}
		else
		{
			kik_file_set_cloexec( _mouse.fd) ;

			_mouse.x = _display.width / 2 ;
			_mouse.y = _display.height / 2 ;
			_disp_mouse.display = (Display*)&_mouse ;

			tcgetattr( _mouse.fd , &tm) ;
			tm.c_iflag = IGNBRK|IGNPAR;
			tm.c_oflag = 0 ;
			tm.c_lflag = 0 ;
			tm.c_cc[VTIME] = 0 ;
			tm.c_cc[VMIN] = 1 ;
			tm.c_cflag = CS8 | CSTOPB | CREAD | CLOCAL | HUPCL ;
			cfsetispeed( &tm , B1200) ;
			cfsetospeed( &tm , B1200) ;
			tcsetattr( _mouse.fd , TCSAFLUSH , &tm) ;

			info.operation = MOUSE_HIDE ;
			ioctl( STDIN_FILENO , CONS_MOUSECTL , &info) ;
		}
	}
#ifdef  DEBUG
	else
	{
		kik_debug_printf( KIK_DEBUG_TAG " Failed to open /dev/sysmouse.\n") ;
	}
#endif

	return  1 ;

error:
	if( _display.fb)
	{
		munmap( _display.fb , _display.smem_len) ;
		_display.fb = NULL ;
	}

	close( _display.fb_fd) ;

	return  0 ;
}

static int
receive_mouse_event(void)
{
	u_char  buf[64] ;
	ssize_t  len ;

	while( ( len = read( _mouse.fd , buf , sizeof(buf))) > 0)
	{
		static u_char  packet[SYSMOUSE_PACKET_SIZE] ;
		static ssize_t  packet_len ;
		ssize_t  count ;

		for( count = 0 ; count < len ; count++)
		{
			int  x ;
			int  y ;
			int  z ;
			int  move ;
			struct timeval  tv ;
			XButtonEvent  xev ;
			x_window_t *  win ;

			if( packet_len == 0)
			{
				if( (buf[count] & 0xf8) != 0x80)
				{
					/* is not packet header */
					continue ;
				}
			}

			packet[packet_len++] = buf[count] ;

			if( packet_len < SYSMOUSE_PACKET_SIZE)
			{
				continue ;
			}

			packet_len = 0 ;

			/* set mili seconds */
			gettimeofday( &tv , NULL) ;
			xev.time = tv.tv_sec * 1000 + tv.tv_usec / 1000 ;

			move = 0 ;

			if( ( x = (char)packet[1] + (char)packet[3]) != 0)
			{
				restore_hidden_region() ;

				_mouse.x += x ;

				if( _mouse.x < 0)
				{
					_mouse.x = 0 ;
				}
				else if( _display.width <= _mouse.x)
				{
					_mouse.x = _display.width - 1 ;
				}

				move = 1 ;
			}

			if( ( y = (char)packet[2] + (char)packet[4]) != 0)
			{
				restore_hidden_region() ;

				_mouse.y -= y ;

				if( _mouse.y < 0)
				{
					_mouse.y = 0 ;
				}
				else if( _display.height <= _mouse.y)
				{
					_mouse.y = _display.height - 1 ;
				}

				move = 1 ;
			}

			z = ((char)(packet[5] << 1) + (char)(packet[6] << 1)) >> 1 ;

			if( move)
			{
				update_mouse_cursor_state() ;
			}

			if( ~packet[0] & 0x04)
			{
				xev.button = Button1 ;
				_mouse.button_state = Button1Mask ;
			}
			else if( ~packet[0] & 0x02)
			{
				xev.button = Button2 ;
				_mouse.button_state = Button2Mask ;
			}
			else if( ~packet[0] & 0x01)
			{
				xev.button = Button3 ;
				_mouse.button_state = Button3Mask ;
			}
			else if( z < 0)
			{
				xev.button = Button4 ;
				_mouse.button_state = Button4Mask ;
			}
			else if( z > 0)
			{
				xev.button = Button5 ;
				_mouse.button_state = Button5Mask ;
			}
			else
			{
				xev.button = 0 ;
			}

			if( move)
			{
				xev.type = MotionNotify ;
				xev.state = _mouse.button_state | _display.key_state ;
			}
			else
			{
				if( xev.button)
				{
					xev.type = ButtonPress ;
				}
				else
				{
					xev.type = ButtonRelease ;

					/* Reset button_state in releasing button */
					_mouse.button_state = 0 ;
				}

				xev.state = _display.key_state ;
			}

			xev.x = _mouse.x ;
			xev.y = _mouse.y ;

		#ifdef  __DEBUG
			kik_debug_printf( KIK_DEBUG_TAG
				"Button is %s x %d y %d btn %d time %d\n" ,
				xev.type == ButtonPress ? "pressed" :
					xev.type == MotionNotify ? "motion" : "released" ,
				xev.x , xev.y , xev.button , xev.time) ;
		#endif

			if( ! check_virtual_kbd( &xev))
			{
				win = get_window( xev.x , xev.y) ;
				xev.x -= win->x ;
				xev.y -= win->y ;

				x_window_receive_event( win , &xev) ;
			}

			if( move)
			{
				save_hidden_region() ;
				draw_mouse_cursor() ;
			}
		}
	}

	return  1 ;
}

static int
receive_key_event(void)
{
	u_char  code ;

	while( read( _display.fd , &code , 1) == 1)
	{
		XKeyEvent  xev ;
		int  pressed ;

		if( code & 0x80)
		{
			pressed = 0 ;
			code &= 0x7f ;
		}
		else
		{
			pressed = 1 ;
		}

		if( code >= keymap.n_keys)
		{
			continue ;
		}

		if( keymap.key[code].flgs & 2)
		{
			/* The key should react on num-lock(2). (Keypad keys) */

			int  kcode ;

			if( ( kcode = keymap.key[code].map[0]) != 0 && pressed)
			{
				/*
				 * KEY_KP0 etc are 0x100 larger than KEY_INSERT etc to
				 * distinguish them.
				 * (see x.h)
				 */
				xev.ksym = kcode + 0x200 ;

				goto  send_event ;
			}
		}
		else if( ! ( keymap.key[code].spcl & 0x80))
		{
			/* Character keys */

			if( pressed)
			{
				int  idx ;

				idx = (_display.key_state & 0x7) ;

				if( ( keymap.key[code].flgs & 1) &&
				    ( _display.lock_state & CLKED) )
				{
					/* xor shift bit(1) */
					idx ^= 1 ;
				}

			#if  1
				if( code == 41)
				{
					xev.ksym = XK_Zenkaku_Hankaku ;
				}
				else if( code == 121)
				{
					xev.ksym = XK_Henkan_Mode ;
				}
				else if( code == 123)
				{
					xev.ksym = XK_Muhenkan ;
				}
				else
			#endif
				{
					xev.ksym = keymap.key[code].map[idx] ;
				}

				goto  send_event ;
			}
		}
		else
		{
			/* Function keys */

			int  kcode ;

			if( ( kcode = keymap.key[code].map[0]) == 0)
			{
				/* do nothing */
			}
			else if( pressed)
			{
				if( kcode == KEY_RIGHTSHIFT ||
				    kcode == KEY_LEFTSHIFT)
				{
					_display.key_state |= ShiftMask ;
				}
				else if( kcode == KEY_CAPSLOCK)
				{
					if( _display.key_state & ShiftMask)
					{
						_display.key_state &= ~ShiftMask ;
					}
					else
					{
						_display.key_state |= ShiftMask ;
					}
				}
				else if( kcode == KEY_RIGHTCTRL ||
					 kcode == KEY_LEFTCTRL)
				{
					_display.key_state |= ControlMask ;
				}
				else if( kcode == KEY_RIGHTALT ||
					 kcode == KEY_LEFTALT)
				{
					_display.key_state |= ModMask ;
				}
				else if( kcode == KEY_NUMLOCK)
				{
					_display.lock_state ^= NLKED ;
				}
				else if( kcode == KEY_CAPSLOCK)
				{
					_display.lock_state ^= CLKED ;
				}
				else
				{
					xev.ksym = kcode + 0x100 ;

					goto  send_event ;
				}
			}
			else
			{
				if( kcode == KEY_RIGHTSHIFT ||
				    kcode == KEY_LEFTSHIFT)
				{
					_display.key_state &= ~ShiftMask ;
				}
				else if( kcode == KEY_RIGHTCTRL ||
					 kcode == KEY_LEFTCTRL)
				{
					_display.key_state &= ~ControlMask ;
				}
				else if( kcode == KEY_RIGHTALT ||
					 kcode == KEY_LEFTALT)
				{
					_display.key_state &= ~ModMask ;
				}
			}
		}

		continue ;

	send_event:
		xev.type = KeyPress ;
		xev.state = _mouse.button_state |
			    _display.key_state ;
		xev.keycode = code ;

	#ifdef  __DEBUG
		kik_debug_printf( KIK_DEBUG_TAG
			"scancode %d -> ksym 0x%x state 0x%x\n" ,
			code , xev.ksym , xev.state) ;
	#endif

		receive_event_for_multi_roots( &xev) ;
	}

	return  1 ;
}

#elif  defined(__NetBSD__) || defined(__OpenBSD__)

/* For iBus which requires ps/2 keycode. */
static u_int
get_ps2_kcode(
	u_int  kcode
	)
{
	if( kbd_type == WSKBD_TYPE_USB)
	{
		static u_char  map_table1[] =
		{
			30 ,	/* A (4) */
			48 ,	/* B */
			46 ,	/* C */
			32 ,	/* D */
			18 ,	/* E */
			33 ,	/* F */
			34 ,	/* G (10) */
			35 ,	/* H */
			23 ,	/* I */
			36 ,	/* J */
			37 ,	/* K */
			38 ,	/* L */
			50 ,	/* M */
			49 ,	/* N */
			24 ,	/* O */
			25 ,	/* P */
			16 ,	/* Q (20) */
			19 ,	/* R */
			31 ,	/* S */
			20 ,	/* T */
			22 ,	/* U */
			47 ,	/* V */
			17 ,	/* W */
			45 ,	/* X */
			21 ,	/* Y */
			44 ,	/* Z */
			 2 ,	/* 1 (30) */
			 3 ,	/* 2 */
			 4 ,	/* 3 */
			 5 ,	/* 4 */
			 6 ,	/* 5 */
			 7 ,	/* 6 */
			 8 ,	/* 7 */
			 9 ,	/* 8 */
			10 ,	/* 9 */
			11 ,	/* 0 */
			28 ,	/* Enter (40) */
			 1 ,	/* ESC */
			14 ,	/* BackSpace */
			15 ,	/* Tab */
			57 ,	/* Space */
			12 ,	/* _ - */
			13 ,	/* + = */
			26 ,	/* { [ */
			27 ,	/* } ] */
			43 ,	/* \ | */
			 0 ,	/* (50) */
			39 ,	/* : ; */
			40 ,	/* " ' */
			41 ,	/* ~ ` */
			51 ,	/* < , */
			52 ,	/* > . */
			53 ,	/* ? / */
			58 ,	/* CapsLock */
			59 ,	/* F1 */
			60 ,	/* F2 */
			61 ,	/* F3 (60) */
			62 ,	/* F4 */
			63 ,	/* F5 */
			64 ,	/* F6 */
			65 ,	/* F7 */
			66 ,	/* F8 */
			67 ,	/* F9 */
			68 ,	/* F10 */
			87 ,	/* F11 */
			88 ,	/* F12 */
			 0 ,	/* Print Screen (70) */
			70 ,	/* ScreenLock */
			 0 ,	/* Pause */
			110 ,	/* Insert */
			102 ,	/* Home */
			104 ,	/* Page Up */
			111 ,	/* Delete */
			107 ,	/* End */
			109 ,	/* Page Down */
			106 ,	/* Right */
			105 ,	/* Left (80) */
			108 ,	/* Down */
			103 ,	/* Up */
			69 ,	/* NumLock */
			 0 ,	/* Num / */
			55 ,	/* Num * */
			74 ,	/* Num - */
			78 ,	/* Num + */
			 0 ,	/* Num Enter */
			79 ,	/* Num 1 */
			80 ,	/* Num 2 (90) */
			81 ,	/* Num 3 */
			75 ,	/* Num 4 */
			76 ,	/* Num 5 */
			77 ,	/* Num 6 */
			71 ,	/* Num 7 */
			72 ,	/* Num 8 */
			73 ,	/* Num 9 */
			82 ,	/* Num 0 */
			83 ,	/* Num . */
		} ;

		static u_char  map_table2[] =
		{
			29 ,	/* Control L (224) */
			42 ,	/* Shift L */
			56 ,	/* Alt L */
			 0 ,	/* Windows L */
			97 ,	/* Control R */
			54 ,	/* Shift R */
			100 ,	/* Alt R (230) */
			 0 ,	/* Windows R */
		} ;

		if( 4 <= kcode)
		{
			if( kcode <= 99)
			{
				return  map_table1[kcode - 4] ;
			}
			else if( 224 <= kcode)
			{
				if( kcode <= 231)
				{
					return  map_table2[kcode - 224] ;
				}
			}
		}

		return  0 ;
	}
	else
	{
		return  kcode ;
	}
}

static void
process_wskbd_event(
	struct wscons_event *  ev
	)
{
	keysym_t  ksym ;

	if( keymap.map[ev->value].command == KS_Cmd_ResetEmul)
	{
		/* XXX */
		ksym = XK_BackSpace ;
	}
	else if( _display.key_state & ShiftMask)
	{
		ksym = keymap.map[ev->value].group1[1] ;
	}
	else
	{
		ksym = keymap.map[ev->value].group1[0] ;
	}

	if( KS_f1 <= ksym && ksym <= KS_f20)
	{
		/* KS_f1 => KS_F1 */
		ksym += 0x40 ;
	}

	if( ev->type == WSCONS_EVENT_KEY_DOWN)
	{
		if( ksym == KS_Shift_R ||
		    ksym == KS_Shift_L)
		{
			_display.key_state |= ShiftMask ;
		}
		else if( ksym == KS_Caps_Lock)
		{
			if( _display.key_state & ShiftMask)
			{
				_display.key_state &= ~ShiftMask ;
			}
			else
			{
				_display.key_state |= ShiftMask ;
			}
		}
		else if( ksym == KS_Control_R ||
			 ksym == KS_Control_L)
		{
			_display.key_state |= ControlMask ;
		}
		else if( ksym == KS_Alt_R ||
			 ksym == KS_Alt_L)
		{
			_display.key_state |= ModMask ;
		}
		else if( ksym == KS_Num_Lock)
		{
			_display.lock_state ^= NLKED ;
		}
		else
		{
			XKeyEvent  xev ;

			xev.type = KeyPress ;
			xev.ksym = ksym ;
			xev.state = _mouse.button_state |
				    _display.key_state ;
			xev.keycode = get_ps2_kcode( ev->value) ;

			receive_event_for_multi_roots( &xev) ;

			prev_key_event = *ev ;
		#ifdef  __NetBSD__
			wskbd_repeat_wait = (wskbd_repeat_1 + KEY_REPEAT_UNIT - 1) /
						KEY_REPEAT_UNIT ;
		#endif
		}
	}
	else if( ev->type == WSCONS_EVENT_KEY_UP)
	{
		if( ksym == KS_Shift_R ||
		    ksym == KS_Shift_L)
		{
			_display.key_state &= ~ShiftMask ;
		}
		else if( ksym == KS_Control_R ||
			 ksym == KS_Control_L)
		{
			_display.key_state &= ~ControlMask ;
		}
		else if( ksym == KS_Alt_R ||
			 ksym == KS_Alt_L)
		{
			_display.key_state &= ~ModMask ;
		}
		else if( ev->value == prev_key_event.value)
		{
			prev_key_event.value = 0 ;
		}
	}
}

#if  _MACHINE == x68k

static void
setup_reg(
	fb_reg_t *  reg ,
	fb_reg_conf_t *  conf
	)
{
	if( ( reg->crtc.r20 & 0x3) < ( conf->crtc.r20 & 0x3) ||
	    ( ( reg->crtc.r20 & 0x3) == ( conf->crtc.r20 & 0x3) &&
	      ( reg->crtc.r20 & 0x10) < ( conf->crtc.r20 & 0x10)))
	{
		/* to higher resolution */

		reg->crtc.r00 = conf->crtc.r00 ;
		reg->crtc.r01 = conf->crtc.r01 ;
		reg->crtc.r02 = conf->crtc.r02 ;
		reg->crtc.r03 = conf->crtc.r03 ;
		reg->crtc.r04 = conf->crtc.r04 ;
		reg->crtc.r05 = conf->crtc.r05 ;
		reg->crtc.r06 = conf->crtc.r06 ;
		reg->crtc.r07 = conf->crtc.r07 ;
		reg->crtc.r20 = conf->crtc.r20 ;
	}
	else
	{
		/* to lower resolution */

		reg->crtc.r20 = conf->crtc.r20 ;
		reg->crtc.r01 = conf->crtc.r01 ;
		reg->crtc.r02 = conf->crtc.r02 ;
		reg->crtc.r03 = conf->crtc.r03 ;
		reg->crtc.r04 = conf->crtc.r04 ;
		reg->crtc.r05 = conf->crtc.r05 ;
		reg->crtc.r06 = conf->crtc.r06 ;
		reg->crtc.r07 = conf->crtc.r07 ;
		reg->crtc.r00 = conf->crtc.r00 ;
	}

	reg->crtc.r08 = conf->crtc.r08 ;

	reg->videoc.r0 = conf->videoc.r0 ;
	reg->videoc.r1 = conf->videoc.r1 ;
	reg->videoc.r2 = conf->videoc.r2 ;
}

static int
open_display(void)
{
	char *  dev ;
	struct grfinfo  vinfo ;
	fb_reg_t *  reg ;
	fb_reg_conf_t *  conf ;
	fb_reg_conf_t  conf_512_512_15 =
		{ { 91 , 9 , 17 , 81 , 567 , 5 , 40 , 552 , 27 , 789 } ,
		  { 3 , 0x21e4 , 0x000f } } ;
	fb_reg_conf_t  conf_512_512_8 =
		{ { 91 , 9 , 17 , 81 , 567 , 5 , 40 , 552 , 27 , 277 } ,
		  { 1 , 0x21e4 , 0x0003 } } ;
	fb_reg_conf_t  conf_768_512_4 =
		{ { 137 , 14 , 28 , 124 , 567 , 5 , 40 , 552 , 27 , 1046 } ,
		  { 4 , 0x21e4 , 0x0010 } } ;
	fb_reg_conf_t  conf_1024_768_4 =
		{ { 169 , 14 , 28 , 156 , 439 , 5 , 40 , 424 , 27 , 1050 } ,
		  { 4 , 0x21e4 , 0x0010 } } ;
	struct rgb_info  rgb_info_15bpp = { 3 , 3 , 3 , 6 , 11 , 1 } ;
	struct termios  tm ;

	if( ! ( _display.fb_fd = open( ( dev = getenv("FRAMEBUFFER")) ? dev : "/dev/grf1" ,
					O_RDWR)))
	{
		kik_msg_printf( "Couldn't open %s.\n" , dev ? dev : "/dev/grf1") ;

		return  0 ;
	}

	kik_file_set_cloexec( _display.fb_fd) ;

	if( ioctl( _display.fb_fd , GRFIOCGINFO , &vinfo) == -1)
	{
		goto  error ;
	}

	_display.smem_len = vinfo.gd_fbsize + vinfo.gd_regsize ;

	if( ( _display.fb = mmap( NULL , _display.smem_len ,
				PROT_WRITE|PROT_READ , MAP_FILE|MAP_SHARED ,
				_display.fb_fd , (off_t)0)) == MAP_FAILED)
	{
		kik_msg_printf( "Retry another mode of resolution and depth.\n") ;

		goto  error ;
	}

	reg = _display.fb ;

	/* XXX Here reg->crtc.rXX are 0 which will slide the screen unexpectedly on exit. */
#if  0
	orig_reg.crtc.r00 = reg->crtc.r00 ;
	orig_reg.crtc.r01 = reg->crtc.r01 ;
	orig_reg.crtc.r02 = reg->crtc.r02 ;
	orig_reg.crtc.r03 = reg->crtc.r03 ;
	orig_reg.crtc.r04 = reg->crtc.r04 ;
	orig_reg.crtc.r05 = reg->crtc.r05 ;
	orig_reg.crtc.r06 = reg->crtc.r06 ;
	orig_reg.crtc.r07 = reg->crtc.r07 ;
	orig_reg.crtc.r08 = reg->crtc.r08 ;
	orig_reg.crtc.r20 = reg->crtc.r20 ;
	orig_reg.videoc.r0 = reg->videoc.r0 ;
	orig_reg.videoc.r1 = reg->videoc.r1 ;
	orig_reg.videoc.r2 = reg->videoc.r2 ;

	kik_debug_printf( KIK_DEBUG_TAG
		" crtc %d %d %d %d %d %d %d %d %d 0x%x videoc 0x%x 0x%x 0x%x\n" ,
		orig_reg.crtc.r00 , orig_reg.crtc.r01 , orig_reg.crtc.r02 ,
		orig_reg.crtc.r03 , orig_reg.crtc.r04 , orig_reg.crtc.r05 ,
		orig_reg.crtc.r06 , orig_reg.crtc.r07 , orig_reg.crtc.r08 ,
		orig_reg.crtc.r20 , orig_reg.videoc.r0 , orig_reg.videoc.r1 ,
		orig_reg.videoc.r2) ;
#else
	orig_reg = conf_768_512_4 ;
	orig_reg.videoc.r2 = 0x20 ;
#endif

	if( fb_depth == 15)
	{
		conf = &conf_512_512_15 ;

		_display.width = _disp.width = 512 ;
		_display.height = _disp.height = 512 ;
		_disp.depth = 15 ;

		_display.rgbinfo = rgb_info_15bpp ;
	}
	else
	{
		if( fb_depth == 8)
		{
			conf = &conf_512_512_8 ;

			_display.width = _disp.width = 512 ;
			_display.height = _disp.height = 512 ;
			_disp.depth = 8 ;
		}
		else /* if( fb_depth == 4) */
		{
			if( fb_width == 1024 && fb_height == 768)
			{
				conf = &conf_1024_768_4 ;

				_display.width = _disp.width = 1024 ;
				_display.height = _disp.height = 768 ;
			}
			else
			{
				conf = &conf_768_512_4 ;

				_display.width = _disp.width = 768 ;
				_display.height = _disp.height = 512 ;
			}

			if( fb_depth == 1)
			{
				_disp.depth = 1 ;
			}
			else
			{
				_disp.depth = 4 ;
			}
		}

		if( ! cmap_init())
		{
			goto  error ;
		}
	}

	_display.bytes_per_pixel = 2 ;
	_display.pixels_per_byte = 1 ;

	_display.line_length = (_disp.width * 2 + 1023) / 1024 * 1024 ;
	_display.xoffset = 0 ;
	/* XXX gd_regsize is regarded as multiple of line_length */
	_display.yoffset = vinfo.gd_regsize / _display.line_length ;

	setup_reg( reg , conf) ;

#ifdef  ENABLE_DOUBLE_BUFFER
	if( _display.pixels_per_byte > 1 &&
	    ! ( _display.back_fb = malloc( _display.smem_len)))
	{
		goto  error ;
	}
#endif

	tcgetattr( STDIN_FILENO , &tm) ;
	orig_tm = tm ;
	tm.c_iflag = tm.c_oflag = 0 ;
	tm.c_cflag &= ~CSIZE ;
	tm.c_cflag |= CS8 ;
	tm.c_lflag &= ~(ECHO|ISIG|IEXTEN|ICANON) ;
	tm.c_cc[VMIN] = 1 ;
	tm.c_cc[VTIME] = 0 ;
	tcsetattr( STDIN_FILENO , TCSAFLUSH , &tm) ;

	_display.fd = STDIN_FILENO ;
	_mouse.fd = -1 ;

	_disp.display = &_display ;

	console_id = get_active_console() ;

	return  1 ;

error:
	cmap_final() ;

	if( _display.fb)
	{
		setup_reg( reg , &orig_reg) ;
		munmap( _display.fb , _display.smem_len) ;
		_display.fb = NULL ;
	}

	close( _display.fb_fd) ;

	ioctl( _display.fb_fd , GRFIOCOFF , 0) ;

	return  0 ;
}

#else	/* _MACHINE != x68k */

#ifdef  __NetBSD__
static void
auto_repeat(void)
{
	if( prev_key_event.value && --wskbd_repeat_wait == 0)
	{
		process_wskbd_event( &prev_key_event) ;
		wskbd_repeat_wait = (wskbd_repeat_N + KEY_REPEAT_UNIT - 1) / KEY_REPEAT_UNIT ;
	}
}
#endif

static int
open_display(void)
{
	char *  dev ;
	struct wsdisplay_fbinfo  vinfo ;
	int  mode ;
	int  wstype ;
	struct rgb_info  rgbinfos[] =
	{
		{ 3 , 3 , 3 , 10 , 5 , 0 } ,
		{ 3 , 2 , 3 , 11 , 5 , 0 } ,
		{ 0 , 0 , 0 , 16 , 8 , 0 } ,
	} ;
	struct termios  tm ;
	static struct wscons_keymap  map[KS_NUMKEYCODES] ;

#if  defined(__NetBSD__)
#define  DEFAULT_FBDEV  "/dev/ttyE0"
#else  /* __OpenBSD__ */
#define  DEFAULT_FBDEV  "/dev/ttyC0"
#endif

	if( ! ( _display.fb_fd = open( ( dev = getenv("FRAMEBUFFER")) ? dev : DEFAULT_FBDEV ,
					O_RDWR)))
	{
		kik_msg_printf( "Couldn't open %s.\n" , dev ? dev : DEFAULT_FBDEV) ;

		return  0 ;
	}

	kik_file_set_cloexec( _display.fb_fd) ;

	ioctl( STDIN_FILENO , WSDISPLAYIO_GMODE , &orig_console_mode) ;

#ifdef  __NetBSD__
	mode = WSDISPLAYIO_MODE_MAPPED ;
#else
	{
		struct wsdisplay_gfx_mode  gfx_mode ;

		gfx_mode.width = fb_width ;
		gfx_mode.height = fb_height ;
		gfx_mode.depth = fb_depth ;

		if( ioctl( _display.fb_fd , WSDISPLAYIO_SETGFXMODE , &gfx_mode) == -1)
		{
			goto  error ;
		}
	}

	mode = WSDISPLAYIO_MODE_DUMBFB ;
#endif

	if( ioctl( STDIN_FILENO , WSDISPLAYIO_SMODE , &mode) == -1)
	{
	#ifdef  DEBUG
		kik_debug_printf( KIK_DEBUG_TAG " WSDISPLAYIO_SMODE failed.\n") ;
	#endif

		goto  error ;
	}

	if( ioctl( _display.fb_fd , WSDISPLAYIO_GINFO , &vinfo) == -1 ||
	    ioctl( _display.fb_fd , WSDISPLAYIO_GTYPE , &wstype) == -1)
	{
	#ifdef  DEBUG
		kik_debug_printf( KIK_DEBUG_TAG
			" WSDISPLAYIO_GINFO or WSDISPLAYIO_GTYPE failed.\n") ;
	#endif

		goto  error ;
	}

	_display.xoffset = 0 ;
	_display.yoffset = 0 ;

	_display.width = _disp.width = vinfo.width ;
	_display.height = _disp.height = vinfo.height ;

	if( ( _disp.depth = vinfo.depth) < 8)
	{
	#ifdef  ENABLE_2_4_PPB
		_display.pixels_per_byte = 8 / _disp.depth ;
	#else
		/* XXX Forcibly set 1 bpp */
		_display.pixels_per_byte = 8 ;
		_disp.depth = 1 ;
	#endif

		_display.shift_0 = FB_SHIFT_0(_display.pixels_per_byte,_disp.depth) ;
		_display.mask = FB_MASK(_display.pixels_per_byte) ;
	}
	else
	{
		_display.pixels_per_byte = 1 ;
	}

	if( ( _display.bytes_per_pixel = (_disp.depth + 7) / 8) == 3)
	{
		_display.bytes_per_pixel = 4 ;
	}

	if( ioctl( _display.fb_fd , WSDISPLAYIO_LINEBYTES , &_display.line_length) == -1)
	{
		/* WSDISPLAYIO_LINEBYTES isn't defined in some ports. */

	#ifdef  MACHINE
		/* XXX Hack for NetBSD 5.x/hpcmips */
		if( strcmp( MACHINE , "hpcmips") == 0 && _disp.depth == 16)
		{
			_display.line_length = _display.width * 5 / 2 ;
		}
		else
	#endif
		{
			_display.line_length = _display.width * _display.bytes_per_pixel /
					_display.pixels_per_byte ;
		}
	}

	_display.smem_len = _display.line_length * _display.height ;

	if( ( _display.fb = mmap( NULL , _display.smem_len ,
				PROT_WRITE|PROT_READ , MAP_SHARED , _display.fb_fd , (off_t)0))
				== MAP_FAILED)
	{
		kik_msg_printf( "Retry another mode of resolution and depth.\n") ;

		goto  error ;
	}

#ifdef  WSDISPLAY_TYPE_LUNA
	if( wstype == WSDISPLAY_TYPE_LUNA)
	{
		_display.fb += 8 ;
	}
#endif

	if( _disp.depth == 15)
	{
		_display.rgbinfo = rgbinfos[0] ;
	}
	else if( _disp.depth == 16)
	{
		_display.rgbinfo = rgbinfos[1] ;
	}
	else if( _disp.depth >= 24)
	{
		_display.rgbinfo = rgbinfos[2] ;
	}
	else
	{
		if( ! cmap_init())
		{
			goto  error ;
		}
	}

#ifdef  ENABLE_DOUBLE_BUFFER
	if( _display.pixels_per_byte > 1 &&
	    ! ( _display.back_fb = malloc( _display.smem_len)))
	{
		goto  error ;
	}
#endif

	tcgetattr( STDIN_FILENO , &tm) ;
	orig_tm = tm ;
	tm.c_iflag = tm.c_oflag = 0 ;
	tm.c_cflag &= ~CSIZE ;
	tm.c_cflag |= CS8 ;
	tm.c_lflag &= ~(ECHO|ISIG|IEXTEN|ICANON) ;
	tm.c_cc[VMIN] = 1 ;
	tm.c_cc[VTIME] = 0 ;
	tcsetattr( STDIN_FILENO , TCSAFLUSH , &tm) ;

	kik_priv_restore_euid() ;
	kik_priv_restore_egid() ;

	if( ! getenv( "WSKBD") ||
	    /* If you want to use /dev/wskbd0, export WSKBD=/dev/wskbd0 */
	    ( _display.fd = open( getenv( "WSKBD") , O_RDWR|O_NONBLOCK|O_EXCL)) == -1)
	{
		_display.fd = open( "/dev/wskbd" , O_RDWR|O_NONBLOCK|O_EXCL) ;
	}

	_mouse.fd = open( "/dev/wsmouse" , O_RDWR|O_NONBLOCK|O_EXCL) ;

	kik_priv_change_euid( kik_getuid()) ;
	kik_priv_change_egid( kik_getgid()) ;

	if( _display.fd == -1)
	{
	#ifdef  DEBUG
		kik_debug_printf( KIK_DEBUG_TAG " Failed to open /dev/wskbd.\n") ;
	#endif

		_display.fd = STDIN_FILENO ;
	}
	else
	{
		kik_file_set_cloexec( _display.fd) ;

	#ifdef  WSKBDIO_EVENT_VERSION
		mode = WSKBDIO_EVENT_VERSION ;
		ioctl( _display.fd , WSKBDIO_SETVERSION , &mode) ;
	#endif

		ioctl( _display.fd , WSKBDIO_GTYPE , &kbd_type) ;

		keymap.maplen = KS_NUMKEYCODES ;
		keymap.map = map ;
		ioctl( _display.fd , WSKBDIO_GETMAP , &keymap) ;

	#if  0
		kik_debug_printf( "DUMP KEYMAP (LEN %d)\n" , keymap.maplen) ;
		{
			int  count ;

			for( count = 0 ; count < keymap.maplen ; count++)
			{
				kik_debug_printf( "%d: %x %x %x %x %x\n" ,
					count ,
					keymap.map[count].command ,
					keymap.map[count].group1[0] ,
					keymap.map[count].group1[1] ,
					keymap.map[count].group2[0] ,
					keymap.map[count].group2[1]) ;
			}
		}
	#endif

		tcgetattr( _display.fd , &tm) ;
		tm.c_iflag = IGNBRK | IGNPAR ;
		tm.c_oflag = 0 ;
		tm.c_lflag = 0 ;
		tm.c_cc[VTIME] = 0 ;
		tm.c_cc[VMIN] = 1 ;
		tm.c_cflag = CS8 | CSTOPB | CREAD | CLOCAL | HUPCL ;
		cfsetispeed( &tm , B1200) ;
		cfsetospeed( &tm , B1200) ;
		tcsetattr( _display.fd , TCSAFLUSH , &tm) ;

		ioctl( _display.fd , WSKBDIO_GETLEDS , &_display.lock_state) ;

	#ifdef  __NetBSD__
		x_event_source_add_fd( -10 , auto_repeat) ;
	#endif
	}

	_disp.display = &_display ;

	if( _mouse.fd != -1)
	{
		kik_file_set_cloexec( _mouse.fd) ;

	#ifdef  WSMOUSE_EVENT_VERSION
		mode = WSMOUSE_EVENT_VERSION ;
		ioctl( _mouse.fd , WSMOUSEIO_SETVERSION , &mode) ;
	#endif

		_mouse.x = _display.width / 2 ;
		_mouse.y = _display.height / 2 ;
		_disp_mouse.display = (Display*)&_mouse ;

		tcgetattr( _mouse.fd , &tm) ;
		tm.c_iflag = IGNBRK | IGNPAR;
		tm.c_oflag = 0 ;
		tm.c_lflag = 0 ;
		tm.c_cc[VTIME] = 0 ;
		tm.c_cc[VMIN] = 1 ;
		tm.c_cflag = CS8 | CSTOPB | CREAD | CLOCAL | HUPCL ;
		cfsetispeed( &tm , B1200) ;
		cfsetospeed( &tm , B1200) ;
		tcsetattr( _mouse.fd , TCSAFLUSH , &tm) ;
	}
#ifdef  DEBUG
	else
	{
		kik_debug_printf( KIK_DEBUG_TAG " Failed to open /dev/wsmouse.\n") ;
	}
#endif

	console_id = get_active_console() ;

	return  1 ;

error:
	if( _display.fb)
	{
		munmap( _display.fb , _display.smem_len) ;
		_display.fb = NULL ;
	}

	close( _display.fb_fd) ;

	ioctl( STDIN_FILENO , WSDISPLAYIO_SMODE , &orig_console_mode) ;

	return  0 ;
}

#endif	/* _MACHINE == x68k */


static int
receive_mouse_event(void)
{
	struct wscons_event  ev ;
	ssize_t  len ;

	while( ( len = read( _mouse.fd , memset( &ev , 0 , sizeof(ev)) , sizeof(ev))) > 0)
	{
	#ifdef  DEBUG
		kik_debug_printf( KIK_DEBUG_TAG " MOUSE event (len)%d (type)%d (val)%d\n" ,
			len , ev.type , ev.value) ;
	#endif

		if( console_id != get_active_console())
		{
			return  0 ;
		}

		if( ev.type == WSCONS_EVENT_MOUSE_ABSOLUTE_X)
		{
			_mouse.x = ev.value ;

			continue ;	/* Wait for ABSOLUTE_Y */
		}
		else if( ev.type == WSCONS_EVENT_MOUSE_ABSOLUTE_Y)
		{
			restore_hidden_region() ;

			_mouse.y = ev.value ;
			update_mouse_cursor_state() ;

			/* XXX MotionNotify event is not sent. */

			save_hidden_region() ;
			draw_mouse_cursor() ;

			ev.type = WSCONS_EVENT_MOUSE_DOWN ;
			ev.value = 0 ;	/* Button1 */
		}

		if( ev.type == WSCONS_EVENT_MOUSE_DOWN ||
		    ev.type == WSCONS_EVENT_MOUSE_UP)
		{
			XButtonEvent  xev ;
			x_window_t *  win ;

			if( ev.value == 0)
			{
				xev.button = Button1 ;
				_mouse.button_state = Button1Mask ;
			}
			else if( ev.value == 1)
			{
				xev.button = Button2 ;
				_mouse.button_state = Button2Mask ;
			}
			else if( ev.value == 2)
			{
				xev.button = Button3 ;
				_mouse.button_state = Button3Mask ;
			}
			else
			{
				continue ;

				while(1)
				{
				button4:
					xev.button = Button4 ;
					_mouse.button_state = Button4Mask ;
					break ;

				button5:
					xev.button = Button5 ;
					_mouse.button_state = Button5Mask ;
					break ;

				button6:
					xev.button = Button6 ;
					_mouse.button_state = Button6Mask ;
					break ;

				button7:
					xev.button = Button7 ;
					_mouse.button_state = Button7Mask ;
					break ;
				}

				ev.value = 1 ;
			}

			if( ev.type != WSCONS_EVENT_MOUSE_UP)
			{
				/*
				 * WSCONS_EVENT_MOUSE_UP,
				 * WSCONS_EVENT_MOUSE_DELTA_Z
				 * WSCONS_EVENT_MOUSE_DELTA_W
				 */
				xev.type = ButtonPress ;
			}
			else /* if( ev.type == WSCONS_EVENT_MOUSE_UP) */
			{
				xev.type = ButtonRelease ;

				/* Reset button_state in releasing button. */
				_mouse.button_state = 0 ;
			}

			xev.time = ev.time.tv_sec * 1000 + ev.time.tv_nsec / 1000000 ;
			xev.x = _mouse.x ;
			xev.y = _mouse.y ;
			xev.state = _display.key_state ;

		#ifdef  __DEBUG
			kik_debug_printf( KIK_DEBUG_TAG
				"Button is %s x %d y %d btn %d time %d\n" ,
				xev.type == ButtonPress ? "pressed" : "released" ,
				xev.x , xev.y , xev.button , xev.time) ;
		#endif

			if( ! check_virtual_kbd( &xev))
			{
				win = get_window( xev.x , xev.y) ;
				xev.x -= win->x ;
				xev.y -= win->y ;

				x_window_receive_event( win , &xev) ;
			}
		}
		else if( ev.type == WSCONS_EVENT_MOUSE_DELTA_X ||
		         ev.type == WSCONS_EVENT_MOUSE_DELTA_Y ||
			 ev.type == WSCONS_EVENT_MOUSE_DELTA_Z ||
			 ev.type == WSCONS_EVENT_MOUSE_DELTA_W)
		{
			XMotionEvent  xev ;
			x_window_t *  win ;

			if( ev.type == WSCONS_EVENT_MOUSE_DELTA_X)
			{
				restore_hidden_region() ;

				_mouse.x += (int)ev.value ;

				if( _mouse.x < 0)
				{
					_mouse.x = 0 ;
				}
				else if( _display.width <= _mouse.x)
				{
					_mouse.x = _display.width - 1 ;
				}
			}
			else if( ev.type == WSCONS_EVENT_MOUSE_DELTA_Y)
			{
				restore_hidden_region() ;

				_mouse.y -= (int)ev.value ;

				if( _mouse.y < 0)
				{
					_mouse.y = 0 ;
				}
				else if( _display.height <= _mouse.y)
				{
					_mouse.y = _display.height - 1 ;
				}
			}
			else if( ev.type == WSCONS_EVENT_MOUSE_DELTA_Z)
			{
				if( ev.value < 0)
				{
					/* Up */
					goto  button4 ;
				}
				else if( ev.value > 0)
				{
					/* Down */
					goto  button5 ;
				}
			}
			else /* if( ev.type == WSCONS_EVENT_MOUSE_DELTA_W) */
			{
				if( ev.value < 0)
				{
					/* Left */
					goto  button6 ;
				}
				else if( ev.value > 0)
				{
					/* Right */
					goto  button7 ;
				}
			}

			update_mouse_cursor_state() ;

			xev.type = MotionNotify ;
			xev.x = _mouse.x ;
			xev.y = _mouse.y ;
			xev.time = ev.time.tv_sec * 1000 + ev.time.tv_nsec / 1000000 ;
			xev.state = _mouse.button_state | _display.key_state ;

		#ifdef  __DEBUG
			kik_debug_printf( KIK_DEBUG_TAG
				" Button is moved %d x %d y %d btn %d time %d\n" ,
				xev.type , xev.x , xev.y , xev.state , xev.time) ;
		#endif

			win = get_window( xev.x , xev.y) ;
			xev.x -= win->x ;
			xev.y -= win->y ;

			x_window_receive_event( win , &xev) ;

			save_hidden_region() ;
			draw_mouse_cursor() ;
		}
	}

	return  1 ;
}

static int
receive_key_event(void)
{
	if( _display.fd == STDIN_FILENO)
	{
		return  receive_stdin_key_event() ;
	}
	else
	{
		ssize_t  len ;
		struct wscons_event  ev ;

		while( ( len = read( _display.fd , memset( &ev , 0 , sizeof(ev)) ,
				sizeof(ev))) > 0)
		{
		#ifdef  DEBUG
			kik_debug_printf( KIK_DEBUG_TAG " KEY event (len)%d (type)%d (val)%d\n" ,
				len , ev.type , ev.value) ;
		#endif

			if( console_id != get_active_console())
			{
				return  0 ;
			}

			process_wskbd_event( &ev) ;
		}
	}

	return  1 ;
}

#else	/* FreeBSD/NetBSD/OpenBSD/linux */

static int
get_key_state(void)
{
	int  ret ;
	char  state ;

	state = 6 ;

	ret = ioctl( STDIN_FILENO , TIOCLINUX , &state) ;

	if( ret == -1)
	{
		return  0 ;
	}
	else
	{
		/* ShiftMask and ControlMask is the same. */

		return  state | ( (state & (1 << KG_ALT)) ? ModMask : 0) ;
	}
}

static int
kcode_to_ksym(
	int  kcode ,
	int  state
	)
{
	if( kcode == KEY_ENTER || kcode == KEY_KPENTER)
	{
		/* KDGKBENT returns '\n'(0x0a) */
		return  0x0d ;
	}
	else if( kcode == KEY_BACKSPACE)
	{
		/* KDGKBDENT returns 0x7f */
		return  0x08 ;
	}
	else if( kcode <= KEY_SLASH || kcode == KEY_SPACE || kcode == KEY_YEN || kcode == KEY_RO)
	{
		struct kbentry  ent ;
		int  ret ;

		if( state & ShiftMask)
		{
			ent.kb_table = (1 << KG_SHIFT) ;
		}
	#ifdef  READ_CTRL_KEYMAP
		else if( state & ControlMask)
		{
			ent.kb_table = (1 << KG_CTRL) ;
		}
	#endif
		else
		{
			ent.kb_table = 0 ;
		}

		ent.kb_index = kcode ;

		ret = ioctl( STDIN_FILENO , KDGKBENT , &ent) ;

		if( ret != -1 && ent.kb_value != K_HOLE && ent.kb_value != K_NOSUCHMAP)
		{
			ent.kb_value &= 0xff ;

			return  ent.kb_value ;
		}
	}

	return  kcode + 0x100 ;
}

static void
set_use_console_backscroll(
	int  use
	)
{
	struct kbentry  ent ;

	kik_priv_restore_euid() ;
	kik_priv_restore_egid() ;

	ent.kb_table = (1 << KG_SHIFT) ;
	ent.kb_index = KEY_PAGEUP ;
	ent.kb_value = use ? K_SCROLLBACK : K_PGUP ;
	ioctl( STDIN_FILENO , KDSKBENT , &ent) ;

	ent.kb_index = KEY_PAGEDOWN ;
	ent.kb_value = use ? K_SCROLLFORW : K_PGDN ;
	ioctl( STDIN_FILENO , KDSKBENT , &ent) ;

	kik_priv_change_euid( kik_getuid()) ;
	kik_priv_change_egid( kik_getgid()) ;
}

static void
get_event_device_num(
	int *  kbd ,
	int *  mouse
	)
{
	char  class[] = "/sys/class/input/inputN/name" ;
	int  count ;
	FILE *  fp ;

	*kbd = -1 ;
	*mouse = -1 ;

	for( count = 0 ; ; count++)
	{
		class[22] = count + 0x30 ;

		if( ! ( fp = fopen( class , "r")))
		{
			break ;
		}
		else
		{
			char  buf[128] ;

			if( fgets( buf , sizeof(buf) , fp))
			{
				char *  p ;

				/* To lower case */
				for( p = buf ; *p ; p++)
				{
					/*
					 * "0x41 <=" is not necessary to check if
					 * "mouse" or "key" exits in buf.
					 */
					if( /* 0x41 <= *p && */ *p <= 0x5a)
					{
						*p += 0x20 ;
					}
				}

				if( strstr( buf , "key"))
				{
					*kbd = count ;
				}
				else
				{
					static char *  mouse_names[] =
						{ "mouse" , "ts" , "touch" } ;
					u_int  idx ;

					for( idx = 0 ;
					     idx < sizeof(mouse_names) / sizeof(mouse_names[0]) ;
					     idx++)
					{
						if( strstr( buf , mouse_names[idx]))
						{
							*mouse = count ;

							break ;
						}
					}
				}
			}

			fclose( fp) ;
		}
	}
}

static int
open_event_device(
	int  num
	)
{
	char  event[] = "/dev/input/eventN" ;
	int  fd ;

	kik_priv_restore_euid() ;
	kik_priv_restore_egid() ;

	event[16] = num + 0x30 ;

	if( ( fd = open( event , O_RDONLY|O_NONBLOCK)) == -1)
	{
		kik_msg_printf( "Failed to open %s.\n" , event) ;
	}
#if  0
	else
	{
		/* Occupy /dev/input/eventN */
		ioctl( fd , EVIOCGRAB , 1) ;
	}
#endif

	kik_priv_change_euid( kik_getuid()) ;
	kik_priv_change_egid( kik_getgid()) ;

	return  fd ;
}

static int
open_display(void)
{
	char *  dev ;
	struct  fb_fix_screeninfo  finfo ;
	struct  fb_var_screeninfo  vinfo ;
	int  kbd_num ;
	int  mouse_num ;
	struct termios  tm ;

	if( ! ( _display.fb_fd = open( ( dev = getenv("FRAMEBUFFER")) ? dev : "/dev/fb0" ,
					O_RDWR)))
	{
		kik_msg_printf( "Couldn't open %s.\n" , dev ? dev : "/dev/fb0") ;

		return  0 ;
	}

	kik_file_set_cloexec( _display.fb_fd) ;

	ioctl( _display.fb_fd , FBIOGET_FSCREENINFO , &finfo) ;
	ioctl( _display.fb_fd , FBIOGET_VSCREENINFO , &vinfo) ;

	if( ( _disp.depth = vinfo.bits_per_pixel) < 8)
	{
	#if  0
		/* 1/2/4 bpp is not supported. */
		kik_msg_printf( "%d bpp is not supported.\n" , vinfo.bits_per_pixel) ;

		goto  error ;
	#else
	#ifdef  ENABLE_2_4_PPB
		_display.pixels_per_byte = 8 / _disp.depth ;
	#else
		/* XXX Forcibly set 1 bpp */
		_display.pixels_per_byte = 8 ;
		_disp.depth = 1 ;
	#endif
	#endif

		_display.shift_0 = FB_SHIFT_0(_display.pixels_per_byte,_disp.depth) ;
		_display.mask = FB_MASK(_display.pixels_per_byte) ;
	}
	else
	{
		_display.pixels_per_byte = 1 ;
	}

	if( ( _display.fb = mmap( NULL , (_display.smem_len = finfo.smem_len) ,
				PROT_WRITE|PROT_READ , MAP_SHARED , _display.fb_fd , (off_t)0))
				== MAP_FAILED)
	{
		goto  error ;
	}

	if( ( _display.bytes_per_pixel = (_disp.depth + 7) / 8) == 3)
	{
		_display.bytes_per_pixel = 4 ;
	}

	if( _disp.depth < 15 && ! cmap_init())
	{
		goto  error ;
	}

#ifdef  ENABLE_DOUBLE_BUFFER
	if( _display.pixels_per_byte > 1 &&
	    ! ( _display.back_fb = malloc( _display.smem_len)))
	{
		goto  error ;
	}
#endif

	_display.line_length = finfo.line_length ;
	_display.xoffset = vinfo.xoffset ;
	_display.yoffset = vinfo.yoffset ;

	_display.width = _disp.width = vinfo.xres ;
	_display.height = _disp.height = vinfo.yres ;

	_display.rgbinfo.r_limit = 8 - vinfo.red.length ;
	_display.rgbinfo.g_limit = 8 - vinfo.green.length ;
	_display.rgbinfo.b_limit = 8 - vinfo.blue.length ;
	_display.rgbinfo.r_offset = vinfo.red.offset ;
	_display.rgbinfo.g_offset = vinfo.green.offset ;
	_display.rgbinfo.b_offset = vinfo.blue.offset ;

	get_event_device_num( &kbd_num , &mouse_num) ;

	tcgetattr( STDIN_FILENO , &tm) ;
	orig_tm = tm ;
	tm.c_iflag = tm.c_oflag = 0 ;
	tm.c_cflag &= ~CSIZE ;
	tm.c_cflag |= CS8 ;
	tm.c_lflag &= ~(ECHO|ISIG|ICANON) ;
	tm.c_cc[VMIN] = 1 ;
	tm.c_cc[VTIME] = 0 ;
	tcsetattr( STDIN_FILENO , TCSAFLUSH , &tm) ;

	/* Disable backscrolling of default console. */
	set_use_console_backscroll( 0) ;

	if( kbd_num == -1 ||
	    ( _display.fd = open_event_device( kbd_num)) == -1)
	{
		_display.fd = STDIN_FILENO ;
	}
	else
	{
		kik_file_set_cloexec( _display.fd) ;
	}

	_disp.display = &_display ;

	if( mouse_num == -1)
	{
		_mouse.fd = -1 ;
	}
	else if( ( _mouse.fd = open_event_device( mouse_num)) != -1)
	{
		kik_file_set_cloexec( _mouse.fd) ;
		_mouse.x = _display.width / 2 ;
		_mouse.y = _display.height / 2 ;
		_disp_mouse.display = (Display*)&_mouse ;
	}

	console_id = get_active_console() ;

	return  1 ;

error:
	if( _display.fb)
	{
		munmap( _display.fb , _display.smem_len) ;
		_display.fb = NULL ;
	}

	close( _display.fb_fd) ;

	return  0 ;
}

static int
receive_mouse_event(void)
{
	struct input_event  ev ;

	while( read( _mouse.fd , &ev , sizeof(ev)) > 0)
	{
		if( console_id != get_active_console())
		{
			return  0 ;
		}

		if( ev.type == EV_ABS)
		{
			if( ev.code == ABS_PRESSURE)
			{
				ev.type = EV_KEY ;
				ev.code = BTN_LEFT ;
				ev.value = 1 ;	/* ButtonPress */
			}
			else if( ev.code == ABS_X)
			{
				ev.type = EV_REL ;
				ev.code = REL_X ;
				ev.value = ev.value - _mouse.x ;
			}
			else if( ev.code == ABS_Y)
			{
				ev.type = EV_REL ;
				ev.code = REL_Y ;
				ev.value = ev.value - _mouse.y ;
			}
			else
			{
				continue ;
			}
		}

		if( ev.type == EV_KEY)
		{
			XButtonEvent  xev ;
			x_window_t *  win ;

			if( ev.code == BTN_LEFT)
			{
				xev.button = Button1 ;
				_mouse.button_state = Button1Mask ;
			}
			else if( ev.code == BTN_MIDDLE)
			{
				xev.button = Button2 ;
				_mouse.button_state = Button2Mask ;
			}
			else if( ev.code == BTN_RIGHT)
			{
				xev.button = Button3 ;
				_mouse.button_state = Button3Mask ;
			}
			else
			{
				continue ;

				while(1)
				{
				button4:
					xev.button = Button4 ;
					_mouse.button_state = Button4Mask ;
					break ;

				button5:
					xev.button = Button5 ;
					_mouse.button_state = Button5Mask ;
					break ;

				button6:
					xev.button = Button6 ;
					_mouse.button_state = Button6Mask ;
					break ;

				button7:
					xev.button = Button7 ;
					_mouse.button_state = Button7Mask ;
					break ;
				}

				ev.value = 1 ;
			}

			if( ev.value == 1)
			{
				xev.type = ButtonPress ;
			}
			else if( ev.value == 0)
			{
				xev.type = ButtonRelease ;

				/* Reset button_state in releasing button. */
				_mouse.button_state = 0 ;
			}
			else
			{
				continue ;
			}

			xev.time = ev.time.tv_sec * 1000 + ev.time.tv_usec / 1000 ;
			xev.x = _mouse.x ;
			xev.y = _mouse.y ;
			xev.state = _display.key_state ;

		#ifdef  __DEBUG
			kik_debug_printf( KIK_DEBUG_TAG
				"Button is %s x %d y %d btn %d time %d\n" ,
				xev.type == ButtonPress ? "pressed" : "released" ,
				xev.x , xev.y , xev.button , xev.time) ;
		#endif

			if( ! check_virtual_kbd( &xev))
			{
				win = get_window( xev.x , xev.y) ;
				xev.x -= win->x ;
				xev.y -= win->y ;

				x_window_receive_event( win , &xev) ;
			}
		}
		else if( ev.type == EV_REL)
		{
			XMotionEvent  xev ;
			x_window_t *  win ;

			if( ev.code == REL_X)
			{
				restore_hidden_region() ;

				_mouse.x += (int)ev.value ;

				if( _mouse.x < 0)
				{
					_mouse.x = 0 ;
				}
				else if( _display.width <= _mouse.x)
				{
					_mouse.x = _display.width - 1 ;
				}
			}
			else if( ev.code == REL_Y)
			{
				restore_hidden_region() ;

				_mouse.y += (int)ev.value ;

				if( _mouse.y < 0)
				{
					_mouse.y = 0 ;
				}
				else if( _display.height <= _mouse.y)
				{
					_mouse.y = _display.height - 1 ;
				}
			}
			else if( ev.code == REL_WHEEL)
			{
				if( ev.value > 0)
				{
					/* Up */
					goto  button4 ;
				}
				else if( ev.value < 0)
				{
					/* Down */
					goto  button5 ;
				}
			}
			else if( ev.code == REL_HWHEEL)
			{
				if( ev.value < 0)
				{
					/* Left */
					goto  button6 ;
				}
				else if( ev.value > 0)
				{
					/* Right */
					goto  button7 ;
				}
			}
			else
			{
				continue ;
			}

			update_mouse_cursor_state() ;

			xev.type = MotionNotify ;
			xev.x = _mouse.x ;
			xev.y = _mouse.y ;
			xev.time = ev.time.tv_sec * 1000 + ev.time.tv_usec / 1000 ;
			xev.state = _mouse.button_state | _display.key_state ;

		#ifdef  __DEBUG
			kik_debug_printf( KIK_DEBUG_TAG
				" Button is moved %d x %d y %d btn %d time %d\n" ,
				xev.type , xev.x , xev.y , xev.state , xev.time) ;
		#endif

			win = get_window( xev.x , xev.y) ;
			xev.x -= win->x ;
			xev.y -= win->y ;

			x_window_receive_event( win , &xev) ;

			save_hidden_region() ;
			draw_mouse_cursor() ;
		}
	}

	return  1 ;
}

static int
receive_key_event(void)
{
	if( _display.fd == STDIN_FILENO)
	{
		return  receive_stdin_key_event() ;
	}
	else
	{
		struct input_event  ev ;

		while( read( _display.fd , &ev , sizeof(ev)) > 0)
		{
			if( console_id != get_active_console())
			{
				return  0 ;
			}

			if( ev.type == EV_KEY &&
			    ev.code < 0x100 /* Key event is less than 0x100 */)
			{
				if( ev.value == 1 /* Pressed */ || ev.value == 2 /* auto repeat */)
				{
					if( ev.code == KEY_RIGHTSHIFT ||
					    ev.code == KEY_LEFTSHIFT)
					{
						_display.key_state |= ShiftMask ;
					}
					else if( ev.code == KEY_CAPSLOCK)
					{
						if( _display.key_state & ShiftMask)
						{
							_display.key_state &= ~ShiftMask ;
						}
						else
						{
							_display.key_state |= ShiftMask ;
						}
					}
					else if( ev.code == KEY_RIGHTCTRL ||
					         ev.code == KEY_LEFTCTRL)
					{
						_display.key_state |= ControlMask ;
					}
					else if( ev.code == KEY_RIGHTALT ||
					         ev.code == KEY_LEFTALT)
					{
						_display.key_state |= ModMask ;
					}
					else if( ev.code == KEY_NUMLOCK)
					{
						_display.lock_state ^= NLKED ;
					}
					else
					{
						XKeyEvent  xev ;

						xev.type = KeyPress ;
						xev.ksym = kcode_to_ksym( ev.code ,
								_display.key_state) ;
						xev.keycode = ev.code ;
						xev.state = _mouse.button_state |
							    _display.key_state ;

						receive_event_for_multi_roots( &xev) ;
					}
				}
				else if( ev.value == 0 /* Released */)
				{
					if( ev.code == KEY_RIGHTSHIFT ||
					    ev.code == KEY_LEFTSHIFT)
					{
						_display.key_state &= ~ShiftMask ;
					}
					else if( ev.code == KEY_RIGHTCTRL ||
					         ev.code == KEY_LEFTCTRL)
					{
						_display.key_state &= ~ControlMask ;
					}
					else if( ev.code == KEY_RIGHTALT ||
					         ev.code == KEY_LEFTALT)
					{
						_display.key_state &= ~ModMask ;
					}
				}
			}
		}
	}

	return  1 ;
}

#endif	/* FreeBSD/NetBSD/OpenBSD/linux */


/* --- global functions --- */

x_display_t *
x_display_open(
	char *  disp_name ,
	u_int  depth
	)
{
	if( ! DISP_IS_INITED)
	{
		if( ! open_display())
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

		if( CMAP_IS_INITED)
		{
			cmap_final() ;
		}

	#ifdef  ENABLE_DOUBLE_BUFFER
		free( _display.back_fb) ;
	#endif

		write( STDIN_FILENO , "\x1b[?25h" , 6) ;
		tcsetattr( STDIN_FILENO , TCSAFLUSH , &orig_tm) ;

	#if  defined(__FreeBSD__)
		ioctl( STDIN_FILENO , KDSKBMODE , K_XLATE) ;
	#elif  defined(__NetBSD__)
	#if  _MACHINE == x68k
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

	return  _display.cmap && cmap_init() ;
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
	u_char *  fb ;
	u_int  ppb ;
	u_char *  buf ;
	int  y_off ;

	fb = get_fb( x , y) ;

	if( ( ppb = _display.pixels_per_byte) > 1)
	{
		u_int  bpp ;
		u_char *  fb_end ;
		u_char *  buf_end ;
		u_int  surplus ;
		u_int  surplus_end ;
		int  packed_pixel ;
		u_int  count ;
		int  shift ;

		fb_end = get_fb( x + width , y) ;
	#ifdef  ENABLE_DOUBLE_BUFFER
		fb_end = _display.back_fb + ( fb_end - _display.fb) ;
	#else
		if( ! ( buf = alloca( fb_end - fb + 1)))
		{
			return ;
		}

		buf_end = buf + (fb_end - fb) ;
	#endif

		bpp = 8 / ppb ;
		surplus = MOD_PPB(x,ppb) ;
		surplus_end = MOD_PPB(x+width,ppb) ;

		packed_pixel = 0 ;
		if( pixel)
		{
			if( ppb == 8)
			{
				packed_pixel = 0xff ;
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
							pix |= (pixel << shift) ;

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
						pix |= (pixel << shift) ;

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

	/* XXX cheap implementation. */
	restore_hidden_region() ;

	/* XXX could be different from FB_WIDTH_BYTES(display, dst_x, width) */
	copy_len = FB_WIDTH_BYTES(&_display, src_x, width) ;

	if( src_y <= dst_y)
	{
		src = get_fb( src_x , src_y + height - 1) ;
		dst = get_fb( dst_x , dst_y + height - 1) ;

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
		src = get_fb( src_x , src_y) ;
		dst = get_fb( dst_x , dst_y) ;

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
