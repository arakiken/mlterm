/*
 *	$Id$
 */

#include  "ml_vt100_command.h"

#include  <stdio.h>		/* sprintf */
#include  <unistd.h>		/* getcwd */
#include  <limits.h>		/* PATH_MAX */
#include  <kiklib/kik_util.h>	/* DIGIT_STR_LEN */
#include  <kiklib/kik_str.h>	/* kik_str_to_int */
#include  <kiklib/kik_locale.h>	/* kik_get_locale */

#include  "ml_xic.h"		/* ml_xic_get_xim_name */


/* --- global functions --- */

/*
 * for VT100 commands
 *
 * !! Notice !!
 * these functions considers termscr->image to be logical order.
 * call ml_term_screen_start_vt100_cmd() before using them.
 */

int
ml_vt100_cmd_combine_with_prev_char(
	ml_term_screen_t *  termscr ,
	u_char *  bytes ,
	size_t  ch_size ,
	ml_font_t *  font ,
	ml_font_decor_t  font_decor ,
	ml_color_t  fg_color ,
	ml_color_t  bg_color
	)
{
	ml_char_t *  ch ;
	ml_image_line_t *  line ;
	int  result ;
	
	ml_cursor_go_back( termscr->image , WRAPAROUND) ;
	
	if( ( line = ml_image_get_line( termscr->image , ml_cursor_row( termscr->image))) == NULL ||
		ml_imgline_is_empty( line))
	{
		result = 0 ;

		goto  end ;
	}
	
	ch = &line->chars[ ml_cursor_char_index( termscr->image)] ;
	
	if( ( result = ml_char_combine( ch , bytes , ch_size , font , font_decor ,
		fg_color , bg_color)))
	{
		ml_imgline_set_modified( line ,
			ml_cursor_char_index( termscr->image) ,
			ml_cursor_char_index( termscr->image) , 0) ;
	}

end:
	ml_cursor_go_forward( termscr->image , WRAPAROUND) ;

	return  result ;
}

int
ml_vt100_cmd_insert_chars(
	ml_term_screen_t *  termscr ,
	ml_char_t *  chars ,
	u_int  len
	)
{
	return  ml_image_insert_chars( termscr->image , chars , len) ;
}

int
ml_vt100_cmd_insert_blank_chars(
	ml_term_screen_t *  termscr ,
	u_int  len
	)
{
	return  ml_image_insert_blank_chars( termscr->image , len) ;
}

int
ml_vt100_cmd_vertical_tab(
	ml_term_screen_t *  termscr
	)
{
	return  ml_image_vertical_tab( termscr->image) ;
}

int
ml_vt100_cmd_set_tab_stop(
	ml_term_screen_t *  termscr
	)
{
	return  ml_image_set_tab_stop( termscr->image) ;
}

int
ml_vt100_cmd_clear_tab_stop(
	ml_term_screen_t *  termscr
	)
{
	return  ml_image_clear_tab_stop( termscr->image) ;
}

int
ml_vt100_cmd_clear_all_tab_stops(
	ml_term_screen_t *  termscr
	)
{
	return  ml_image_clear_all_tab_stops( termscr->image) ;
}

int
ml_vt100_cmd_insert_new_lines(
	ml_term_screen_t *  termscr ,
	u_int  size
	)
{
	int  counter ;
		
	for( counter = 0 ; counter < size ; counter ++)
	{
		ml_image_insert_new_line( termscr->image) ;
	}

	return  1 ;
}

int
ml_vt100_cmd_line_feed(
	ml_term_screen_t *  termscr
	)
{	
	return  ml_cursor_go_downward( termscr->image , SCROLL | BREAK_BOUNDARY) ;
}

int
ml_vt100_cmd_overwrite_chars(
	ml_term_screen_t *  termscr ,
	ml_char_t *  chars ,
	u_int  len
	)
{
	return  ml_image_overwrite_chars( termscr->image , chars , len) ;
}

