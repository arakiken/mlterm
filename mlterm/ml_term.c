/*
 *	$Id$
 */

#include  "ml_term.h"

#include  <kiklib/kik_mem.h>	/* malloc/free */
#include  <kiklib/kik_debug.h>
#include  <kiklib/kik_str.h>	/* strdup */

#include  "ml_pty.h"
#include  "ml_vt100_parser.h"
#include  "ml_screen.h"


/* --- global functions --- */

ml_term_t *
ml_term_new(
	u_int  cols ,
	u_int  rows ,
	u_int  tab_size ,
	u_int  log_size ,
	ml_char_encoding_t  encoding ,
	int  is_auto_encoding ,
	ml_unicode_font_policy_t  policy ,
	int  col_size_a ,
	int  use_char_combining ,
	int  use_multi_col_char ,
	int  use_bidi ,
	int  use_bce ,
	int  use_dynamic_comb ,
	ml_bs_mode_t  bs_mode ,
	ml_vertical_mode_t  vertical_mode ,
	ml_iscii_lang_type_t  iscii_lang_type
	)
{
	ml_term_t *  term ;

	if( ( term = malloc( sizeof( ml_term_t))) == NULL)
	{
	#ifdef  DEBUG
		kik_warn_printf( KIK_DEBUG_TAG " malloc failed.\n") ;
	#endif
	
		return  NULL ;
	}

	term->pty = NULL ;
	term->pty_listener = NULL ;

	if( ( term->screen = ml_screen_new( cols , rows , tab_size , log_size , use_bce , bs_mode)) == NULL)
	{
	#ifdef  DEBUG
		kik_warn_printf( KIK_DEBUG_TAG " ml_screen_new failed.\n") ;
	#endif
	
		goto  error ;
	}

	if( ( term->parser = ml_vt100_parser_new( term->screen , encoding , policy ,
					col_size_a , use_char_combining , use_multi_col_char)) == NULL)
	{
	#ifdef  DEBUG
		kik_warn_printf( KIK_DEBUG_TAG " ml_vt100_parser_new failed.\n") ;
	#endif
	
		goto  error ;
	}

	if( ! ml_config_menu_init( &term->config_menu))
	{
	#ifdef  DEBUG
		kik_warn_printf( KIK_DEBUG_TAG " ml_config_menu_init failed.\n") ;
	#endif
	
		goto  error ;
	}

	term->shape = NULL ;
	term->iscii_lang = NULL ;
	term->iscii_lang_type = iscii_lang_type ;
	term->vertical_mode = vertical_mode ;
	term->use_bidi = use_bidi ;
	term->use_dynamic_comb = use_dynamic_comb ;

	term->is_auto_encoding = is_auto_encoding ;

	term->win_name = NULL ;
	term->icon_name = NULL ;
	term->icon_path = NULL ;
	term->is_mouse_pos_sending = 0 ;
	term->is_app_keypad = 0 ;
	term->is_app_cursor_keys = 0 ;

	term->is_attached = 0 ;
	
	return  term ;

error:
	if( term->screen)
	{
		ml_screen_delete( term->screen) ;
	}

	if( term->parser)
	{
		ml_vt100_parser_delete( term->parser) ;
	}

	free( term) ;

	return  NULL ;
}

int
ml_term_delete(
	ml_term_t *  term
	)
{
	if( term->pty)
	{
		if( term->pty_listener && term->pty_listener->closed)
		{
			(*term->pty_listener->closed)( term->pty_listener->self) ;
		}

		ml_pty_delete( term->pty) ;
	}
	
	if( term->shape)
	{
		(*term->shape->delete)( term->shape) ;
	}

	if( term->iscii_lang)
	{
		ml_iscii_lang_delete( term->iscii_lang) ;
	}

	free( term->win_name) ;
	free( term->icon_name) ;

	ml_screen_delete( term->screen) ;
	ml_vt100_parser_delete( term->parser) ;

	ml_config_menu_final( &term->config_menu) ;
	
	free( term) ;

	return  1 ;
}

