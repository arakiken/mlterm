/*
 *	$Id$
 */

#include  <stdio.h>
#include  <fcitx/frontend.h>
#include  <fcitx-gclient/fcitxclient.h>

#include  <kiklib/kik_debug.h>
#include  <kiklib/kik_mem.h>

#include  <x_im.h>

#include  "../im_common.h"
#include  "../im_info.h"

#define  FCITX_ID  -3

#if  0
#define  IM_FCITX_DEBUG  1
#endif

#ifdef  USE_FRAMEBUFFER
#define  KeyPress  2	/* see xwindow/fb/x_display.h */
#endif


typedef struct im_fcitx
{
	/* input method common object */
	x_im_t  im ;

	FcitxClient *  client ;

	ml_char_encoding_t  term_encoding ;

#ifdef  USE_FRAMEBUFFER
	mkf_parser_t *  parser_term ;	/* for term encoding */
#endif
	mkf_conv_t *  conv ;		/* for term encoding */

	/*
	 * Cache a result of fcitx_input_context_is_enabled() which uses
	 * DBus connection internally.
	 */
	gboolean  is_enabled ;

	XKeyEvent  prev_key ;

}  im_fcitx_t ;


/* --- static variables --- */

static int  ref_count = 0 ;
static mkf_parser_t *  parser_utf8 = NULL ;
static x_im_export_syms_t *  syms = NULL ; /* mlterm internal symbols */
#ifdef  DEBUG_MODKEY
static int  mod_key_debug = 0 ;
#endif


/* --- static functions --- */

/*
 * methods of x_im_t
 */

static int
delete(
	x_im_t *  im
	)
{
	im_fcitx_t *  fcitx ;

	fcitx = (im_fcitx_t*) im ;

	g_object_unref( fcitx->client) ;

	if( fcitx->conv)
	{
		(*fcitx->conv->delete)( fcitx->conv) ;
	}

#ifdef  USE_FRAMEBUFFER
	if( fcitx->parser_term)
	{
		(*fcitx->parser_term->delete)( fcitx->parser_term) ;
	}
#endif

	free( fcitx) ;

	if( --ref_count == 0)
	{
		(*syms->x_event_source_remove_fd)( FCITX_ID) ;

		if( parser_utf8)
		{
			(*parser_utf8->delete)( parser_utf8) ;
			parser_utf8 = NULL ;
		}
	}

#ifdef  IM_FCITX_DEBUG
	kik_debug_printf( KIK_DEBUG_TAG " An object was deleted. ref_count: %d\n", ref_count) ;
#endif

	return  ref_count ;
}

#ifdef  USE_FRAMEBUFFER
static KeySym
native_to_fcitx_ksym(
	KeySym  ksym
	)
{
	switch( ksym)
	{
	case  XK_BackSpace:
		return  FcitxKey_BackSpace ;

	case  XK_Tab:
		return  FcitxKey_Tab ;

	case  XK_Return:
		return  FcitxKey_Return ;

	case  XK_Escape:
		return  FcitxKey_Escape ;

	case  XK_Zenkaku_Hankaku:
		return  FcitxKey_Zenkaku_Hankaku ;

	case  XK_Hiragana_Katakana:
		return  FcitxKey_Hiragana_Katakana ;

	case  XK_Muhenkan:
		return  FcitxKey_Muhenkan ;

	case  XK_Henkan_Mode:
		return  FcitxKey_Henkan_Mode ;

	case  XK_Home:
		return  FcitxKey_Home ;

	case  XK_Left:
		return  FcitxKey_Left ;

	case  XK_Up:
		return  FcitxKey_Up ;

	case  XK_Right:
		return  FcitxKey_Right ;

	case  XK_Down:
		return  FcitxKey_Down ;

	case  XK_Prior:
		return  FcitxKey_Prior ;

	case  XK_Next:
		return  FcitxKey_Next ;

	case  XK_Insert:
		return  FcitxKey_Insert ;

	case  XK_End:
		return  FcitxKey_End ;

	case  XK_Num_Lock:
		return  FcitxKey_Num_Lock ;

	case  XK_Shift_L:
		return  FcitxKey_Shift_L ;

	case  XK_Shift_R:
		return  FcitxKey_Shift_R ;

	case  XK_Control_L:
		return  FcitxKey_Control_L ;

	case  XK_Control_R:
		return  FcitxKey_Control_R ;

	case  XK_Caps_Lock:
		return  FcitxKey_Caps_Lock ;

	case  XK_Meta_L:
		return  FcitxKey_Meta_L ;

	case  XK_Meta_R:
		return  FcitxKey_Meta_R ;

	case  XK_Alt_L:
		return  FcitxKey_Alt_L ;

	case  XK_Alt_R:
		return  FcitxKey_Alt_R ;

	case  XK_Delete:
		return  FcitxKey_Delete ;

	default:
		return  ksym ;
	}
}
#else
#define  native_to_fcitx_ksym( ksym)  (ksym)
#endif

