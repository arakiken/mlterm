/*
 *	$Id$
 */

#include  <stdio.h>
#include  <ibus.h>
#include  <x_im.h>
#include  <kiklib/kik_list.h>
#include  <kiklib/kik_debug.h>
#include  "../im_info.h"

#if  0
#define  IM_IBUS_DEBUG  1
#endif

#define  IBUS_ID  -2


typedef struct im_ibus
{
	/* input method common object */
	x_im_t  im ;

	IBusInputContext *  context ;

	ml_char_encoding_t  term_encoding ;

	mkf_parser_t *  parser_ibus ;	/* for ibus encoding  */

	/* Not used for now */
	u_int  pressing_mod_key ;
	u_int  mod_ignore_mask ;

}  im_ibus_t ;

KIK_LIST_TYPEDEF( im_ibus_t) ;


/* --- static variables --- */

static IBusBus *  ibus_bus ;
static KIK_LIST( im_ibus_t)  ibus_list = NULL ;

static int  ref_count = 0 ;
static x_im_export_syms_t *  syms = NULL ; /* mlterm internal symbols */
static int  mod_key_debug = 0 ;


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
				ibus_input_context_set_cursor_location(
					ibus->context , x , y , 10 , 10) ;
			}
		}
		
		if( ( p = realloc( ibus->im.preedit.chars , sizeof(ml_char_t) * len)) == NULL)
		{
			return ;
		}

		(*syms->ml_str_init)( ibus->im.preedit.chars = p ,
				ibus->im.preedit.num_of_chars = len) ;
		ibus->im.preedit.filled_len = 0 ;

		(*ibus->parser_ibus->init)( ibus->parser_ibus) ;
		(*ibus->parser_ibus->set_str)( ibus->parser_ibus ,
				text->text , strlen( text->text)) ;

		index = 0 ;
		while( (*ibus->parser_ibus->next_char)( ibus->parser_ibus , &ch))
		{
			u_int  count ;
			IBusAttribute *  attr ;
			int  is_biwidth = 0 ;
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

			if( ! is_comb ||
			    ! (*syms->ml_char_combine)( p - 1 , ch.ch , ch.size , ch.cs ,
					is_biwidth , is_comb , fg_color , bg_color , 0 , 1))
			{
				/*
				 * If char is not a combination or failed to be combined ,
				 * it is normally appended.
				 */
				if( (*syms->ml_is_msb_set)( ch.cs))
				{
					SET_MSB( ch.ch[0]) ;
				}

				(*syms->ml_char_set)( p , ch.ch , ch.size , ch.cs ,
						      is_biwidth , is_comb ,
						      fg_color , bg_color ,
						      0 , 1) ;

				p ++ ;
				ibus->im.preedit.filled_len ++ ;
			}

			index ++ ;
		}
	}
	else
	{
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
commit_text(
	IBusInputContext *  context ,
	IBusText *  text ,
	gpointer  data
	)
{
	im_ibus_t *  ibus ;
	mkf_conv_t *  conv ;
	u_char  conv_buf[256] ;
	size_t  filled_len ;

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
		return ;
	}

	if( ibus->term_encoding == ML_UTF8)
	{
		(*ibus->im.listener->write_to_term)(
						ibus->im.listener->self ,
						text->text , strlen( text->text)) ;
		return ;
	}

	if( ! ( conv = (*syms->ml_conv_new)( ibus->term_encoding)))
	{
		return ;
	}

	(*ibus->parser_ibus->init)( ibus->parser_ibus) ;
	(*ibus->parser_ibus->set_str)( ibus->parser_ibus , text->text , strlen( text->text)) ;

	(*conv->init)( conv) ;

	while( ! ibus->parser_ibus->is_eos)
	{
		filled_len = (*conv->convert)( conv , conv_buf , sizeof( conv_buf) ,
					ibus->parser_ibus) ;

		if( filled_len == 0)
		{
			/* finished converting */
			break ;
		}

		(*ibus->im.listener->write_to_term)( ibus->im.listener->self ,
						conv_buf , filled_len) ;
	}

	(*conv->delete)( conv) ;
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

	if( ibus->parser_ibus)
	{
		(*ibus->parser_ibus->delete)( ibus->parser_ibus) ;
	}

	ibus_object_destroy( (IBusObject*)ibus->context) ;

	ref_count -- ;

#ifdef  IM_IBUS_DEBUG
	kik_debug_printf( KIK_DEBUG_TAG " An object was deleted. ref_count: %d\n", ref_count) ;
#endif

	kik_list_search_and_remove( im_ibus_t , ibus_list , ibus) ;

	free( ibus) ;

	if( ref_count == 0)
	{
		int  fd ;

		if( dbus_connection_get_unix_fd( ibus_connection_get_connection(
				ibus_bus_get_connection( ibus_bus)) , &fd))
		{
			(*syms->x_term_manager_remove_fd)( fd) ;
		}
		(*syms->x_term_manager_remove_fd)( IBUS_ID) ;

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
	}

	return  ref_count ;
}

