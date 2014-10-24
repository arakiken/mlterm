/*
 *	$Id$
 */

#include  <stdio.h>
#include  <ibus.h>

#include  <kiklib/kik_list.h>
#include  <kiklib/kik_debug.h>
#include  <kiklib/kik_mem.h>

#include  <x_im.h>

#include  "../im_common.h"
#include  "../im_info.h"

#if  0
#define  IM_IBUS_DEBUG  1
#endif

#define  IBUS_ID  -2

#if  IBUS_CHECK_VERSION(1,5,0)
#define  ibus_input_context_is_enabled(context)  (TRUE)
#define  ibus_input_context_disable(context)  (0)
#define  ibus_input_context_enable(context)  (0)
#endif

typedef struct im_ibus
{
	/* input method common object */
	x_im_t  im ;

	IBusInputContext *  context ;

	ml_char_encoding_t  term_encoding ;

#ifdef  USE_FRAMEBUFFER
	mkf_parser_t *  parser_term ;	/* for term encoding */
#endif
	mkf_conv_t *  conv ;		/* for term encoding */

	/*
	 * Cache a result of ibus_input_context_is_enabled() which uses
	 * DBus connection internally.
	 */
	gboolean  is_enabled ;

	XKeyEvent  prev_key ;

#ifdef  USE_FRAMEBUFFER
	gchar *  prev_first_cand ;
	u_int  prev_num_of_cands ;
#endif

}  im_ibus_t ;

KIK_LIST_TYPEDEF( im_ibus_t) ;


/* --- static variables --- */

static int  is_init ;
static IBusBus *  ibus_bus ;
static int  ibus_bus_fd = -1 ;
static KIK_LIST( im_ibus_t)  ibus_list = NULL ;
static int  ref_count = 0 ;
static mkf_parser_t *  parser_utf8 = NULL ;
static x_im_export_syms_t *  syms = NULL ; /* mlterm internal symbols */
#ifdef  DEBUG_MODKEY
static int  mod_key_debug = 0 ;
#endif


/* --- static functions --- */

#if  0
static ml_color_t
get_near_color(
	u_int  rgb
	)
{
	u_int  rgb_bit = 0 ;

	if( ( rgb & 0xff0000) > 0x7f0000)
	{
		rgb_bit |= 0x4 ;
	}
	if( ( rgb & 0xff00) > 0x7f00)
	{
		rgb_bit |= 0x2 ;
	}
	if( ( rgb & 0xff) > 0x7f)
	{
		rgb_bit |= 0x1 ;
	}

	switch( rgb_bit)
	{
	case  0:
		return  ML_BLACK ;
	case  1:
		return  ML_BLUE ;
	case  2:
		return  ML_GREEN ;
	case  3:
		return  ML_CYAN ;
	case  4:
		return  ML_RED ;
	case  5:
		return  ML_MAGENTA ;
	case  6:
		return  ML_YELLOW ;
	case  7:
		return  ML_WHITE ;
	default:
		return  ML_BLACK ;
	}
}
#endif