static int
key_event(
	x_im_t *  im ,
	u_char  key_char ,
	KeySym  ksym ,
	XKeyEvent *  event
	)
{
	im_fcitx_t *  fcitx ;

	fcitx = (im_fcitx_t*) im ;

	if( event->state & FcitxKeyState_IgnoredMask)
	{
		/* Is put back in forward_key_event */
		event->state &= ~FcitxKeyState_IgnoredMask ;
	}
	else if( fcitx_client_process_key_sync( fcitx->client ,
			native_to_fcitx_ksym( ksym) ,
		#ifdef  USE_FRAMEBUFFER
			event->keycode ,
			event->state ,
		#else
			event->keycode - 8 ,
			event->state ,
		#endif
			event->type == KeyPress ? FCITX_PRESS_KEY : FCITX_RELEASE_KEY ,
		#ifdef  USE_FRAMEBUFFER
			0L	/* CurrentTime */
		#else
			event->time
		#endif
			))
	{
		fcitx->is_enabled = TRUE ;
		memcpy( &fcitx->prev_key , event , sizeof(XKeyEvent)) ;

		g_main_context_iteration( g_main_context_default() , FALSE) ;

		return  0 ;
	}
	else
	{
		fcitx->is_enabled = FALSE ;

		if( fcitx->im.preedit.filled_len > 0)
		{
			g_main_context_iteration( g_main_context_default() , FALSE) ;
		}
	}

	return  1 ;
}

static int
switch_mode(
	x_im_t *  im
	)
{
	im_fcitx_t *  fcitx ;

	fcitx =  (im_fcitx_t*)  im ;

	if( fcitx->is_enabled)
	{
		fcitx_client_close_ic( fcitx->client) ;
		fcitx->is_enabled = FALSE ;
	}
	else
	{
		fcitx_client_enable_ic( fcitx->client) ;
		fcitx->is_enabled = TRUE ;
	}

	return  1 ;
}

static int
is_active(
	x_im_t *  im
	)
{
	return  ((im_fcitx_t*)im)->is_enabled ;
}

static void
focused(
	x_im_t *  im
	)
{
	im_fcitx_t *  fcitx ;

	fcitx =  (im_fcitx_t*)  im ;

	fcitx_client_focus_in( fcitx->client) ;

	if( fcitx->im.stat_screen)
	{
		(*fcitx->im.stat_screen->show)( fcitx->im.stat_screen) ;
	}
}

static void
unfocused(
	x_im_t *  im
	)
{
	im_fcitx_t *  fcitx ;

	fcitx = (im_fcitx_t*)  im ;

	fcitx_client_focus_out( fcitx->client) ;

	if( fcitx->im.stat_screen)
	{
		(*fcitx->im.stat_screen->hide)( fcitx->im.stat_screen) ;
	}
}


static void
connected(
	FcitxClient *  client ,
	void *  data
	)
{
	im_fcitx_t *  fcitx ;

	fcitx = data ;

	fcitx_client_set_capacity( client ,
	#ifdef  USE_FRAMEBUFFER
		CAPACITY_CLIENT_SIDE_UI|CAPACITY_CLIENT_SIDE_CONTROL_STATE
	#else
		CAPACITY_PREEDIT|CAPACITY_FORMATTED_PREEDIT
	#endif
		) ;
	fcitx_client_focus_in( client) ;
}

static void
disconnected(
	FcitxClient *  client ,
	void *  data
	)
{
	im_fcitx_t *  fcitx ;

	fcitx = data ;

	fcitx->is_enabled = FALSE ;

	if( fcitx->im.stat_screen)
	{
		(*fcitx->im.stat_screen->delete)( fcitx->im.stat_screen) ;
		fcitx->im.stat_screen = NULL ;
	}
}

