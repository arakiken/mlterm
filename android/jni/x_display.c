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
static int  rotate_display ;
static int  locked ;

static int  visible_frame_changed ;
static int  new_yoffset ;
static u_int  new_width ;
static u_int  new_height ;


/* --- static functions --- */

static int
display_lock(void)
{
	if( locked < 0)
	{
		return  0 ;
	}
	else if( ! locked)
	{
		ANativeWindow_Buffer  buf ;

		if( ANativeWindow_lock( _display.app->window , &buf , NULL) != 0)
		{
			return  0 ;
		}

		if( _display.buf.bits != buf.bits)
		{
			if( _display.buf.bits &&
			    _display.buf.height == buf.height &&
			    _display.buf.stride == buf.stride)
			{
				memcpy( buf.bits , _display.buf.bits ,
					_display.buf.height * _display.buf.stride *
					_display.bytes_per_pixel) ;
			}

			_display.buf = buf ;
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
	return  ((u_char*)_display.buf.bits) +
		((_display.yoffset + y) * _display.buf.stride + x) * _display.bytes_per_pixel ;
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

		case AKEYCODE_ESCAPE:
			return  '\x1b' ;

		default:
			return  kcode + 0x100 ;
		}
	}
}

static int
process_key_event(
	int  action ,
	int  code
	)
{
	if( code == AKEYCODE_BACK)
	{
		return  0 ;
	}

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
		XKeyEvent  xev ;

		xev.type = KeyPress ;
		xev.ksym = 0 ;
		xev.keycode = 0 ;
		xev.state = 0 ;

		x_window_receive_event( _disp.roots[0] , &xev) ;
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

	return  1 ;
}

static void
show_soft_input(
	JavaVM *  vm
	)
{
	JNIEnv *  env ;
	jobject  this ;

	(*vm)->AttachCurrentThread( vm , &env , NULL) ;

	this = _display.app->activity->clazz ;
	(*env)->CallVoidMethod( env , this ,
		(*env)->GetMethodID( env , (*env)->GetObjectClass( env , this) ,
			"showSoftInput" , "()V")) ;

	(*vm)->DetachCurrentThread(vm) ;
}

static int
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
		static int  click_num ;

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
			return  1 ;
		}

		xev.time = time / 1000000 ;
		if( rotate_display)
		{
			if( rotate_display > 0)
			{
				xev.x = y ;
				xev.y = _disp.height - x - 1 ;
			}
			else
			{
				xev.x = _disp.width - y - 1 ;
				xev.y = x ;
			}
		}
		else
		{
			xev.x = x ;
			xev.y = y ;
		}
		xev.state = _display.key_state ;

		if( xev.type == ButtonPress)
		{
			if( xev.x + xev.y + 20 >= _disp.width + _disp.height)
			{
				if( click_num == 0)
				{
					click_num = 1 ;
				}
				else /* if( click_num == 1) */
				{
					click_num = 0 ;

				#if  0
					/* This doesn't work on Android 3.x and 4.x. */
					ANativeActivity_showSoftInput( _display.app->activity ,
						ANATIVEACTIVITY_SHOW_SOFT_INPUT_FORCED) ;
				#else
					show_soft_input( _display.app->activity->vm) ;
				#endif
				}
			}
			else
			{
				click_num = 0 ;
			}
		}

	#if  0
		kik_debug_printf( KIK_DEBUG_TAG
			"Button is %s x %d y %d btn %d time %d\n" ,
			xev.type == ButtonPress ? "pressed" :
				(xev.type == MotionNotify ? "motion" : "released") ,
			xev.x , xev.y , xev.button , xev.time) ;
	#endif

		x_window_receive_event( _disp.roots[0] , &xev) ;
	}

	return  1 ;
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
		kik_debug_printf( "MOTION %d %d x %d y %d\n" ,
			AInputEvent_getSource( event) ,
			AMotionEvent_getAction( event) ,
			AMotionEvent_getX( event , 0) ,
			AMotionEvent_getY( event , 0) - _display.yoffset) ;
	#endif
		return  process_mouse_event( AInputEvent_getSource( event) ,
				AMotionEvent_getAction( event) ,
				AMotionEvent_getEventTime( event) ,
				AMotionEvent_getX( event , 0) ,
				AMotionEvent_getY( event , 0) - _display.yoffset) ;

	case  AINPUT_EVENT_TYPE_KEY:
	#if  0
		kik_debug_printf( "KEY %d %d\n" , AKeyEvent_getScanCode(event) ,
			AKeyEvent_getKeyCode( event)) ;
	#endif
		return  process_key_event( AKeyEvent_getAction( event) ,
				AKeyEvent_getKeyCode( event)) ;

	default:
		return  0 ;
	}
}

static void
update_window(
	x_window_t *  win
	)
{
	u_int  count ;

	x_window_clear_margin_area( win) ;

	if( win->window_exposed)
	{
		(*win->window_exposed)( win , 0 , 0 , win->width , win->height) ;
	}

	for( count = 0 ; count < win->num_of_children ; count++)
	{
		update_window( win->children[count]) ;
	}
}

