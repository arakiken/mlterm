/*
 *	$Id$
 */

#ifndef  __ML_SCREEN_H__
#define  __ML_SCREEN_H__


#include  <time.h>
#include  <kiklib/kik_types.h>		/* int8_t */

#include  "ml_edit.h"
#include  "ml_logs.h"
#include  "ml_logical_visual.h"


typedef struct  ml_screen_event_listener
{
	void *  self ;
	
	int  (*screen_is_static)( void *) ;
	void  (*line_scrolled_out)( void *) ;
	int  (*window_scroll_upward_region)( void * , int , int , u_int) ;
	int  (*window_scroll_downward_region)( void * , int , int , u_int) ;

}  ml_screen_event_listener_t ;

typedef enum  ml_bs_mode
{
	BSM_NONE = 0x0 ,
	BSM_DEFAULT ,
	BSM_STATIC ,

	BSM_MAX
	
} ml_bs_mode_t ;

typedef struct  ml_stored_edits
{
	ml_edit_t  normal_edit ;
	ml_edit_t  alt_edit ;
	clock_t  time ;

} ml_stored_edits_t ;

typedef struct  ml_screen
{
	/* public(readonly) */
	ml_edit_t *  edit ;

	/*
	 * private
	 */

	ml_edit_t  normal_edit ;
	ml_edit_t  alt_edit ;
	ml_stored_edits_t *  stored_edits ;	/* Store logical edits. */

	ml_edit_scroll_event_listener_t  edit_scroll_listener ;
	
	ml_logs_t  logs ;

	ml_logical_visual_t *  logvis ;
	ml_logical_visual_t *  container_logvis ;

	ml_screen_event_listener_t *  screen_listener ;

	struct
	{
		int (*match)( size_t * , size_t * , void * , u_char * , int) ;

		/* Logical order */
		int  char_index ;
		int  row ;

	} *  search ;

	u_int  backscroll_rows ;
	ml_bs_mode_t  backscroll_mode ;
	int8_t  is_backscrolling ;

	int8_t  use_dynamic_comb ;	/* public */
	int8_t  is_cursor_visible ;

} ml_screen_t ;


int  ml_set_word_separators( char *  seps) ;

#define  ml_free_word_separators()  ml_set_word_separators(NULL)


ml_screen_t *  ml_screen_new( u_int  cols , u_int  rows , u_int  tab_size ,
	u_int  num_of_log_lines , int  use_bce , ml_bs_mode_t  bs_mode) ;

int  ml_screen_delete( ml_screen_t *  screen) ;

int  ml_screen_set_listener( ml_screen_t *  screen , ml_screen_event_listener_t *  screen_listener) ;

int  ml_screen_resize( ml_screen_t *  screen , u_int  cols , u_int  rows) ;

#define  ml_screen_set_bce_fg_color( screen , fg_color) \
		ml_edit_set_bce_fg_color( (screen)->edit , fg_color)

#define  ml_screen_set_bce_bg_color( screen , bg_color) \
		ml_edit_set_bce_bg_color( (screen)->edit , bg_color)

#define  ml_screen_cursor_char_index( screen)  ml_cursor_char_index( (screen)->edit)

#define  ml_screen_cursor_col( screen)  ml_cursor_col( (screen)->edit)

#define  ml_screen_cursor_row( screen)  ml_cursor_row( (screen)->edit)

#define  ml_screen_cursor_relative_col( screen)  ml_cursor_relative_col( (screen)->edit)

#define  ml_screen_cursor_relative_row( screen)  ml_cursor_relative_row( (screen)->edit)

int  ml_screen_cursor_row_in_screen( ml_screen_t *  screen) ;

#define  ml_screen_get_cols( screen)  ml_edit_get_cols( (screen)->edit)

#define  ml_screen_get_rows( screen)  ml_edit_get_rows( (screen)->edit)

u_int  ml_screen_get_logical_cols( ml_screen_t *  screen) ;

u_int  ml_screen_get_logical_rows( ml_screen_t *  screen) ;

#define  ml_screen_get_log_size( screen)  ml_get_log_size( &(screen)->logs)

#define  ml_screen_change_log_size( screen , log_size) \
		ml_change_log_size( &(screen)->logs , log_size)

#define  ml_screen_unlimit_log_size( screen)  ml_unlimit_log_size( &(screen)->logs)

#define  ml_screen_log_size_is_unlimited( screen)  ml_log_size_is_unlimited( &(screen)->logs)

#define  ml_screen_get_num_of_logged_lines( screen) \
		ml_get_num_of_logged_lines( &(screen)->logs)

int  ml_screen_convert_scr_row_to_abs( ml_screen_t *  screen , int  row) ;

