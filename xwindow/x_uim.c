/*
 *	$Id$
 */

#include  <sys/types.h>		/* kill */
#include  <signal.h>		/* kill */

#include  <X11/keysym.h>	/* XK_xxx */
#include  <kiklib/kik_str.h>	/* kik_str_sep, kik_snprintf */

#include  "x_uim.h"

/* --- static variables --- */

static int  use_uim = 0 ;
static int  helper_fd = -1 ;
static int  initialized = 0 ;
static int  num_of_uims = 0 ;
static x_uim_t *  last_focused_uim = NULL ;

/* --- static functions --- */

static void
invoke_candwin_if_not_available(
	candwin_helper_t *  candwin
	)
{
	if( candwin->pid)
	{
		return ;
	}

	candwin->pid = uim_ipc_open_command( 0 ,
					     &candwin->pipe_r ,
					     &candwin->pipe_w ,
					     "uim-helper-candwin-gtk") ;
}

static void
candwin_activate(
	x_uim_t *  uim ,
	int  num
	)
{
	uim_candidate  candidate ;
	int  i ;

	invoke_candwin_if_not_available( &uim->candwin) ;

	fprintf( uim->candwin.pipe_w , "activate\ncharset=%s\n", uim->encoding) ;
	for( i = 0 ; i < num ; i++)
	{
		/* what is accel_enumeration_hint? */
		candidate = uim_get_candidate( uim->context , i , i) ;
		fprintf( uim->candwin.pipe_w , "%s\n",
			 uim_candidate_get_cand_str( candidate)) ;
	#ifdef  UIM_DEBUG
		kik_debug_printf( "%d: %s\n", i,
				  uim_candidate_get_cand_str( candidate)) ;
	#endif
		uim_candidate_free( candidate) ;
	}
	fprintf( uim->candwin.pipe_w , "\n");
	fflush( uim->candwin.pipe_w) ;

	uim->candwin.is_visible = 1 ;
}

static void
candwin_deactivate(
	x_uim_t *  uim
	)
{
	invoke_candwin_if_not_available( &uim->candwin) ;

	fprintf( uim->candwin.pipe_w , "deactivate\n\n") ;
	fflush( uim->candwin.pipe_w) ;

	uim->candwin.is_visible = 0 ;
}

static void
candwin_select(
	x_uim_t *  uim ,
	int  index
	)
{
	invoke_candwin_if_not_available( &uim->candwin) ;

	fprintf( uim->candwin.pipe_w , "select\n") ;
	fprintf( uim->candwin.pipe_w , "%d\n\n" , index) ;
	fflush( uim->candwin.pipe_w) ;
}

static void
candwin_show(
	x_uim_t *  uim
	)
{
	invoke_candwin_if_not_available( &uim->candwin) ;

	fprintf( uim->candwin.pipe_w , "show\n\n") ;
	fflush( uim->candwin.pipe_w) ;

	uim->candwin.is_visible = 1 ;
}

static void
candwin_hide(
	x_uim_t *  uim
	)
{
	invoke_candwin_if_not_available( &uim->candwin) ;

	fprintf( uim->candwin.pipe_w , "hide\n\n") ;
	fflush( uim->candwin.pipe_w) ;

	uim->candwin.is_visible = 0 ;
}

static void
candwin_update_position(
	x_uim_t *  uim
	)
{
	int  x ;
	int  y ;

	invoke_candwin_if_not_available( &uim->candwin) ;

	if( ! (*uim->listener->get_spot)( uim->listener->self ,
					  uim->preedit.chars ,
					  uim->preedit.segment_offset ,
					  &x , &y))
	{
		fprintf( uim->candwin.pipe_w , "hide\n\n") ;
		fflush( uim->candwin.pipe_w) ;

		return  ;
	}

	fprintf( uim->candwin.pipe_w , "show\n\n") ;
	fflush( uim->candwin.pipe_w) ;

	if( uim->candwin.x == x && uim->candwin.y == y)
	{
		return ;
	}

	if( ! uim->candwin.is_visible)
	{
		return ;
	}

	fprintf( uim->candwin.pipe_w , "move\n") ;
	fprintf( uim->candwin.pipe_w , "%d\n" , x) ;
	fprintf( uim->candwin.pipe_w , "%d\n\n" , y) ;
	fflush( uim->candwin.pipe_w) ;

	uim->candwin.x = x ;
	uim->candwin.y = y ;
}