static void
init_window(
	ANativeWindow *  window
	)
{
	struct rgb_info  rgbinfos[] =
	{
		{ 3 , 2 , 3 , 11 , 5 , 0 } ,
		{ 0 , 0 , 0 , 16 , 8 , 0 } ,
	} ;

	if( _disp.width == 0)
	{
		_disp.width = ANativeWindow_getWidth( window) ;
		_disp.height = ANativeWindow_getHeight( window) ;

		if( rotate_display)
		{
			u_int  tmp ;

			tmp = _disp.width ;
			_disp.width = _disp.height ;
			_disp.height = tmp ;
		}
	}
	else
	{
		/* Changed in visibleFrameChanged. */
	}

	_disp.depth = 16 ;
	_display.bytes_per_pixel = 2 ;
	_display.rgbinfo = rgbinfos[0] ;

	ANativeWindow_setBuffersGeometry( window , 0 , 0 , WINDOW_FORMAT_RGB_565) ;

	if( _display.buf.bits)
	{
		/* mlterm restarted */

		_display.buf.bits = NULL ;

		if( locked < 0)
		{
			/* If mlterm exited and restarted, locked is -1 here. */
			locked = 0 ;
		}
		else
		{
			u_int  count ;

			/* In case of locked is 1 here. */
			locked = 0 ;

			/* mlterm paused and restarted. */
			for( count = 0 ; count < _disp.num_of_roots ; count++)
			{
				update_window( _disp.roots[count]) ;
			}
		}
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
	#ifdef  DEBUG
		kik_debug_printf( "SAVE_STATE\n") ;
	#endif
		break ;

	case APP_CMD_INIT_WINDOW:
	#ifdef  DEBUG
		kik_debug_printf( "INIT_WINDOW\n") ;
	#endif
		init_window( app->window) ;
		break ;

	case APP_CMD_WINDOW_RESIZED:
	#ifdef  DEBUG
		kik_debug_printf( "WINDOW_RESIZED\n") ;
	#endif
		break ;

	case APP_CMD_TERM_WINDOW:
	#ifdef  DEBUG
		kik_debug_printf( "TERM_WINDOW\n") ;
	#endif
		locked = -1 ;	/* Don't lock until APP_CMD_INIT_WINDOW after restart. */

		break ;

	case APP_CMD_WINDOW_REDRAW_NEEDED:
	#ifdef  DEBUG
		kik_debug_printf( "WINDOW_REDRAW_NEEDED\n") ;
	#endif
		break ;

	case APP_CMD_CONFIG_CHANGED:
	#ifdef  DEBUG
		kik_debug_printf( "CONFIG_CHANGED\n") ;
	#endif

	case APP_CMD_GAINED_FOCUS:
	#ifdef  DEBUG
		kik_debug_printf( "GAINED_FOCUS\n") ;
	#endif
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
	#ifdef  DEBUG
		kik_debug_printf( "LOST_FOCUS\n") ;
	#endif
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
	}

	return  1 ;
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

void
x_display_set_use_ansi_colors(
	int  use
	)
{
}

void
x_display_rotate(
	int  rotate
	)
{
	if( rotate == rotate_display)
	{
		return ;
	}

	if( rotate_display + rotate != 0)
	{
		u_int  tmp ;

		tmp = _disp.width ;
		_disp.width = _disp.height ;
		_disp.height = tmp ;

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


int
x_display_init(
	struct android_app *  app
	)
{
	int  ret ;
	int  ident ;
	int  events ;
	struct android_poll_source *  source ;

	/* Make sure glue isn't stripped. */
	app_dummy() ;

	ret = _display.app ? 0 : 1 ;
	_display.app = app ;

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

	/* Prepare to monitor accelerometer. */
	_display.sensor_man = ASensorManager_getInstance() ;
	_display.accel_sensor = ASensorManager_getDefaultSensor(
					_display.sensor_man , ASENSOR_TYPE_ACCELEROMETER) ;
	_display.sensor_evqueue = ASensorManager_createEventQueue(
					_display.sensor_man , app->looper ,
					LOOPER_ID_USER , NULL , NULL) ;

	return  ret ;
}

void
x_display_final(void)
{
	if( locked >= 0)
	{
		locked = -1 ;	/* Don't lock until APP_CMD_INIT_WINDOW after restart. */
		ANativeActivity_finish( _display.app->activity) ;
	}
}

int
x_display_process_event(
	struct android_poll_source *  source ,
	int  ident
	)
{
	if( visible_frame_changed)
	{
		/* XXX should synchronize */
		_display.yoffset = new_yoffset ;
		_disp.width = new_width ;
		_disp.height = new_height ;

		visible_frame_changed = 0 ;

		if( _disp.num_of_roots > 0)
		{
			x_window_resize_with_margin( _disp.roots[0] ,
				_disp.width , _disp.height , NOTIFY_TO_MYSELF) ;
		}
	}

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
			#if  0
				kik_debug_printf( "Accelerometer: x=%f y=%f z=%f" ,
					event.acceleration.x ,
					event.acceleration.y ,
					event.acceleration.z) ;
			#endif
			}
		}
	}

	/* Check if we are exiting. */
	if( _display.app->destroyRequested)
	{
	#ifdef  DEBUG
		kik_debug_printf( KIK_DEBUG_TAG " destroy requested.\n") ;
	#endif

		locked = -1 ;	/* Don't lock until APP_CMD_INIT_WINDOW after restart. */

		return  0 ;
	}

	return  1 ;
}