static int
key_event(
	x_im_t *  im ,
	u_char  key_char ,
	KeySym  ksym ,
	XKeyEvent *  event
	)
{
	im_ibus_t *  ibus ;
	int  is_enabled ;

	ibus = (im_ibus_t*) im ;

	is_enabled = ibus_input_context_is_enabled( ibus->context) ;

	if( ibus_input_context_process_key_event( ibus->context , ksym , event->keycode - 8 ,
			event->state | (event->type == KeyRelease ? IBUS_RELEASE_MASK : 0)))
	{
		if( is_enabled != ibus_input_context_is_enabled( ibus->context) ||
		   (ibus_input_context_is_enabled( ibus->context) &&
		    (ibus->im.preedit.filled_len > 0 || key_char >= 0x20)))
		{
			return  0 ;
		}
		else
		{
			/*
			 * Even if input context is enabled, enter, backspace etc keys are
			 * avaiable as far as no characters are input.
			 */
		}
	}

	return  1 ;
}

static int
switch_mode(
	x_im_t *  im
	)
{
	im_ibus_t *  ibus ;

	ibus =  (im_ibus_t*)  im ;

	if( ibus_input_context_is_enabled( ibus->context))
	{
		ibus_input_context_disable( ibus->context) ;
	}
	else
	{
		ibus_input_context_enable( ibus->context) ;
	}
	
	return  1 ;
}

static int
is_active(
	x_im_t *  im
	)
{
	return  ibus_input_context_is_enabled( ((im_ibus_t*)im)->context) ;
}

static void
focused(
	x_im_t *  im
	)
{
	im_ibus_t *  ibus ;

	ibus =  (im_ibus_t*)  im ;

	ibus_input_context_focus_in( ibus->context) ;

	if( ibus->im.cand_screen)
	{
		(*ibus->im.cand_screen->show)( ibus->im.cand_screen) ;
	}

	ibus->pressing_mod_key = 0 ;
}

static void
unfocused(
	x_im_t *  im
	)
{
	im_ibus_t *  ibus ;

	ibus = (im_ibus_t*)  im ;

	ibus_input_context_focus_out( ibus->context) ;

	if( ibus->im.cand_screen)
	{
		(*ibus->im.cand_screen->hide)( ibus->im.cand_screen) ;
	}

	ibus->pressing_mod_key = 0 ;
}


static void
connection_handler_idling(void)
{
	DBusConnection *  connection ;

	connection = ibus_connection_get_connection( ibus_bus_get_connection( ibus_bus)) ;

	dbus_connection_read_write( connection , 0) ;

	while( ! dbus_connection_dispatch( connection)) ;
}

static void
connection_handler(void)
{
	DBusConnection *  connection ;

	connection = ibus_connection_get_connection( ibus_bus_get_connection( ibus_bus)) ;

	dbus_connection_read_write( connection , 0) ;

	while( ! dbus_connection_dispatch( connection)) ;
}


/* --- global functions --- */

x_im_t *
im_ibus_new(
	u_int64_t  magic ,
	ml_char_encoding_t  term_encoding ,
	x_im_export_syms_t *  export_syms ,
	char *  engine ,
	u_int  mod_ignore_mask
	)
{
	im_ibus_t *  ibus = NULL ;
	
	if( magic != (u_int64_t) IM_API_COMPAT_CHECK_MAGIC)
	{
		kik_error_printf( "Incompatible input method API.\n") ;

		return  NULL ;
	}

	if( getenv( "MOD_KEY_DEBUG"))
	{
		mod_key_debug = 1 ;
	}

	if( ! ibus_bus)
	{
		int  fd ;

		syms = export_syms ;

		ibus_init() ;

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

		if( ! dbus_connection_get_unix_fd( ibus_connection_get_connection(
				ibus_bus_get_connection( ibus_bus)) , &fd))
		{
			goto  error ;
		}

		(*syms->x_term_manager_add_fd)( fd , connection_handler) ;
		(*syms->x_term_manager_add_fd)( IBUS_ID , connection_handler_idling) ;

		kik_list_new( im_ibus_t , ibus_list) ;
	}

	if( ! ( ibus = malloc( sizeof( im_ibus_t))))
	{
	#ifdef  DEBUG
		kik_warn_printf( KIK_DEBUG_TAG " malloc failed.\n") ;
	#endif

		goto  error ;
	}

	ibus->context = ibus_bus_create_input_context( ibus_bus , "mlterm") ;
	ibus_input_context_set_capabilities( ibus->context ,
		IBUS_CAP_PREEDIT_TEXT | IBUS_CAP_FOCUS | IBUS_CAP_SURROUNDING_TEXT) ;

        g_signal_connect( ibus->context , "update-preedit-text" ,
			G_CALLBACK( update_preedit_text) , ibus) ;
        g_signal_connect( ibus->context , "commit-text" , G_CALLBACK( commit_text) , ibus) ;

	ibus->term_encoding = term_encoding ;
	ibus->parser_ibus = NULL ;
	ibus->pressing_mod_key = 0 ;
	ibus->mod_ignore_mask =  mod_ignore_mask ;

	if( ! ( ibus->parser_ibus = (*syms->ml_parser_new)( ML_UTF8)))
	{
		goto  error ;
	}

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
	}

	if( ibus)
	{
		if( ibus->parser_ibus)
		{
			(*ibus->parser_ibus->delete)( ibus->parser_ibus) ;
		}

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
