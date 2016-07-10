/*
 *	$Id$
 */

#include  "x_display.h"

#include  <stdio.h>	/* printf */
#include  <unistd.h>	/* STDIN_FILENO */
#include  <fcntl.h>	/* fcntl */
#include  <sys/ioctl.h>	/* ioctl */
#include  <string.h>	/* memset/memcpy */
#include  <termios.h>
#include  <signal.h>
#include  <time.h>

#include  <kiklib/kik_debug.h>
#include  <kiklib/kik_privilege.h>	/* kik_priv_change_e(u|g)id */
#include  <kiklib/kik_unistd.h>		/* kik_getuid */
#include  <kiklib/kik_file.h>
#include  <kiklib/kik_mem.h>		/* alloca */

#include  <ml_color.h>

#include  "../x_window.h"
#include  "../x_picture.h"


#define  DISP_IS_INITED   (_disp.display)


/* --- static variables --- */

static Display  _display ;
static x_display_t  _disp ;
static int  use_ansi_colors = 1 ;

static struct termios  orig_tm ;

static ml_char_encoding_t  encoding = ML_UTF8 ;


/* --- static functions --- */

#ifdef  __linux__
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
#else
#define get_key_state()  (0)
#endif

static inline x_window_t *
get_window_intern(
	x_window_t *  win ,
	int  x ,
	int  y
	)
{
	u_int  count ;

	for( count = 0 ; count < win->num_of_children ; count++)
	{
		x_window_t *  child ;

		if( ( child = win->children[count])->is_mapped)
		{
			if( child->x <= x && x < child->x + ACTUAL_WIDTH(child) &&
			    child->y <= y && y < child->y + ACTUAL_HEIGHT(child))
			{
				return  get_window_intern( child ,
						x - child->x , y - child->y) ;
			}
		}
	}

	return  win ;
}

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
	return  get_window_intern( _disp.roots[0] , x , y) ;
}

/* XXX defined in console/x_window.c */
int  x_window_clear_margin_area( x_window_t *  win) ;

