/*
 *	$Id$
 */

#include  <uim/uim.h>
#include  <uim/uim-helper.h>

#include  <X11/keysym.h>	/* XK_xxx */
#include  <kiklib/kik_str.h>	/* kik_str_sep, kik_snprintf */
#include  <signal.h>

#include  "x_im.h"

#if 0
#define UIM_DEBUG 1
#endif

#if 0
#define UIM_DEBUG_COMB 1
#endif

typedef struct im_uim
{
	/* input method common object */
	x_im_t  im ;

	uim_context  context ;

	char *  encoding_name ;		/* "anthy", "py", ... */

	mkf_parser_t *  parser ;	/* uim encoding  */
	mkf_conv_t *  conv ;		/* term encoding */

}  im_uim_t ;


/* --- static variables --- */

static int  ref_count = 0 ;
static int  initialized = 0 ;
static int  helper_fd = -1 ;
static im_uim_t *  last_focused_uim = NULL ;

/* mlterm internal symbols */
static x_im_export_syms_t *  mlterm_syms = NULL ;


/* --- static functions --- */

static int
find_engine(
	char *  engine ,
	char **  encoding
	)
{
	uim_context  u ;
	int  i ;
	int  result ;
	
	if( engine == NULL || encoding == NULL)
	{
		return  0 ;
	}

	if( ! ( u = uim_create_context( NULL , "UTF-8" , NULL , NULL ,
					NULL , NULL)))
	{
		return  0 ;
	}

	result = 0 ;

	for( i = 0 ; i < uim_get_nr_im( u) ; i++)
	{
		if( strcmp( uim_get_im_name( u , i) , engine) == 0)
		{
			*encoding = (char*) uim_get_im_encoding( u , i) ;
		#ifdef  UIM_DEBUG_COMB
			*encoding = strdup( "UTF-8");
		#endif

			if( encoding)
			{
				result = 1 ;

				break ;
			}
		}
	}

	uim_release_context( u) ;

	return  result ;
}

static int
xksym_to_ukey(
	KeySym ksym
	)
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
helper_disconnected( void)
{
#ifdef  UIM_DEBUG
	kik_debug_printf("helper was disconnected\n");
#endif

	(*mlterm_syms->func_x_term_manager_remove_fd)( helper_fd) ;

	helper_fd = -1 ;
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


	len = 26 + strlen( last_focused_uim->encoding_name) + strlen( str) + 1 ;

	if( len > sizeof( buf))
	{
	#ifdef  DEBUG
		kik_debug_printf( "property list string is too long.");
	#endif

		return ;
	}

	kik_snprintf( buf , sizeof(buf) , "prop_list_update\ncharset=%s\n%s" ,
		      last_focused_uim->encoding_name , str) ;

	uim_helper_send_message( helper_fd , buf) ;
}

static void
prop_label_update(
	void *  p ,
	const char *  str
	)
{
	im_uim_t *  uim ;
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

	len = 27 + strlen( last_focused_uim->encoding_name) + strlen( str) + 1 ;

	if( len > sizeof( buf))
	{
	#ifdef  DEBUG
		kik_debug_printf( "property label string is too long.");
	#endif

		return ;
	}

	kik_snprintf( buf , sizeof(buf) , "prop_label_update\ncharset=%s\n%s" ,
		      last_focused_uim->encoding_name , str) ;

	uim_helper_send_message( helper_fd , buf) ;
}


static void
commit(
	void *  p ,
	const char *  str
	)
{
	im_uim_t *  uim ;
	u_char  conv_buf[256] ;
	size_t  filled_len ;

	uim = (im_uim_t*) p ;

	(*uim->parser->init)( uim->parser) ;
#if  0
	(*uim->parser->set_str)( uim->parser , str , strlen(str)) ;
#else
	(*uim->parser->set_str)( uim->parser , (char*)str , strlen(str)) ;
#endif

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

		(*uim->im.listener->write_to_term)( uim->im.listener->self ,
						    conv_buf ,
						    filled_len) ;
	}
}