int
ml_vt100_cmd_delete_cols(
	ml_term_screen_t *  termscr ,
	u_int  len
	)
{	
	return  ml_image_delete_cols( termscr->image , len) ;
}

int
ml_vt100_cmd_delete_lines(
	ml_term_screen_t *  termscr ,
	u_int  size
	)
{
	int  counter ;

	for( counter = 0 ; counter < size ; counter ++)
	{
		if( ! ml_image_delete_line( termscr->image))
		{
		#ifdef  DEBUG
			kik_warn_printf( KIK_DEBUG_TAG " deleting nonexisting lines.\n") ;
		#endif
			
			return  0 ;
		}
	}
	
	return  1 ;
}

int
ml_vt100_cmd_clear_line_to_right(
	ml_term_screen_t *  termscr
	)
{	
	return  ml_image_clear_line_to_right( termscr->image) ;
}

int
ml_vt100_cmd_clear_line_to_left(
	ml_term_screen_t *  termscr
	)
{
	return  ml_image_clear_line_to_left( termscr->image) ;
}

int
ml_vt100_cmd_clear_below(
	ml_term_screen_t *  termscr
	)
{	
	return  ml_image_clear_below( termscr->image) ;
}

int
ml_vt100_cmd_clear_above(
	ml_term_screen_t *  termscr
	)
{
	return  ml_image_clear_above( termscr->image) ;
}

int
ml_vt100_cmd_set_scroll_region(
	ml_term_screen_t *  termscr ,
	int  beg ,
	int  end
	)
{
	return  ml_image_set_scroll_region( termscr->image , beg , end) ;
}

int
ml_vt100_cmd_scroll_image_upward(
	ml_term_screen_t *  termscr ,
	u_int  size
	)
{
	return  ml_cursor_go_downward( termscr->image , SCROLL | BREAK_BOUNDARY) ;
}

int
ml_vt100_cmd_scroll_image_downward(
	ml_term_screen_t *  termscr ,
	u_int  size
	)
{
	return  ml_cursor_go_upward( termscr->image , SCROLL | BREAK_BOUNDARY) ;
}

int
ml_vt100_cmd_go_forward(
	ml_term_screen_t *  termscr ,
	u_int  size
	)
{
	int  counter ;

	for( counter = 0 ; counter < size ; counter ++)
	{
		if( ! ml_cursor_go_forward( termscr->image , BREAK_BOUNDARY))
		{
		#ifdef  DEBUG
			kik_warn_printf( KIK_DEBUG_TAG " cursor cannot go forward any more.\n") ;
		#endif
		
			return  0 ;
		}
	}

	return  1 ;
}

int
ml_vt100_cmd_go_back(
	ml_term_screen_t *  termscr ,
	u_int  size
	)
{
	int  counter ;

	for( counter = 0 ; counter < size ; counter ++)
	{
		if( ! ml_cursor_go_back( termscr->image , 0))
		{
		#ifdef  DEBUG
			kik_warn_printf( KIK_DEBUG_TAG " cursor cannot go back any more.\n") ;
		#endif
					
			return  0 ;
		}
	}

	return  1 ;
}

int
ml_vt100_cmd_go_upward(
	ml_term_screen_t *  termscr ,
	u_int  size
	)
{
	int  counter ;
	
	for( counter = 0 ; counter < size ; counter ++)
	{
		if( ! ml_cursor_go_upward( termscr->image , BREAK_BOUNDARY))
		{
		#ifdef  DEBUG
			kik_warn_printf( KIK_DEBUG_TAG " cursor cannot go upward any more.\n") ;
		#endif
				
			return  0 ;
		}
	}

	return  1 ;
}

int
ml_vt100_cmd_go_downward(
	ml_term_screen_t *  termscr ,
	u_int  size
	)
{
	int  counter ;
	
	for( counter = 0 ; counter < size ; counter ++)
	{
		if( ! ml_cursor_go_downward( termscr->image , BREAK_BOUNDARY))
		{
		#ifdef  DEBUG
			kik_warn_printf( KIK_DEBUG_TAG " cursor cannot go downward any more.\n") ;
		#endif
		
			return  0 ;
		}
	}

	return  1 ;
}

