/*
 *	$Id$
 */

#ifndef  __ML_BACKSCROLL_H__
#define  __ML_BACKSCROLL_H__


#include  "ml_image.h"
#include  "ml_logs.h"


typedef struct  ml_bs_event_listener
{
	void *  self ;
	
	int  (*window_scroll_upward)( void * , u_int) ;
	int  (*window_scroll_downward)( void * , u_int) ;

}  ml_bs_event_listener_t ;

typedef struct  ml_bs_image
{
	ml_image_t *  image ;
	ml_logs_t *  logs ;

	ml_bs_event_listener_t *  bs_listener ;

	u_int  backscroll_rows ;
	int  is_backscroll_mode ;

	ml_char_t  nl_ch ;
	
} ml_bs_image_t ;


int  ml_set_word_separators( char *  seps) ;

int  ml_free_word_separators(void) ;


int  ml_bs_init( ml_bs_image_t *  bs_image , ml_image_t *  image ,
	ml_logs_t *  logs , ml_bs_event_listener_t *  bs_listener , ml_char_t *  nl_ch) ;

int  ml_bs_final( ml_bs_image_t *  image) ;

int  ml_bs_change_image( ml_bs_image_t *  bs_image , ml_image_t *  image) ;

int  ml_is_backscroll_mode( ml_bs_image_t *  bs_image) ;

void  ml_set_backscroll_mode( ml_bs_image_t *  bs_image) ;

void  ml_unset_backscroll_mode( ml_bs_image_t *  bs_image) ;

int  ml_convert_row_to_bs_row( ml_bs_image_t *  bs_image , int  row) ;

int  ml_convert_row_to_scr_row( ml_bs_image_t *  bs_image , int  row) ;

ml_image_line_t *  ml_bs_get_image_line_in_all( ml_bs_image_t *  bs_image , int  row) ;

ml_image_line_t *  ml_bs_get_image_line_in_screen( ml_bs_image_t *  bs_image , int  row) ;

int  ml_bs_scroll_to( ml_bs_image_t *  bs_image , int  beg_row) ;

int  ml_bs_scroll_upward( ml_bs_image_t *  bs_image , u_int  size) ;

int  ml_bs_scroll_downward( ml_bs_image_t *  bs_image , u_int  size) ;

int  ml_bs_reverse_color( ml_bs_image_t *  bs_image , int  col , int  row) ;

int  ml_bs_restore_color( ml_bs_image_t *  bs_image , int  col , int  row) ;

u_int  ml_bs_copy_region( ml_bs_image_t *  bs_image , ml_char_t *  chars , u_int  num_of_chars ,
        int  beg_col , int  beg_row , int  end_col , int  end_row) ;

u_int  ml_bs_get_region_size( ml_bs_image_t *  bs_image , int  beg_col , int  beg_row ,
	int  end_col , int  end_row) ;

int  ml_bs_get_line_region( ml_bs_image_t *  bs_image , int *  beg_row , int *  end_char_index ,
	int *  end_row , int  base_row) ;

int  ml_bs_get_word_region( ml_bs_image_t *  bs_image , int *  beg_char_index , int *  beg_row ,
	int *  end_char_index , int *  end_row , int  base_char_index , int  base_row) ;


#endif