static void
preedit_clear(
	void *  ptr
	)
{
	im_uim_t *  uim ;
	int  i ;

#ifdef  UIM_DEBUG
	kik_debug_printf( "preedit_clear\n") ;
#endif

	uim = (im_uim_t*) ptr ;

	if( uim->im.preedit.chars)
	{
		(*mlterm_syms->func_ml_str_delete)(
						uim->im.preedit.chars ,
						uim->im.preedit.num_of_chars) ;
		uim->im.preedit.chars = NULL ;
	}

	uim->im.preedit.num_of_chars = 0 ;
	uim->im.preedit.filled_len = 0 ;
	uim->im.preedit.segment_offset = 0 ;
	uim->im.preedit.cursor_offset = X_IM_PREEDIT_NOCURSOR ;
}

static void
preedit_pushback(
	void *  ptr ,
	int  attr ,
	const char *  str
	)
{
	im_uim_t *  uim ;
	ml_char_t *  p ;
	mkf_char_t  ch ;
	ml_color_t  fg_color = ML_FG_COLOR ;
	ml_color_t  bg_color = ML_BG_COLOR ;
	int  is_underline = 0 ;
	u_int  count = 0 ;

#ifdef UIM_DEBUG_COMB
	char _str[512];
	static int n = 1;
	if( attr & UPreeditAttr_UnderLine && strlen(str))
	{
		printf("str:%s\n", str);
		int i;
		for (i = 0; i < n * 6; i+=6) {
			_str[i] = 0xe3;
			_str[i+1] = 0x83;
			_str[i+2] = 0xbd;
			_str[i+3] = 0xe3;
			_str[i+4] = 0x82;
			_str[i+5] = 0x99;
		}
		_str[i] = '\0';
		str = &_str[0];
		n++;
	}
#endif

#ifdef  UIM_DEBUG
	kik_debug_printf( "preedit_pushback attr: %d, str:%s, length:%d\n" , attr , str, strlen( str)) ;
#endif

	uim = (im_uim_t*) ptr ;

	if( attr & UPreeditAttr_Cursor)
	{
		uim->im.preedit.cursor_offset = uim->im.preedit.filled_len ;
	}

	if( ! strlen( str))
	{
		return ;
	}

	if( attr & UPreeditAttr_Reverse)
	{
		uim->im.preedit.segment_offset = uim->im.preedit.filled_len ;
		uim->im.preedit.cursor_offset = X_IM_PREEDIT_NOCURSOR ;
		fg_color = ML_BG_COLOR ;
		bg_color = ML_FG_COLOR ;
	}

	if( attr & UPreeditAttr_UnderLine)
	{
		is_underline = 1 ;
	}

	/* TODO: UPreeditAttr_Separator */

	/*
	 * count number of characters to re-allocate im.preedit.chars
	 */
	
	(*uim->parser->init)( uim->parser) ;
	(*uim->parser->set_str)( uim->parser , (char*) str , strlen( str)) ;

	while( (*uim->parser->next_char)( uim->parser , &ch))
	{
		count++ ;
	}

	/* no space left? (current array size < new array size?)*/
	if( uim->im.preedit.num_of_chars < uim->im.preedit.filled_len + count)
	{
		if( ! ( p = realloc( uim->im.preedit.chars , sizeof(ml_char_t) * ( uim->im.preedit.filled_len + count))))
		{
		#ifdef  DEBUG
			kik_warn_printf( KIK_DEBUG_TAG " realloc failed.\n") ;
		#endif

			(*mlterm_syms->func_ml_str_delete)(
						uim->im.preedit.chars ,
						uim->im.preedit.num_of_chars) ;
			uim->im.preedit.chars = NULL ;
			uim->im.preedit.num_of_chars = 0 ;
			uim->im.preedit.filled_len = 0 ;

			return ;
		}

		uim->im.preedit.chars = p ;
		uim->im.preedit.num_of_chars = uim->im.preedit.filled_len + count;
	}

	p = &uim->im.preedit.chars[uim->im.preedit.filled_len];
	(*mlterm_syms->func_ml_str_init)( p , count);

	/*
	 * uim encoding -> term encoding
	 */
	(*uim->parser->init)( uim->parser) ;
	(*uim->parser->set_str)( uim->parser , (char*)str , strlen( str)) ;

	count = 0 ;

	while( (*uim->parser->next_char)( uim->parser , &ch))
	{
		int  is_biwidth = 0 ;
		int  is_comb = 0 ;

		if( ch.cs == ISO10646_UCS4_1)
		{
			if( ch.property & MKF_BIWIDTH)
			{
				is_biwidth = 1 ;
			}
			else if( ch.property & MKF_AWIDTH)
			{
				/* TODO: check col_size_of_width_a */
				is_biwidth = 1 ;
			}
		}

		if( ch.property & MKF_COMBINING)
		{
			is_comb = 1 ;
		}

		if( is_comb)
		{
			if( (*mlterm_syms->func_ml_char_combine)(
							p - 1 , ch.ch ,
							ch.size , ch.cs ,
							is_biwidth , is_comb ,
							fg_color , bg_color ,
							0 , is_underline))
			{
				continue;
			}

			/*
			 * if combining failed , char is normally appended.
			 */
		}

		(*mlterm_syms->func_ml_char_set)( p , ch.ch , ch.size , ch.cs ,
						  is_biwidth , is_comb ,
						  fg_color , bg_color ,
						  0 , is_underline) ;

		p++ ;
		uim->im.preedit.filled_len++;
	}
}