static inline int
xksym_to_ukey(KeySym ksym)
{
	/* Latin 1 */
	if (XK_space <= ksym && ksym <= XK_asciitilde)
	{
		return  ksym ;
	}

	switch (ksym)
	{
	/* TTY Functions */
	case  XK_BackSpace:
		return  UKey_Backspace ;
	case  XK_Tab:
		return  UKey_Tab ;
	case  XK_Return:
		return  UKey_Return ;
	case  XK_Escape:
		return  UKey_Escape ;
	case  XK_Delete:
		return  UKey_Delete ;
	/* International & multi-key character composition */
	case  XK_Multi_key:
		return  UKey_Multi_key ;
	/* Japanese keyboard support */
	case  XK_Muhenkan:
		return  UKey_Muhenkan ;
	case  XK_Henkan_Mode:
		return  UKey_Henkan_Mode ;
	case  XK_Zenkaku_Hankaku:
		return  UKey_Zenkaku_Hankaku ;
	/* Cursor control & motion */
	case  XK_Home:
		return  UKey_Home ;
	case  XK_Left:
		return  UKey_Left ;
	case  XK_Up:
		return  UKey_Up ;
	case  XK_Right:
		return  UKey_Right ;
	case  XK_Down:
		return  UKey_Down ;
	case  XK_Prior:
		return  XK_Prior ;
	case  XK_Next:
		return  UKey_Next ;
	case  XK_End:
		return  UKey_End ;
	case  XK_F1:
		return  UKey_F1 ;
	case  XK_F2:
		return  UKey_F2 ;
	case  XK_F3:
		return  UKey_F3 ;
	case  XK_F4:
		return  UKey_F4 ;
	case  XK_F5:
		return  UKey_F5 ;
	case  XK_F6:
		return  UKey_F6 ;
	case  XK_F7:
		return  UKey_F7 ;
	case  XK_F8:
		return  UKey_F8 ;
	case  XK_F9:
		return  UKey_F9 ;
	case  XK_F10:
		return  UKey_F10 ;
	case  XK_F11:
		return  UKey_F11 ;
	case  XK_F12:
		return  UKey_F12 ;
	case  XK_F13:
		return  UKey_F13 ;
	case  XK_F14:
		return  UKey_F14 ;
	case  XK_F15:
		return  UKey_F15 ;
	case  XK_F16:
		return  UKey_F16 ;
	case  XK_F17:
		return  UKey_F17 ;
	case  XK_F18:
		return  UKey_F18 ;
	case  XK_F19:
		return  UKey_F19 ;
	case  XK_F20:
		return  UKey_F20 ;
	case  XK_F21:
		return  UKey_F21 ;
	case  XK_F22:
		return  UKey_F22 ;
	case  XK_F23:
		return  UKey_F23 ;
	case  XK_F24:
		return  UKey_F24 ;
	case  XK_F25:
		return  UKey_F25 ;
	case  XK_F26:
		return  UKey_F26 ;
	case  XK_F27:
		return  UKey_F27 ;
	case  XK_F28:
		return  UKey_F28 ;
	case  XK_F29:
		return  UKey_F29 ;
	case  XK_F30:
		return  UKey_F30 ;
	case  XK_F31:
		return  UKey_F31 ;
	case  XK_F32:
		return  UKey_F32 ;
	case  XK_F33:
		return  UKey_F33 ;
	case  XK_F34:
		return  UKey_F34 ;
	case  XK_F35:
		return  UKey_F35 ;
	case  XK_KP_Space:
		return  ' ' ;
	case  XK_KP_Tab:
		return  UKey_Tab ;
	case  XK_KP_Enter:
		return  UKey_Return ;
	case  XK_KP_F1:
		return  UKey_F1 ;
	case  XK_KP_F2:
		return  UKey_F2 ;
	case  XK_KP_F3:
		return  UKey_F3 ;
	case  XK_KP_F4:
		return  UKey_F4 ;
	case  XK_KP_Home:
		return  UKey_Home ;
	case  XK_KP_Left:
		return  UKey_Left ;
	case  XK_KP_Up:
		return  UKey_Up ;
	case  XK_KP_Right:
		return  UKey_Right ;
	case  XK_KP_Down:
		return  UKey_Down ;
	case  XK_KP_Prior:
		return  UKey_Prior ;
	case  XK_KP_Next:
		return  UKey_Next ;
	case  XK_KP_End:
		return  UKey_End ;
	case  XK_KP_Delete:
		return  UKey_Delete ;
	case  XK_KP_Equal:
		return  '=' ;
	case  XK_KP_Multiply:
		return  '*' ;
	case  XK_KP_Add:
		return  '+' ;
	case  XK_KP_Separator:
		return  ',' ;
	case  XK_KP_Subtract:
		return  '-' ;
	case  XK_KP_Decimal:
		return  '.' ;
	case  XK_KP_Divide:
		return  '/' ;
	/* keypad numbers */
	case  XK_KP_0:
		return  '0' ;
	case  XK_KP_1:
		return  '1' ;
	case  XK_KP_2:
		return  '2' ;
	case  XK_KP_3:
		return  '3' ;
	case  XK_KP_4:
		return  '4' ;
	case  XK_KP_5:
		return  '5' ;
	case  XK_KP_6:
		return  '6' ;
	case  XK_KP_7:
		return  '7' ;
	case  XK_KP_8:
		return  '8' ;
	case  XK_KP_9:
		return  '9' ;
	default:
		return  UKey_Other ;
	}
}

