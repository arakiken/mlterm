/*
 *	$Id$
 */

#include  <sys/consio.h>
#include  <sys/mouse.h>
#include  <sys/time.h>


#define  SYSMOUSE_PACKET_SIZE  8


/* --- static variables --- */

static keymap_t  keymap ;


/* --- static functions --- */

static int
open_display(
	u_int  depth
	)
{
	char *  dev ;
	int  vmode ;
	video_info_t  vinfo ;
	video_adapter_info_t  vainfo ;
	video_display_start_t  vstart ;
	struct termios  tm ;

	if( ( _display.fb_fd = open( ( dev = getenv("FRAMEBUFFER")) ?
						dev : "/dev/ttyv0" ,
					O_RDWR)) < 0)
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