static void
preedit_update(
	void *  ptr
	)
{
	im_uim_t *  uim ;

	uim = (im_uim_t*) ptr ;

	(*uim->im.listener->draw_preedit_str)( uim->im.listener->self ,
					       uim->im.preedit.chars ,
					       uim->im.preedit.filled_len ,
					       uim->im.preedit.cursor_offset) ;
}

/*
 * callback for candidate screen events
 */

static void
candidate_selected(
	void *  p ,
	u_int  index
	)
{
	im_uim_t *  uim ;
	u_int  num_of_candidates ;

#ifdef  UIM_DEBUG
	kik_debug_printf( KIK_DEBUG_TAG " index : %d\n" , index) ;
#endif

	uim = (im_uim_t*) p ;

	uim_set_candidate_index( uim->context , index) ;
}


/*
 * callbacks for candidate selector
 */

static void
candidate_activate(
	void *  p ,
	int  num ,
	int  limit
	)
{
	im_uim_t *  uim ;
	int  x ;
	int  y ;
	int  i ;

#ifdef  UIM_DEBUG
	kik_debug_printf( KIK_DEBUG_TAG " num: %d limit: %d\n", num , limit) ;
#endif

	uim = (im_uim_t*) p ;

	if( uim->im.cand_screen == NULL)
	{
		int  is_vertical ;
		u_int  line_height ;

		(*uim->im.listener->get_spot)( uim->im.listener->self ,
					       uim->im.preedit.chars ,
					       uim->im.preedit.segment_offset ,
					       &x , &y) ;

		if( ! ( uim->im.cand_screen = (*mlterm_syms->func_x_im_candidate_screen_new)(
				(*uim->im.listener->get_win_man)(uim->im.listener->self) ,
				(*uim->im.listener->get_font_man)(uim->im.listener->self) ,
				(*uim->im.listener->get_color_man)(uim->im.listener->self) ,
				(*uim->im.listener->is_vertical)(uim->im.listener->self) ,
				(*uim->im.listener->get_line_height)(uim->im.listener->self) ,
				x , y)))
		{
		#ifdef  DEBUG
			kik_warn_printf( KIK_DEBUG_TAG " x_im_candidate_screen_new() failed.\n") ;
		#endif
			
			return ;
		}

		uim->im.cand_screen->listener.self = uim ;
		uim->im.cand_screen->listener.selected = candidate_selected ;
	}

	if( ! (*uim->im.cand_screen->init)( uim->im.cand_screen , num))
	{
		(*uim->im.cand_screen->delete)( uim->im.cand_screen) ;

		uim->im.cand_screen = NULL ;

		return ;
	}

	(*uim->im.cand_screen->set_spot)( uim->im.cand_screen , x , y) ;

	for( i = 0 ; i < num ; i++)
	{
		uim_candidate  c ;
		u_char *  s ;

		c = uim_get_candidate( uim->context , i , i) ;
		s = (u_char*)uim_candidate_get_cand_str( c) ;

	#ifdef  UIM_DEBUG
		kik_debug_printf( KIK_DEBUG_TAG " %d| %s\n", i , s) ;
	#endif

		(*uim->im.cand_screen->set)( uim->im.cand_screen ,
					uim->parser , uim->conv ,
					s , i) ;

		uim_candidate_free( c) ;
	}

	(*uim->im.cand_screen->select)( uim->im.cand_screen , 0) ;
}

static void
candidate_select(
	void *  p ,
	int  index
	)
{
	im_uim_t *  uim ;

#ifdef  UIM_DEBUG
	kik_debug_printf( KIK_DEBUG_TAG " index: %d\n", index) ;
#endif

	uim = (im_uim_t*) p ;

	if( uim->im.cand_screen)
	{
			(*uim->im.cand_screen->select)( uim->im.cand_screen ,
							index) ;
	}
}