int
ml_term_open_pty(
	ml_term_t *  term ,
	char *  cmd_path ,
	char **  argv ,
	char **  env ,
	char *  host
	)
{
	if( term->pty)
	{
		/* already opened */
		
		return  1 ;
	}
	else
	{
		if( ( term->pty = ml_pty_new( cmd_path , argv , env , host ,
					ml_screen_get_logical_cols( term->screen) ,
					ml_screen_get_logical_rows( term->screen))) == NULL)
		{
		#ifdef  DEBUG
			kik_warn_printf( KIK_DEBUG_TAG " ml_pty_new failed.\n") ;
		#endif

			return  0 ;
		}

		ml_vt100_parser_set_pty( term->parser , term->pty) ;

		return  1 ;
	}
}

int
ml_term_attach(
	ml_term_t *  term ,
	ml_xterm_event_listener_t *  xterm_listener ,
	ml_config_event_listener_t *  config_listener ,
	ml_screen_event_listener_t *  screen_listener ,
	ml_pty_event_listener_t *  pty_listener
	)
{
	ml_vt100_parser_set_xterm_listener( term->parser , xterm_listener) ;
	ml_vt100_parser_set_config_listener( term->parser , config_listener) ;
	ml_screen_set_listener( term->screen , screen_listener) ;

	term->pty_listener = pty_listener ;

	term->is_attached = 1 ;

	return  1 ;
}

int
ml_term_detach(
	ml_term_t *  term
	)
{
	ml_vt100_parser_set_xterm_listener( term->parser , NULL) ;
	ml_vt100_parser_set_config_listener( term->parser , NULL) ;
	ml_screen_set_listener( term->screen , NULL) ;

	term->pty_listener = NULL ;

	term->is_attached = 0 ;

	return  1 ;
}

int
ml_term_is_attached(
	ml_term_t *  term
	)
{
	return  term->is_attached ;
}

int
ml_term_parse_vt100_sequence(
	ml_term_t *  term
	)
{
	return  ml_parse_vt100_sequence( term->parser) ;
}

int
ml_term_change_encoding(
	ml_term_t *  term ,
	ml_char_encoding_t  encoding
	)
{
	return  ml_vt100_parser_change_encoding( term->parser , encoding) ;
}

ml_char_encoding_t
ml_term_get_encoding(
	ml_term_t *  term
	)
{
	return  ml_vt100_parser_get_encoding( term->parser) ;
}

int
ml_term_set_auto_encoding(
	ml_term_t *  term ,
	int  is_auto_encoding
	)
{
	term->is_auto_encoding = is_auto_encoding ;

	return  1 ;
}

int
ml_term_is_auto_encoding(
	ml_term_t *  term
	)
{
	return  term->is_auto_encoding ;
}

int
ml_term_set_unicode_font_policy(
	ml_term_t *  term ,
	ml_unicode_font_policy_t  policy
	)
{
	return  ml_vt100_parser_set_unicode_font_policy( term->parser , policy) ;
}

size_t
ml_term_convert_to(
	ml_term_t *  term ,
	u_char *  dst ,
	size_t  len ,
	mkf_parser_t *  parser
	)
{
	return  ml_vt100_parser_convert_to( term->parser , dst , len , parser) ;
}

int
ml_term_init_encoding_parser(
	ml_term_t *  term
	)
{
	return  ml_init_encoding_parser( term->parser) ;
}

int
ml_term_init_encoding_conv(
	ml_term_t *  term
	)
{
	return  ml_init_encoding_conv( term->parser) ;
}

int
ml_term_enable_logging_vt_seq(
	ml_term_t *  term
	)
{
	return  ml_vt100_parser_enable_logging_vt_seq( term->parser) ;
}

int
ml_term_get_pty_fd(
	ml_term_t *  term
	)
{
	if( term->pty == NULL)
	{
		return  -1 ;
	}
	 
	return  term->pty->master ;
}

char *
ml_term_get_slave_name(
	ml_term_t *  term
	)
{
	if( term->pty == NULL)
	{
		return  NULL ;
	}

	return  term->pty->slave_name ;
}

pid_t
ml_term_get_child_pid(
	ml_term_t *  term
	)
{
	if( term->pty == NULL)
	{
		return  -1 ;
	}
	
	return  term->pty->child_pid ;
}

size_t
ml_term_write(
	ml_term_t *  term ,
	u_char *  buf ,
	size_t  len ,
	int  to_menu
	)
{
	if( to_menu)
	{
		return  ml_config_menu_write( &term->config_menu , buf , len) ;
	}
	else
	{
		if( term->pty == NULL)
		{
			return  0 ;
		}

		return  ml_write_to_pty( term->pty , buf , len) ;
	}
}

