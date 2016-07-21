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
#include  <errno.h>
#include  <sys/select.h>	/* select */

#include  <kiklib/kik_debug.h>
#include  <kiklib/kik_privilege.h>	/* kik_priv_change_e(u|g)id */
#include  <kiklib/kik_unistd.h>		/* kik_getuid */
#include  <kiklib/kik_file.h>
#include  <kiklib/kik_mem.h>		/* alloca */
#include  <kiklib/kik_conf_io.h>
#include  <kiklib/kik_net.h>
#include  <kiklib/kik_util.h>

#include  <ml_color.h>

#include  "../x_window.h"
#include  "../x_picture.h"


/* --- static variables --- */

static int  sock_fd = -1 ;

static x_display_t **  displays ;
static u_int  num_of_displays ;

static struct termios  orig_tm ;

static ml_char_encoding_t  encoding = ML_UTF8 ;


/* --- static functions --- */

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
set_winsize(
	x_display_t *  disp ,
	char *  seq
	)
{
	struct winsize  ws ;

	memset( &ws , 0 , sizeof(ws)) ;

	if( seq)
	{
		int  col ;
		int  row ;
		int  x ;
		int  y ;

		if( sscanf( seq , "8;%d;%d;4;%d;%dt" , &row , &col , &y , &x) != 4)
		{
			return  0 ;
		}

		ws.ws_col = col ;
		ws.ws_row = row ;
		disp->width = x ;
		disp->height = y ;
	}
	else
	{
		if( ioctl( fileno(disp->display->fp) , TIOCGWINSZ , &ws) == 0)
		{
			disp->width = ws.ws_xpixel ;
			disp->height = ws.ws_ypixel ;
		}
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
		disp->display->col_width = 8 ;
	}
	else
	{
		disp->display->col_width = disp->width / ws.ws_col ;
		disp->width = disp->display->col_width * ws.ws_col ;
	}

	if( disp->height == 0)
	{
		disp->height = ws.ws_row * 16 ;
		disp->display->line_height = 16 ;
	}
	else
	{
		disp->display->line_height = disp->height / ws.ws_row ;
		disp->height = disp->display->line_height * ws.ws_row ;
	}

	return  1 ;
}

/* XXX */
int  x_font_cache_unload_all(void) ;

static void
sig_winch(
	int  sig
	)
{
	u_int  count ;

	set_winsize( displays[0] , NULL) ;

	/* XXX */
	x_font_cache_unload_all() ;

	for( count = 0 ; count < displays[0]->num_of_roots ; count++)
	{
		x_window_resize_with_margin( displays[0]->roots[count] ,
			displays[0]->width , displays[0]->height , NOTIFY_TO_MYSELF) ;
	}

	signal( SIGWINCH , sig_winch) ;
}

static void
end_server(void)
{
	close( sock_fd) ;
}

static int
start_server(void)
{
	char *  path ;
	struct sockaddr_un  servaddr ;
	int  fd ;

	if( ! ( path = kik_get_user_rc_path( "mlterm/socket-con")))
	{
		return  0 ;
	}

	memset( &servaddr , 0 , sizeof( servaddr)) ;
	servaddr.sun_family = AF_LOCAL ;
	strcpy( servaddr.sun_path , path) ;
	free( path) ;
	path = servaddr.sun_path ;

	if( ( fd = socket( PF_LOCAL , SOCK_STREAM , 0)) < 0)
	{
		return  0 ;
	}
	kik_file_set_cloexec( fd) ;

	for( ;;)
	{
		int  ret ;
		int  saved_errno ;
		mode_t  mode ;

		mode = umask( 077) ;
		ret = bind( fd , (struct sockaddr *) &servaddr , sizeof( servaddr)) ;
		saved_errno = errno ;
		umask( mode) ;

		if( ret == 0)
		{
			break ;
		}
		else if( saved_errno == EADDRINUSE)
		{
			if( connect( fd , (struct sockaddr*) &servaddr , sizeof( servaddr)) == 0)
			{
				kik_msg_printf( "Disable server because another server "
					"has already started.\n") ;
				close( fd) ;
				sock_fd = -2 ;

				return  0 ;
			}

			if( unlink( path) == 0)
			{
				continue ;
			}
		}
		else
		{
			close( fd) ;

			return  0 ;
		}
	}

	if( listen( fd , 1024) < 0)
	{
		close( fd) ;
		unlink( path) ;

		return  0 ;
	}

	sock_fd = fd ;

	return  1 ;
}

/* XXX */
int  x_mlclient( char *  args , FILE *  fp) ;