#if  0
static void
enable_im(
	FcitxClient *  client ,
	void *  data
	)
{
}
#endif

static void
close_im(
	FcitxClient *  client ,
	void *  data
	)
{
	disconnected( client , data) ;
}

static void
commit_string(
	FcitxClient *  client ,
	char *  str ,
	void *  data
	)
{
	im_fcitx_t *  fcitx ;
	size_t  len ;

	fcitx = data ;

	if( fcitx->im.preedit.filled_len > 0)
	{
		/* Reset preedit */
		fcitx->im.preedit.filled_len = 0 ;
		fcitx->im.preedit.cursor_offset = 0 ;
		(*fcitx->im.listener->draw_preedit_str)( fcitx->im.listener->self ,
						       fcitx->im.preedit.chars ,
						       fcitx->im.preedit.filled_len ,
						       fcitx->im.preedit.cursor_offset) ;
	}

	if( ( len = strlen( str)) == 0)
	{
		/* do nothing */
	}
	else if( fcitx->term_encoding == ML_UTF8)
	{
		(*fcitx->im.listener->write_to_term)(
						fcitx->im.listener->self ,
						str , len) ;
	}
	else
	{
		u_char  conv_buf[256] ;
		size_t  filled_len ;

		(*parser_utf8->init)( parser_utf8) ;
		(*parser_utf8->set_str)( parser_utf8 , str , len) ;

		(*fcitx->conv->init)( fcitx->conv) ;

		while( ! parser_utf8->is_eos)
		{
			filled_len = (*fcitx->conv->convert)( fcitx->conv , conv_buf ,
						sizeof( conv_buf) , parser_utf8) ;

			if( filled_len == 0)
			{
				/* finished converting */
				break ;
			}

			(*fcitx->im.listener->write_to_term)( fcitx->im.listener->self ,
							conv_buf , filled_len) ;
		}
	}

#ifdef  USE_FRAMEBUFFER
	if( fcitx->im.stat_screen)
	{
		(*fcitx->im.stat_screen->delete)( fcitx->im.stat_screen) ;
		fcitx->im.stat_screen = NULL ;
	}
#endif
}

static void
forward_key(
	FcitxClient *  client ,
	guint  keyval ,
	guint  state ,
	gint  type ,
	void *  data
	)
{
	im_fcitx_t *  fcitx ;

	fcitx = data ;

	if( fcitx->prev_key.keycode ==
		#ifdef  USE_FRAMEBUFFER
			keyval
		#else
			keyval + 8
		#endif
			)
	{
		fcitx->prev_key.state |= FcitxKeyState_IgnoredMask ;
	#ifndef  USE_FRAMEBUFFER
		XPutBackEvent( fcitx->prev_key.display , &fcitx->prev_key) ;
	#endif
		memset( &fcitx->prev_key , 0 , sizeof(XKeyEvent)) ;
	}
}

#ifdef  USE_FRAMEBUFFER

