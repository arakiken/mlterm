/*
 *	$Id$
 */

#ifndef  __ML_IMAGE_H__
#define  __ML_IMAGE_H__


#include  <kiklib/kik_types.h>

#include  "ml_char.h"
#include  "ml_line_hints.h"
#include  "ml_image_line.h"
#include  "ml_image_model.h"


typedef struct  ml_cursor
{
	int  row ;
	int  char_index ;
	int  col ;
	
	u_long  fg_color ;
	u_long  bg_color ;

	u_long  orig_fg ;
	u_long  orig_bg ;
	
	int  is_highlighted ;
	
} ml_cursor_t ;

typedef struct ml_image_scroll_event_listener
{
	void *  self ;

	void (*receive_upward_scrolled_out_line)( void * , ml_image_line_t *) ;
	void (*receive_downward_scrolled_out_line)( void * , ml_image_line_t *) ;
	
	int  (*window_scroll_upward_region)( void * , int , int , u_int) ;
	int  (*window_scroll_downward_region)( void * , int , int , u_int) ;

} ml_image_scroll_event_listener_t ;

typedef struct  ml_image
{
	ml_image_model_t  model ;

	/* used for rendering image */
	ml_line_hints_t  line_hints ;

	/* public */
	ml_cursor_t  cursor ;

	ml_cursor_t  saved_cursor ;
	int8_t  cursor_is_saved ;

	int8_t  is_logging ;

	u_int8_t *  tab_stops ;
	u_int  tab_size ;

	ml_char_t  sp_ch ;

	/* used for line overlapping */
	ml_image_line_t *  wraparound_ready_line ;
	ml_char_t  prev_recv_ch ;	/* a char which was inserted or overwritten an opearation before */
	
	int  scroll_region_beg ;
	int  scroll_region_end ;

	ml_image_scroll_event_listener_t *  scroll_listener ;

} ml_image_t ;


int  ml_image_init( ml_image_t *  image , ml_image_scroll_event_listener_t *  scroll_listener ,
	u_int  num_of_cols , u_int  num_of_rows , ml_char_t  sp_ch , u_int  tab_size , int  is_logging) ;

int  ml_image_final( ml_image_t *  image) ;

int  ml_image_resize( ml_image_t *  image , u_int  num_of_cols , u_int  num_of_rows) ;

int  ml_image_insert_chars( ml_image_t *  image , ml_char_t *  chars , u_int  num_of_chars) ;

int  ml_image_insert_blank_chars( ml_image_t *  image , u_int  num_of_blank_chars) ;

int  ml_image_overwrite_chars( ml_image_t *  image , ml_char_t *  chars , u_int  num_of_chars) ;

int  ml_image_delete_cols( ml_image_t *  image , u_int  delete_len) ;

int  ml_image_insert_new_line( ml_image_t *  image) ;

int  ml_image_delete_line( ml_image_t *  image) ;

int  ml_image_clear_line_to_right( ml_image_t *  image) ;

int  ml_image_clear_line_to_left( ml_image_t *  image) ;

int  ml_image_clear_below( ml_image_t *  image) ;

int  ml_image_clear_above( ml_image_t *  image) ;

int  ml_image_set_scroll_region( ml_image_t *  image , int  beg , int  end) ;

int  ml_image_scroll_upward( ml_image_t *  image , u_int  size) ;

int  ml_image_scroll_downward( ml_image_t *  image , u_int  size) ;

int  ml_image_vertical_tab( ml_image_t *  image) ;

int  ml_image_set_tab_size( ml_image_t *  image , u_int  tab_size) ;

int  ml_image_set_tab_stop( ml_image_t *  image) ;

int  ml_image_clear_tab_stop( ml_image_t *  image) ;

int  ml_image_clear_all_tab_stops( ml_image_t *  image) ;

ml_image_line_t *  ml_image_get_line( ml_image_t *  image , int  row) ;

int  ml_cursor_go_forward( ml_image_t *  image , int  flag) ;

int  ml_cursor_go_back( ml_image_t *  image , int  flag) ;

int  ml_cursor_go_upward( ml_image_t *  image , int  flag) ;

int  ml_cursor_go_downward( ml_image_t *  image , int  flag) ;

int  ml_cursor_goto_beg_of_line( ml_image_t *  image) ;

int  ml_cursor_goto_home( ml_image_t *  image) ;

int  ml_cursor_goto_end( ml_image_t *  image) ;

int  ml_cursor_goto( ml_image_t *  image , int  col , int  row , int  flag) ;

int  ml_cursor_is_beg_of_line( ml_image_t *  image) ;

int  ml_cursor_set_color( ml_image_t *  image , ml_color_t  fg_color , ml_color_t  bg_color) ;

int  ml_cursor_save( ml_image_t *  image) ;

int  ml_cursor_restore( ml_image_t *  image) ;

int  ml_highlight_cursor( ml_image_t *  image) ;

int  ml_unhighlight_cursor( ml_image_t *  image) ;

int  ml_cursor_char_index( ml_image_t *  image) ;

int  ml_cursor_col( ml_image_t *  image) ;

int  ml_cursor_row( ml_image_t *  image) ;

void  ml_image_set_modified_all( ml_image_t *  image) ;

u_int ml_image_get_cols( ml_image_t *  image) ;

u_int ml_image_get_rows( ml_image_t *  image) ;

int  ml_image_fill_all( ml_image_t *  image , ml_char_t *  ch) ;
	
#ifdef  DEBUG

void  ml_image_dump( ml_image_t *  image) ;

void  ml_image_dump_updated( ml_image_t *  image) ;

#endif


#endif