static void
client_connected(void)
{
	struct sockaddr_un  addr ;
	socklen_t  sock_len ;
	int  fd ;
	char  ch ;
	char  cmd[1024] ;
	size_t  cmd_len ;
	int  dopt_added = 0 ;

	sock_len = sizeof( addr) ;

	if( ( fd = accept( sock_fd , (struct sockaddr *) &addr , &sock_len)) < 0)
	{
		return ;
	}

	for( cmd_len = 0 ; cmd_len < sizeof(cmd) - 1 ; cmd_len++)
	{
		if( read( fd , &ch , 1) <= 0)
		{
			close( fd) ;

			return ;
		}

		if( ch == '\n')
		{
			break ;
		}
		else
		{
			if( ! dopt_added && ch == ' ')
			{
				if( cmd_len + 11 + DIGIT_STR_LEN(fd) + 1 > sizeof(cmd))
				{
					close( fd) ;

					return ;
				}

				sprintf( cmd + cmd_len , " -d client:%d" , fd) ;
				cmd_len += strlen( cmd + cmd_len) ;
				dopt_added = 1 ;
			}

			cmd[cmd_len] = ch ;
		}
	}
	cmd[cmd_len] = '\0' ;

	x_mlclient( cmd , stdout) ;

	if( num_of_displays == 0 ||
	    fileno( displays[num_of_displays - 1]->display->fp) != fd)
	{
		close( fd) ;
	}
}

static x_display_t *
open_display_socket(
	int  fd
	)
{
	void *  p ;

	if( ! ( p = realloc( displays , sizeof(x_display_t*) * (num_of_displays + 1))))
	{
		return  NULL ;
	}

	displays = p ;

	if( ! ( displays[num_of_displays] = calloc( 1 , sizeof( x_display_t))) ||
	    ! ( displays[num_of_displays]->display = calloc( 1 , sizeof( Display))))
	{
		free( displays[num_of_displays]) ;

		return  NULL ;
	}

	if( ! ( displays[num_of_displays]->display->fp = fdopen( fd , "r+")))
	{
		free( displays[num_of_displays]->display) ;
		free( displays[num_of_displays]) ;

		return  NULL ;
	}

	/*
	 * Set the close-on-exec flag.
	 * If this flag off, this fd remained open until the child process forked in
	 * open_screen_intern()(ml_term_open_pty()) close it.
	 */
	kik_file_set_cloexec( fd) ;
	set_blocking( fd , 1) ;

	write( fd , "\x1b[?25l" , 6) ;
	write( fd , "\x1b[?1002h\x1b[?1006h" , 16) ;

	displays[num_of_displays]->display->conv = ml_conv_new( encoding) ;
	set_winsize( displays[num_of_displays] , "8;24;80;4;384;640t") ;

	return  displays[num_of_displays++] ;
}

static x_display_t *
open_display_console(void)
{
	void *  p ;
	struct termios  tio ;
	int  fd ;

	if( num_of_displays > 0 || ! isatty( STDIN_FILENO) ||
	    ! ( p = realloc( displays , sizeof(x_display_t*))))
	{
		return  NULL ;
	}

	displays = p ;

	if( ! ( displays[0] = calloc( 1 , sizeof( x_display_t))) ||
	    ! ( displays[0]->display = calloc( 1 , sizeof(Display))))
	{
		free( displays[0]) ;

		return  NULL ;
	}

	tcgetattr( STDIN_FILENO , &orig_tm) ;

	if( ! ( displays[0]->display->fp = fopen( ttyname(STDIN_FILENO) , "r+")))
	{
		free( displays[0]->display) ;
		free( displays[0]) ;

		return  NULL ;
	}

	fd = fileno( displays[0]->display->fp) ;

	close( STDIN_FILENO) ;
	close( STDOUT_FILENO) ;
	close( STDERR_FILENO) ;

	write( fd , "\x1b[?25l" , 6) ;
	write( fd , "\x1b[?1002h\x1b[?1006h" , 16) ;

	tio = orig_tm ;
	tio.c_iflag &= ~(IXON|IXOFF|ICRNL|INLCR|IGNCR|IMAXBEL|ISTRIP) ;
	tio.c_iflag |= IGNBRK ;
	tio.c_oflag &= ~(OPOST|ONLCR|OCRNL|ONLRET) ;
	tio.c_lflag &= ~(IEXTEN|ICANON|ECHO|ECHOE|ECHONL|ECHOCTL|ECHOPRT|ECHOKE|ECHOCTL|ISIG) ;
	tio.c_cc[VMIN] = 1 ;
	tio.c_cc[VTIME] = 0 ;

	tcsetattr( fd , TCSANOW , &tio) ;

	displays[0]->display->conv = ml_conv_new( encoding) ;

	set_winsize( displays[0] , NULL) ;

	signal( SIGWINCH , sig_winch) ;

	return  displays[num_of_displays++] ;
}

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
 * disp->roots[1] is ignored.
 */