ml_line_t *  ml_screen_get_line( ml_screen_t *  screen , int  row) ;

ml_line_t *  ml_screen_get_line_in_screen( ml_screen_t *  screen , int  row) ;

ml_line_t *  ml_screen_get_cursor_line( ml_screen_t *  screen) ;

int  ml_screen_set_modified_all( ml_screen_t *  screen) ;

int  ml_screen_add_logical_visual( ml_screen_t *  screen , ml_logical_visual_t *  logvis) ;

int  ml_screen_delete_logical_visual( ml_screen_t *  screen) ;

int  ml_screen_render( ml_screen_t *  screen) ;

int  ml_screen_visual( ml_screen_t *  screen) ;

int  ml_screen_logical( ml_screen_t *  screen) ;

#define  ml_screen_logical_visual_is_reversible( screen) \
		(! (screen)->logvis || (screen)->logvis->is_reversible)

ml_bs_mode_t  ml_screen_is_backscrolling( ml_screen_t *  screen) ;

int  ml_set_backscroll_mode( ml_screen_t *  screen , ml_bs_mode_t  mode) ;

int  ml_enter_backscroll_mode( ml_screen_t *  screen) ;

int  ml_exit_backscroll_mode( ml_screen_t *  screen) ;

int  ml_screen_backscroll_to( ml_screen_t *  screen , int  row) ;

int  ml_screen_backscroll_upward( ml_screen_t *  screen , u_int  size) ;

int  ml_screen_backscroll_downward( ml_screen_t *  screen , u_int  size) ;

#define  ml_screen_get_tab_size( screen)  ml_edit_get_tab_size( (screen)->edit)

#define  ml_screen_set_tab_size( screen , tab_size) \
		ml_edit_set_tab_size( (screen)->edit , tab_size)

int  ml_screen_restore_color( ml_screen_t *  screen , int  beg_char_index , int  beg_row ,
	int  end_char_index , int  end_row) ;

int  ml_screen_reverse_color( ml_screen_t *  screen , int  beg_char_index , int  beg_row ,
	int  end_char_index , int  end_row) ;

u_int  ml_screen_copy_region( ml_screen_t *  screen , ml_char_t *  chars ,
	u_int  num_of_chars , int  beg_char_index , int  beg_row , int  end_char_index , int  end_row) ;

u_int  ml_screen_get_region_size( ml_screen_t *  screen , int  beg_char_index ,
	int  beg_row , int  end_char_index , int  end_row) ;

int  ml_screen_get_line_region( ml_screen_t *  screen , int *  beg_row ,
	int *  end_char_index , int *  end_row , int  base_row) ;

int  ml_screen_get_word_region( ml_screen_t *  screen , int *  beg_char_index ,
	int *  beg_row , int *  end_char_index , int *  end_row , int  base_char_index , int  base_row) ;

int  ml_screen_search_init( ml_screen_t *  screen ,
	int (*match)( size_t * , size_t * , void * , u_char * , int)) ;

int  ml_screen_search_final( ml_screen_t *  screen) ;

int  ml_screen_search_reset_position( ml_screen_t *  screen) ;

int  ml_screen_search_find( ml_screen_t *  screen , int *  beg_char_index , int *  beg_row ,
	int *  end_char_index , int *  end_row , void *  regex , int  backward) ;


/*
 * VT100 commands (called in logical context)
 */
 
ml_char_t *  ml_screen_get_n_prev_char( ml_screen_t *  screen , int  n) ;

int  ml_screen_combine_with_prev_char( ml_screen_t *  screen , u_int32_t  code ,
	mkf_charset_t  cs , int  is_fullwidth , int  is_comb ,
	ml_color_t  fg_color , ml_color_t  bg_color , int  is_bold ,
	int  is_italic , int  is_underlined , int  is_crossed_out) ;

int  ml_screen_insert_chars( ml_screen_t *  screen , ml_char_t *  chars , u_int  len) ;

#define  ml_screen_insert_blank_chars( screen , len) \
		ml_edit_insert_blank_chars( (screen)->edit , len)

#define  ml_screen_vertical_forward_tabs( screen , num) \
		ml_edit_vertical_forward_tabs( (screen)->edit , num) \

#define  ml_screen_vertical_backward_tabs( screen , num) \
		ml_edit_vertical_backward_tabs( (screen)->edit , num) \

#define  ml_screen_set_tab_stop( screen)  ml_edit_set_tab_stop( (screen)->edit)

#define  ml_screen_clear_tab_stop( screen)  ml_edit_clear_tab_stop( (screen)->edit)

#define  ml_screen_clear_all_tab_stops( screen)  ml_edit_clear_all_tab_stops( (screen)->edit)