int
ml_vt100_cmd_goto_beg_of_line(
	ml_term_screen_t *  termscr
	)
{
	return  ml_cursor_goto_beg_of_line( termscr->image) ;
}

int
ml_vt100_cmd_go_horizontally(
	ml_term_screen_t *  termscr ,
	int  col
	)
{
	return  ml_vt100_cmd_goto( termscr , col , ml_cursor_row( termscr->image)) ;
}

int
ml_vt100_cmd_go_vertically(
	ml_term_screen_t *  termscr ,
	int  row
	)
{
	return  ml_vt100_cmd_goto( termscr , ml_cursor_col( termscr->image) , row) ;
}

int
ml_vt100_cmd_goto_home(
	ml_term_screen_t *  termscr
	)
{
	return  ml_cursor_goto_home( termscr->image) ;
}

int
ml_vt100_cmd_goto(
	ml_term_screen_t *  termscr ,
	int  col ,
	int  row
	)
{
	return  ml_cursor_goto( termscr->image , col , row , BREAK_BOUNDARY) ;
}

int
ml_vt100_cmd_save_cursor(
	ml_term_screen_t *  termscr
	)
{
	return  ml_cursor_save( termscr->image) ;
}

int
ml_vt100_cmd_restore_cursor(
	ml_term_screen_t *  termscr
	)
{
	return  ml_cursor_restore( termscr->image) ;
}

int
ml_vt100_cmd_bel(
	ml_term_screen_t *  termscr
	)
{	
	if( termscr->bel_mode == BEL_SOUND)
	{
		XBell( termscr->window.display , 0) ;
	}
	else if( termscr->bel_mode == BEL_VISUAL)
	{
		ml_window_fill_all( &termscr->window) ;

		XFlush( termscr->window.display) ;

		ml_window_clear_all( &termscr->window) ;
		ml_image_set_modified_all( termscr->image) ;
	}

	return  1 ;
}

int
ml_vt100_cmd_resize_columns(
	ml_term_screen_t *  termscr ,
	u_int  cols
	)
{
	if( cols == ml_image_get_cols( termscr->image))
	{
		return  0 ;
	}

	/*
	 * ml_term_screen_{start|stop}_vt100_cmd are necessary since
	 * window is redrawn in window_resized().
	 */
	 
	ml_term_screen_stop_vt100_cmd( termscr) ;
	
	ml_window_resize( &termscr->window , ml_col_width((termscr)->font_man) * cols ,
		ml_line_height((termscr)->font_man) * ml_image_get_rows( termscr->image) ,
		NOTIFY_TO_PARENT) ;

	/*
	 * !! Notice !!
	 * ml_window_resize() will invoke ConfigureNotify event but window_resized() won't be
	 * called , since xconfigure.width , xconfigure.height are the same as the already
	 * resized window.
	 */
	if( termscr->window.window_resized)
	{
		(*termscr->window.window_resized)( &termscr->window) ;
	}

	ml_term_screen_stop_vt100_cmd( termscr) ;
	
	return  1 ;
}

int
ml_vt100_cmd_reverse_video(
	ml_term_screen_t *  termscr
	)
{
	if( termscr->is_reverse == 1)
	{
		return  0 ;
	}

	ml_window_reverse_video( &termscr->window) ;
	
	ml_image_set_modified_all( termscr->image) ;

	termscr->is_reverse = 1 ;

	return  1 ;
}

int
ml_vt100_cmd_restore_video(
	ml_term_screen_t *  termscr
	)
{
	if( termscr->is_reverse == 0)
	{
		return  0 ;
	}
	
	ml_window_reverse_video( &termscr->window) ;
	
	ml_image_set_modified_all( termscr->image) ;
	
	termscr->is_reverse = 0 ;

	return  0 ;
}

