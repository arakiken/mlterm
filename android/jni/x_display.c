/*
 *	$Id$
 */

#include  "x_display.h"

#include  <unistd.h>	/* STDIN_FILENO */
#include  <linux/kd.h>
#include  <linux/keyboard.h>

#include  <kiklib/kik_debug.h>

#include  "../x_window.h"


#define  DISP_IS_INITED   (_disp.display)


/* --- static functions --- */

static x_display_t  _disp ;
static Display  _display ;
static int  locked ;


/* --- static functions --- */

static int
display_lock(void)
{
	if( ! locked)
	{
		ANativeWindow_Buffer  buf ;

		while( 1)
		{
			if( ANativeWindow_lock( _display.app->window , &buf , NULL) != 0)
			{
				return  0 ;
			}

			if( _display.buf.bits && _display.buf.bits != buf.bits)
			{
				memcpy( buf.bits , _display.buf.bits ,
					_display.buf.stride * _display.buf.height *
					_display.bytes_per_pixel) ;
				ANativeWindow_unlockAndPost( _display.app->window) ;
			}
			else
			{
				_display.buf = buf ;

				break ;
			}
		}

		locked = 1 ;
	}

	return  1 ;
}

static inline u_char *
get_fb(
	int  x ,
	int  y
	)
{
	return  _display.buf.bits + (y * _display.buf.stride + x) * _display.bytes_per_pixel ;
}

static int
kcode_to_ksym(
	int  kcode ,
	int  state
	)
{
	/* US Keyboard */

	if( AKEYCODE_0 <= kcode && kcode <= AKEYCODE_9)
	{
		if( state & ShiftMask)
		{
			char *  num_key_shift = ")!@#$%^&*(" ;

			return  num_key_shift[kcode - AKEYCODE_0] ;
		}
		else
		{
			return  kcode + 41 ;
		}
	}
	else if( AKEYCODE_A <= kcode && kcode <= AKEYCODE_Z)
	{
		kcode += 68 ;

		if( state & ShiftMask)
		{
			kcode -= 0x20 ;
		}

		return  kcode ;
	}
	else
	{
		if( state & ShiftMask)
		{
			switch(kcode)
			{
			case  AKEYCODE_COMMA:
				return  '<' ;

			case  AKEYCODE_PERIOD:
				return  '>' ;

			case  AKEYCODE_SPACE:
				return  ' ' ;

			case AKEYCODE_GRAVE:
				return  '~' ;

			case AKEYCODE_MINUS:
				return  '_' ;

			case AKEYCODE_EQUALS:
				return  '+' ;

			case AKEYCODE_LEFT_BRACKET:
				return  '{' ;

			case AKEYCODE_RIGHT_BRACKET:
				return  '}' ;

			case AKEYCODE_BACKSLASH:
				return  '|' ;

			case AKEYCODE_SEMICOLON:
				return  ':' ;

			case AKEYCODE_APOSTROPHE:
				return  '\"' ;

			case AKEYCODE_SLASH:
				return  '?' ;
			}
		}

		switch(kcode)
		{
		case  AKEYCODE_ENTER:
			return  0x0d ;

		case  AKEYCODE_DEL:
			return  0x08 ;

		case  AKEYCODE_COMMA:
			return  ',' ;

		case  AKEYCODE_PERIOD:
			return  '.' ;

		case  AKEYCODE_TAB:
			return  '\t' ;

		case  AKEYCODE_SPACE:
			return  ' ' ;

		case AKEYCODE_GRAVE:
			return  '`' ;

		case AKEYCODE_MINUS:
			return  '-' ;

		case AKEYCODE_EQUALS:
			return  '=' ;

		case AKEYCODE_BACKSLASH:
			return  '\\' ;

		case AKEYCODE_LEFT_BRACKET:
			return  '[' ;

		case AKEYCODE_RIGHT_BRACKET:
			return  ']' ;

		case AKEYCODE_SEMICOLON:
			return  ';' ;

		case AKEYCODE_APOSTROPHE:
			return  '\'' ;

		case AKEYCODE_SLASH:
			return  '/' ;

		case AKEYCODE_AT:
			return  '@' ;

		default:
			return  kcode + 0x100 ;
		}
	}
}