int
ml_term_flush(
	ml_term_t *  term
	)
{
	if( term->pty == NULL)
	{
		return  0 ;
	}
	
	return  ml_flush_pty( term->pty) ;
}

int
ml_term_resize(
	ml_term_t *  term ,
	u_int  cols ,
	u_int  rows
	)
{
	if( term->pty)
	{
		ml_set_pty_winsize( term->pty , cols , rows) ;
	}

	ml_screen_logical( term->screen) ;
	ml_screen_resize( term->screen , cols , rows) ;
	ml_screen_render( term->screen) ;
	ml_screen_visual( term->screen) ;

	return  1 ;
}

int
ml_term_cursor_col(
	ml_term_t *  term
	)
{
	return  ml_screen_cursor_col( term->screen) ;
}

int
ml_term_cursor_char_index(
	ml_term_t *  term
	)
{
	return  ml_screen_cursor_char_index( term->screen) ;
}

int
ml_term_cursor_row(
	ml_term_t *  term
	)
{
	return  ml_screen_cursor_row( term->screen) ;
}

int
ml_term_cursor_row_in_screen(
	ml_term_t *  term
	)
{
	return  ml_screen_cursor_row_in_screen( term->screen) ;
}

int
ml_term_unhighlight_cursor(
	ml_term_t *  term
	)
{
	ml_line_t *  line ;

	ml_screen_logical( term->screen) ;
	
	if( ( line = ml_screen_get_cursor_line( term->screen)) == NULL || ml_line_is_empty( line))
	{
		/* ml_screen_render( term->screen) ; */
		ml_screen_visual( term->screen) ;

		return  0 ;
	}

	ml_line_set_modified( line , ml_screen_cursor_char_index( term->screen) ,
		ml_screen_cursor_char_index( term->screen)) ;

	/* ml_screen_render( term->screen) ; */
	ml_screen_visual( term->screen) ;

	return  1 ;
}

u_int
ml_term_get_cols(
	ml_term_t *  term
	)
{
	return  ml_screen_get_cols( term->screen) ;
}

u_int
ml_term_get_rows(
	ml_term_t *  term
	)
{
	return  ml_screen_get_rows( term->screen) ;
}

u_int
ml_term_get_logical_cols(
	ml_term_t *  term
	)
{
	return  ml_screen_get_logical_cols( term->screen) ;
}

u_int
ml_term_get_logical_rows(
	ml_term_t *  term
	)
{
	return  ml_screen_get_logical_rows( term->screen) ;
}

u_int
ml_term_get_log_size(
	ml_term_t *  term
	)
{
	return  ml_screen_get_log_size( term->screen) ;
}

int
ml_term_change_log_size(
	ml_term_t *  term ,
	u_int  log_size
	)
{
	return  ml_screen_change_log_size( term->screen , log_size) ;
}

u_int
ml_term_get_num_of_logged_lines(
	ml_term_t *  term
	)
{
	return  ml_screen_get_num_of_logged_lines( term->screen) ;
}

int
ml_term_convert_scr_row_to_abs(
	ml_term_t *  term ,
	int  row
	)
{
	return  ml_screen_convert_scr_row_to_abs( term->screen , row) ;
}

ml_line_t *
ml_term_get_line(
	ml_term_t *  term ,
	int  row
	)
{
	return  ml_screen_get_line( term->screen , row) ;
}

ml_line_t *
ml_term_get_line_in_screen(
	ml_term_t *  term ,
	int  row
	)
{
	return  ml_screen_get_line_in_screen( term->screen , row) ;
}

ml_line_t *
ml_term_get_cursor_line(
	ml_term_t *  term
	)
{
	return  ml_screen_get_cursor_line( term->screen) ;
}

int
ml_term_is_cursor_visible(
	ml_term_t *  term
	)
{
	return  ml_screen_is_cursor_visible( term->screen) ;
}

/*
 * Not implemented yet.
 */
#if  0
int
ml_term_set_modified_region(
	ml_term_t *  term ,
	int  beg_char_index ,
	int  beg_row ,
	u_int  nchars ,
	u_int  nrows
	)
{
	return  0 ;
}
#endif

/*
 * Not used.
 */