static void
preedit_clear(
	void *  ptr
	)
{
	x_uim_t *  uim ;
	int  i ;

#ifdef  UIM_DEBUG
	kik_debug_printf( "preedit_clear\n") ;
#endif

	uim = (x_uim_t*) ptr ;

	if( uim->preedit.chars)
	{
		ml_str_delete( uim->preedit.chars , uim->preedit.num_of_chars) ;
		uim->preedit.chars = NULL ;
	}
	uim->preedit.num_of_chars = 0 ;
	uim->preedit.segment_offset = 0 ;
	uim->preedit.cursor_position = -1 ;
}

static void
preedit_pushback(
	void *  ptr ,
	int  attr ,
	char *  str
	)
{
	x_uim_t *  uim ;
	ml_char_t *  p ;
	mkf_char_t  ch ;
	ml_color_t  fg_color = ML_FG_COLOR ;
	ml_color_t  bg_color = ML_BG_COLOR ;
	int  is_underline = 0 ;
	u_int  num_of_chars = 0 ;

#if 0 /* for debugging combining character wraparound */
	char _str[512];
	if( attr & UPreeditAttr_UnderLine)
	{
		int i;
		for (i = 0; i < (strlen(str) * 3); i+=6) {
			_str[i] = 0xe3;
			_str[i+1] = 0x83;
			_str[i+2] = 0xbd;
			_str[i+3] = 0xe3;
			_str[i+4] = 0x82;
			_str[i+5] = 0x99;
		}
		_str[i] = 0;
		str = &_str[0];
	}
#endif

#ifdef  UIM_DEBUG
	kik_debug_printf( "preedit_pushback attr: %d, str:%s, length:%d\n" , attr , str, strlen( str)) ;
#endif

	uim = (x_uim_t*) ptr ;

	if( attr & UPreeditAttr_Cursor)
	{
		uim->preedit.cursor_position = uim->preedit.num_of_chars ;
	}

	if( ! strlen( str))
	{
		return ;
	}

	if( attr & UPreeditAttr_Reverse)
	{
		uim->preedit.segment_offset = uim->preedit.num_of_chars ;
		uim->preedit.cursor_position = -1 ;
		fg_color = ML_BG_COLOR ;
		bg_color = ML_FG_COLOR ;
	}

	if( attr & UPreeditAttr_UnderLine)
	{
		is_underline = 1 ;
	}

	/* TODO: UPreeditAttr_Separator */

	/* count number of character */
	(*uim->parser->init)( uim->parser) ;
	(*uim->parser->set_str)( uim->parser , str , strlen( str)) ;

	while( (*uim->parser->next_char)( uim->parser , &ch))
	{
		num_of_chars++ ;
	}

	if( ! ( p = realloc( uim->preedit.chars , sizeof(ml_char_t) * ( uim->preedit.num_of_chars + num_of_chars))))
	{
	#ifdef  DEBUG
		kik_warn_printf( KIK_DEBUG_TAG " realloc failed.\n") ;
	#endif
		ml_str_delete( uim->preedit.chars, uim->preedit.num_of_chars) ;
		uim->preedit.chars = NULL ;
		uim->preedit.num_of_chars = 0 ;

		return ;
	}
	uim->preedit.chars = p ;
	p = &uim->preedit.chars[uim->preedit.num_of_chars];

	/* uim encoding -> term encoding */
	(*uim->parser->init)( uim->parser) ;
	(*uim->parser->set_str)( uim->parser , str , strlen(str)) ;

	while( (*uim->parser->next_char)( uim->parser , &ch))
	{
		ml_char_init( p) ;

		/* XXX: should check use_dynamic_comb? */
		if( ch.property & MKF_COMBINING)
		{
			ml_char_combine( p - 1 ,
					 ch.ch , ch.size , ch.cs ,
					 ch.property & MKF_BIWIDTH ? 1 : 0 ,
					 1 ,
					 fg_color , bg_color ,
					 0 , is_underline) ;
		}
		else
		{
			ml_char_set( p , ch.ch , ch.size , ch.cs ,
				     ch.property & MKF_BIWIDTH ? 1 : 0 ,
				     0 ,
				     fg_color , bg_color ,
				     0 , is_underline) ;
		}
		p++ ;
	}

	uim->preedit.num_of_chars += num_of_chars ;
}