static void
process_key_event(
	int  action ,
	int  code
	)
{
	if( action == AKEY_EVENT_ACTION_DOWN)
	{
		if( code == AKEYCODE_SHIFT_RIGHT ||
		    code == AKEYCODE_SHIFT_LEFT)
		{
			_display.key_state |= ShiftMask ;
		}
	#if  0
		else if( code == KEY_CAPSLOCK)
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
	#endif
		else if( code == AKEYCODE_CONTROL_RIGHT ||
			 code == AKEYCODE_CONTROL_LEFT)
		{
			_display.key_state |= ControlMask ;
		}
		else if( code == AKEYCODE_ALT_RIGHT ||
			 code == AKEYCODE_ALT_LEFT)
		{
			_display.key_state |= ModMask ;
		}
	#if  0
		else if( code == KEY_NUMLOCK)
		{
			_display.lock_state ^= NLKED ;
		}
	#endif
		else
		{
			XKeyEvent  xev ;

			xev.type = KeyPress ;
			xev.ksym = kcode_to_ksym( code ,
					_display.key_state) ;
			xev.keycode = code ;
			xev.state = _display.button_state |
				    _display.key_state ;

			x_window_receive_event( _disp.roots[0] , &xev) ;
		}
	}
	else if( action == AKEY_EVENT_ACTION_MULTIPLE)
	{
		kik_debug_printf( "MULTIPLE\n") ;
	}
	else /* if( action == AKEY_EVENT_ACTION_UP) */
	{
		if( code == AKEYCODE_SHIFT_RIGHT ||
		    code == AKEYCODE_SHIFT_LEFT)
		{
			_display.key_state &= ~ShiftMask ;
		}
		else if( code == AKEYCODE_CONTROL_RIGHT ||
			 code == AKEYCODE_CONTROL_LEFT)
		{
			_display.key_state &= ~ControlMask ;
		}
		else if( code == AKEYCODE_ALT_RIGHT ||
			 code == AKEYCODE_ALT_LEFT)
		{
			_display.key_state &= ~ModMask ;
		}
	}
}

static void
process_mouse_event(
	int  source ,
	int  action ,
	int64_t  time ,
	int  x ,
	int  y
	)
{
	if( source & AINPUT_SOURCE_MOUSE)
	{
		XButtonEvent  xev ;

		switch( action & AMOTION_EVENT_ACTION_POINTER_INDEX_MASK)
		{
		case  0x100:
			_display.button_state = Button1Mask ;
			xev.button = Button1 ;
			break ;

		case  0x200:
			xev.button = Button2 ;
			_display.button_state = Button2Mask ;
			break ;

		case  0x300:
			xev.button = Button3 ;
			_display.button_state = Button3Mask ;
			break ;

		case  0x0:
		default:
			xev.button = Button1 ;
			break ;
		}

		switch( action & AMOTION_EVENT_ACTION_MASK)
		{
		case  AMOTION_EVENT_ACTION_DOWN:
			xev.type = ButtonPress ;
			break ;

		case  AMOTION_EVENT_ACTION_UP:
			xev.type = ButtonRelease ;
			/* Reset button_state in releasing button. */
			_display.button_state = 0 ;
			break ;

		case  AMOTION_EVENT_ACTION_MOVE:
			xev.type = MotionNotify ;
			break ;

		default:
			return ;
		}

		xev.time = time / 1000000 ;
		xev.x = x ;
		xev.y = y ;
		xev.state = _display.key_state ;

	#if  0
		kik_debug_printf( KIK_DEBUG_TAG
			"Button is %s x %d y %d btn %d time %d\n" ,
			xev.type == ButtonPress ? "pressed" :
				(xev.type == MotionNotify ? "motion" : "released") ,
			xev.x , xev.y , xev.button , xev.time) ;
	#endif

		x_window_receive_event( _disp.roots[0] , &xev) ;
	}
}