#if  0
int
ml_term_set_modified_region_in_screen(
	ml_term_t *  term ,
	int  beg_char_index ,
	int  beg_row ,
	u_int  nchars ,
	u_int  nrows
	)
{
	int  row ;
	ml_line_t *  line ;

	ml_screen_logical( term->screen) ;

	for( row = beg_row ; row < beg_row + nrows ; row ++)
	{
		if( ( line = ml_screen_get_line_in_screen( term->screen , row)))
		{
			ml_line_set_modified( line , beg_char_index , beg_char_index + nchars - 1) ;
		}
	}

	/* ml_screen_render( term->screen) ; */
	ml_screen_visual( term->screen) ;

	return  1 ;
}
#endif

int
ml_term_set_modified_lines(
	ml_term_t *  term ,
	u_int  beg ,
	u_int  end
	)
{
	int  row ;
	ml_line_t *  line ;

	ml_screen_logical( term->screen) ;

	for( row = beg ; row <= end ; row ++)
	{
		if( ( line = ml_screen_get_line( term->screen , row)))
		{
			ml_line_set_modified_all( line) ;
		}
	}

	/* ml_screen_render( term->screen) ; */
	ml_screen_visual( term->screen) ;

	return  1 ;
}

int
ml_term_set_modified_lines_in_screen(
	ml_term_t *  term ,
	u_int  beg ,
	u_int  end
	)
{
	int  row ;
	ml_line_t *  line ;

	ml_screen_logical( term->screen) ;

	for( row = beg ; row <= end ; row ++)
	{
		if( ( line = ml_screen_get_line_in_screen( term->screen , row)))
		{
			ml_line_set_modified_all( line) ;
		}
	}

	/* ml_screen_render( term->screen) ; */
	ml_screen_visual( term->screen) ;

	return  1 ;
}

int
ml_term_set_modified_all_lines_in_screen(
	ml_term_t *  term
	)
{
	ml_screen_logical( term->screen) ;
	ml_screen_set_modified_all( term->screen) ;
	/* ml_screen_render( term->screen) ; */
	ml_screen_visual( term->screen) ;

	return  1 ;
}

int
ml_term_updated_all(
	ml_term_t *  term
	)
{
	int  row ;
	ml_line_t *  line ;

	ml_screen_logical( term->screen) ;

	for( row = 0 ; row < ml_edit_get_rows( term->screen->edit) ; row ++)
	{
		if( ( line = ml_screen_get_line_in_screen( term->screen , row)))
		{
			ml_line_updated( line) ;
		}
	}

	/* ml_screen_render( term->screen) ; */
	ml_screen_visual( term->screen) ;

	return  1 ;
}