static void
preedit_update(
	void *  ptr
	)
{
	x_uim_t *  uim ;

#ifdef  UIM_DEBUG
	kik_debug_printf( "preedit_update\n") ;
#endif

	uim = (x_uim_t*) ptr ;

	(*uim->listener->draw_preedit_str)( uim->listener->self ,
					    uim->preedit.chars ,
					    uim->preedit.num_of_chars ,
					    uim->preedit.cursor_position) ;

	candwin_update_position( uim) ;
}

static void
candidate_activate(
	void *  ptr ,
	int  num ,
	int  limit
	)
{
	x_uim_t *  uim ;

#ifdef  UIM_DEBUG
	kik_debug_printf( "candidate_activate: num: %d limit: %d\n", num , limit) ;
#endif

	uim = (x_uim_t*) ptr ;

	candwin_activate( uim , num) ;
}

static void
candidate_select(
	void *  ptr ,
	int  index
	)
{
	x_uim_t *  uim ;

#ifdef  UIM_DEBUG
	kik_debug_printf( "candidate_select: index: %d\n", index) ;
#endif

	uim = (x_uim_t*) ptr ;

	candwin_select( uim , index) ;
}

static void
candidate_shift_page(
	void *  ptr ,
	int  direction
	)
{
	x_uim_t *  uim ;

#ifdef  UIM_DEBUG
	kik_debug_printf( "candidate_shift_page\n") ;
#endif
}