int  ml_screen_insert_new_lines( ml_screen_t *  screen , u_int  size) ;

#define  ml_screen_line_feed( screen)  ml_edit_go_downward( (screen)->edit , SCROLL)

int  ml_screen_overwrite_chars( ml_screen_t *  screen , ml_char_t *  chars , u_int  len) ;

#define  ml_screen_delete_cols( screen , len)  ml_edit_delete_cols( (screen)->edit , len)

int  ml_screen_delete_lines( ml_screen_t *  screen , u_int  size) ;

#define  ml_screen_clear_cols( screen , cols)  ml_edit_clear_cols( (screen)->edit , cols)

#define  ml_screen_clear_line_to_right( screen)  ml_edit_clear_line_to_right( (screen)->edit)

#define  ml_screen_clear_line_to_left( screen)  ml_edit_clear_line_to_left( (screen)->edit)

#define  ml_screen_clear_below( screen)  ml_edit_clear_below( (screen)->edit)

#define  ml_screen_clear_above( screen)  ml_edit_clear_above( (screen)->edit)

#define  ml_screen_set_scroll_region( screen , beg , end) \
		ml_edit_set_scroll_region( (screen)->edit , beg , end)

#define  ml_screen_set_use_margin( screen , use) \
		ml_edit_set_use_margin( (screen)->edit , use)

#define  ml_screen_set_margin( screen , beg , end) \
		ml_edit_set_margin( (screen)->edit , beg , end)

#define  ml_screen_index( screen)  ml_edit_go_downward( (screen)->edit , SCROLL)

#define  ml_screen_reverse_index( screen)  ml_edit_go_upward( (screen)->edit , SCROLL)

#define  ml_screen_scroll_upward( screen , size)  ml_edit_scroll_upward( (screen)->edit , size)

#define  ml_screen_scroll_downward( screen , size)  ml_edit_scroll_downward( (screen)->edit , size)

int  ml_screen_go_forward( ml_screen_t *  screen , u_int  size) ;

int  ml_screen_go_back( ml_screen_t *  screen , u_int  size) ;

int  ml_screen_go_upward( ml_screen_t *  screen , u_int  size) ;

int  ml_screen_go_downward( ml_screen_t *  screen , u_int  size) ;

#define  ml_screen_goto_beg_of_line( screen)  ml_edit_goto_beg_of_line( (screen)->edit)

int  ml_screen_go_horizontally( ml_screen_t *  screen , int  col) ;

int  ml_screen_go_vertically( ml_screen_t *  screen , int  row) ;

#define  ml_screen_goto_home( screen)  ml_edit_goto_home( (screen)->edit)

#define  ml_screen_goto( screen , col , row)  ml_edit_goto( (screen)->edit , col , row)

#define  ml_screen_set_relative_origin( screen)  ml_edit_set_relative_origin( (screen)->edit)

#define  ml_screen_set_absolute_origin( screen)  ml_edit_set_absolute_origin( (screen)->edit)

#define  ml_screen_set_auto_wrap( screen , flag)  ml_edit_set_auto_wrap( (screen)->edit , flag)

#define  ml_screen_save_cursor( screen)  ml_edit_save_cursor( (screen)->edit)

#define  ml_screen_restore_cursor( screen)  ml_edit_restore_cursor( (screen)->edit)

int  ml_screen_cursor_visible( ml_screen_t *  screen) ;

int  ml_screen_cursor_invisible( ml_screen_t *  screen) ;

#define  ml_screen_is_cursor_visible( screen)  ((screen)->is_cursor_visible)

int  ml_screen_use_normal_edit( ml_screen_t *  screen) ;

int  ml_screen_use_alternative_edit( ml_screen_t *  screen) ;

int  ml_screen_is_alternative_edit( ml_screen_t *  screen) ;

int  ml_screen_enable_local_echo( ml_screen_t *  screen) ;

int  ml_screen_local_echo_wait( ml_screen_t *  screen , int  msec) ;

int  ml_screen_disable_local_echo( ml_screen_t *  screen) ;

int  ml_screen_fill_area( ml_screen_t *  screen , int  code ,
	int  col , int  beg , u_int  num_of_cols , u_int  num_of_rows) ;

#define  ml_screen_copy_area( screen , src_col , src_row , \
		num_of_cols , num_of_rows , dst_col , dst_row) \
		ml_edit_copy_area( (screen)->edit , src_col , src_row , \
			num_of_cols , num_of_rows , dst_col , dst_row)

#define  ml_screen_erase_area( screen , col , row , num_of_cols , num_of_rows) \
		ml_edit_erase_area( (screen)->edit , col , row , num_of_cols , num_of_rows)


#endif