int
ml_vt100_cmd_set_app_keypad(
	ml_term_screen_t *  termscr
	)
{
	termscr->is_app_keypad = 1 ;

	return  1 ;
}

int
ml_vt100_cmd_set_normal_keypad(
	ml_term_screen_t *  termscr
	)
{
	termscr->is_app_keypad = 0 ;

	return  1 ;
}

int
ml_vt100_cmd_set_app_cursor_keys(
	ml_term_screen_t *  termscr
	)
{
	termscr->is_app_cursor_keys = 1 ;

	return  1 ;
}

int
ml_vt100_cmd_set_normal_cursor_keys(
	ml_term_screen_t *  termscr
	)
{
	termscr->is_app_cursor_keys = 0 ;

	return  1 ;
}

int
ml_vt100_cmd_set_mouse_pos_sending(
	ml_term_screen_t *  termscr
	)
{
	termscr->is_mouse_pos_sending = 1 ;

	return  1 ;
}

int
ml_vt100_cmd_unset_mouse_pos_sending(
	ml_term_screen_t *  termscr
	)
{
	termscr->is_mouse_pos_sending = 0 ;

	return  1 ;
}

int
ml_vt100_cmd_use_normal_image(
	ml_term_screen_t *  termscr
	)
{
	ml_window_clear_all( &termscr->window) ;

	ml_image_set_modified_all( &termscr->normal_image) ;
	
	termscr->image = &termscr->normal_image ;

	ml_bs_change_image( &termscr->bs_image , termscr->image) ;

	if( termscr->logvis)
	{
		(*termscr->logvis->change_image)( termscr->logvis , termscr->image) ;
	}

	return  1 ;
}

int
ml_vt100_cmd_use_alternative_image(
	ml_term_screen_t *  termscr
	)
{
	termscr->image = &termscr->alt_image ;

	ml_vt100_cmd_goto_home( termscr) ;
	ml_vt100_cmd_clear_below( termscr) ;

	ml_bs_change_image( &termscr->bs_image , termscr->image) ;

	if( termscr->logvis)
	{
		(*termscr->logvis->change_image)( termscr->logvis , termscr->image) ;
	}
	
	ml_window_clear_all( &termscr->window) ;
	
	return  1 ;
}

int
ml_vt100_cmd_cursor_visible(
	ml_term_screen_t *  termscr
	)
{
	termscr->is_cursor_visible = 1 ;

	return  1 ;
}

int
ml_vt100_cmd_cursor_invisible(
	ml_term_screen_t *  termscr
	)
{
	termscr->is_cursor_visible = 0 ;

	return  1 ;
}

int
ml_vt100_cmd_send_device_attr(
	ml_term_screen_t *  termscr
	)
{
	/* vt100 answerback */
	char  seq[] = "\x1b[?1;2c" ;

	ml_write_to_pty( termscr->pty , seq , strlen( seq)) ;

	return  1 ;
}

int
ml_vt100_cmd_report_device_status(
	ml_term_screen_t *  termscr
	)
{
	ml_write_to_pty( termscr->pty , "\x1b[0n" , 4) ;

	return  1 ;
}

int
ml_vt100_cmd_report_cursor_position(
	ml_term_screen_t *  termscr
	)
{
	char  seq[4 + DIGIT_STR_LEN(u_int) + 1] ;

	sprintf( seq , "\x1b[%d;%dR" ,
		ml_cursor_row( termscr->image) + 1 , ml_cursor_col( termscr->image) + 1) ;

	ml_write_to_pty( termscr->pty , seq , strlen( seq)) ;

	return  1 ;
}

int
ml_vt100_cmd_set_window_name(
	ml_term_screen_t *  termscr ,
	u_char *  name
	)
{
	return  ml_set_window_name( &termscr->window , name) ;
}

int
ml_vt100_cmd_set_icon_name(
	ml_term_screen_t *  termscr ,
	u_char *  name
	)
{
	return  ml_set_icon_name( &termscr->window , name) ;
}