static void
expose_window(
	x_window_t *  win ,
	int  x ,
	int  y ,
	u_int  width ,
	u_int  height
	)
{
	if( x + width <= win->x || win->x + ACTUAL_WIDTH(win) < x ||
	    y + height <= win->y || win->y + ACTUAL_HEIGHT(win) < y)
	{
		return ;
	}

	if( x < win->x + win->hmargin || y < win->y + win->vmargin ||
	    x - win->x + width > win->hmargin + win->width ||
	    y - win->y + height > win->vmargin + win->height)
	{
		x_window_clear_margin_area( win) ;
	}

	if( win->window_exposed)
	{
		if( x < win->x + win->hmargin)
		{
			width -= (win->x + win->hmargin - x) ;
			x = 0 ;
		}
		else
		{
			x -= (win->x + win->hmargin) ;
		}

		if( y < win->y + win->vmargin)
		{
			height -= (win->y + win->vmargin - y) ;
			y = 0 ;
		}
		else
		{
			y -= (win->y + win->vmargin) ;
		}

		(*win->window_exposed)( win , x , y , width , height) ;
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
	u_int  count ;

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

	expose_window( _disp.roots[0] , x , y , width , height) ;

	for( count = 0 ; count < _disp.roots[0]->num_of_children ; count++)
	{
		expose_window( _disp.roots[0]->children[count] , x , y , width , height) ;
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

static void
set_blocking(
	int  fd ,
	int  block
	)
{
	fcntl( fd , F_SETFL ,
		block ? (fcntl( fd , F_GETFL , 0) & ~O_NONBLOCK) :
		        (fcntl( fd , F_GETFL , 0) | O_NONBLOCK)) ;
}

static int
receive_stdin(void)
{
	ssize_t  len ;

	if( ( len = read( fileno( _display.fp) , _display.buf + _display.buf_len ,
				sizeof(_display.buf) - _display.buf_len - 1)) > 0)
	{
		_display.buf_len += len ;
		_display.buf[_display.buf_len] = '\0' ;

		return  1 ;
	}
	else
	{
		return  0 ;
	}
}

static u_char *
skip_range(
	u_char *  seq ,
	int  beg ,
	int  end
	)
{
	while( beg <= *seq && *seq <= end)
	{
		seq ++ ;
	}

	return  seq ;
}

static int
parse(
	u_char **  param ,
	u_char **  intermed ,
	u_char **  ft ,
	u_char *  seq
	)
{
	*param = seq ;

	if( ( *intermed = skip_range( *param , 0x30 , 0x3f)) == *param)
	{
		*param = NULL ;
	}

	if( ( *ft = skip_range( *intermed , 0x20 , 0x2f)) == *intermed)
	{
		*intermed = NULL ;
	}

	if( 0x40 <= **ft && **ft <= 0x7e)
	{
		return  1 ;
	}
	else
	{
		return  0 ;
	}
}

/* Same as fb/x_display */
static int
receive_stdin_key_event(void)
{
	u_char *  p ;
	int  esc_alone = 0 ;

	p = _display.buf ;

	receive_stdin() ;

	while( *p)
	{
		XKeyEvent  kev ;
		u_char *  param ;
		u_char *  intermed ;
		u_char *  ft ;

		kev.type = KeyPress ;
		kev.state = get_key_state() ;
		kev.ksym = 0 ;
		kev.keycode = 0 ;

		if( *p == '\x1b' && p[1] == '\x0')
		{
			if( ! esc_alone)
			{
				usleep( 1000) ;
				set_blocking( fileno( _display.fp) , 0) ;
				if( ! receive_stdin())
				{
					break ;
				}
				set_blocking( fileno( _display.fp) , 1) ;
				esc_alone = 1 ;

				continue ;
			}

			esc_alone = 0 ;
		}

		if( *p == '\x1b' && ( p[1] == '[' || p[1] == 'O'))
		{
			if( p[1] == '[')
			{
				u_char *  tmp ;

				if( ! parse( &param , &intermed , &ft , p + 2))
				{
					set_blocking( fileno( _display.fp) , 0) ;
					if( ! receive_stdin())
					{
						break ;
					}

					set_blocking( fileno( _display.fp) , 1) ;
					continue ;
				}

				p = ft + 1 ;

				if( *ft == '~')
				{
					if( param && ! intermed)
					{
						switch( atoi( param))
						{
						case  2:
							kev.ksym = XK_Insert ;
							break ;
						case  3:
							kev.ksym = XK_Delete ;
							break ;
						case  5:
							kev.ksym = XK_Prior ;
							break ;
						case  6:
							kev.ksym = XK_Next ;
							break ;
						case  7:
							kev.ksym = XK_Home ;
							break ;
						case  8:
							kev.ksym = XK_End ;
							break ;
						case  17:
							kev.ksym = XK_F6 ;
							break ;
						case  18:
							kev.ksym = XK_F7 ;
							break ;
						case  19:
							kev.ksym = XK_F8 ;
							break ;
						case  20:
							kev.ksym = XK_F9 ;
							break ;
						case  21:
							kev.ksym = XK_F10 ;
							break ;
						case  23:
							kev.ksym = XK_F11 ;
							break ;
						case  24:
							kev.ksym = XK_F12 ;
							break ;
						default:
							continue ;
						}
					}
				}
				else if( *ft == 'M' || *ft == 'm')
				{
					XButtonEvent  bev ;
					int  state ;
					struct timeval  tv ;
					x_window_t *  win ;

					if( *ft == 'M')
					{
						if( _display.is_pressing)
						{
							bev.type = MotionNotify ;
						}
						else
						{
							bev.type = ButtonPress ;
							_display.is_pressing = 1 ;
						}

						if( ! param)
						{
							state = *(p++) - 0x20 ;
							bev.x = *(p++) - 0x20 ;
							bev.y = *(p++) - 0x20 ;

							goto  make_event ;
						}
					}
					else
					{
						bev.type = ButtonRelease ;
						_display.is_pressing = 0 ;
					}

					*ft = '\0' ;
					if( ! param ||
					    sscanf( param , "<%d;%d;%d" ,
						&state , &bev.x , &bev.y) != 3)
					{
						continue ;
					}

				make_event:
					bev.button = (state & 0x2) + 1 ;
					if( bev.type == MotionNotify)
					{
						bev.state = Button1Mask << (bev.button - 1) ;
					}

					bev.x -- ;
					bev.x *= _display.col_width ;
					bev.y -- ;
					bev.y *= _display.line_height ;

					win = get_window( bev.x , bev.y) ;
					bev.x -= win->x ;
					bev.y -= win->y ;

					gettimeofday( &tv , NULL) ;
					bev.time = tv.tv_sec * 1000 + tv.tv_usec / 1000 ;
					bev.state = 0 ;

					set_blocking( fileno(_display.fp) , 1) ;
					x_window_receive_event( win , &bev) ;
					set_blocking( fileno(_display.fp) , 0) ;

					continue ;
				}
				else if( 'P' <= *ft && *ft <= 'S')
				{
					kev.ksym = XK_F1 + (*ft - 'P') ;
				}
			#ifdef  __FreeBSD__
				else if( 'Y' <= *ft && *ft <= 'Z')
				{
					kev.ksym = XK_F1 + (*ft - 'Y') ;
					kev.state = ShiftMask ;
				}
				else if( 'a' <= *ft && *ft <= 'j')
				{
					kev.ksym = XK_F3 + (*ft - 'a') ;
					kev.state = ShiftMask ;
				}
				else if( 'k' <= *ft && *ft <= 'v')
				{
					kev.ksym = XK_F1 + (*ft - 'k') ;
					kev.state = ControlMask ;
				}
				else if( 'w' <= *ft && *ft <= 'z')
				{
					kev.ksym = XK_F1 + (*ft - 'w') ;
					kev.state = ControlMask|ShiftMask ;
				}
				else if( *ft == '@')
				{
					kev.ksym = XK_F5 ;
					kev.state = ControlMask|ShiftMask ;
				}
				else if( '[' <= *ft && *ft <= '\`')
				{
					kev.ksym = XK_F6 + (*ft - '[') ;
					kev.state = ControlMask|ShiftMask ;
				}
				else if( *ft == '{')
				{
					kev.ksym = XK_F12 ;
					kev.state = ControlMask|ShiftMask ;
				}
			#endif
				else
				{
					switch( *ft)
					{
					case  'A':
						kev.ksym = XK_Up ;
						break ;
					case  'B':
						kev.ksym = XK_Down ;
						break ;
					case  'C':
						kev.ksym = XK_Right ;
						break ;
					case  'D':
						kev.ksym = XK_Left ;
						break ;
					case  'F':
						kev.ksym = XK_End ;
						break ;
					case  'H':
						kev.ksym = XK_Home ;
						break ;
					default:
						continue ;
					}
				}

				if( param && ( tmp = strchr( param , ';')))
				{
					param = tmp + 1 ;
				}
			}
			else /* if( p[1] == 'O') */
			{
				if( ! parse( &param , &intermed , &ft , p + 2))
				{
					set_blocking( fileno( _display.fp) , 0) ;
					if( ! receive_stdin())
					{
						break ;
					}
					set_blocking( fileno( _display.fp) , 1) ;

					continue ;
				}

				p = ft + 1 ;

				switch( *ft)
				{
				case  'P':
					kev.ksym = XK_F1 ;
					break ;
				case  'Q':
					kev.ksym = XK_F2 ;
					break ;
				case  'R':
					kev.ksym = XK_F3 ;
					break ;
				case  'S':
					kev.ksym = XK_F4 ;
					break ;
				default:
					continue ;
				}
			}

			if( param && '1' <= *param && *param <= '9')
			{
				int  state ;

				state = atoi( param) - 1 ;

				if( state & 0x1)
				{
					kev.state |= ShiftMask ;
				}
				if( state & 0x2)
				{
					kev.state |= ModMask ;
				}
				if( state & 0x4)
				{
					kev.state |= ControlMask ;
				}
			}
		}
		else
		{
			kev.ksym = *(p++) ;

			if( (u_int)kev.ksym <= 0x1f)
			{
				if( kev.ksym == '\0')
				{
					/* CTL+' ' instead of CTL+@ */
					kev.ksym = ' ' ;
				}
				else if( 0x01 <= kev.ksym && kev.ksym <= 0x1a)
				{
					/* Lower case alphabets instead of upper ones. */
					kev.ksym = kev.ksym + 0x60 ;
				}
				else
				{
					kev.ksym = kev.ksym + 0x40 ;
				}

				kev.state = ControlMask ;
			}
		}

		set_blocking( fileno(_display.fp) , 1) ;
		receive_event_for_multi_roots( &kev) ;
		set_blocking( fileno(_display.fp) , 0) ;
	}

	if( ( _display.buf_len = _display.buf + _display.buf_len - p) > 0)
	{
		memcpy( _display.buf , p , _display.buf_len + 1) ;
	}

	set_blocking( fileno(_display.fp) , 1) ;

	return  1 ;
}

static void
set_winsize(
	x_display_t *  disp
	)
{
	struct winsize  ws ;

	memset( &ws , 0 , sizeof(ws)) ;
	if( ioctl( fileno(disp->display->fp) , TIOCGWINSZ , &ws) == 0)
	{
		disp->width = ws.ws_xpixel ;
		disp->height = ws.ws_ypixel ;
	}

	if( ws.ws_col == 0)
	{
		kik_error_printf( "winsize.ws_col is 0\n") ;
		ws.ws_col = 80 ;
	}

	if( ws.ws_row == 0)
	{
		kik_error_printf( "winsize.ws_row is 0\n") ;
		ws.ws_row = 24 ;
	}

	if( disp->width == 0)
	{
		disp->width = ws.ws_col * 8 ;
	}
	disp->display->col_width = disp->width / ws.ws_col ;

	if( disp->height == 0)
	{
		disp->height = ws.ws_row * 16 ;
	}
	disp->display->line_height = disp->height / ws.ws_row ;
}

/* XXX */
int  x_font_cache_unload_all(void) ;

static void
sig_winch(
	int  sig
	)
{
	u_int  count ;

	set_winsize( &_disp) ;

	/* XXX */
	x_font_cache_unload_all() ;

	for( count = 0 ; count < _disp.num_of_roots ; count++)
	{
		x_window_resize_with_margin( _disp.roots[count] ,
			_disp.width , _disp.height , NOTIFY_TO_MYSELF) ;
	}
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
		struct termios  tio ;
		int  fd ;

		write( STDOUT_FILENO , "\x1b[?25l" , 6) ;
		write( STDOUT_FILENO , "\x1b[?1002h\x1b[?1006h" , 16) ;
		tcgetattr( STDIN_FILENO , &orig_tm) ;

		if( ! ( _display.fp = fopen( ttyname(STDIN_FILENO) , "r+")))
		{
			return  NULL ;
		}

		fd = fileno( _display.fp) ;

		close( STDIN_FILENO) ;
		close( STDOUT_FILENO) ;
		close( STDERR_FILENO) ;

		tio = orig_tm ;
		tio.c_iflag &= ~(IXON|IXOFF|ICRNL|INLCR|IGNCR|IMAXBEL|ISTRIP) ;
		tio.c_iflag |= IGNBRK ;
		tio.c_oflag &= ~(OPOST|ONLCR|OCRNL|ONLRET) ;
		tio.c_lflag &= ~(IEXTEN|ICANON|ECHO|ECHOE|ECHONL|ECHOCTL|ECHOPRT|ECHOKE|ECHOCTL|ISIG) ;
		tio.c_cc[VMIN] = 1 ;
		tio.c_cc[VTIME] = 0 ;

		tcsetattr( fd , TCSANOW , &tio) ;

		_display.conv = ml_conv_new( encoding) ;

		_disp.display = &_display ;

		set_winsize( &_disp) ;

		signal( SIGWINCH , sig_winch) ;
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
		tcsetattr( fileno(_display.fp) , TCSAFLUSH , &orig_tm) ;
		write( fileno(_display.fp) , "\x1b[?25h" , 6) ;
		write( fileno(_display.fp) , "\x1b[?1002l\x1b[?1006l" , 16) ;
		fclose( _display.fp) ;
		(*_display.conv->delete)(_display.conv) ;

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
		x_picture_display_closed( _disp.display) ;

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
	static x_display_t *  opened_disp = &_disp ;

	if( ! DISP_IS_INITED)
	{
		*num = 0 ;

		return  NULL ;
	}

	*num = 1 ;

	return  &opened_disp ;
}

int
x_display_fd(
	x_display_t *  disp
	)
{
	return  fileno( disp->display->fp) ;
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
	return  receive_stdin_key_event() ;
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

#undef  x_display_reset_cmap
#undef  x_display_set_use_ansi_colors

int
x_display_reset_cmap(void)
{
	return  1 ;
}

void
x_display_set_use_ansi_colors(
	int  use
	)
{
	return ;
}

void
x_display_set_char_encoding(
	ml_char_encoding_t  e
	)
{
	encoding = e ;
}