static inline x_window_t *
get_window(
	x_display_t *  disp ,
	int  x ,	/* X in display */
	int  y		/* Y in display */
	)
{
	return  get_window_intern( disp->roots[0] , x , y) ;
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
	x_display_t *  disp ,
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

	if( x + width > disp->width)
	{
		width = disp->width - x ;
	}

	if( y + height > disp->height)
	{
		height = disp->height - y ;
	}

	expose_window( disp->roots[0] , x , y , width , height) ;

	for( count = 0 ; count < disp->roots[0]->num_of_children ; count++)
	{
		expose_window( disp->roots[0]->children[count] , x , y , width , height) ;
	}
}

static int
check_visibility_of_im_window(
	x_display_t *  disp
	)
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

	if( disp->num_of_roots == 2 && disp->roots[1]->is_mapped)
	{
		if( im_region.saved)
		{
			if( im_region.x == disp->roots[1]->x &&
			    im_region.y == disp->roots[1]->y &&
			    im_region.width == ACTUAL_WIDTH(disp->roots[1]) &&
			    im_region.height == ACTUAL_HEIGHT(disp->roots[1]))
			{
				return  0 ;
			}

			if( im_region.x < disp->roots[1]->x ||
			    im_region.y < disp->roots[1]->y ||
			    im_region.x + im_region.width >
				disp->roots[1]->x + ACTUAL_WIDTH(disp->roots[1]) ||
			    im_region.y + im_region.height >
				disp->roots[1]->y + ACTUAL_HEIGHT(disp->roots[1]))
			{
				expose_display( disp , im_region.x , im_region.y ,
					im_region.width , im_region.height) ;
				redraw_im_win = 1 ;
			}
		}

		im_region.saved = 1 ;
		im_region.x = disp->roots[1]->x ;
		im_region.y = disp->roots[1]->y ;
		im_region.width = ACTUAL_WIDTH(disp->roots[1]) ;
		im_region.height = ACTUAL_HEIGHT(disp->roots[1]) ;
	}
	else
	{
		if( im_region.saved)
		{
			expose_display( disp , im_region.x , im_region.y ,
				im_region.width , im_region.height) ;
			im_region.saved = 0 ;
		}
	}

	return  redraw_im_win ;
}

static void
receive_event_for_multi_roots(
	x_display_t *  disp ,
	XEvent *  xev
	)
{
	int  redraw_im_win ;

	if( ( redraw_im_win = check_visibility_of_im_window( disp)))
	{
		/* Stop drawing input method window */
		disp->roots[1]->is_mapped = 0 ;
	}

	x_window_receive_event( disp->roots[0] , xev) ;

	if( redraw_im_win && disp->num_of_roots == 2)
	{
		/* Restart drawing input method window */
		disp->roots[1]->is_mapped = 1 ;
	}
	else if( ! check_visibility_of_im_window( disp))
	{
		return ;
	}

	expose_window( disp->roots[1] , disp->roots[1]->x , disp->roots[1]->y ,
				ACTUAL_WIDTH(disp->roots[1]) , ACTUAL_HEIGHT(disp->roots[1])) ;
}