int
ml_term_update_special_visual(
	ml_term_t *  term
	)
{
	ml_logical_visual_t *  logvis ;

	if( term->shape)
	{
		(*term->shape->delete)( term->shape) ;
		term->shape = NULL ;
	}

	if( term->iscii_lang)
	{
		ml_iscii_lang_delete( term->iscii_lang) ;
		term->iscii_lang = NULL ;
	}
	
	term->screen->use_dynamic_comb = 0 ;
	ml_screen_delete_logical_visual( term->screen) ;

	if( ml_term_get_encoding( term) == ML_ISCII)
	{
		/*
		 * It is impossible to process ISCII with other encoding proper auxes.
		 */

		if( ( term->iscii_lang = ml_iscii_lang_new( term->iscii_lang_type)) == NULL)
		{
		#ifdef  DEBUG
			kik_warn_printf( KIK_DEBUG_TAG " ml_iscii_new() failed.\n") ;
		#endif

			return  0 ;
		}

		if( ( term->shape = ml_iscii_shape_new( term->iscii_lang)) == NULL)
		{
		#ifdef  DEBUG
			kik_warn_printf( KIK_DEBUG_TAG " ml_iscii_shape_new() failed.\n") ;
		#endif

			goto  error ;
		}

		if( ( logvis = ml_logvis_iscii_new( term->iscii_lang)) == NULL)
		{
		#ifdef  DEBUG
			kik_warn_printf( KIK_DEBUG_TAG " ml_logvis_iscii_new() failed.\n") ;
		#endif

			goto  error ;
		}

		if( ! ml_screen_add_logical_visual( term->screen , logvis))
		{
			goto  error ;
		}
	}
	else
	{
		if( term->use_dynamic_comb)
		{
			if( ( logvis = ml_logvis_comb_new()) == NULL)
			{
			#ifdef  DEBUG
				kik_warn_printf( KIK_DEBUG_TAG " ml_logvis_comb_new() failed.\n") ;
			#endif

				goto  error ;
			}

			if( ! ml_screen_add_logical_visual( term->screen , logvis))
			{
				(*logvis->delete)( logvis) ;
				
				goto  error ;
			}

			term->screen->use_dynamic_comb = 1 ;
		}
		
		if( term->vertical_mode)
		{
			if( ( logvis = ml_logvis_vert_new( term->vertical_mode)) == NULL)
			{
			#ifdef  DEBUG
				kik_warn_printf( KIK_DEBUG_TAG " ml_logvis_vert_new() failed.\n") ;
			#endif

				goto  error ;
			}

			if( ! ml_screen_add_logical_visual( term->screen , logvis))
			{
				goto  error ;
			}
		}
		else if( term->use_bidi && ml_term_get_encoding( term) == ML_UTF8)
		{
			if( ( term->shape = ml_arabic_shape_new()) == NULL)
			{
			#ifdef  DEBUG
				kik_warn_printf( KIK_DEBUG_TAG " x_arabic_shape_new() failed.\n") ;
			#endif

				goto  error ;
			}

		#if  1
			if( ( logvis = ml_logvis_bidi_new( 0)) == NULL)
		#else
			if( ( logvis = ml_logvis_bidi_new( 1)) == NULL)
		#endif
			{
			#ifdef  DEBUG
				kik_warn_printf( KIK_DEBUG_TAG " ml_logvis_bidi_new() failed.\n") ;
			#endif

				goto  error ;
			}

			if( ! ml_screen_add_logical_visual( term->screen , logvis))
			{
				goto  error ;
			}
		}
	}
	
	ml_screen_render( term->screen) ;
	ml_screen_visual( term->screen) ;

	return  1 ;

error:
	if( term->shape)
	{
		(*term->shape->delete)( term->shape) ;
		term->shape = NULL ;
	}

	if( term->iscii_lang)
	{
		ml_iscii_lang_delete( term->iscii_lang) ;
		term->iscii_lang = NULL ;
	}

	term->screen->use_dynamic_comb = 0 ;
	ml_screen_delete_logical_visual( term->screen) ;

	return  0 ;
}

ml_bs_mode_t
ml_term_is_backscrolling(
	ml_term_t *  term
	)
{
	return  ml_screen_is_backscrolling( term->screen) ;
}

int
ml_term_set_backscroll_mode(
	ml_term_t *  term ,
	ml_bs_mode_t  mode
	)
{
	return  ml_set_backscroll_mode( term->screen , mode) ;
}

int
ml_term_enter_backscroll_mode(
	ml_term_t *  term
	)
{
	/* XXX */
	if( term->vertical_mode)
	{
		kik_msg_printf( "Not supported backscrolling in vertical mode.\n") ;
		
		return  0 ;
	}
	
	return  ml_enter_backscroll_mode( term->screen) ;
}

int
ml_term_exit_backscroll_mode(
	ml_term_t *  term
	)
{
	return  ml_exit_backscroll_mode( term->screen) ;
}

int
ml_term_backscroll_to(
	ml_term_t *  term ,
	int  row
	)
{
	return  ml_screen_backscroll_to( term->screen , row) ;
}

int
ml_term_backscroll_upward(
	ml_term_t *  term ,
	u_int  size
	)
{
	return  ml_screen_backscroll_upward( term->screen , size) ;
}

int
ml_term_backscroll_downward(
	ml_term_t *  term ,
	u_int  size
	)
{
	return  ml_screen_backscroll_downward( term->screen , size) ;
}

u_int
ml_term_get_tab_size(
	ml_term_t *  term
	)
{
	return  ml_screen_get_tab_size( term->screen) ;
}

int
ml_term_set_tab_size(
	ml_term_t *  term ,
	u_int  tab_size
	)
{
	return  ml_screen_set_tab_size( term->screen , tab_size) ;
}

int
ml_term_reverse_color(
	ml_term_t *  term ,
	int  beg_char_index ,
	int  beg_row ,
	int  end_char_index ,
	int  end_row
	)
{
	return  ml_screen_reverse_color( term->screen , beg_char_index , beg_row ,
			end_char_index , end_row) ;
}