static void
update_client_side_ui(
	FcitxClient *  client ,
	char *  auxup ,
	char *  auxdown ,
	char *  preedit ,
	char *  candidateword ,
	char *  imname ,
	int  cursor_pos ,
	void *  data
	)
{
	im_fcitx_t *  fcitx ;
	int  x ;
	int  y ;
	mkf_char_t  ch ;
	ml_char_t *  p ;
	u_int  num_of_chars ;
	size_t  preedit_len ;

	fcitx = (im_fcitx_t*) data ;

	if( ( preedit_len = strlen(preedit)) == 0)
	{
		if( fcitx->im.preedit.filled_len == 0)
		{
			return ;
		}

		/* Stop preediting. */
		fcitx->im.preedit.filled_len = 0 ;
	}
	else
	{
		fcitx->im.preedit.cursor_offset = num_of_chars = 0 ;
		(*parser_utf8->init)( parser_utf8) ;
		(*parser_utf8->set_str)( parser_utf8 , preedit , preedit_len) ;
		while( (*parser_utf8->next_char)( parser_utf8 , &ch))
		{
			if( preedit_len - parser_utf8->left > cursor_pos)
			{
				fcitx->im.preedit.cursor_offset = num_of_chars ;
				cursor_pos = preedit_len ;	/* Not to enter here twice. */
			}
			num_of_chars ++ ;
		}

		if( ( p = realloc( fcitx->im.preedit.chars ,
				sizeof(ml_char_t) * num_of_chars)) == NULL)
		{
			return ;
		}

		(*syms->ml_str_init)( fcitx->im.preedit.chars = p ,
				fcitx->im.preedit.num_of_chars = num_of_chars) ;
		fcitx->im.preedit.filled_len = 0 ;

		(*parser_utf8->init)( parser_utf8) ;
		(*parser_utf8->set_str)( parser_utf8 , preedit , preedit_len) ;

		while( (*parser_utf8->next_char)( parser_utf8 , &ch))
		{
			int  is_fullwidth = 0 ;
			int  is_comb = 0 ;

			if( (*syms->ml_convert_to_internal_ch)( &ch ,
				(*fcitx->im.listener->get_unicode_policy)(
					fcitx->im.listener->self) ,
				US_ASCII) <= 0)
			{
				continue ;
			}

			if( ch.property & MKF_FULLWIDTH)
			{
				is_fullwidth = 1 ;
			}
			else if( ch.property & MKF_AWIDTH)
			{
				/* TODO: check col_size_of_width_a */
				is_fullwidth = 1 ;
			}

			if( ch.property & MKF_COMBINING)
			{
				is_comb = 1 ;

				if( (*syms->ml_char_combine)( p - 1 ,
					mkf_char_to_int(&ch) ,
					ch.cs , is_fullwidth , is_comb ,
					ML_FG_COLOR , ML_BG_COLOR ,
					0 , 0 , 1 , 0 , 0))
				{
					continue ;
				}

				/*
				 * if combining failed , char is normally appended.
				 */
			}

			(*syms->ml_char_set)( p , mkf_char_to_int(&ch) , ch.cs ,
					      is_fullwidth , is_comb ,
					      ML_FG_COLOR , ML_BG_COLOR ,
					      0 , 0 , 1 , 0 , 0) ;

			p ++ ;
			fcitx->im.preedit.filled_len ++ ;
		}
	}

	(*fcitx->im.listener->draw_preedit_str)( fcitx->im.listener->self ,
					       fcitx->im.preedit.chars ,
					       fcitx->im.preedit.filled_len ,
					       fcitx->im.preedit.cursor_offset) ;

	if( strlen( candidateword) == 0)
	{
	#ifdef  USE_FRAMEBUFFER
		if( fcitx->im.stat_screen)
		{
			(*fcitx->im.stat_screen->delete)( fcitx->im.stat_screen) ;
			fcitx->im.stat_screen = NULL ;
		}
	#endif
	}
	else
	{
		(*fcitx->im.listener->get_spot)( fcitx->im.listener->self ,
					       fcitx->im.preedit.chars ,
					       fcitx->im.preedit.segment_offset ,
					       &x , &y) ;

		if( fcitx->im.stat_screen == NULL)
		{
			if( ! ( fcitx->im.stat_screen = (*syms->x_im_status_screen_new)(
					fcitx->im.disp , fcitx->im.font_man ,
					fcitx->im.color_man ,
					(*fcitx->im.listener->is_vertical)(
						fcitx->im.listener->self) ,
					(*fcitx->im.listener->get_line_height)(
						fcitx->im.listener->self) ,
					x , y)))
			{
			#ifdef  DEBUG
				kik_warn_printf( KIK_DEBUG_TAG
					" x_im_candidate_screen_new() failed.\n") ;
			#endif

				return ;
			}
		}
		else
		{
			(*fcitx->im.stat_screen->show)( fcitx->im.stat_screen) ;
			(*fcitx->im.stat_screen->set_spot)( fcitx->im.stat_screen , x , y) ;
		}

		(*fcitx->im.stat_screen->set)( fcitx->im.stat_screen ,
			parser_utf8 , candidateword) ;
	}
}

#else