static int32_t
on_input_event(
	struct android_app *  app ,
	AInputEvent *  event
	)
{
	switch( AInputEvent_getType(event))
	{
	case  AINPUT_EVENT_TYPE_MOTION:
	#if  0
		kik_debug_printf( "MOTION %d %d %d x %d y %d\n" ,
			AInputEvent_getSource( event) ,
			AMotionEvent_getAction( event) , AMotionEvent_getEventTime( event) ,
			AMotionEvent_getX( event , 0) , AMotionEvent_getY( event , 0)) ;
	#endif
		process_mouse_event( AInputEvent_getSource( event) ,
			AMotionEvent_getAction( event) , AMotionEvent_getEventTime( event) ,
			AMotionEvent_getX( event , 0) , AMotionEvent_getY( event , 0)) ;

		return  1 ;

	case  AINPUT_EVENT_TYPE_KEY:
	#if  0
		kik_debug_printf( "KEY %d %d\n" , AKeyEvent_getScanCode(event) ,
			AKeyEvent_getKeyCode( event)) ;
	#endif
		process_key_event( AKeyEvent_getAction( event) ,
			AKeyEvent_getKeyCode( event)) ;

		return  1 ;

	default:
		return  0 ;
	}
}

static void
on_app_cmd(
	struct android_app *  app ,
	int32_t cmd
	)
{
	switch(cmd)
	{
	case APP_CMD_SAVE_STATE:
		break ;

	case APP_CMD_INIT_WINDOW:
		break ;

	case APP_CMD_TERM_WINDOW:
		x_display_close_all() ;
		break ;

	case APP_CMD_GAINED_FOCUS:
		/* When our app gains focus, we start monitoring the accelerometer. */
		if( _display.accel_sensor)
		{
			ASensorEventQueue_enableSensor( _display.sensor_evqueue ,
				_display.accel_sensor) ;
			/* We'd like to get 60 events per second (in us). */
			ASensorEventQueue_setEventRate( _display.sensor_evqueue ,
				_display.accel_sensor , (1000L/60)*1000) ;
		}

		break ;

	case APP_CMD_LOST_FOCUS:
		/*
		 * When our app loses focus, we stop monitoring the accelerometer.
		 * This is to avoid consuming battery while not being used.
		 */
		if( _display.accel_sensor)
		{
			ASensorEventQueue_disableSensor( _display.sensor_evqueue ,
				_display.accel_sensor) ;
		}

		break ;
	}
}


/* --- global functions --- */

x_display_t *
x_display_open(
	char *  disp_name ,
	u_int  depth
	)
{
	struct rgb_info  rgbinfos[] =
	{
		{ 3 , 2 , 3 , 11 , 5 , 0 } ,
		{ 0 , 0 , 0 , 16 , 8 , 0 } ,
	} ;

	switch( ANativeWindow_getFormat( _display.app->window))
	{
#if  0
	case  WINDOW_FORMAT_RGBA_8888:
		_disp.depth = 32 ;
		_display.bytes_per_pixel = 4 ;
		_display.rgbinfo = rgbinfos[1] ;
		break ;

	case  WINDOW_FORMAT_RGBX_8888:
		_disp.depth = 24 ;
		_display.bytes_per_pixel = 4 ;
		_display.rgbinfo = rgbinfos[1] ;
		break ;

	case  WINDOW_FORMAT_RGB_565:
#endif
	default:
		_disp.depth = 16 ;
		_display.bytes_per_pixel = 2 ;
		_display.rgbinfo = rgbinfos[0] ;
	}

	_disp.width = ANativeWindow_getWidth( _display.app->window) ;
	_disp.height = ANativeWindow_getHeight( _display.app->window) ;

	_disp.display = &_display ;

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
		u_int  count ;

		for( count = 0 ; count < _disp.num_of_roots ; count ++)
		{
		#if  0
			x_window_unmap( _disp.roots[count]) ;
		#endif
			x_window_final( _disp.roots[count]) ;
		}

		free( _disp.roots) ;
		_disp.roots = NULL ;

		/* DISP_IS_INITED is false from here. */
		_disp.display = NULL ;

		android_app_post_exec_cmd( _display.app , APP_CMD_TERM_WINDOW) ;
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

	*num = 1 ;
	opened_disp = &_disp ;

	return  &opened_disp ;
}