static void
candidate_deactivate(
	void *  ptr
	)
{
	x_uim_t *  uim ;

#ifdef  UIM_DEBUG
	kik_debug_printf( "candidate_deactivate\n") ;
#endif

	uim = (x_uim_t*) ptr ;

	candwin_deactivate( uim) ;
}

static void
commit(
	void *  p ,
	const char *  str
	)
{
	x_uim_t *  uim ;
	u_char  conv_buf[256] ;
	size_t  filled_len ;

	uim = (x_uim_t*) p ;

	(*uim->parser->init)( uim->parser) ;
	(*uim->parser->set_str)( uim->parser , (char*)str , strlen(str)) ;

	(*uim->conv->init)( uim->conv) ;

	while( ! uim->parser->is_eos)
	{
		filled_len = (*uim->conv->convert)( uim->conv ,
						    conv_buf ,
						    sizeof( conv_buf) ,
						    uim->parser) ;

		if( filled_len == 0)
		{
			/* finished converting */
			break ;
		}

		(*uim->listener->write_to_term)( uim->listener->self ,
						 conv_buf ,
						 filled_len) ;
	}
}

static void
prop_list_update(
	void *  p ,
	const char *  str
	)
{
	char  buf[BUFSIZ] ;
	int  len ;

#ifdef  UIM_DEBUG
	kik_debug_printf("prop_list_update(), str: %s\n", str);
#endif

	if( last_focused_uim == NULL)
	{
		return ;
	}

	if( last_focused_uim->context != p)
	{
		uim_prop_list_update( last_focused_uim->context) ;

		return ;
	}


	len = 26 + strlen( last_focused_uim->encoding) + strlen( str) + 1 ;

	if( len > sizeof( buf))
	{
	#ifdef  DEBUG
		kik_debug_printf( "property list string is too long.");
	#endif

		return ;
	}

	kik_snprintf( buf , sizeof(buf) , "prop_list_update\ncharset=%s\n%s" ,
		      last_focused_uim->encoding , str) ;

	uim_helper_send_message( helper_fd , buf) ;
}

static void
prop_label_update(
	void *  p ,
	const char *  str
	)
{
	x_uim_t *  uim ;
	char  buf[BUFSIZ] ;
	int  len ;

#ifdef  UIM_DEBUG
	kik_debug_printf("prop_label_update(), str: %s\n", str);
#endif

	if( last_focused_uim == NULL)
	{
		return ;
	}

	if( last_focused_uim->context != p)
	{
		uim_prop_label_update( last_focused_uim->context) ;

		return ;
	}

	len = 27 + strlen( last_focused_uim->encoding) + strlen( str) + 1 ;

	if( len > sizeof( buf))
	{
	#ifdef  DEBUG
		kik_debug_printf( "property label string is too long.");
	#endif

		return ;
	}

	kik_snprintf( buf , sizeof(buf) , "prop_label_update\ncharset=%s\n%s" ,
		      last_focused_uim->encoding , str) ;

	uim_helper_send_message( helper_fd , buf) ;
}

static void
helper_disconnected( void)
{
#ifdef  UIM_DEBUG
	kik_debug_printf("helper_disconnected()\n");
#endif

	helper_fd = -1 ;
}

/* --- global functions --- */

int
x_uim_init(
	int  _use_uim
	)
{
	if( ! ( use_uim = _use_uim))
	{
		return  0 ;
	}

	if( helper_fd == -1)
	{
		helper_fd = uim_helper_init_client_fd( helper_disconnected) ;
	}
}