static void
update_formatted_preedit(
	FcitxClient *  client ,
	GPtrArray *  list ,
	int  cursor_pos ,
	void *  data
	)
{
	im_fcitx_t *  fcitx ;

	fcitx = data ;

	if( list->len > 0)
	{
		FcitxPreeditItem *  item ;
		mkf_char_t  ch ;
		ml_char_t *  p ;
		u_int  num_of_chars ;
		guint  count ;

		if( fcitx->im.preedit.filled_len == 0)
		{
			/* Start preediting. */
			int  x ;
			int  y ;

			if( (*fcitx->im.listener->get_spot)(
					fcitx->im.listener->self , NULL , 0 , &x , &y))
			{
				fcitx_client_set_cursor_rect(
					fcitx->client , x , y , 10 , 10) ;
			}
		}

		fcitx->im.preedit.cursor_offset = num_of_chars = 0 ;

		for( count = 0 ; count < list->len ; count++)
		{
			size_t  str_len ;

			item = g_ptr_array_index( list , count) ;

			str_len = strlen( item->string) ;

			if( cursor_pos >= 0 && ( cursor_pos -= str_len) < 0)
			{
				fcitx->im.preedit.cursor_offset = num_of_chars ;
			}

			(*parser_utf8->init)( parser_utf8) ;
			(*parser_utf8->set_str)( parser_utf8 , item->string , str_len) ;

			while( (*parser_utf8->next_char)( parser_utf8 , &ch))
			{
				num_of_chars ++ ;
			}
		}

		if( ( p = realloc( fcitx->im.preedit.chars ,
				sizeof(ml_char_t) * num_of_chars)) == NULL)
		{
			return ;
		}

		(*syms->ml_str_init)( fcitx->im.preedit.chars = p ,
				fcitx->im.preedit.num_of_chars = num_of_chars) ;
		fcitx->im.preedit.filled_len = 0 ;

		for( count = 0 ; count < list->len ; count++)
		{
			item = g_ptr_array_index( list , count) ;

			(*parser_utf8->init)( parser_utf8) ;
			(*parser_utf8->set_str)( parser_utf8 , item->string ,
				strlen( item->string)) ;

			while( (*parser_utf8->next_char)( parser_utf8 , &ch))
			{
				int  is_fullwidth = 0 ;
				int  is_comb = 0 ;
				ml_color_t  fg_color ;
				ml_color_t  bg_color ;

				if( item->type != 0)
				{
					fg_color = ML_BG_COLOR ;
					bg_color = ML_FG_COLOR ;
				}
				else
				{
					fg_color = ML_FG_COLOR ;
					bg_color = ML_BG_COLOR ;
				}

				if( (*syms->ml_convert_to_internal_ch)( &ch ,
					(*fcitx->im.listener->get_unicode_policy)(
						fcitx->im.listener->self) ,
					US_ASCII) <= 0)
				{
					continue ;
				}

				if( ch.property & MKF_FULLWIDTH)
				{
					is_fullwidth = 1 ;
				}
				else if( ch.property & MKF_AWIDTH)
				{
					/* TODO: check col_size_of_width_a */
					is_fullwidth = 1 ;
				}

				if( ch.property & MKF_COMBINING)
				{
					is_comb = 1 ;

					if( (*syms->ml_char_combine)( p - 1 ,
						mkf_char_to_int(&ch) ,
						ch.cs , is_fullwidth , is_comb ,
						fg_color , bg_color ,
						0 , 0 , 1 , 0 , 0))
					{
						continue ;
					}

					/*
					 * if combining failed , char is normally appended.
					 */
				}

				(*syms->ml_char_set)( p , mkf_char_to_int(&ch) , ch.cs ,
						      is_fullwidth , is_comb ,
						      fg_color , bg_color ,
						      0 , 0 , 1 , 0 , 0) ;

				p ++ ;
				fcitx->im.preedit.filled_len ++ ;
			}
		}
	}
	else
	{
		if( fcitx->im.preedit.filled_len == 0)
		{
			return ;
		}

		/* Stop preediting. */
		fcitx->im.preedit.filled_len = 0 ;
	}

	(*fcitx->im.listener->draw_preedit_str)( fcitx->im.listener->self ,
					       fcitx->im.preedit.chars ,
					       fcitx->im.preedit.filled_len ,
					       fcitx->im.preedit.cursor_offset) ;
}

#endif


static void
connection_handler(void)
{
	g_main_context_iteration( g_main_context_default() , FALSE) ;
}


/* --- global functions --- */