static void
candidate_shift_page(
	void *  p ,
	int  direction
	)
{
	im_uim_t *  uim ;

#ifdef  UIM_DEBUG
	kik_debug_printf( KIK_DEBUG_TAG " direction: %d\n", direction) ;
#endif

	uim = (im_uim_t*) p ;

	/* XXX: not implemented yet */
}

static void
candidate_deactivate(
	void *  p
	)
{
	im_uim_t *  uim ;

#ifdef  UIM_DEBUG
	kik_debug_printf( KIK_DEBUG_TAG "\n") ;
#endif

	uim = (im_uim_t*) p ;

	if( uim->im.cand_screen)
	{
		(*uim->im.cand_screen->delete)( uim->im.cand_screen) ;
		uim->im.cand_screen = NULL ;
	}
}

/*
 * methods of x_im_t
 */

static void
delete(
	x_im_t *  im
	)
{
	im_uim_t *  uim ;

	uim = (im_uim_t*) im ;

	if( last_focused_uim == uim)
	{
		last_focused_uim = NULL ;
	}

	(*uim->parser->delete)( uim->parser) ;
	(*uim->conv->delete)( uim->conv) ;

	if( uim->im.cand_screen)
	{
		(*uim->im.cand_screen->delete)( uim->im.cand_screen) ;
	}

	uim_release_context( uim->context) ;

	ref_count-- ;

#ifdef  UIM_DEBUG
	kik_debug_printf( KIK_DEBUG_TAG " An object was deleted. ref_count: %d\n", ref_count) ;
#endif

	free( uim) ;

	if( ref_count == 0 && initialized)
	{
		uim_helper_close_client_fd( helper_fd) ;
		(*mlterm_syms->func_x_term_manager_remove_fd)( helper_fd) ;
		helper_fd = -1 ;

		uim_quit() ;

		initialized = 0 ;
	}
}