static void
update_preedit_text(
	IBusInputContext *  context ,
	IBusText *  text ,
	gint  cursor_pos ,
	gboolean  visible ,
	gpointer  data
	)
{
	im_ibus_t *  ibus ;
	ml_char_t *  p ;
	u_int  len ;
	mkf_char_t  ch ;

	ibus = (im_ibus_t*) data ;

	if( ( len = ibus_text_get_length( text)) > 0)
	{
		u_int  index ;

		if( ibus->im.preedit.filled_len == 0)
		{
			/* Start preediting. */
			int  x ;
			int  y ;

			if( (*ibus->im.listener->get_spot)(
					ibus->im.listener->self , NULL , 0 , &x , &y))
			{
				u_int  line_height ;

				line_height = (*ibus->im.listener->get_line_height)(
						ibus->im.listener->self) ;
				ibus_input_context_set_cursor_location(
					ibus->context ,
					x , y - line_height , 0 , line_height) ;
			}
		}
		
		if( ( p = realloc( ibus->im.preedit.chars , sizeof(ml_char_t) * len)) == NULL)
		{
			return ;
		}

		(*syms->ml_str_init)( ibus->im.preedit.chars = p ,
				ibus->im.preedit.num_of_chars = len) ;
		ibus->im.preedit.filled_len = 0 ;

		(*parser_utf8->init)( parser_utf8) ;
		(*parser_utf8->set_str)( parser_utf8 ,
				text->text , strlen( text->text)) ;

		index = 0 ;
		while( (*parser_utf8->next_char)( parser_utf8 , &ch))
		{
			u_int  count ;
			IBusAttribute *  attr ;
			int  is_fullwidth = 0 ;
			int  is_comb = 0 ;
			int  is_underlined = 0 ;
			ml_color_t  fg_color = ML_FG_COLOR ;
			ml_color_t  bg_color = ML_BG_COLOR ;

			for( count = 0 ; ( attr = ibus_attr_list_get( text->attrs , count)) ;
				count++)
			{
				if( attr->start_index <= index && index < attr->end_index)
				{
					if( attr->type == IBUS_ATTR_TYPE_UNDERLINE)
					{
						is_underlined =
							(attr->value != IBUS_ATTR_UNDERLINE_NONE) ;

					}
				#if  0
					else if( attr->type == IBUS_ATTR_TYPE_FOREGROUND)
					{
						fg_color = get_near_color( attr->value) ;
					}
					else if( attr->type == IBUS_ATTR_TYPE_BACKGROUND)
					{
						bg_color = get_near_color( attr->value) ;
					}
				#else
					else if( attr->type == IBUS_ATTR_TYPE_BACKGROUND)
					{
						fg_color = ML_BG_COLOR ;
						bg_color = ML_FG_COLOR ;
					}
				#endif
				}
			}

			if( (*syms->ml_convert_to_internal_ch)( &ch ,
				(*ibus->im.listener->get_unicode_policy)(ibus->im.listener->self) ,
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

				if( (*syms->ml_char_combine)( p - 1 , mkf_char_to_int(&ch) ,
					ch.cs , is_fullwidth , is_comb , fg_color , bg_color ,
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
			ibus->im.preedit.filled_len ++ ;

			index ++ ;
		}
	}
	else
	{
	#ifdef  USE_FRAMEBUFFER
		if( ibus->im.cand_screen)
		{
			(*ibus->im.cand_screen->delete)( ibus->im.cand_screen) ;
			ibus->im.cand_screen = NULL ;
		}
	#endif

		if( ibus->im.preedit.filled_len == 0)
		{
			return ;
		}

		/* Stop preediting. */
		ibus->im.preedit.filled_len = 0 ;
	}

	ibus->im.preedit.cursor_offset = cursor_pos ;

	(*ibus->im.listener->draw_preedit_str)( ibus->im.listener->self ,
					       ibus->im.preedit.chars ,
					       ibus->im.preedit.filled_len ,
					       ibus->im.preedit.cursor_offset) ;
}

static void
hide_preedit_text(
	IBusInputContext *  context ,
	gpointer  data
	)
{
	im_ibus_t *  ibus ;

	ibus = (im_ibus_t*) data ;

	if( ibus->im.preedit.filled_len == 0)
	{
		return ;
	}

#ifdef  USE_FRAMEBUFFER
	if( ibus->im.cand_screen)
	{
		(*ibus->im.cand_screen->hide)( ibus->im.cand_screen) ;
	}
#endif

	/* Stop preediting. */
	ibus->im.preedit.filled_len = 0 ;
	ibus->im.preedit.cursor_offset = 0 ;

	(*ibus->im.listener->draw_preedit_str)( ibus->im.listener->self ,
					       ibus->im.preedit.chars ,
					       ibus->im.preedit.filled_len ,
					       ibus->im.preedit.cursor_offset) ;
}

static void
commit_text(
	IBusInputContext *  context ,
	IBusText *  text ,
	gpointer  data
	)
{
	im_ibus_t *  ibus ;

	ibus = (im_ibus_t*) data ;

	if( ibus->im.preedit.filled_len > 0)
	{
		/* Reset preedit */
		ibus->im.preedit.filled_len = 0 ;
		ibus->im.preedit.cursor_offset = 0 ;
		(*ibus->im.listener->draw_preedit_str)( ibus->im.listener->self ,
						       ibus->im.preedit.chars ,
						       ibus->im.preedit.filled_len ,
						       ibus->im.preedit.cursor_offset) ;
	}

	if( ibus_text_get_length( text) == 0)
	{
		/* do nothing */
	}
	else if( ibus->term_encoding == ML_UTF8)
	{
		(*ibus->im.listener->write_to_term)(
						ibus->im.listener->self ,
						text->text , strlen( text->text)) ;
	}
	else
	{
		u_char  conv_buf[256] ;
		size_t  filled_len ;

		(*parser_utf8->init)( parser_utf8) ;
		(*parser_utf8->set_str)( parser_utf8 , text->text , strlen( text->text)) ;

		(*ibus->conv->init)( ibus->conv) ;

		while( ! parser_utf8->is_eos)
		{
			filled_len = (*ibus->conv->convert)( ibus->conv , conv_buf ,
						sizeof( conv_buf) , parser_utf8) ;

			if( filled_len == 0)
			{
				/* finished converting */
				break ;
			}

			(*ibus->im.listener->write_to_term)( ibus->im.listener->self ,
							conv_buf , filled_len) ;
		}
	}

#ifdef  USE_FRAMEBUFFER
	if( ibus->im.cand_screen)
	{
		(*ibus->im.cand_screen->delete)( ibus->im.cand_screen) ;
		ibus->im.cand_screen = NULL ;
	}
#endif
}

static void
forward_key_event(
	IBusInputContext *  context ,
	guint  keyval ,
	guint  keycode ,
	guint  state ,
	gpointer  data
	)
{
	im_ibus_t *  ibus ;

	ibus = (im_ibus_t*) data ;

	if( ibus->prev_key.keycode ==
		#ifdef  USE_FRAMEBUFFER
			keycode
		#else
			keycode + 8
		#endif
			)
	{
		ibus->prev_key.state |= IBUS_IGNORED_MASK ;
	#ifndef  USE_FRAMEBUFFER
		XPutBackEvent( ibus->prev_key.display , &ibus->prev_key) ;
	#endif
		memset( &ibus->prev_key , 0 , sizeof(XKeyEvent)) ;
	}
}

#ifdef  USE_FRAMEBUFFER

static void
show_lookup_table(
	IBusInputContext *  context ,
	gpointer  data
	)
{
	im_ibus_t *  ibus ;

	ibus = (im_ibus_t*) data ;

	if( ibus->im.cand_screen)
	{
		(*ibus->im.cand_screen->show)( ibus->im.cand_screen) ;
	}
}

static void
hide_lookup_table(
	IBusInputContext *  context ,
	gpointer  data
	)
{
	im_ibus_t *  ibus ;

	ibus = (im_ibus_t*) data ;

	if( ibus->im.cand_screen)
	{
		(*ibus->im.cand_screen->hide)( ibus->im.cand_screen) ;
	}
}

static void
update_lookup_table(
	IBusInputContext *  context ,
	IBusLookupTable *  table ,
	gboolean  visible ,
	gpointer  data
	)
{
	im_ibus_t *  ibus ;
	u_int  num_of_cands ;
	int  cur_pos ;
	u_int  i ;
	int  x ;
	int  y ;

	ibus = (im_ibus_t*) data ;

	if( ( num_of_cands = ibus_lookup_table_get_number_of_candidates( table)) == 0)
	{
		return ;
	}

	cur_pos = ibus_lookup_table_get_cursor_pos( table) ;

	(*ibus->im.listener->get_spot)( ibus->im.listener->self ,
				       ibus->im.preedit.chars ,
				       ibus->im.preedit.segment_offset ,
				       &x , &y) ;

	if( ibus->im.cand_screen == NULL)
	{
		if( cur_pos == 0)
		{
			return ;
		}

		if( ! ( ibus->im.cand_screen = (*syms->x_im_candidate_screen_new)(
				ibus->im.disp , ibus->im.font_man , ibus->im.color_man ,
				(*ibus->im.listener->is_vertical)(ibus->im.listener->self) ,
				1 ,
				(*ibus->im.listener->get_unicode_policy)(ibus->im.listener->self) ,
				(*ibus->im.listener->get_line_height)(ibus->im.listener->self) ,
				x , y)))
		{
		#ifdef  DEBUG
			kik_warn_printf( KIK_DEBUG_TAG " x_im_candidate_screen_new() failed.\n") ;
		#endif

			return ;
		}
	}
	else
	{
		(*ibus->im.cand_screen->show)( ibus->im.cand_screen) ;
	}

	if( ! (*ibus->im.cand_screen->init)( ibus->im.cand_screen , num_of_cands , 10))
	{
		(*ibus->im.cand_screen->delete)( ibus->im.cand_screen) ;
		ibus->im.cand_screen = NULL ;

		return ;
	}

	(*ibus->im.cand_screen->set_spot)( ibus->im.cand_screen , x , y) ;

	for( i = 0 ; i < num_of_cands ; i++)
	{
		u_char *  str ;

		/* ibus 1.4.1 on Ubuntu 12.10 can return NULL if num_of_cands > 0. */
		if( ! ( str = ibus_text_get_text( ibus_lookup_table_get_candidate( table , i))))
		{
			continue ;
		}

		if( ibus->term_encoding != ML_UTF8)
		{
			u_char *  p ;

			(*parser_utf8->init)( parser_utf8) ;
			(*ibus->conv->init)( ibus->conv) ;

			if( im_convert_encoding( parser_utf8 , ibus->conv ,
						 str , &p ,
						 strlen( str) + 1))
			{
				(*ibus->im.cand_screen->set)(
							ibus->im.cand_screen ,
							ibus->parser_term ,
							p , i) ;
				free( p) ;
			}
		}
		else
		{
			(*ibus->im.cand_screen->set)( ibus->im.cand_screen ,
						     ibus->parser_term ,
						     str , i) ;
		}
	}

	(*ibus->im.cand_screen->select)( ibus->im.cand_screen , cur_pos) ;
}

#endif	/* USE_FRAMEBUFFER */


static void
connection_handler(void)
{
#ifdef  DBUS_H
	DBusConnection *  connection ;

	connection = ibus_connection_get_connection( ibus_bus_get_connection( ibus_bus)) ;

	dbus_connection_read_write( connection , 0) ;

	while( dbus_connection_dispatch( connection) == DBUS_DISPATCH_DATA_REMAINS) ;
#else
#if  0
	g_dbus_connection_flush_sync( ibus_bus_get_connection( ibus_bus) , NULL , NULL) ;
#endif
	g_main_context_iteration( g_main_context_default() , FALSE) ;
#endif
}

static int
add_event_source(void)
{
#ifdef  DBUS_H
	if( ! dbus_connection_get_unix_fd( ibus_connection_get_connection(
			ibus_bus_get_connection( ibus_bus)) , &ibus_bus_fd))
	{
		return  0 ;
	}
#else
	/*
	 * GIOStream returned by g_dbus_connection_get_stream() is forcibly
	 * regarded as GSocketConnection.
	 */
	if( ( ibus_bus_fd = g_socket_get_fd( g_socket_connection_get_socket(
				g_dbus_connection_get_stream(
					ibus_bus_get_connection( ibus_bus))))) == -1)
	{
		return  0 ;
	}
#endif
	(*syms->x_event_source_add_fd)( ibus_bus_fd , connection_handler) ;
	(*syms->x_event_source_add_fd)( IBUS_ID , connection_handler) ;

	return  1 ;
}

static void
remove_event_source(
	int  complete
	)
{
	if( ibus_bus_fd >= 0)
	{
		(*syms->x_event_source_remove_fd)( ibus_bus_fd) ;
		ibus_bus_fd = -1 ;
	}

	if( complete)
	{
		(*syms->x_event_source_remove_fd)( IBUS_ID) ;
	}
}


/*
 * methods of x_im_t
 */

static int
delete(
	x_im_t *  im
	)
{
	im_ibus_t *  ibus ;

	ibus = (im_ibus_t*) im ;

	if( ibus->context)
	{
	#ifdef  DBUS_H
		ibus_object_destroy( (IBusObject*)ibus->context) ;
	#else
		ibus_proxy_destroy( (IBusProxy*)ibus->context) ;
	#endif
	}

	kik_list_search_and_remove( im_ibus_t , ibus_list , ibus) ;

	if( ibus->conv)
	{
		(*ibus->conv->delete)( ibus->conv) ;
	}

#ifdef  USE_FRAMEBUFFER
	if( ibus->parser_term)
	{
		(*ibus->parser_term->delete)( ibus->parser_term) ;
	}

	free( ibus->prev_first_cand) ;
#endif

	free( ibus) ;

	if( -- ref_count == 0)
	{
		remove_event_source(1) ;

		ibus_object_destroy( (IBusObject*)ibus_bus) ;
		ibus_bus = NULL ;

		if( ! kik_list_is_empty( ibus_list))
		{
		#ifdef  DEBUG
			kik_warn_printf( KIK_DEBUG_TAG " ibus list is not empty.\n") ;
		#endif
		}

		kik_list_delete( im_ibus_t , ibus_list) ;
		ibus_list = NULL ;

		if( parser_utf8)
		{
			(*parser_utf8->delete)( parser_utf8) ;
			parser_utf8 = NULL ;
		}
	}

#ifdef  IM_IBUS_DEBUG
	kik_debug_printf( KIK_DEBUG_TAG " An object was deleted. ref_count: %d\n", ref_count) ;
#endif

	return  ref_count ;
}

#ifdef  USE_FRAMEBUFFER
static KeySym
native_to_ibus_ksym(
	KeySym  ksym
	)
{
	switch( ksym)
	{
	case  XK_BackSpace:
		return  IBUS_BackSpace ;

	case  XK_Tab:
		return  IBUS_Tab ;

	case  XK_Return:
		return  IBUS_Return ;

	case  XK_Escape:
		return  IBUS_Escape ;

	case  XK_Zenkaku_Hankaku:
		return  IBUS_Zenkaku_Hankaku ;

	case  XK_Hiragana_Katakana:
		return  IBUS_Hiragana_Katakana ;

	case  XK_Muhenkan:
		return  IBUS_Muhenkan ;

	case  XK_Henkan_Mode:
		return  IBUS_Henkan_Mode ;

	case  XK_Home:
		return  IBUS_Home ;

	case  XK_Left:
		return  IBUS_Left ;

	case  XK_Up:
		return  IBUS_Up ;

	case  XK_Right:
		return  IBUS_Right ;

	case  XK_Down:
		return  IBUS_Down ;

	case  XK_Prior:
		return  IBUS_Prior ;

	case  XK_Next:
		return  IBUS_Next ;

	case  XK_Insert:
		return  IBUS_Insert ;

	case  XK_End:
		return  IBUS_End ;

	case  XK_Num_Lock:
		return  IBUS_Num_Lock ;

	case  XK_Shift_L:
		return  IBUS_Shift_L ;

	case  XK_Shift_R:
		return  IBUS_Shift_R ;

	case  XK_Control_L:
		return  IBUS_Control_L ;

	case  XK_Control_R:
		return  IBUS_Control_R ;

	case  XK_Caps_Lock:
		return  IBUS_Caps_Lock ;

	case  XK_Meta_L:
		return  IBUS_Meta_L ;

	case  XK_Meta_R:
		return  IBUS_Meta_R ;

	case  XK_Alt_L:
		return  IBUS_Alt_L ;

	case  XK_Alt_R:
		return  IBUS_Alt_R ;

	case  XK_Delete:
		return  IBUS_Delete ;

	default:
		return  ksym ;
	}
}
#else
#define  native_to_ibus_ksym( ksym)  (ksym)
#endif

static int
key_event(
	x_im_t *  im ,
	u_char  key_char ,
	KeySym  ksym ,
	XKeyEvent *  event
	)
{
	im_ibus_t *  ibus ;

	ibus = (im_ibus_t*) im ;

	if( ! ibus->context)
	{
		return  1 ;
	}

	if( event->state & IBUS_IGNORED_MASK)
	{
		/* Is put back in forward_key_event */
		event->state &= ~IBUS_IGNORED_MASK ;
	}
	else if( ibus_input_context_process_key_event( ibus->context , native_to_ibus_ksym( ksym) ,
		#ifdef  USE_FRAMEBUFFER
			event->keycode ,
			event->state
		#else
			event->keycode - 8 ,
			event->state | (event->type == KeyRelease ? IBUS_RELEASE_MASK : 0)
		#endif
			))
	{
		gboolean  is_enabled_old ;

		is_enabled_old = ibus->is_enabled ;
		ibus->is_enabled = ibus_input_context_is_enabled( ibus->context) ;

	#if  ! IBUS_CHECK_VERSION(1,5,0)
		if( ibus->is_enabled != is_enabled_old)
		{
			return  0 ;
		}
		else
	#endif
		if( ibus->is_enabled)
		{
		#ifndef  DBUS_H
		#if  0
			g_dbus_connection_flush_sync( ibus_bus_get_connection( ibus_bus) ,
				NULL , NULL) ;
		#endif
			g_main_context_iteration( g_main_context_default() , FALSE) ;
		#endif

			memcpy( &ibus->prev_key , event , sizeof(XKeyEvent)) ;

			return  0 ;
		}
	}
	else if( ibus->im.preedit.filled_len > 0)
	{
		/* Pressing "q" in preediting. */
	#ifndef  DBUS_H
	#if  0
		g_dbus_connection_flush_sync( ibus_bus_get_connection( ibus_bus) , NULL , NULL) ;
	#endif
		g_main_context_iteration( g_main_context_default() , FALSE) ;
	#endif
	}

	return  1 ;
}

static void
set_engine(
	IBusInputContext *  context ,
	gchar *  name
	)
{
	kik_msg_printf( "iBus engine is %s\n" , name) ;
	ibus_input_context_set_engine( context , name) ;
}

#if  IBUS_CHECK_VERSION(1,5,0)
static void
next_engine(
	im_ibus_t *  ibus
	)
{
	IBusConfig *  config ;
	GVariant *  var ;

	if( ! ibus->context)
	{
		return ;
	}

	config = ibus_bus_get_config( ibus_bus) ;
	if( ( var = ibus_config_get_value( config , "general" , "preload-engines")))
	{
		gchar *  cur_name ;
		GVariantIter *  iter ;
		gchar *  name ;

		cur_name = ibus_engine_desc_get_name(
				ibus_input_context_get_engine( ibus->context)) ;

		g_variant_get( var , "as" , &iter) ;

		if( g_variant_iter_loop( iter , "s" , &name))
		{
			gchar *  first_name ;
			int  loop ;

			first_name = g_strdup( name) ;
			loop = 1 ;

			do
			{
				if( strcmp( cur_name , name) == 0)
				{
					loop = 0 ;
				}

				if( ! g_variant_iter_loop( iter , "s" , &name))
				{
					loop = 0 ;
					name = first_name ;
				}
			}
			while( loop) ;

			set_engine( ibus->context , name) ;

			free( first_name) ;
		}

		g_variant_iter_free( iter) ;
		g_variant_unref( var) ;
	}
}
#endif

static int
switch_mode(
	x_im_t *  im
	)
{
	im_ibus_t *  ibus ;

	ibus =  (im_ibus_t*)  im ;

	if( ! ibus->context)
	{
		return  0 ;
	}

#if  IBUS_CHECK_VERSION(1,5,0)
	next_engine( ibus) ;
#else
	if( ibus->is_enabled)
	{
		ibus_input_context_disable( ibus->context) ;
		ibus->is_enabled = FALSE ;
	}
	else
	{
		ibus_input_context_enable( ibus->context) ;
		ibus->is_enabled = TRUE ;
	}
#endif

	return  1 ;
}

static int
is_active(
	x_im_t *  im
	)
{
	return  ((im_ibus_t*)im)->is_enabled ;
}

static void
focused(
	x_im_t *  im
	)
{
	im_ibus_t *  ibus ;

	ibus =  (im_ibus_t*)  im ;

	if( ! ibus->context)
	{
		return ;
	}

	ibus_input_context_focus_in( ibus->context) ;

	if( ibus->im.cand_screen)
	{
		(*ibus->im.cand_screen->show)( ibus->im.cand_screen) ;
	}
}

static void
unfocused(
	x_im_t *  im
	)
{
	im_ibus_t *  ibus ;

	ibus = (im_ibus_t*)  im ;

	if( ! ibus->context)
	{
		return ;
	}

	ibus_input_context_focus_out( ibus->context) ;

	if( ibus->im.cand_screen)
	{
		(*ibus->im.cand_screen->hide)( ibus->im.cand_screen) ;
	}
}


static IBusInputContext *
context_new(
	im_ibus_t *  ibus ,
	char *  engine
	)
{
	IBusInputContext *  context ;

	context = ibus_bus_create_input_context( ibus_bus , "mlterm") ;
	ibus_input_context_set_capabilities( context ,
	#ifdef  USE_FRAMEBUFFER
		IBUS_CAP_PREEDIT_TEXT | IBUS_CAP_LOOKUP_TABLE
	#else
		IBUS_CAP_PREEDIT_TEXT | IBUS_CAP_FOCUS | IBUS_CAP_SURROUNDING_TEXT
	#endif
		) ;

	g_signal_connect( context , "update-preedit-text" ,
			G_CALLBACK( update_preedit_text) , ibus) ;
	g_signal_connect( context , "hide-preedit-text" ,
			G_CALLBACK( hide_preedit_text) , ibus) ;
	g_signal_connect( context , "commit-text" , G_CALLBACK( commit_text) , ibus) ;
	g_signal_connect( context , "forward-key-event" ,
			G_CALLBACK( forward_key_event) , ibus) ;
#ifdef  USE_FRAMEBUFFER
	g_signal_connect( context , "update-lookup-table" ,
			G_CALLBACK( update_lookup_table) , ibus) ;
	g_signal_connect( context , "show-lookup-table" ,
			G_CALLBACK( show_lookup_table) , ibus) ;
	g_signal_connect( context , "hide-lookup-table" ,
			G_CALLBACK( hide_lookup_table) , ibus) ;
#endif

	if( engine)
	{
		set_engine( context , engine) ;
	}
#if  defined(USE_FRAMEBUFFER) && IBUS_CHECK_VERSION(1,5,0)
	else
	{
		next_engine( ibus) ;
	}
#endif

	return  context ;
}

static void
connected(
	IBusBus *  bus ,
	gpointer  data
	)
{
	KIK_ITERATOR( im_ibus_t)  iterator ;

	if( bus != ibus_bus || ! ibus_bus_is_connected( ibus_bus) || ! add_event_source())
	{
		return ;
	}

	for( iterator = ibus_list->first ; iterator ; iterator = iterator->next)
	{
		iterator->object->context = context_new( iterator->object , NULL) ;
	}
}

static void
disconnected(
	IBusBus *  bus ,
	gpointer  data
	)
{
	KIK_ITERATOR( im_ibus_t)  iterator ;

	if( bus != ibus_bus)
	{
		return ;
	}

	remove_event_source(0) ;

	for( iterator = ibus_list->first ; iterator ; iterator = iterator->next)
	{
	#ifdef  DBUS_H
		ibus_object_destroy( (IBusObject*)iterator->object->context) ;
	#else
		ibus_proxy_destroy( (IBusProxy*)iterator->object->context) ;
	#endif
		iterator->object->context = NULL ;
		iterator->object->is_enabled = 0 ;
	}
}


/* --- global functions --- */

x_im_t *
im_ibus_new(
	u_int64_t  magic ,
	ml_char_encoding_t  term_encoding ,
	x_im_export_syms_t *  export_syms ,
	char *  engine ,
	u_int  mod_ignore_mask		/* Not used for now. */
	)
{
	im_ibus_t *  ibus = NULL ;

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

	if( ! is_init)
	{
		ibus_init() ;

		/* Don't call ibus_init() again if ibus daemon is not found below. */
		is_init = 1 ;
	}

	if( ! ibus_bus)
	{
		syms = export_syms ;

		/* g_getenv( "DISPLAY") will be called in ibus_get_socket_path(). */
	#if  0
		ibus_set_display( g_getenv( "DISPLAY")) ;
	#endif

		ibus_bus = ibus_bus_new() ;

		if( ! ibus_bus_is_connected( ibus_bus))
		{
			kik_error_printf( "IBus daemon is not found.\n") ;

			goto  error ;
		}

		if( ! add_event_source())
		{
			goto  error ;
		}

		kik_list_new( im_ibus_t , ibus_list) ;

		if( ! ( parser_utf8 = (*syms->ml_parser_new)( ML_UTF8)))
		{
			goto  error ;
		}

		g_signal_connect( ibus_bus , "connected" ,
				G_CALLBACK( connected) , NULL) ;
		g_signal_connect( ibus_bus , "disconnected" ,
				G_CALLBACK( disconnected) , NULL) ;
	}

	if( ! ( ibus = calloc( 1 , sizeof( im_ibus_t))))
	{
	#ifdef  DEBUG
		kik_warn_printf( KIK_DEBUG_TAG " malloc failed.\n") ;
	#endif

		goto  error ;
	}

	ibus->context = context_new( ibus , engine) ;

	ibus->term_encoding = term_encoding ;
	ibus->is_enabled = FALSE ;

	if( term_encoding != ML_UTF8)
	{
		if( ! ( ibus->conv = (*syms->ml_conv_new)( term_encoding)))
		{
			goto  error ;
		}
	}

#ifdef  USE_FRAMEBUFFER
	if( ! ( ibus->parser_term = (*syms->ml_parser_new)( term_encoding)))
	{
		goto  error ;
	}
#endif

	/*
	 * set methods of x_im_t
	 */
	ibus->im.delete = delete ;
	ibus->im.key_event = key_event ;
	ibus->im.switch_mode = switch_mode ;
	ibus->im.is_active = is_active ;
	ibus->im.focused = focused ;
	ibus->im.unfocused = unfocused ;

	kik_list_insert_head( im_ibus_t , ibus_list , ibus) ;

	ref_count ++;

#ifdef  IM_IBUS_DEBUG
	kik_debug_printf("New object was created. ref_count is %d.\n", ref_count) ;
#endif

	return  (x_im_t*) ibus ;

error:
	if( ref_count == 0)
	{
		ibus_object_destroy( (IBusObject*)ibus_bus) ;
		ibus_bus = NULL ;

		if( parser_utf8)
		{
			(*parser_utf8->delete)( parser_utf8) ;
			parser_utf8 = NULL ;
		}
	}

	if( ibus)
	{
		if( ibus->conv)
		{
			(*ibus->conv->delete)( ibus->conv) ;
		}

	#ifdef  USE_FRAMEBUFFER
		if( ibus->parser_term)
		{
			(*ibus->parser_term->delete)( ibus->parser_term) ;
		}
	#endif

		free( ibus) ;
	}

	return  NULL ;
}


/* --- module entry point for external tools --- */

im_info_t *
im_ibus_get_info(
	char *  locale ,
	char *  encoding
	)
{
	im_info_t *  result ;

	if( ! ( result = malloc( sizeof( im_info_t))))
	{
		return  NULL ;
	}

	result->id = strdup( "ibus") ;
	result->name = strdup( "iBus") ;
	result->num_of_args = 0;
	result->args = NULL ;
	result->readable_args = NULL ;

	return  result;
}