int
ml_vt100_cmd_fill_all_with_e(
	ml_term_screen_t *  termscr
	)
{
	ml_char_t  e_ch ;

	ml_char_init( &e_ch) ;
	ml_char_set( &e_ch , "E" , 1 , ml_get_usascii_font( termscr->font_man) ,
		0 , MLC_FG_COLOR , MLC_BG_COLOR) ;

	ml_image_fill_all( termscr->image , &e_ch) ;

	ml_char_final( &e_ch) ;

	return  1 ;
}

int
ml_vt100_cmd_set_config(
	ml_term_screen_t *  termscr ,
	char *  key ,
	char *  value
	)
{
#ifdef  __DEBUG
	kik_debug_printf( KIK_DEBUG_TAG " %s=%s\n" , key , value) ;
#endif

	/*
	 * ml_term_screen_{start|stop}_vt100_cmd are necessary since
	 * window is redrawn in chagne_wall_picture().
	 */

	ml_term_screen_stop_vt100_cmd( termscr) ;

	if( strcmp( key , "encoding") == 0)
	{
		ml_char_encoding_t  encoding ;

		if( ( encoding = ml_get_char_encoding( value)) == ML_UNKNOWN_ENCODING)
		{
			return  0 ;
		}

		if( termscr->config_menu_listener.change_char_encoding)
		{
			(*termscr->config_menu_listener.change_char_encoding)( termscr , encoding) ;
		}
	}
	else if( strcmp( key , "fg_color") == 0)
	{
		ml_color_t  color ;

		if( ( color = ml_get_color( value)) == MLC_UNKNOWN_COLOR)
		{
			return  0 ;
		}

		if( termscr->config_menu_listener.change_fg_color)
		{
			(*termscr->config_menu_listener.change_fg_color)( termscr , color) ;
		}
	}
	else if( strcmp( key , "bg_color") == 0)
	{
		ml_color_t  color ;

		if( ( color = ml_get_color( value)) == MLC_UNKNOWN_COLOR)
		{
			return  0 ;
		}

		if( termscr->config_menu_listener.change_bg_color)
		{
			(*termscr->config_menu_listener.change_bg_color)( termscr , color) ;
		}
	}
	else if( strcmp( key , "tabsize") == 0)
	{
		u_int  tab_size ;

		if( ! kik_str_to_uint( &tab_size , value))
		{
			return  0 ;
		}

		if( termscr->config_menu_listener.change_tab_size)
		{
			(*termscr->config_menu_listener.change_tab_size)( termscr , tab_size) ;
		}
	}
	else if( strcmp( key , "logsize") == 0)
	{
		u_int  log_size ;

		if( ! kik_str_to_uint( &log_size , value))
		{
			return  0 ;
		}

		if( termscr->config_menu_listener.change_log_size)
		{
			(*termscr->config_menu_listener.change_log_size)( termscr , log_size) ;
		}
	}
	else if( strcmp( key , "fontsize") == 0)
	{
		u_int  font_size ;

		if( strcmp( value , "larger") == 0)
		{
			if( termscr->config_menu_listener.larger_font_size)
			{
				(*termscr->config_menu_listener.larger_font_size)( termscr) ;
			}
		}
		else if( strcmp( value , "smaller") == 0)
		{
			if( termscr->config_menu_listener.smaller_font_size)
			{
				(*termscr->config_menu_listener.smaller_font_size)( termscr) ;
			}
		}
		else
		{
			if( ! kik_str_to_uint( &font_size , value))
			{
				return  0 ;
			}

			if( termscr->config_menu_listener.change_font_size)
			{
				(*termscr->config_menu_listener.change_font_size)( termscr , font_size) ;
			}
		}
	}
	else if( strcmp( key , "line_space") == 0)
	{
		u_int  line_space ;

		if( ! kik_str_to_uint( &line_space , value))
		{
			return  0 ;
		}

		if( termscr->config_menu_listener.change_line_space)
		{
			(*termscr->config_menu_listener.change_line_space)( termscr , line_space) ;
		}
	}
	else if( strcmp( key , "screen_width_ratio") == 0)
	{
		u_int  ratio ;

		if( ! kik_str_to_uint( &ratio , value))
		{
			return  0 ;
		}

		if( termscr->config_menu_listener.change_screen_width_ratio)
		{
			(*termscr->config_menu_listener.change_screen_width_ratio)( termscr , ratio) ;
		}
	}
	else if( strcmp( key , "screen_height_ratio") == 0)
	{
		u_int  ratio ;

		if( ! kik_str_to_uint( &ratio , value))
		{
			return  0 ;
		}

		if( termscr->config_menu_listener.change_screen_height_ratio)
		{
			(*termscr->config_menu_listener.change_screen_height_ratio)( termscr , ratio) ;
		}
	}
	else if( strcmp( key , "mod_meta_mode") == 0)
	{
		if( termscr->config_menu_listener.change_mod_meta_mode)
		{
			(*termscr->config_menu_listener.change_mod_meta_mode)(
				termscr , ml_get_mod_meta_mode( value)) ;
		}
	}
	else if( strcmp( key , "bel_mode") == 0)
	{
		if( termscr->config_menu_listener.change_bel_mode)
		{
			(*termscr->config_menu_listener.change_bel_mode)(
				termscr , ml_get_bel_mode( value)) ;
		}
	}
	else if( strcmp( key , "vertical_mode") == 0)
	{
		if( termscr->config_menu_listener.change_vertical_mode)
		{
			(*termscr->config_menu_listener.change_vertical_mode)(
				termscr , ml_get_vertical_mode( value)) ;
		}
	}
	else if( strcmp( key , "use_combining") == 0)
	{
		int  flag ;
		
		if( strcmp( value , "true") == 0)
		{
			flag = 1 ;
		}
		else if( strcmp( value , "false") == 0)
		{
			flag = 0 ;
		}
		else
		{
			return  0 ;
		}
		
		if( termscr->config_menu_listener.change_char_combining_flag)
		{
			(*termscr->config_menu_listener.change_char_combining_flag)( termscr , flag) ;
		}
	}
	else if( strcmp( key , "copy_paste_via_ucs") == 0)
	{
		int  flag ;
		
		if( strcmp( value , "true") == 0)
		{
			flag = 1 ;
		}
		else if( strcmp( value , "false") == 0)
		{
			flag = 0 ;
		}
		else
		{
			return  0 ;
		}
		
		if( termscr->config_menu_listener.change_copy_paste_via_ucs_flag)
		{
			(*termscr->config_menu_listener.change_copy_paste_via_ucs_flag)( termscr , flag) ;
		}
	}
	else if( strcmp( key , "use_transbg") == 0)
	{
		int  flag ;
		
		if( strcmp( value , "true") == 0)
		{
			flag = 1 ;
		}
		else if( strcmp( value , "false") == 0)
		{
			flag = 0 ;
		}
		else
		{
			return  0 ;
		}
		
		if( termscr->config_menu_listener.change_transparent_flag)
		{
			(*termscr->config_menu_listener.change_transparent_flag)( termscr , flag) ;
		}
	}
	else if( strcmp( key , "brightness") == 0)
	{
		u_int  brightness ;

		if( ! kik_str_to_uint( &brightness , value))
		{
			return  0 ;
		}

		if( termscr->config_menu_listener.change_brightness)
		{
			(*termscr->config_menu_listener.change_brightness)( termscr , brightness) ;
		}
	}
	else if( strcmp( key , "fade_ratio") == 0)
	{
		u_int  fade_ratio ;

		if( ! kik_str_to_uint( &fade_ratio , value))
		{
			return  0 ;
		}

		if( termscr->config_menu_listener.change_fade_ratio)
		{
			(*termscr->config_menu_listener.change_fade_ratio)( termscr , fade_ratio) ;
		}
	}
	else if( strcmp( key , "use_anti_alias") == 0)
	{
		ml_font_present_t  font_present ;

		font_present = termscr->font_present ;
		
		if( strcmp( value , "true") == 0)
		{
			font_present |= FONT_AA ;
		}
		else if( strcmp( value , "false") == 0)
		{
			font_present &= ~FONT_AA ;
		}
		else
		{
			return  0 ;
		}
		
		if( termscr->config_menu_listener.change_font_present)
		{
			(*termscr->config_menu_listener.change_font_present)( termscr , font_present) ;
		}
	}
	else if( strcmp( key , "use_variable_column_width") == 0)
	{
		ml_font_present_t  font_present ;

		font_present = termscr->font_present ;
		
		if( strcmp( value , "true") == 0)
		{
			font_present |= FONT_VAR_WIDTH ;
		}
		else if( strcmp( value , "false") == 0)
		{
			font_present &= ~FONT_VAR_WIDTH ;
		}
		else
		{
			return  0 ;
		}
		
		if( termscr->config_menu_listener.change_font_present)
		{
			(*termscr->config_menu_listener.change_font_present)( termscr , font_present) ;
		}
	}
	else if( strcmp( key , "use_bidi") == 0)
	{
		int  flag ;
		
		if( strcmp( value , "true") == 0)
		{
			flag = 1 ;
		}
		else if( strcmp( value , "false") == 0)
		{
			flag = 0 ;
		}
		else
		{
			return  0 ;
		}
		
		if( termscr->config_menu_listener.change_bidi_flag)
		{
			(*termscr->config_menu_listener.change_bidi_flag)( termscr , flag) ;
		}
	}
	else if( strcmp( key , "xim") == 0)
	{
		char *  xim ;
		char *  locale ;
		char *  p ;

		xim = value ;

		if( ( p = strchr( value , ':')) == NULL)
		{
			locale = "" ;
		}
		else
		{
			*p = '\0' ;
			locale = p + 1 ;
		}

		if( termscr->config_menu_listener.change_xim)
		{
			(*termscr->config_menu_listener.change_xim)( termscr , xim , locale) ;
		}
	}
	else if( strcmp( key , "wall_picture") == 0)
	{
		if( termscr->config_menu_listener.change_wall_picture)
		{
			(*termscr->config_menu_listener.change_wall_picture)( termscr , value) ;
		}
	}
	else if( strcmp( key , "full_reset") == 0)
	{
		if( termscr->config_menu_listener.full_reset)
		{
			(*termscr->config_menu_listener.full_reset)( termscr) ;
		}
	}
	
	ml_term_screen_start_vt100_cmd( termscr) ;

	return  1 ;
}