int
x_display_fd(
	x_display_t *  disp
	)
{
	return  -1 ;
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

int
x_display_reset_cmap(void)
{
	return  0 ;
}


int
x_display_init(
	struct android_app *  app
	)
{
	int  ident ;
	int  events ;
	struct android_poll_source *  source ;

	/* Make sure glue isn't stripped. */
	app_dummy() ;

	app->onAppCmd = on_app_cmd ;

	do
	{
		if( ( ident = ALooper_pollAll(
					-1 /* block forever waiting for events */ ,
					NULL , &events, (void**)&source)) >= 0)
		{
			/* Process this event. */
			if( source)
			{
				(*source->process)( app , source) ;
			}
		}
	}
	while( ! app->window) ;

	app->onInputEvent = on_input_event ;

	_display.app = app ;

	/* Prepare to monitor accelerometer. */
	_display.sensor_man = ASensorManager_getInstance() ;
	_display.accel_sensor = ASensorManager_getDefaultSensor(
					_display.sensor_man , ASENSOR_TYPE_ACCELEROMETER) ;
	_display.sensor_evqueue = ASensorManager_createEventQueue(
					_display.sensor_man , app->looper ,
					LOOPER_ID_USER , NULL , NULL) ;
}

int
x_display_process_event(
	struct android_poll_source *  source ,
	int  ident
	)
{
	/* Process this event. */
	if( source)
	{
		(*source->process)( _display.app , source) ;
	}

	/* If a sensor has data, process it now. */
	if( ident == LOOPER_ID_USER)
	{
		if( _display.accel_sensor)
		{
			ASensorEvent event ;

			while( ASensorEventQueue_getEvents(
				_display.sensor_evqueue , &event , 1) > 0)
			{
				kik_debug_printf( "Accelerometer: x=%f y=%f z=%f" ,
					event.acceleration.x ,
					event.acceleration.y ,
					event.acceleration.z) ;
			}
		}
	}

	/* Check if we are exiting. */
	if( _display.app->destroyRequested)
	{
		return  0 ;
	}

	return  1 ;
}

void
x_display_unlock(void)
{
	if( locked)
	{
		ANativeWindow_unlockAndPost( _display.app->window) ;
		locked = 0 ;
	}
}

u_long
x_display_get_pixel(
	int  x ,
	int  y
	)
{
	u_char *  fb ;

	fb = get_fb( x , y) ;

	if( _display.bytes_per_pixel == 4)
	{
		return  *((u_int32_t*)fb) ;
	}
	else /* if( _display.bytes_per_pixel == 2) */
	{
		return  *((u_int16_t*)fb) ;
	}
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
	if( display_lock())
	{
		memcpy( get_fb( x , y) , image , size) ;
	}
}

void
x_display_fill_with(
	int  x ,
	int  y ,
	u_int  width ,
	u_int  height ,
	u_int8_t  pixel
	)
{
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
	if( display_lock())
	{
		u_char *  src ;
		u_char *  dst ;
		u_int  copy_len ;
		u_int  count ;
		size_t  line_length ;

		copy_len = width * _display.bytes_per_pixel ;
		line_length = _display.buf.stride * _display.bytes_per_pixel ;

		if( src_y <= dst_y)
		{
			src = get_fb( src_x , src_y + height - 1) ;
			dst = get_fb( dst_x , dst_y + height - 1) ;

			if( src_y == dst_y)
			{
				for( count = 0 ; count < height ; count++)
				{
					memmove( dst , src , copy_len) ;
					dst -= line_length ;
					src -= line_length ;
				}
			}
			else
			{
				for( count = 0 ; count < height ; count++)
				{
					memcpy( dst , src , copy_len) ;
					dst -= line_length ;
					src -= line_length ;
				}
			}
		}
		else
		{
			src = get_fb( src_x , src_y) ;
			dst = get_fb( dst_x , dst_y) ;

			for( count = 0 ; count < height ; count++)
			{
				memcpy( dst , src , copy_len) ;
				dst += line_length ;
				src += line_length ;
			}
		}
	}
}

int
x_display_check_visibility_of_im_window(void)
{
	return  0 ;
}

int
x_cmap_get_closest_color(
	u_long *  closest ,
	int  red ,
	int  green ,
	int  blue
	)
{
	return  0 ;
}

int
x_cmap_get_pixel_rgb(
	u_int8_t *  red ,
	u_int8_t *  green ,
	u_int8_t *  blue ,
	u_long  pixel
	)
{
	return  0 ;
}