void
x_display_unlock(void)
{
	if( locked > 0)
	{
		ANativeWindow_unlockAndPost( _display.app->window) ;
		locked = 0 ;
	}
}

size_t
x_display_get_str(
	u_char *  seq ,
	size_t  seq_len
	)
{
	JNIEnv *  env ;
	JavaVM *  vm ;
	jobject  this ;
	jstring  jstr_key ;
	char *  key ;
	size_t  len ;

	vm = _display.app->activity->vm ;
	(*vm)->AttachCurrentThread( vm , &env , NULL) ;

	this = _display.app->activity->clazz ;

	jstr_key = (*env)->GetObjectField( env , this ,
			(*env)->GetFieldID( env , (*env)->GetObjectClass( env , this) ,
				"keyString" , "Ljava/lang/String;")) ;

	if( jstr_key)
	{
		key = (*env)->GetStringUTFChars( env , jstr_key , NULL) ;

		if( ( len = strlen(key)) > seq_len)
		{
			len = 0 ;
		}
		else
		{
			memcpy( seq , key , len) ;
		}

		(*env)->ReleaseStringUTFChars( env , jstr_key , key) ;
	}
	else
	{
		len = 0 ;
	}

	(*vm)->DetachCurrentThread(vm) ;

	return  len ;
}


u_long
x_display_get_pixel(
	int  x ,
	int  y
	)
{
	u_char *  fb ;

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
		if( rotate_display)
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
				line_length = _display.buf.stride * _display.bytes_per_pixel ;
			}
			else
			{
				x = y ;
				y = _disp.width - tmp - 1 ;
				line_length = -(_display.buf.stride * _display.bytes_per_pixel) ;
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
		}
		else
		{
			memcpy( get_fb( x , y) , image , size) ;
		}
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

u_char *
x_display_get_bitmap(
	char *  path ,
	u_int *  width ,
	u_int *  height
	)
{
	JNIEnv *  env ;
	JavaVM *  vm ;
	jobject  this ;
	jintArray  jarray ;
	char *  image ;

	vm = _display.app->activity->vm ;
	(*vm)->AttachCurrentThread( vm , &env , NULL) ;

	this = _display.app->activity->clazz ;

	image = NULL ;
	if( ( jarray = (*env)->CallObjectMethod( env , this ,
			(*env)->GetMethodID( env ,
				(*env)->GetObjectClass( env , this) ,
				"getBitmap" , "(Ljava/lang/String;II)[I") ,
			(*env)->NewStringUTF( env , path) ,
			*width , *height)))
	{
		jint  len ;

		len = (*env)->GetArrayLength( env , jarray) ;

		if( ( image = malloc( (len - 2) * sizeof(u_int32_t))))
		{
			jint *  elems ;

			elems = (*env)->GetIntArrayElements( env , jarray , NULL) ;

			*width = elems[len - 2] ;
			*height = elems[len - 1] ;
			memcpy( image , elems , (len - 2) * sizeof(u_int32_t)) ;

			(*env)->ReleaseIntArrayElements( env , jarray , elems , 0) ;
		}
	}

	(*vm)->DetachCurrentThread(vm) ;

	return  image ;
}


/* Called in the main thread (not in the native activity thread) */
void
Java_mlterm_native_1activity_MLActivity_visibleFrameChanged(
	JNIEnv *  env ,
	jobject  this ,
	jint  yoffset ,
	jint  width ,
	jint  height
	)
{
#ifdef  DEBUG
	kik_debug_printf( "Visible frame changed yoff %d w %d h %d => yoff %d w %d h %d\n" ,
		_display.yoffset , _disp.width , _disp.height , yoffset , width , height) ;
#endif

	if( width < 50 || height < 50)
	{
		/* Don't resize because it may be impossible to show any characters. */
		return ;
	}

	if( _disp.roots)
	{
		/* XXX should synchronize */

		new_yoffset = yoffset ;

		if( rotate_display)
		{
			new_width = height ;
			new_height = width ;
		}
		else
		{
			new_width = width ;
			new_height = height ;
		}

		visible_frame_changed = 1 ;
	}
	else
	{
		_display.yoffset = yoffset ;

		if( rotate_display)
		{
			_disp.width = height ;
			_disp.height = width ;
		}
		else
		{
			_disp.width = width ;
			_disp.height = height ;
		}
	}
}
