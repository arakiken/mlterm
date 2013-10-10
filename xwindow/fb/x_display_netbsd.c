/*
 *	$Id$
 */

#include  <sys/param.h>				/* MACHINE */
#include  <dev/wscons/wsdisplay_usl_io.h>	/* VT_GETSTATE */
#ifdef  USE_GRF
#include  <machine/grfioctl.h>
#endif

#ifdef  __NetBSD__
#include  "../x_event_source.h"
#endif


#ifdef  __NetBSD__
#define  KEY_REPEAT_UNIT  25	/* msec (see x_event_source.c) */
#define  DEFAULT_KEY_REPEAT_1  400	/* msec */
#define  DEFAULT_KEY_REPEAT_N  50	/* msec */
#endif

#define get_key_state()  (0)


#ifdef  USE_GRF
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

static u_int  kbd_type ;
static struct wskbd_map_data  keymap ;
static int  console_id = -1 ;
static struct wscons_event  prev_key_event ;
u_int  fb_width = 640 ;
u_int  fb_height = 480 ;
u_int  fb_depth = 8 ;
#ifdef  __NetBSD__
static int  wskbd_repeat_wait = (DEFAULT_KEY_REPEAT_1 + KEY_REPEAT_UNIT - 1) / KEY_REPEAT_UNIT ;
int  wskbd_repeat_1 = DEFAULT_KEY_REPEAT_1 ;
int  wskbd_repeat_N = DEFAULT_KEY_REPEAT_N ;
#endif
#ifdef  USE_GRF
int  separate_wall_picture ;
static fb_reg_conf_t  orig_reg ;
static int  grf0_fd = -1 ;
static size_t  grf0_len ;
static fb_reg_t *  grf0_reg ;
static u_char *  grf0_cmap_orig ;
#else
static int  orig_console_mode = WSDISPLAYIO_MODE_EMUL ;	/* 0 */
#endif


/* --- static functions --- */

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

#ifdef  USE_GRF

static void
close_grf0(void)
{
	if( grf0_fd != -1)
	{
		/* Remove TP_COLOR */
		memcpy( grf0_reg->gpal , grf0_reg->tpal , sizeof(u_short) * 16) ;

		if( grf0_cmap_orig)
		{
			memcpy( grf0_reg->tpal , grf0_cmap_orig , sizeof(u_short) * 16) ;
		}

		grf0_reg->videoc.r2 = 0x0010 ;

		munmap( grf0_reg , grf0_len) ;

		close( grf0_fd) ;
		grf0_fd = -1 ;
	}
}

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
		  { 4 , 0x24e4 /* Graphic vram is prior to text one. */ , 0x0010 } } ;
	fb_reg_conf_t  conf_1024_768_4 =
		{ { 169 , 14 , 28 , 156 , 439 , 5 , 40 , 424 , 27 , 1050 } ,
		  { 4 , 0x21e4 , 0x0010 } } ;
	struct rgb_info  rgb_info_15bpp = { 3 , 3 , 3 , 6 , 11 , 1 } ;
	struct termios  tm ;

	if( ( _display.fb_fd = open( ( dev = getenv("FRAMEBUFFER")) ? dev : "/dev/grf1" ,
					O_RDWR)) < 0)
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

	kik_priv_restore_euid() ;
	kik_priv_restore_egid() ;

	_mouse.fd = open( "/dev/mouse" , O_RDWR|O_NONBLOCK|O_EXCL) ;

	kik_priv_change_euid( kik_getuid()) ;
	kik_priv_change_egid( kik_getgid()) ;

	if( _mouse.fd != -1)
	{
		kik_file_set_cloexec( _mouse.fd) ;

		_mouse.x = _display.width / 2 ;
		_mouse.y = _display.height / 2 ;
		_disp_mouse.display = (Display*)&_mouse ;

	#if  0
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
	#endif
	}
#ifdef  DEBUG
	else
	{
		kik_debug_printf( KIK_DEBUG_TAG " Failed to open /dev/wsmouse.\n") ;
	}
#endif
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