static int
receive_stdin(
	Display *  display
	)
{
	ssize_t  len ;

	if( ( len = read( fileno( display->fp) , display->buf + display->buf_len ,
			sizeof(display->buf) - display->buf_len - 1)) > 0)
	{
		display->buf_len += len ;
		display->buf[display->buf_len] = '\0' ;

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
receive_stdin_event(
	x_display_t *  disp
	)
{
	u_char *  p ;

	if( ! receive_stdin( disp->display))
	{
		u_int  count ;

		for( count = disp->num_of_roots ; count > 0 ; count--)
		{
			if( disp->roots[count - 1]->window_deleted)
			{
				(*disp->roots[count - 1]->window_deleted)(
					disp->roots[count - 1]) ;
			}
		}

		return  1 ;
	}

	p = disp->display->buf ;

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
			fd_set  fds ;
			struct timeval  tv ;

			FD_ZERO(&fds) ;
			FD_SET( fileno( disp->display->fp) , &fds) ;
			tv.tv_usec = 50000 ;	/* 0.05 sec */
			tv.tv_sec = 0 ;

			if( select( fileno( disp->display->fp) + 1 , &fds , NULL ,
				NULL , &tv) == 1)
			{
				receive_stdin( disp->display) ;
			}
		}

		if( *p == '\x1b' && ( p[1] == '[' || p[1] == 'O'))
		{
			if( p[1] == '[')
			{
				u_char *  tmp ;

				if( ! parse( &param , &intermed , &ft , p + 2))
				{
					set_blocking( fileno( disp->display->fp) , 0) ;
					if( ! receive_stdin( disp->display))
					{
						break ;
					}
					set_blocking( fileno( disp->display->fp) , 1) ;

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
						if( disp->display->is_pressing)
						{
							bev.type = MotionNotify ;
						}
						else
						{
							bev.type = ButtonPress ;
							disp->display->is_pressing = 1 ;
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
						disp->display->is_pressing = 0 ;
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
					bev.x *= disp->display->col_width ;
					bev.y -- ;
					bev.y *= disp->display->line_height ;

					win = get_window( disp , bev.x , bev.y) ;
					bev.x -= win->x ;
					bev.y -= win->y ;

					gettimeofday( &tv , NULL) ;
					bev.time = tv.tv_sec * 1000 + tv.tv_usec / 1000 ;
					bev.state = 0 ;

					set_blocking( fileno(disp->display->fp) , 1) ;
					x_window_receive_event( win , &bev) ;
					set_blocking( fileno(disp->display->fp) , 0) ;

					continue ;
				}
				else if( param && set_winsize( disp , param))
				{
					u_int  count ;

					/* XXX */
					x_font_cache_unload_all() ;

					for( count = 0 ; count < disp->num_of_roots ; count++)
					{
						x_window_resize_with_margin( disp->roots[count] ,
							disp->width , disp->height ,
							NOTIFY_TO_MYSELF) ;
					}
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
					set_blocking( fileno( disp->display->fp) , 0) ;
					if( ! receive_stdin( disp->display))
					{
						break ;
					}
					set_blocking( fileno( disp->display->fp) , 1) ;

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

		set_blocking( fileno(disp->display->fp) , 1) ;
		receive_event_for_multi_roots( disp , &kev) ;
		set_blocking( fileno(disp->display->fp) , 0) ;
	}

	if( ( disp->display->buf_len = disp->display->buf + disp->display->buf_len - p) > 0)
	{
		memcpy( disp->display->buf , p , disp->display->buf_len + 1) ;
	}

	set_blocking( fileno(disp->display->fp) , 1) ;

	return  1 ;
}


/* --- global functions --- */

x_display_t *
x_display_open(
	char *  disp_name ,
	u_int  depth
	)
{
	if( disp_name && strncmp( disp_name , "client:" , 7) == 0)
	{
		return  open_display_socket( atoi( disp_name + 7)) ;
	}
	else if( ! displays)
	{
		return  open_display_console() ;
	}
	else
	{
		return  displays[0] ;
	}
}

int
x_display_close(
	x_display_t *  disp
	)
{
	u_int  count ;

	x_picture_display_closed( disp->display) ;

	if( isatty( fileno(disp->display->fp)))
	{
		tcsetattr( fileno(disp->display->fp) , TCSAFLUSH , &orig_tm) ;
		signal( SIGWINCH , SIG_IGN) ;
	}

	write( fileno(disp->display->fp) , "\x1b[?25h" , 6) ;
	write( fileno(disp->display->fp) , "\x1b[?1002l\x1b[?1006l" , 16) ;
	fclose( disp->display->fp) ;
	(*disp->display->conv->delete)( disp->display->conv) ;

	for( count = 0 ; count < num_of_displays ; count++)
	{
		if( displays[count] == disp)
		{
			memcpy( displays + count , displays + count + 1 ,
				sizeof(x_display_t*) * (num_of_displays - count - 1)) ;
			num_of_displays -- ;

			break ;
		}
	}

	return  1 ;
}

int
x_display_close_all(void)
{
	u_int  count ;

	end_server() ;

	for( count = num_of_displays ; count > 0 ; count--)
	{
		x_display_close( displays[count - 1]) ;
	}

	free( displays) ;
	displays = NULL ;

	return  1 ;
}

/* XXX */
int  x_event_source_add_fd( int  fd , void (*handler)(void)) ;

x_display_t **
x_get_opened_displays(
	u_int *  num
	)
{
	if( sock_fd == -1)
	{
		start_server() ;
		x_event_source_add_fd( sock_fd , client_connected) ;
	}

	*num = num_of_displays ;

	return  displays ;
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
	return  receive_stdin_event( disp) ;
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
	if( displays[0]->num_of_roots == 2 && displays[0]->roots[1]->is_mapped)
#endif
	{
		check_visibility_of_im_window( displays[0]) ;
		x_window_clear_margin_area( displays[0]->roots[1]) ;
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
