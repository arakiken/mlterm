/*
 *	$Id$
 */

#include  "ml_vt100_command.h"

#include  <stdio.h>		/* sprintf */
#include  <kiklib/kik_util.h>	/* DIGIT_STR_LEN */


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
	char  seq[] = "\1b[?1;2c" ;

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
ml_vt100_cmd_change_wall_picture(
	ml_term_screen_t * termscr ,
	char * path
	)
{
	if( *path == '\0')
	{
		/* Do not change current image but alter diaplay setting */
		/* XXX nothing can be done for now */

		return 0 ;
	}

	/*
	 * XXX
	 * trigger config_menu_event_listener.
	 */
	if( termscr->config_menu_listener.change_wall_picture)
	{
		/*
		 * ml_term_screen_{start|stop}_vt100_cmd are necessary since
		 * window is redrawn in chagne_wall_picture().
		 */
		
		ml_term_screen_stop_vt100_cmd( termscr) ;
		
		(*termscr->config_menu_listener.change_wall_picture)( termscr , path) ;

		ml_term_screen_start_vt100_cmd( termscr) ;
	}

	return  1 ;
}
