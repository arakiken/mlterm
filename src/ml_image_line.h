/*
 *	$Id$
 */

#ifndef  __ML_IMAGE_LINE_H__
#define  __ML_IMAGE_LINE_H__


#include  "ml_char.h"
#include  "ml_shaping.h"
#include  "ml_bidi.h"		/* ml_bidi_state_t */


enum
{
	WRAPAROUND   = 0x01 ,
	BREAK_BOUNDARY = 0x02 ,
	SCROLL         = 0x04 ,
} ;


/*
 * this object size should be kept as small as possible.
 * (160bit ILP32) (224bit ILP64)
 */
typedef struct  ml_image_line
{
	/* public(readonly) */
	ml_char_t *  chars ;

	/* private */
	ml_bidi_state_t *  bidi_state ;

	/* public(readonly) */
	u_int16_t  num_of_chars ;		/* 0 - 65536 */
	u_int16_t  num_of_filled_chars ;	/* 0 - 65536 */

	/* private */
	u_int16_t  change_beg_char_index ;	/* 0 - 65536 */
	u_int16_t  change_end_char_index ;	/* 0 - 65536 */

	/*
	 * private
	 *
	 * total 8 bit
	 * 5 bit : not used
	 * 1 bit : is_cleared_to_end
	 * 1 bit : is_modified
	 * 1 bit : is_continued_to_next
	 */
	u_int8_t  flag ;

} ml_image_line_t ;


int  ml_imgline_init( ml_image_line_t *  line , u_int  num_of_chars) ;

int  ml_imgline_clone( ml_image_line_t *  clone , ml_image_line_t *  orig , u_int  num_of_chars) ;

int  ml_imgline_final( ml_image_line_t *  line) ;

u_int  ml_imgline_break_boundary( ml_image_line_t *  line , u_int  size , ml_char_t *  sp_ch) ;

int  ml_imgline_reset( ml_image_line_t *  line) ;

int  ml_imgline_clear( ml_image_line_t *  line , int  char_index , ml_char_t *  sp_ch) ;

int  ml_imgline_overwrite_chars( ml_image_line_t *  line , int  change_char_index ,
	ml_char_t *  chars , u_int  len , u_int  cols , ml_char_t *  sp_ch) ;

int  ml_imgline_overwrite_all( ml_image_line_t *  line , int  change_char_index ,
	ml_char_t *  chars , int  len , u_int  cols) ;

int  ml_imgline_fill_all( ml_image_line_t *  line , ml_char_t *  ch , u_int  num) ;

void  ml_imgline_set_modified( ml_image_line_t *  line ,
	int  beg_char_index , int  end_char_index , int  is_cleared_to_end) ;

void  ml_imgline_set_modified_all( ml_image_line_t *  line) ;

int  ml_imgline_is_cleared_to_end( ml_image_line_t *  line) ;

int  ml_imgline_is_modified( ml_image_line_t *  line) ;

int  ml_imgline_get_beg_of_modified( ml_image_line_t *  line) ;

int  ml_imgline_get_end_of_modified( ml_image_line_t *  line) ;

u_int  ml_imgline_get_num_of_redrawn_chars( ml_image_line_t *  line) ;

void  ml_imgline_updated( ml_image_line_t *  line) ;

int  ml_imgline_is_continued_to_next( ml_image_line_t *  line) ;

void  ml_imgline_set_continued_to_next( ml_image_line_t *  line) ;

void  ml_imgline_unset_continued_to_next( ml_image_line_t *  line) ;

u_int  ml_imgline_get_num_of_filled_cols( ml_image_line_t *  line) ;

int  ml_convert_char_index_to_col( ml_image_line_t *  line , int  char_index , int  flag) ;

int  ml_convert_col_to_char_index( ml_image_line_t *  line , u_int *  cols_rest , int  col , int  flag) ;
	
int  ml_convert_char_index_to_x( ml_image_line_t *  line , int  char_index , ml_shape_t *  shape) ;

int  ml_convert_x_to_char_index( ml_image_line_t *  line , u_int *  x_rest , int  x , ml_shape_t *  shape) ;

int  ml_imgline_reverse_color( ml_image_line_t *  line , int  char_index) ;

int  ml_imgline_restore_color( ml_image_line_t *  line , int  char_index) ;

int  ml_imgline_copy_line( ml_image_line_t *  dst , ml_image_line_t *  src) ;

int  ml_imgline_share( ml_image_line_t *  dst , ml_image_line_t *  src) ;

int  ml_imgline_end_char_index( ml_image_line_t *  line) ;

int  ml_imgline_is_empty( ml_image_line_t *  line) ;

u_int  ml_get_num_of_filled_chars_except_end_space( ml_image_line_t *  line) ;

int  ml_imgline_get_word_pos( ml_image_line_t *  line ,
	int *  beg_char_index , int *  end_char_index , int  char_index) ;


int  ml_imgline_is_using_bidi( ml_image_line_t *  line) ;
	
int  ml_imgline_use_bidi( ml_image_line_t *  line) ;

int  ml_imgline_unuse_bidi( ml_image_line_t *  line) ;

int  ml_imgline_bidi_render( ml_image_line_t *  line , int  cursor_pos) ;

int  ml_imgline_bidi_visual( ml_image_line_t *  line) ;

int  ml_imgline_bidi_logical( ml_image_line_t *  line) ;

int  ml_bidi_convert_logical_char_index_to_visual( ml_image_line_t *  line , int  char_index ,
	int *  ltr_rtl_meet_pos) ;

int  ml_imgline_copy_str( ml_image_line_t *  line , ml_char_t *  dst , int  beg , u_int  len) ;


int  ml_imgline_iscii_visual( ml_image_line_t *  line , ml_iscii_state_t  iscii_state) ;

int  ml_iscii_convert_logical_char_index_to_visual( ml_image_line_t *  line , int  logical_char_index) ;


ml_image_line_t *  ml_imgline_shape( ml_image_line_t *  line , ml_shape_t *  shape) ;

int  ml_imgline_unshape( ml_image_line_t *  line , ml_image_line_t *  orig) ;


#endif