int
ml_term_restore_color(
	ml_term_t *  term ,
	int  beg_char_index ,
	int  beg_row ,
	int  end_char_index ,
	int  end_row
	)
{
	return  ml_screen_restore_color( term->screen , beg_char_index , beg_row ,
			end_char_index , end_row) ;
}

u_int
ml_term_copy_region(
	ml_term_t *  term ,
	ml_char_t *  chars ,
	u_int  num_of_chars ,
	int  beg_char_index ,
	int  beg_row ,
	int  end_char_index ,
	int  end_row
	)
{
	return  ml_screen_copy_region( term->screen , chars , num_of_chars ,
			beg_char_index , beg_row , end_char_index , end_row) ;
}

u_int
ml_term_get_region_size(
	ml_term_t *  term ,
	int  beg_char_index ,
	int  beg_row ,
	int  end_char_index ,
	int  end_row
	)
{
	return  ml_screen_get_region_size( term->screen , beg_char_index , beg_row ,
			end_char_index , end_row) ;
}

int
ml_term_get_line_region(
	ml_term_t *  term ,
	int *  beg_row ,
	int *  end_char_index ,
	int *  end_row ,
	int  base_row
	)
{
	return  ml_screen_get_line_region( term->screen , beg_row , end_char_index ,
			end_row , base_row) ;
}

int
ml_term_get_word_region(
	ml_term_t *  term ,
	int *  beg_char_index ,
	int *  beg_row ,
	int *  end_char_index ,
	int *  end_row ,
	int  base_char_index ,
	int  base_row
	)
{
	return  ml_screen_get_word_region( term->screen , beg_char_index , beg_row , end_char_index ,
			end_row , base_char_index , base_row) ;
}

int
ml_term_set_char_combining_flag(
	ml_term_t *  term ,
	int  flag
	)
{
	term->parser->use_char_combining = flag ;

	return  1 ;
}

int
ml_term_is_using_char_combining(
	ml_term_t *  term
	)
{
	return  term->parser->use_char_combining ;
}

int
ml_term_set_multi_col_char_flag(
	ml_term_t *  term ,
	int  flag
	)
{
	term->parser->use_multi_col_char = flag ;

	return  1 ;
}

int
ml_term_is_using_multi_col_char(
	ml_term_t *  term
	)
{
	return  term->parser->use_multi_col_char ;
}

int
ml_term_set_mouse_report(
	ml_term_t *  term ,
	int  flag
	)
{
	term->is_mouse_pos_sending = flag ;

	return  1 ;
}

int
ml_term_is_mouse_pos_sending(
	ml_term_t *  term
	)
{
	return  term->is_mouse_pos_sending ;
}

int
ml_term_set_app_keypad(
	ml_term_t *  term ,
	int  flag
	)
{
	term->is_app_keypad = flag ;

	return  1 ;
}

int
ml_term_is_app_keypad(
	ml_term_t *  term
	)
{
	return  term->is_app_keypad ;
}

int
ml_term_set_app_cursor_keys(
	ml_term_t *  term ,
	int  flag
	)
{
	term->is_app_cursor_keys = flag ;

	return  1 ;
}

int
ml_term_is_app_cursor_keys(
	ml_term_t *  term
	)
{
	return  term->is_app_cursor_keys ;
}

int
ml_term_set_window_name(
	ml_term_t *  term ,
	char *  name
	)
{
	free( term->win_name) ;
	term->win_name = strdup( name) ;

	return  1 ;
}

int
ml_term_set_icon_name(
	ml_term_t *  term ,
	char *  name
	)
{
	free( term->icon_name) ;
	term->icon_name = strdup( name) ;

	return  1 ;
}

int
ml_term_set_icon_path(
	ml_term_t *  term ,
	char *  path
	)
{
	free( term->icon_path) ;
	term->icon_path = path ;
}

char *
ml_term_window_name(
	ml_term_t *  term
	)
{
	return  term->win_name ;
}

char *
ml_term_icon_name(
	ml_term_t *  term
	)
{
	return  term->icon_name ;
}

char *
ml_term_icon_path(
	ml_term_t *  term
	)
{
	return term->icon_path ;
}

int
ml_term_start_config_menu(
	ml_term_t *  term ,
	char *  cmd_path ,
	int  x ,
	int  y ,
	char *  display
	)
{
	return  ml_config_menu_start( &term->config_menu , cmd_path , x , y , display, term->pty->slave) ;
}