static int
receive_mouse_event(void)
{
#define  MS_LEFT      0x7f20  /* left mouse button */
#define  MS_MIDDLE    0x7f21  /* middle mouse button */
#define  MS_RIGHT     0x7f22  /* right mouse button */
#define  LOC_X_DELTA  0x7f80  /* mouse delta-X */
#define  LOC_Y_DELTA  0x7f81  /* mouse delta-Y */
#define  VKEY_UP      0
#define  VKEY_DOWN    1

	struct
	{
		u_short  id ;
		u_short  pad ;
		int  value ;
		struct timeval  time ;
	} ev ;
	ssize_t  len ;

	if( console_id != get_active_console())
	{
		return  0 ;
	}

	while( ( len = read( _mouse.fd , memset( &ev , 0 , sizeof(ev)) , sizeof(ev))) > 0)
	{
	#ifdef  DEBUG
		kik_debug_printf( KIK_DEBUG_TAG " MOUSE event (len)%d (id)%d (val)%d\n" ,
			len , ev.id , ev.value) ;
	#endif

		if( ev.value == VKEY_DOWN || ev.value == VKEY_UP)
		{
			XButtonEvent  xev ;
			x_window_t *  win ;

			if( ev.id == MS_LEFT)
			{
				xev.button = Button1 ;
				_mouse.button_state = Button1Mask ;
			}
			else if( ev.id == MS_MIDDLE)
			{
				xev.button = Button2 ;
				_mouse.button_state = Button2Mask ;
			}
			else if( ev.id == MS_RIGHT)
			{
				xev.button = Button3 ;
				_mouse.button_state = Button3Mask ;
			}
			else
			{
				continue ;
			}

			if( ev.value == VKEY_DOWN)
			{
				xev.type = ButtonPress ;
			}
			else /* if( ev.value == VKEY_UP) */
			{
				xev.type = ButtonRelease ;

				/* Reset button_state in releasing button. */
				_mouse.button_state = 0 ;
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
		else if( ev.id == LOC_X_DELTA || ev.id == LOC_Y_DELTA)
		{
			XMotionEvent  xev ;
			x_window_t *  win ;

			restore_hidden_region() ;

			if( ev.id == LOC_X_DELTA)
			{
				_mouse.x += ((int)ev.value * 2) ;

				if( _mouse.x < 0)
				{
					_mouse.x = 0 ;
				}
				else if( _display.width <= _mouse.x)
				{
					_mouse.x = _display.width - 1 ;
				}
			}
			else /* if( ev.id == LOC_Y_DELTA) */
			{
				_mouse.y -= ((int)ev.value * 2) ;

				if( _mouse.y < 0)
				{
					_mouse.y = 0 ;
				}
				else if( _display.height <= _mouse.y)
				{
					_mouse.y = _display.height - 1 ;
				}
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

#else	/* USE_GRF */

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

	if( ( _display.fb_fd = open( ( dev = getenv("FRAMEBUFFER")) ? dev : DEFAULT_FBDEV ,
					O_RDWR)) < 0)
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
			/* Always failed on OpenBSD/luna88k. */
			kik_msg_printf( "Failed to set screen resolution (gfx mode).\n") ;
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

static int
receive_mouse_event(void)
{
	struct wscons_event  ev ;
	ssize_t  len ;

	if( console_id != get_active_console())
	{
		return  0 ;
	}

	while( ( len = read( _mouse.fd , memset( &ev , 0 , sizeof(ev)) , sizeof(ev))) > 0)
	{
	#ifdef  DEBUG
		kik_debug_printf( KIK_DEBUG_TAG " MOUSE event (len)%d (type)%d (val)%d\n" ,
			len , ev.type , ev.value) ;
	#endif

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

#endif	/* USE_GRF */


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

		if( console_id != get_active_console())
		{
			return  0 ;
		}

		while( ( len = read( _display.fd , memset( &ev , 0 , sizeof(ev)) ,
				sizeof(ev))) > 0)
		{
		#ifdef  DEBUG
			kik_debug_printf( KIK_DEBUG_TAG " KEY event (len)%d (type)%d (val)%d\n" ,
				len , ev.type , ev.value) ;
		#endif

			process_wskbd_event( &ev) ;
		}
	}

	return  1 ;
}


/* --- global functions --- */

#ifdef  USE_GRF

int
x68k_tvram_is_enabled(void)
{
	return  (grf0_fd != -1) ? 1 : 0 ;
}

int
x68k_tvram_set_wall_picture(
	u_short *  image ,
	u_int  width ,
	u_int  height
	)
{
	static u_char *  vram ;
	u_char *  pl0 ;
	u_char *  pl1 ;
	u_char *  pl2 ;
	u_char *  pl3 ;
	u_short *  img ;
	int  y ;
	int  img_y ;

	if( ! separate_wall_picture || _disp.depth != 4)
	{
		return  0 ;
	}

	if( width < 8)
	{
		image = NULL ;
	}

	while( grf0_fd == -1)
	{
		struct grfinfo  vinfo ;

		if( image && ( grf0_fd = open( "/dev/grf0" , O_RDWR)) >= 0)
		{
			kik_file_set_cloexec( grf0_fd) ;

			if( ioctl( grf0_fd , GRFIOCGINFO , &vinfo) >= 0)
			{
				grf0_len = vinfo.gd_fbsize + vinfo.gd_regsize ;

				if( ( grf0_reg = mmap( NULL , grf0_len ,
							PROT_WRITE|PROT_READ ,
							MAP_FILE|MAP_SHARED ,
							grf0_fd , (off_t)0)) != MAP_FAILED)
				{
					u_long  color ;

					/* Enale the text vram. */
					grf0_reg->videoc.r2 = 0x0030 ;

					/* Initialize scroll registers. */
					grf0_reg->crtc.r10 = grf0_reg->crtc.r11 = 0 ;

					grf0_reg->crtc.r21 = 0 ;

					if( ( grf0_cmap_orig = malloc( sizeof(u_short) * 16)))
					{
						memcpy( grf0_cmap_orig , grf0_reg->tpal ,
							sizeof(u_short) * 16) ;
					}

					memcpy( grf0_reg->tpal , grf0_reg->gpal ,
						sizeof(u_short) * 16) ;

					vram = ((u_char*)grf0_reg) + vinfo.gd_regsize ;

					if( x_cmap_get_closest_color( &color , 0 , 0 , 0))
					{
						/* Opaque black */
						grf0_reg->gpal[color] |= 0x1 ;
					}

					/* Transparent (Wall paper is visible) */
					grf0_reg->gpal[TP_COLOR] = 0x0 ;

					break ;
				}
			}

			close( grf0_fd) ;
			grf0_fd = -1 ;
		}

		return  0 ;
	}

	if( ! image)
	{
		close_grf0() ;

		return  0 ;
	}

	pl0 = vram ;
	pl1 = pl0 + 0x20000 ;
	pl2 = pl1 + 0x20000 ;
	pl3 = pl2 + 0x20000 ;
	img = image ;

	for( y = 0 , img_y = 0 ; y < 1024 ; y++ , img_y++)
	{
		int  x ;
		int  img_x ;

		if( img_y >= height)
		{
			img = image ;
			img_y = 0 ;
		}

		img_x = 0 ;

		/* 128 bytes per line */
		for( x = 0 ; x < 128 ; x++)
		{
			*(pl3++) = ((img[img_x] & 0x8) << 4) |
			            ((img[img_x + 1] & 0x8) << 3) |
			            ((img[img_x + 2] & 0x8) << 2) |
			            ((img[img_x + 3] & 0x8) << 1) |
			            (img[img_x + 4] & 0x8) |
			            ((img[img_x + 5] & 0x8) >> 1) |
			            ((img[img_x + 6] & 0x8) >> 2) |
			            ((img[img_x + 7] & 0x8) >> 3) ;
			*(pl2++) = ((img[img_x] & 0x4) << 5) |
			            ((img[img_x + 1] & 0x4) << 4) |
			            ((img[img_x + 2] & 0x4) << 3) |
			            ((img[img_x + 3] & 0x4) << 2) |
			            ((img[img_x + 4] & 0x4) << 1) |
			            (img[img_x + 5] & 0x4) |
			            ((img[img_x + 6] & 0x4) >> 1) |
			            ((img[img_x + 7] & 0x4) >> 2) ;
			*(pl1++) = ((img[img_x] & 0x2) << 6) |
			            ((img[img_x + 1] & 0x2) << 5) |
			            ((img[img_x + 2] & 0x2) << 4) |
			            ((img[img_x + 3] & 0x2) << 3) |
			            ((img[img_x + 4] & 0x2) << 2) |
			            ((img[img_x + 5] & 0x2) << 1) |
			            (img[img_x + 6] & 0x2) |
			            ((img[img_x + 7] & 0x2) >> 1) ;
			*(pl0++) = ((img[img_x] & 0x1) << 7) |
			            ((img[img_x + 1] & 0x1) << 6) |
			            ((img[img_x + 2] & 0x1) << 5) |
			            ((img[img_x + 3] & 0x1) << 4) |
			            ((img[img_x + 4] & 0x1) << 3) |
			            ((img[img_x + 5] & 0x1) << 2) |
			            ((img[img_x + 6] & 0x1) << 1) |
			            (img[img_x + 7] & 0x1) ;

			if( (img_x += 8) >= width)
			{
				/* XXX tiling with chopping the last 7 or less pixels. */
				img_x = 0 ;
			}
		}

		img += width ;
	}

	return  1 ;
}

#endif	/* USE_GRF */