int
ml_vt100_cmd_get_config(
	ml_term_screen_t *  termscr ,
	char *  key
	)
{
	char *  value ;
	char  digit[DIGIT_STR_LEN(u_int) + 1] ;
	char *  true = "true" ;
	char *  false = "false" ;
	char  cwd[PATH_MAX] ;
		
	if( strcmp( key , "encoding") == 0)
	{
		value = ml_get_char_encoding_name( (*termscr->encoding_listener->encoding)(
				termscr->encoding_listener->self)) ;
	}
	else if( strcmp( key , "fg_color") == 0)
	{
		value = ml_get_color_name( ml_window_get_fg_color( &termscr->window)) ;
	}
	else if( strcmp( key , "bg_color") == 0)
	{
		value = ml_get_color_name( ml_window_get_bg_color( &termscr->window)) ;
	}
	else if( strcmp( key , "tabsize") == 0)
	{
		sprintf( digit , "%d" , termscr->image->tab_size) ;
		value = digit ;
	}
	else if( strcmp( key , "logsize") == 0)
	{
		sprintf( digit , "%d" , ml_get_log_size( &termscr->logs)) ;
		value = digit ;
	}
	else if( strcmp( key , "fontsize") == 0)
	{
		sprintf( digit , "%d" , termscr->font_man->font_size) ;
		value = digit ;
	}
	else if( strcmp( key , "line_space") == 0)
	{
		sprintf( digit , "%d" , termscr->font_man->line_space) ;
		value = digit ;
	}
	else if( strcmp( key , "screen_width_ratio") == 0)
	{
		sprintf( digit , "%d" , termscr->screen_width_ratio) ;
		value = digit ;
	}
	else if( strcmp( key , "screen_height_ratio") == 0)
	{
		sprintf( digit , "%d" , termscr->screen_height_ratio) ;
		value = digit ;
	}
	else if( strcmp( key , "mod_meta_mode") == 0)
	{
		value = ml_get_mod_meta_mode_name( termscr->mod_meta_mode) ;
	}
	else if( strcmp( key , "bel_mode") == 0)
	{
		value = ml_get_bel_mode_name( termscr->bel_mode) ;
	}
	else if( strcmp( key , "use_combining") == 0)
	{
		if( ml_is_char_combining())
		{
			value = true ;
		}
		else
		{
			value = false ;
		}
	}
	else if( strcmp( key , "copy_paste_via_ucs") == 0)
	{
		if( termscr->copy_paste_via_ucs)
		{
			value = true ;
		}
		else
		{
			value = false ;
		}
	}
	else if( strcmp( key , "use_transbg") == 0)
	{
		if( termscr->window.is_transparent)
		{
			value = true ;
		}
		else
		{
			value = false ;
		}
	}
	else if( strcmp( key , "brightness") == 0)
	{
		sprintf( digit , "%d" , termscr->brightness) ;
		value = digit ;
	}
	else if( strcmp( key , "fade_ratio") == 0)
	{
		sprintf( digit , "%d" , termscr->fade_ratio) ;
		value = digit ;
	}
	else if( strcmp( key , "use_anti_alias") == 0)
	{
		if( termscr->font_present & FONT_AA)
		{
			value = true ;
		}
		else
		{
			value = false ;
		}
	}
	else if( strcmp( key , "use_variable_column_width") == 0)
	{
		if( termscr->font_present & FONT_VAR_WIDTH)
		{
			value = true ;
		}
		else
		{
			value = false ;
		}
	}
	else if( strcmp( key , "use_bidi") == 0)
	{
		if( termscr->use_bidi)
		{
			value = true ;
		}
		else
		{
			value = false ;
		}
	}
	else if( strcmp( key , "xim") == 0)
	{
		value = ml_xic_get_xim_name( &termscr->window) ;
	}
	else if( strcmp( key , "locale") == 0)
	{
		value = kik_get_locale() ;
	}
	else if( strcmp( key , "wall_picture") == 0)
	{
		if( termscr->pic_file_path)
		{
			value = termscr->pic_file_path ;
		}
		else
		{
			value = "" ;
		}
	}
	else if( strcmp( key , "pwd") == 0)
	{
		value = getcwd( cwd , PATH_MAX) ;
	}
	else
	{
		goto  error ;
	}

	if( value == NULL)
	{
		goto  error ;
	}

	ml_write_to_pty( termscr->pty , "#" , 1) ;
	ml_write_to_pty( termscr->pty , key , strlen( key)) ;
	ml_write_to_pty( termscr->pty , "=" , 1) ;
	ml_write_to_pty( termscr->pty , value , strlen( value)) ;
	ml_write_to_pty( termscr->pty , "\n" , 1) ;

#ifdef  __DEBUG
	kik_debug_printf( KIK_DEBUG_TAG " #%s=%s\n" , key , value) ;
#endif

	return  1 ;

error:
	ml_write_to_pty( termscr->pty , "#error\n" , 7) ;

#ifdef  __DEBUG
	kik_debug_printf( KIK_DEBUG_TAG " #error\n") ;
#endif

	return  0 ;
}
