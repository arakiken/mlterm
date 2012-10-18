/*
 *	$Id$
 */

#include  "../x_display.h"

#include  <stdio.h>	/* printf */
#include  <unistd.h>	/* STDIN_FILENO */
#include  <fcntl.h>	/* open */
#include  <sys/mman.h>	/* mmap */
#include  <sys/ioctl.h>	/* ioctl */
#include  <string.h>	/* memset/memcpy */
#include  <termios.h>
#include  <kiklib/kik_debug.h>

#include  "../x_window.h"


#define  DISP_IS_INITED   (_disp.display)


/* --- static variables --- */

static Display  _display ;
static x_display_t  _disp ;
static struct termios  orig_tm ;


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
		tcsetattr( _display.fd , TCSAFLUSH , &tm) ;

		fcntl( STDIN_FILENO , F_SETFL , fcntl( STDIN_FILENO , F_GETFL , 0) | O_NONBLOCK) ;

		write( STDIN_FILENO , "\x1b[?25l" , 6) ;

		_display.fd = STDIN_FILENO ;

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
		munmap( _display.fp , _display.smem_len) ;
		close( _display.fb_fd) ;

		write( STDIN_FILENO , "\x1b[?25h" , 6) ;

		tcsetattr( STDIN_FILENO , TCSAFLUSH , &orig_tm) ;

		_disp.display = NULL ;

		free( _disp.roots) ;
	}

	return  1 ;
}

x_display_t **
x_get_opened_displays(
	u_int *  num
	)
{
	static x_display_t *  opened_disp ;

	if( ! DISP_IS_INITED)
	{
		*num = 0 ;

		return  NULL ;
	}

	opened_disp = &_disp ;
	*num = 1 ;

	return  &opened_disp ;
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

	return  x_window_show( root , hint) ;
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
	char  buf[1] ;
	ssize_t  len ;

	if( ( len = read( disp->display->fd , buf , sizeof(buf))) > 0)
	{
		XEvent  ev ;

		ev.xkey.ch = buf[0] ;

		x_window_receive_event( disp->roots[0] , &ev) ;
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