x_uim_t *
x_uim_new(
	ml_char_encoding_t  term_encoding ,
	x_uim_event_listener_t *  uim_listener ,
	char *  engine
	)
{
	x_uim_t *  uim = NULL ;
	uim_context  dummy = NULL ;
	ml_char_encoding_t  encoding = ML_UNKNOWN_ENCODING ;
	char *  uim_encoding = NULL ;
	int  i ;

	if( ! use_uim)
	{
		return  NULL ;
	}

	if( ! engine)
	{
		return  NULL ;
	}

	if( ! initialized)
	{
		if( uim_init() == -1)
		{
		#ifdef  DEBUG
			kik_warn_printf( KIK_DEBUG_TAG " failed to initialize uim.") ;
		#endif

			return  NULL ;
		}

		initialized = 1 ;
	}

	if( ! ( dummy = uim_create_context( NULL , "UTF-8" , NULL , NULL ,
					    uim_iconv , NULL)))
	{
		goto  error ;
	}

	for( i = 0 ; i < uim_get_nr_im( dummy) ; i++)
	{
		if( strcmp( uim_get_im_name( dummy , i) , engine) == 0)
		{
			uim_encoding = (char*) uim_get_im_encoding( dummy , i) ;
			encoding = ml_get_char_encoding( uim_encoding) ;
		}
	}

	uim_release_context( dummy) ;

	if( ! uim_encoding)
	{
	#ifdef  DEBUG
		kik_warn_printf( KIK_DEBUG_TAG " %s is unsupported conversion engine.\n" , engine) ;
	#endif

		goto  error ;
	}

	if( encoding == ML_UNKNOWN_ENCODING)
	{
	#ifdef  DEBUG
		kik_warn_printf( KIK_DEBUG_TAG " %s is unknown encoding.\n" , uim_encoding) ;
	#endif

		goto  error ;
	}

	if( ! ( uim = malloc( sizeof( x_uim_t))))
	{
	#ifdef  DEBUG
		kik_warn_printf( KIK_DEBUG_TAG " malloc failed.\n") ;
	#endif

		goto  error ;
	}

	if( ! ( uim->parser = ml_parser_new( encoding)))
	{
		goto  error ;
	}

	if( ! ( uim->conv = ml_conv_new( term_encoding)))
	{
		goto  error ;
	}

	if( ! ( uim->context = uim_create_context( uim ,
						   uim_encoding ,
						   NULL ,
						   engine ,
						   uim_iconv ,
						   commit)))
	{
	#ifdef  DEBUG
		kik_warn_printf( KIK_DEBUG_TAG " could not create uim context.\n") ;
	#endif

		goto  error ;
	}

	uim_set_preedit_cb( uim->context ,
			    preedit_clear ,
			    preedit_pushback ,
			    preedit_update) ;

	uim_set_candidate_selector_cb( uim->context ,
				       candidate_activate ,
				       candidate_select ,
				       candidate_shift_page ,
				       candidate_deactivate) ;

	uim_set_prop_list_update_cb( uim->context , prop_list_update) ;
	uim_set_prop_label_update_cb( uim->context , prop_label_update) ;

	uim_prop_list_update( uim->context) ;

	uim->encoding = uim_encoding ;

	uim->preedit.chars = NULL ;
	uim->preedit.num_of_chars = 0 ;
	uim->preedit.segment_offset = 0 ;
	uim->preedit.cursor_position = -1 ;

	uim->listener = uim_listener ;

	uim->candwin.pid = 0 ;
	uim->candwin.pipe_r = NULL ;
	uim->candwin.pipe_w = NULL ;
	uim->candwin.x = 0 ;
	uim->candwin.y = 0 ;
	uim->candwin.is_visible = 0 ;

	num_of_uims++ ;
#ifdef  UIM_DEBUG
	kik_debug_printf("x_uim_new, num_of_uims: %d\n", num_of_uims) ;
#endif

	return  uim ;

error:
	if( ! initialized && num_of_uims == 0)
	{
		uim_quit() ;

		initialized = 0 ;
	}

	if( uim)
	{
		if( uim->parser)
		{
			(*uim->parser->delete)( uim->parser) ;
		}

		if( uim->conv)
		{
			(*uim->conv->delete)( uim->conv) ;
		}

		free( uim) ;
	}

	return  NULL ;
}

