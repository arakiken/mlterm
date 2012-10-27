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

#include  <linux/kd.h>
#include  <linux/keyboard.h>

#include  <kiklib/kik_debug.h>
#include  <kiklib/kik_privilege.h>	/* kik_priv_change_e(u|g)id */
#include  <kiklib/kik_unistd.h>		/* kik_getuid */
#include  <kiklib/kik_util.h>		/* K_MIN/K_MAX */

#include  "../x_window.h"


#define  DISP_IS_INITED   (_disp.display)
#define  MOUSE_IS_INITED  (_mouse.fd != -1)

#if  0
#define  READ_CTRL_KEYMAP
#endif


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

static struct termios  orig_tm ;

/* Parameters of cursor_shape */
#define  CURSOR_WIDTH   5
#define  CURSOR_X_OFF   -2
#define  CURSOR_HEIGHT  13
#define  CURSOR_Y_OFF   -6

static char *  cursor_shape =
{
	"*****"
	"  *  "
	"  *  "
	"  *  "
	"  *  "
	"  *  "
	"  *  "
	"  *  "
	"  *  "
	"  *  "
	"  *  "
	"  *  "
	"*****"
} ;


/* --- static functions --- */

static int
get_key_state(void)
{
	int  ret ;
	char  state ;

	kik_priv_restore_euid() ;
	kik_priv_restore_egid() ;

	state = 6 ;
	ret = ioctl( STDIN_FILENO , TIOCLINUX , &state) ;

	kik_priv_change_euid( kik_getuid()) ;
	kik_priv_change_egid( kik_getgid()) ;

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
	if( kcode == KEY_ENTER)
	{
		/* KDGKBENT returns '\n' */
		return  '\r' ;
	}
	else if( kcode <= KEY_SPACE || kcode == KEY_YEN || kcode == KEY_RO)
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

		kik_priv_restore_euid() ;
		kik_priv_restore_egid() ;

		ret = ioctl( STDIN_FILENO , KDGKBENT , &ent) ;

		kik_priv_change_euid( kik_getuid()) ;
		kik_priv_change_egid( kik_getgid()) ;

		if( ret != -1 && ent.kb_value != K_HOLE && ent.kb_value != K_NOSUCHMAP)
		{
			ent.kb_value &= 0xff ;

		#ifndef  READ_CTRL_KEYMAP
			if( ( state & ControlMask) &&
			    ( ent.kb_value == ' ' ||
			      /*
			       * Control + '@'(0x40) ... '_'(0x5f) -> 0x00 ... 0x1f
			       *
			       * Not "<= '_'" but "<= 'z'" because Control + 'a' is not
			       * distinguished from Control + 'A'.
			       */
			     ('@' <= ent.kb_value && ent.kb_value <= 'z')) )
			{
				ent.kb_value &= 0x1f ;
			}
		#endif

			return  ent.kb_value ;
		}
	}

	return  kcode + 0x100 ;
}