x_im_t *
im_fcitx_new(
	u_int64_t  magic ,
	ml_char_encoding_t  term_encoding ,
	x_im_export_syms_t *  export_syms ,
	char *  engine ,
	u_int  mod_ignore_mask		/* Not used for now. */
	)
{
	im_fcitx_t *  fcitx = NULL ;

	if( magic != (u_int64_t) IM_API_COMPAT_CHECK_MAGIC)
	{
		kik_error_printf( "Incompatible input method API.\n") ;

		return  NULL ;
	}

#ifdef  DEBUG_MODKEY
	if( getenv( "MOD_KEY_DEBUG"))
	{
		mod_key_debug = 1 ;
	}
#endif

	if( ! syms)
	{
		syms = export_syms ;

		g_type_init() ;
	}

	if( ! ( fcitx = calloc( 1 , sizeof( im_fcitx_t))))
	{
	#ifdef  DEBUG
		kik_warn_printf( KIK_DEBUG_TAG " malloc failed.\n") ;
	#endif

		return  NULL ;
	}

	if( ! ( fcitx->client = fcitx_client_new()))
	{
		goto  error ;
	}

	g_signal_connect( fcitx->client , "connected" , G_CALLBACK(connected) , fcitx) ;
	g_signal_connect( fcitx->client , "disconnected" , G_CALLBACK(disconnected) , fcitx) ;
#if  0
	g_signal_connect( fcitx->client , "enable-im" , G_CALLBACK(enable_im) , fcitx) ;
#endif
	g_signal_connect( fcitx->client , "close-im" , G_CALLBACK(close_im) , fcitx) ;
	g_signal_connect( fcitx->client , "forward-key" , G_CALLBACK(forward_key) , fcitx) ;
	g_signal_connect( fcitx->client , "commit-string" , G_CALLBACK(commit_string) , fcitx) ;
#ifdef  USE_FRAMEBUFFER
	g_signal_connect( fcitx->client , "update-client-side-ui" ,
		G_CALLBACK(update_client_side_ui) , fcitx) ;
#else
	g_signal_connect( fcitx->client , "update-formatted-preedit" ,
		G_CALLBACK(update_formatted_preedit) , fcitx) ;
#endif

	fcitx->term_encoding = term_encoding ;
	fcitx->is_enabled = FALSE ;

	if( term_encoding != ML_UTF8)
	{
		if( ! ( fcitx->conv = (*syms->ml_conv_new)( term_encoding)))
		{
			goto  error ;
		}
	}

#ifdef  USE_FRAMEBUFFER
	if( ! ( fcitx->parser_term = (*syms->ml_parser_new)( term_encoding)))
	{
		goto  error ;
	}
#endif

	/*
	 * set methods of x_im_t
	 */
	fcitx->im.delete = delete ;
	fcitx->im.key_event = key_event ;
	fcitx->im.switch_mode = switch_mode ;
	fcitx->im.is_active = is_active ;
	fcitx->im.focused = focused ;
	fcitx->im.unfocused = unfocused ;

	if( ref_count++ == 0)
	{
		(*syms->x_event_source_add_fd)( FCITX_ID , connection_handler) ;

		if( ! ( parser_utf8 = (*syms->ml_parser_new)( ML_UTF8)))
		{
			goto  error ;
		}
	}

#ifdef  IM_FCITX_DEBUG
	kik_debug_printf("New object was created. ref_count is %d.\n", ref_count) ;
#endif

	return  (x_im_t*) fcitx ;

error:
	if( fcitx)
	{
		if( fcitx->conv)
		{
			(*fcitx->conv->delete)( fcitx->conv) ;
		}

	#ifdef  USE_FRAMEBUFFER
		if( fcitx->parser_term)
		{
			(*fcitx->parser_term->delete)( fcitx->parser_term) ;
		}
	#endif

		if( fcitx->client)
		{
			g_object_unref( fcitx->client) ;
		}

		free( fcitx) ;
	}

	return  NULL ;
}


/* --- module entry point for external tools --- */

im_info_t *
im_fcitx_get_info(
	char *  locale ,
	char *  encoding
	)
{
	im_info_t *  result ;

	if( ! ( result = malloc( sizeof( im_info_t))))
	{
		return  NULL ;
	}

	result->id = strdup( "fcitx") ;
	result->name = strdup( "fcitx") ;
	result->num_of_args = 0;
	result->args = NULL ;
	result->readable_args = NULL ;

	return  result;
}