void
x_uim_delete(
	x_uim_t *  uim
	)
{
	if( ! uim)
	{
		return ;
	}

	if( last_focused_uim == uim)
	{
		last_focused_uim = NULL ;
	}

	if( uim->candwin.pid)
	{
		fclose( uim->candwin.pipe_w) ;
		fclose( uim->candwin.pipe_r) ;
		kill( uim->candwin.pid , SIGKILL) ;
		uim->candwin.pid = 0 ;
		uim->candwin.pipe_w = NULL ;
		uim->candwin.pipe_r = NULL ;
		uim->candwin.x = 0 ;
		uim->candwin.y = 0 ;
		uim->candwin.is_visible = 0 ;
	}

	(*uim->parser->delete)( uim->parser) ;
	(*uim->conv->delete)( uim->conv) ;

	uim_release_context( uim->context) ;

	num_of_uims-- ;

#ifdef  UIM_DEBUG
	kik_debug_printf("x_uim_delete, num_of_uims: %d\n", num_of_uims) ;
#endif

	free( uim) ;

	if( num_of_uims == 0 && initialized)
	{
#if 0
		uim_helper_close_client_fd( helper_fd) ;
#endif

		uim_quit() ;

		initialized = 0 ;
	}
}

int
x_uim_filter_key_event(
	x_uim_t *  uim ,
	KeySym  ksym ,
	XKeyEvent *  event
	)
{
	int  key = 0 ;
	int  state = 0 ;
	int  ret ;

	if( event->state & ShiftMask)
		state += UKey_Shift;
	if( event->state & LockMask)
		state += 0; /* XXX */
	if( event->state & ControlMask)
		state += UKey_Control;
	if( event->state & Mod1Mask)
		state += 0 ; /* XXX */
	if( event->state & Mod2Mask)
		state += 0 ; /* XXX */
	if( event->state & Mod3Mask)
		state += 0 ; /* XXX */
	if( event->state & Mod4Mask)
		state += 0 ; /* XXX */
	if( event->state & Mod5Mask)
		state += 0 ; /* XXX */

	key = xksym_to_ukey(ksym) ;

	ret = uim_press_key( uim->context , key , state) ;
	uim_release_key( uim->context , key , state) ;

	return  ret;
}

void
x_uim_focused(
	x_uim_t *  uim
	)
{
	candwin_show( uim) ;

	last_focused_uim = uim ;

	uim_prop_list_update( uim->context) ;
	uim_prop_label_update( uim->context) ;

	uim_helper_client_focus_in( uim->context) ;
}

void
x_uim_unfocused(
	x_uim_t *  uim
	)
{
	candwin_hide( uim) ;

#if 0
	if( last_focused_uim == uim)
	{
		last_focused_uim = NULL ;
	}
#endif

	uim_helper_client_focus_out( uim->context) ;
}

void
x_uim_redraw_preedit(
	x_uim_t *  uim
	)
{
	(*uim->listener->draw_preedit_str)( uim->listener->self ,
					    uim->preedit.chars ,
					    uim->preedit.num_of_chars ,
					    uim->preedit.cursor_position) ;

	candwin_update_position( uim) ;
}

int
x_uim_get_helper_fd( void)
{
	if( initialized)
	{
		return  helper_fd ;
	}

	return  -1 ;
}

void
x_uim_parse_helper_messege(void)
{
	char *  message ;

#ifdef  UIM_DEBUG
	kik_debug_printf( "x_uim_parse_helper_messege()\n");
#endif

	uim_helper_read_proc(helper_fd);

	while( ( message = uim_helper_get_message()))
	{
		char *  first_line ;
		char *  second_line ;

		if( ( first_line = kik_str_sep( &message , "\n")))
		{
		#ifdef  UIM_DEBUG
			kik_debug_printf(" recieved message: %s\n" , first_line);
		#endif

			if( strcmp( first_line , "prop_activate") == 0)
			{
				second_line = kik_str_sep( &message , "\n") ;
				if( second_line && last_focused_uim)
				{
					uim_prop_activate(
						last_focused_uim->context ,
						second_line) ;
				}
			}
		}

		free( first_line) ;
	}
}