static int
key_event(
	x_im_t *  im ,
	KeySym  ksym ,
	XKeyEvent *  event
	)
{
	im_uim_t *  uim ;
	int  key = 0 ;
	int  state = 0 ;
	int  ret ;

	uim = (im_uim_t*) im ;

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

static void
focused(
	x_im_t *  im
	)
{
	im_uim_t *  uim ;

	uim =  (im_uim_t*)  im ;

	last_focused_uim = uim ;

	uim_prop_list_update( uim->context) ;
	uim_prop_label_update( uim->context) ;

	uim_helper_client_focus_in( uim->context) ;

	if( uim->im.cand_screen)
	{
		(*uim->im.cand_screen->show)( uim->im.cand_screen) ;
	}
}

static void
unfocused(
	x_im_t *  im
	)
{
	im_uim_t *  uim ;

	uim = (im_uim_t*)  im ;

	uim_helper_client_focus_out( uim->context) ;

	if( uim->im.cand_screen)
	{
		(*uim->im.cand_screen->hide)( uim->im.cand_screen) ;
	}
}

static void
draw_preedit(
	x_im_t *  im ,
	int  is_focused
	)
{
	im_uim_t *  uim ;

#ifdef  UIM_DEBUG
	kik_debug_printf( KIK_DEBUG_TAG "is_focused: %d\n", is_focused);
#endif

	uim = (im_uim_t*)  im ;

	(*uim->im.listener->draw_preedit_str)( uim->im.listener->self ,
					       uim->im.preedit.chars ,
					       uim->im.preedit.filled_len ,
					       uim->im.preedit.cursor_offset) ;

	if( uim->im.cand_screen)
	{
		int  x ;
		int  y ;

		if( is_focused)
		{
			if( (*uim->im.listener->get_spot)(
						uim->im.listener->self ,
						uim->im.preedit.chars ,
						uim->im.preedit.segment_offset ,
						&x , &y))
			{
				(*uim->im.cand_screen->show)(
							uim->im.cand_screen) ;
				(*uim->im.cand_screen->set_spot)(
							uim->im.cand_screen ,
							x , y) ;

				return ;
			}
		}

		(*uim->im.cand_screen->hide)( uim->im.cand_screen) ;
	}
}

static void
helper_read_handler( void)
{
	char *  message ;

	uim_helper_read_proc(helper_fd);

	while( ( message = uim_helper_get_message()))
	{
		char *  first_line ;
		char *  second_line ;

	#ifdef  UIM_DEBUG
		kik_debug_printf("message recieved from helper: %s\n" , first_line);
	#endif

		if( ( first_line = kik_str_sep( &message , "\n")))
		{
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


/* --- global functions --- */

x_im_t *
im_new(
	u_int32_t  magic ,
	ml_char_encoding_t  term_encoding ,
	x_im_event_listener_t *  im_listener ,
	x_im_export_syms_t *  export_syms ,
	char *  engine
	)
{
	im_uim_t *  uim ;
	char *  encoding_name ;
	ml_char_encoding_t  encoding ;

	if( magic != (u_int32_t) IM_API_COMPAT_CHECK_MAGIC)
	{
		kik_error_printf( "Incompatible input method API.\n") ;

		return  NULL ;
	}

	uim = NULL ;

	if( ! initialized)
	{
		if( uim_init() == -1)
		{
		#ifdef  DEBUG
			kik_warn_printf( KIK_DEBUG_TAG " failed to initialize uim.") ;
		#endif

			return  NULL ;
		}

		mlterm_syms = export_syms ;

		initialized = 1 ;
	}

	/*
	 * create I/O chanel for uim_helper_server
	 */
	if( helper_fd == -1 &&
	    mlterm_syms &&
	    mlterm_syms->func_x_term_manager_add_fd &&
	    mlterm_syms->func_x_term_manager_remove_fd)
	{
		void * hold_handler;

		hold_handler = signal(SIGCHLD, SIG_DFL) ;

		helper_fd = uim_helper_init_client_fd( helper_disconnected) ;

		signal(SIGCHLD, hold_handler) ;

		(*mlterm_syms->func_x_term_manager_add_fd)(
							helper_fd ,
							helper_read_handler) ;
	}

	if( engine == NULL)
	{
		engine = "default" ;
	}

	if( ! find_engine( engine , &encoding_name))
	{
		kik_error_printf( " Uim does not support %s conversion engine.\n" , engine) ;

		goto  error ;
	}

	if( (encoding = (*mlterm_syms->func_ml_get_char_encoding)( encoding_name)) == ML_UNKNOWN_ENCODING)
	{
	#ifdef  DEBUG
		kik_warn_printf( KIK_DEBUG_TAG " %s is unknown encoding.\n" , encoding_name) ;
	#endif
		goto  error ;
	}

	if( ! ( uim = malloc( sizeof( im_uim_t))))
	{
	#ifdef  DEBUG
		kik_warn_printf( KIK_DEBUG_TAG " malloc failed.\n") ;
	#endif

		goto  error ;
	}

	uim->encoding_name = encoding_name ;
	uim->parser = NULL ;
	uim->conv = NULL ;

	if( ! ( uim->parser = (*mlterm_syms->func_ml_parser_new)( encoding)))
	{
		goto  error ;
	}

	if( ! ( uim->conv = (*mlterm_syms->func_ml_conv_new)( term_encoding)))
	{
		goto  error ;
	}

	if( ! ( uim->context = uim_create_context( uim ,
						   encoding_name ,
						   NULL ,
						   engine ,
#if 0
						   uim_iconv ,
#else
						   NULL ,
#endif
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

	/*
	 * initialize im object
	 */
	uim->im.listener = im_listener ;

	uim->im.cand_screen = NULL ;

	uim->im.preedit.chars = NULL ;
	uim->im.preedit.num_of_chars = 0 ;
	uim->im.preedit.filled_len = 0 ;
	uim->im.preedit.segment_offset = 0 ;
	uim->im.preedit.cursor_offset = X_IM_PREEDIT_NOCURSOR ;

	uim->im.delete = delete ;
	uim->im.key_event = key_event ;
	uim->im.focused = focused ;
	uim->im.unfocused = unfocused ;
	uim->im.draw_preedit = draw_preedit ;

	ref_count++;

#ifdef  UIM_DEBUG
	kik_debug_printf("New object was created. ref_count is %d.\n", ref_count);
#endif

	return  (x_im_t*) uim ;

error:
	if( helper_fd != -1)
	{
		uim_helper_close_client_fd( helper_fd) ;

		(*mlterm_syms->func_x_term_manager_remove_fd)( helper_fd) ;

		helper_fd = -1 ;
	}

	if( initialized && ref_count == 0)
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