static void
get_mouse_event_device_num(
	int *  kbd ,
	int *  mouse
	)
{
	char  class[] = "/sys/class/input/eventN/device/name" ;
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

				if( strstr( buf , "mouse"))
				{
					*mouse = count ;
				}
				else if( strstr( buf , "key"))
				{
					*kbd = count ;
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

static inline x_window_t *
get_window(
	int  x ,	/* X in display */
	int  y		/* Y in display */
	)
{
	return  _disp.roots[0] ;
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
	if( _mouse.cursor.x + _mouse.cursor.width > _disp.width)
	{
		_mouse.cursor.width -=
			(_mouse.cursor.x + _mouse.cursor.width - _disp.width) ;
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
	if( _mouse.cursor.y + _mouse.cursor.height > _disp.height)
	{
		_mouse.cursor.height -=
			(_mouse.cursor.y + _mouse.cursor.height - _disp.height) ;
	}
}

static void
restore_hidden_region(void)
{
	u_char *  fb ;
	int  count ;

	if( ! _mouse.cursor.is_drawn)
	{
		return ;
	}

	/* Set 0 before window_exposed is called. */
	_mouse.cursor.is_drawn = 0 ;

	if( _mouse.hidden_region_saved)
	{
		fb = x_display_get_fb( &_display ,
			_mouse.cursor.x , _mouse.cursor.y) ;

		for( count = 0 ; count < _mouse.cursor.height ; count++)
		{
			memcpy( fb ,
				_mouse.saved_image +
					count * _mouse.cursor.width *
					_display.bytes_per_pixel ,
				_mouse.cursor.width * _display.bytes_per_pixel) ;
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
				_mouse.cursor.x - win->margin ,
				_mouse.cursor.y - win->margin ,
				_mouse.cursor.width ,
				_mouse.cursor.height) ;
		}
	}
}

static void
save_hidden_region(void)
{
	u_char *  fb ;
	int  count ;

	fb = x_display_get_fb( &_display , _mouse.cursor.x , _mouse.cursor.y) ;

	for( count = 0 ; count < _mouse.cursor.height ; count++)
	{
		memcpy( _mouse.saved_image +
				count * _mouse.cursor.width * _display.bytes_per_pixel ,
			fb , _mouse.cursor.width * _display.bytes_per_pixel) ;
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
	u_char  image[CURSOR_WIDTH * sizeof(u_int32_t)] ;
	int  x ;

	fb = x_display_get_fb( &_display , _mouse.cursor.x ,
			_mouse.cursor.y + y) ;
	win = get_window( _mouse.x , _mouse.y) ;

	for( x = 0 ; x < _mouse.cursor.width ; x++)
	{
		if( cursor_shape[(_mouse.cursor.y_off + y) * CURSOR_WIDTH + _mouse.cursor.x_off + x] == '*')
		{
			switch( _display.bytes_per_pixel)
			{
			case  1:
				image[x] = win->fg_color.pixel ;
				break ;

			case  2:
				((u_int16_t*)image)[x] = win->fg_color.pixel ;
				break ;

			case  4:
				((u_int32_t*)image)[x] = win->fg_color.pixel ;
				break ;
			}
		}
		else
		{
			switch( _display.bytes_per_pixel)
			{
			case  1:
				image[x] = fb[x] ;
				break ;

			case  2:
				((u_int16_t*)image)[x] = ((u_int16_t*)fb)[x] ;
				break ;

			case  4:
				((u_int32_t*)image)[x] = ((u_int32_t*)fb)[x] ;
				break ;
			}
		}
	}

	memcpy( fb , image , _mouse.cursor.width * _display.bytes_per_pixel) ;
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


/* --- global functions --- */

x_display_t *
x_display_open(
	char *  disp_name ,
	u_int  depth
	)
{
	if( ! DISP_IS_INITED)
	{
		char *  dev ;
		struct  fb_fix_screeninfo  finfo ;
		struct  fb_var_screeninfo  vinfo ;
		struct termios  tm ;
		int  kbd_num ;
		int  mouse_num ;

		get_mouse_event_device_num( &kbd_num , &mouse_num) ;

		if( ! ( _display.fb_fd = open( ( dev = getenv("FRAMEBUFFER")) ? dev : "/dev/fb0" ,
						O_RDWR)))
		{
			return  NULL ;
		}

		ioctl( _display.fb_fd , FBIOGET_FSCREENINFO , &finfo) ;
		ioctl( _display.fb_fd , FBIOGET_VSCREENINFO , &vinfo) ;

		if( ( _display.fp = mmap( NULL , (_display.smem_len = finfo.smem_len) ,
					PROT_WRITE|PROT_READ , MAP_SHARED , _display.fb_fd , 0))
					== MAP_FAILED)
		{
			close( _display.fb_fd) ;

			return  NULL ;
		}

		_display.line_length = finfo.line_length ;
		_display.xoffset = vinfo.xoffset ;
		_display.yoffset = vinfo.yoffset ;

		if( ( _display.bytes_per_pixel = (vinfo.bits_per_pixel + 7) / 8) == 3)
		{
			_display.bytes_per_pixel = 4 ;
		}

		_display.rgbinfo.r_limit = 8 - vinfo.red.length ;
		_display.rgbinfo.g_limit = 8 - vinfo.green.length ;
		_display.rgbinfo.b_limit = 8 - vinfo.blue.length ;
		_display.rgbinfo.r_offset = vinfo.red.offset ;
		_display.rgbinfo.g_offset = vinfo.green.offset ;
		_display.rgbinfo.b_offset = vinfo.blue.offset ;

		tcgetattr( STDIN_FILENO , &tm) ;
		orig_tm = tm ;
		tm.c_iflag = tm.c_oflag = 0 ;
		tm.c_cflag &= ~CSIZE ;
		tm.c_cflag |= CS8 ;
		tm.c_lflag &= ~(ECHO|ISIG|ICANON) ;
		tm.c_cc[VMIN] = 1 ;
		tm.c_cc[VTIME] = 0 ;
		tcsetattr( STDIN_FILENO , TCSAFLUSH , &tm) ;

		fcntl( STDIN_FILENO , F_SETFL ,
			fcntl( STDIN_FILENO , F_GETFL , 0) | O_NONBLOCK) ;

		/* Hide the cursor of linux console. */
		write( STDIN_FILENO , "\x1b[?25l" , 6) ;

		if( kbd_num == -1 ||
		    ( _display.fd = open_event_device( kbd_num)) == -1)
		{
			_display.fd = STDIN_FILENO ;
		}

		_disp.depth = vinfo.bits_per_pixel ;

	#if  1
		_disp.width = vinfo.xres ;
		_disp.height = vinfo.yres ;
	#else
		_disp.width = _display.line_length / _display.bytes_per_pixel ;
		_disp.height = _display.smem_len / _display.line_length ;
	#endif

	#ifdef  DEBUG
		kik_debug_printf( KIK_DEBUG_TAG " Framebuffer: "
			"len %d line_len %d xoff %d yoff %d depth %db/%dB w %d h %d\n" ,
			_display.smem_len ,
			_display.line_length ,
			_display.xoffset ,
			_display.yoffset ,
			_disp.depth ,
			_display.bytes_per_pixel ,
			_disp.width ,
			_disp.height) ;
	#endif

		_disp.display = &_display ;

		if( mouse_num == -1)
		{
			_mouse.fd = -1 ;
		}
		else if( ( _mouse.fd = open_event_device( mouse_num)) != -1)
		{
			_mouse.x = _disp.width / 2 ;
			_mouse.y = _disp.height / 2 ;
			_disp_mouse.display = (Display*)&_mouse ;
		}
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
		if( MOUSE_IS_INITED)
		{
			close( _mouse.fd) ;
		}

		munmap( _display.fp , _display.smem_len) ;
		close( _display.fb_fd) ;

		write( STDIN_FILENO , "\x1b[?25h" , 6) ;
		tcsetattr( STDIN_FILENO , TCSAFLUSH , &orig_tm) ;

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

	/* XXX Only one root window per display. */
	if( disp->num_of_roots > 0)
	{
		return  0 ;
	}

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
			x_window_unmap( root) ;
			x_window_final( root) ;

			disp->num_of_roots -- ;

			if( count == disp->num_of_roots)
			{
				memset( &disp->roots[count] , 0 , sizeof( disp->roots[0])) ;
			}
			else
			{
				memcpy( &disp->roots[count] ,
					&disp->roots[disp->num_of_roots] ,
					sizeof( disp->roots[0])) ;
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
		struct input_event  ev ;

		if( read( _mouse.fd , &ev , sizeof(ev)) > 0)
		{
			if( ev.type == EV_KEY)
			{
				XButtonEvent  xev ;

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
					return  1 ;

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
					return  1 ;
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

				x_window_receive_event( get_window( xev.x , xev.y) , &xev) ;
			}
			else if( ev.type == EV_REL)
			{
				XMotionEvent  xev ;

				if( ev.code == REL_X)
				{
					restore_hidden_region() ;

					_mouse.x += (int)ev.value ;

					if( _mouse.x < 0)
					{
						_mouse.x = 0 ;
					}
					else if( _disp.width <= _mouse.x)
					{
						_mouse.x = _disp.width - 1 ;
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
					else if( _disp.height <= _mouse.y)
					{
						_mouse.y = _disp.height - 1 ;
					}
				}
				else if( ev.code == REL_WHEEL)
				{
					if( ev.value > 0)
					{
						goto  button4 ;
					}
					else if( ev.value < 0)
					{
						goto  button5 ;
					}
				}
				else
				{
					return  0 ;
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

				x_window_receive_event( get_window( xev.x , xev.y) , &xev) ;

				save_hidden_region() ;
				draw_mouse_cursor() ;
			}
		}
	}
	else if( disp->display->fd == STDIN_FILENO)
	{
		u_char  buf[1] ;
		ssize_t  len ;

		if( ( len = read( disp->display->fd , buf , sizeof(buf))) > 0)
		{
			XKeyEvent  xev ;

			xev.type = KeyPress ;
			xev.ksym = buf[0] ;
			xev.state = get_key_state() ;

			x_window_receive_event( disp->roots[0] , &xev) ;
		}
	}
	else
	{
		struct input_event  ev ;

		if( read( disp->display->fd , &ev , sizeof(ev)) > 0)
		{
			if( ev.type == EV_KEY &&
			    ev.code < 0x100 /* Key event is less than 0x100 */)
			{
				if( ev.value == 1 /* Pressed */ || ev.value == 2 /* auto repeat */)
				{
					if( ev.code == KEY_RIGHTSHIFT ||
					    ev.code == KEY_LEFTSHIFT)
					{
						disp->display->key_state |= ShiftMask ;
					}
					else if( ev.code == KEY_CAPSLOCK)
					{
						if( disp->display->key_state & ShiftMask)
						{
							disp->display->key_state &= ~ShiftMask ;
						}
						else
						{
							disp->display->key_state |= ShiftMask ;
						}
					}
					else if( ev.code == KEY_RIGHTCTRL ||
					         ev.code == KEY_LEFTCTRL)
					{
						disp->display->key_state |= ControlMask ;
					}
					else if( ev.code == KEY_RIGHTALT ||
					         ev.code == KEY_LEFTALT)
					{
						disp->display->key_state |= ModMask ;
					}
					else
					{
						XKeyEvent  xev ;

						xev.type = KeyPress ;
						xev.ksym = kcode_to_ksym( ev.code ,
								disp->display->key_state) ;
						xev.state = _mouse.button_state |
							    disp->display->key_state ;

						x_window_receive_event( disp->roots[0] , &xev) ;
					}
				}
				else if( ev.value == 0 /* Released */)
				{
					if( ev.code == KEY_RIGHTSHIFT ||
					    ev.code == KEY_LEFTSHIFT)
					{
						disp->display->key_state &= ~ShiftMask ;
					}
					else if( ev.code == KEY_RIGHTCTRL ||
					         ev.code == KEY_LEFTCTRL)
					{
						disp->display->key_state &= ~ControlMask ;
					}
					else if( ev.code == KEY_RIGHTALT ||
					         ev.code == KEY_LEFTALT)
					{
						disp->display->key_state &= ~ModMask ;
					}
				}
			}
		}
	}

	return  1 ;
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

u_char *
x_display_get_fb(
	Display *  display ,
	int  x ,
	int  y
	)
{
	return  display->fp + (display->yoffset + y) * display->line_length +
			(display->xoffset + x) * display->bytes_per_pixel ;
}

void
x_display_put_image(
	Display *  display ,
	int  x ,
	int  y ,
	u_char *  image ,
	size_t  size
	)
{
	memmove( x_display_get_fb( display , x , y) , image , size) ;

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

void
x_display_copy_line(
	Display *  display ,
	int  src_x ,
	int  src_y ,
	int  dst_x ,
	int  dst_y ,
	u_int  width
	)
{
	/* XXX cheap implementation. */
	restore_hidden_region() ;

	memmove( x_display_get_fb( display , dst_x , dst_y) ,
		x_display_get_fb( display , src_x , src_y) ,
		width * display->bytes_per_pixel) ;
}
