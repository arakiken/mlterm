/*
 *	$Id$
 */

#ifndef  __ML_IMAGE_LINE_H__
#define  __ML_IMAGE_LINE_H__


#include  "ml_char.h"


#ifdef  DEBUG

#define  END_CHAR_INDEX( image_line) \
	( (image_line).num_of_filled_chars == 0 && \
		kik_warn_printf( "END_CHAR_INDEX()[" __FUNCTION__  "] ml_image_line_t::num_of_filled_chars is 0.\n") ? \
		0 : (image_line).num_of_filled_chars - 1 )

#else

#define  END_CHAR_INDEX( image_line)  ((image_line).num_of_filled_chars - 1)

#endif

#define  END_CHAR( image_line)  ((image_line).chars[END_CHAR_INDEX(image_line)])


/*
 * this object size should be kept as small as possible.
 * (192bit ILP32) (256bit ILP64)
 *
 * !!! Notice !!!
 * these members are initialized in ml_image_init/ml_image_resize(ml_image.c)
 * and ml_logs_init/ml_logs_add(ml_logs.c) , not only in ml_image_line.c
 */
typedef struct  ml_image_line
{
	/* public */
	ml_char_t *  chars ;

	/* private */
	u_int16_t *  visual_order ;		/* if bidi rendering isn't used , this is NULL */

	/* public */
	u_int16_t  num_of_chars ;
	u_int16_t  num_of_filled_chars ;	/* 0 - 65536 */
	u_int16_t  num_of_filled_cols ;		/* 0 - 65536 */

	/* private */
	u_int16_t  visual_order_len ;		/* 0 - 65536 */

	/* private */
	u_int16_t  change_beg_char_index ;	/* 0 - 65536 */
	u_int16_t  change_end_char_index ;	/* 0 - 65536 */

	/* public(read only) */
	int8_t  is_cleared_to_end ;
	int8_t  is_modified ;
	int8_t  is_continued_to_next ;
	int8_t  is_bidi ;

} ml_image_line_t ;


int  ml_set_word_separators( char *  seps) ;

int  ml_free_word_separators(void) ;


u_int  ml_get_cols_of( ml_char_t *  chars , u_int  len) ;


int  ml_imgline_init( ml_image_line_t *  line , u_int  num_of_chars) ;

int  ml_imgline_clone( ml_image_line_t *  clone , ml_image_line_t *  orig , u_int  num_of_chars) ;

int  ml_imgline_final( ml_image_line_t *  line) ;

int  ml_imgline_reset( ml_image_line_t *  line) ;

int  ml_imgline_clear( ml_image_line_t *  line , ml_char_t *  sp_ch) ;

void  ml_imgline_update_change_char_index( ml_image_line_t *  line ,
	int  beg_char_index , int  end_char_index , int  is_cleared_to_end) ;

void  ml_imgline_set_modified( ml_image_line_t *  line) ;

void  ml_imgline_is_updated( ml_image_line_t *  line) ;

int  ml_imgline_get_beg_of_modified( ml_image_line_t *  line) ;

u_int  ml_imgline_get_num_of_redrawn_chars( ml_image_line_t *  line) ;

int  ml_convert_char_index_to_x( ml_image_line_t *  line , int  char_index) ;

int  ml_convert_x_to_char_index( ml_image_line_t *  line , u_int *  x_rest , int  x) ;

int  ml_imgline_reverse_color( ml_image_line_t *  line , int  char_index) ;

int  ml_imgline_restore_color( ml_image_line_t *  line , int  char_index) ;

int  ml_imgline_copy_line( ml_image_line_t *  dst , ml_image_line_t *  src) ;

int  ml_imgline_share( ml_image_line_t *  dst , ml_image_line_t *  src) ;

int  ml_imgline_is_empty( ml_image_line_t *  line) ;

u_int  ml_get_num_of_filled_chars_except_end_space( ml_image_line_t *  line) ;

int  ml_imgline_get_word_pos( ml_image_line_t *  line ,
	int *  beg_char_index , int *  end_char_index , int  char_index) ;

int  ml_imgline_is_using_bidi( ml_image_line_t *  line) ;
	
int  ml_imgline_use_bidi( ml_image_line_t *  line) ;

int  ml_imgline_unuse_bidi( ml_image_line_t *  line) ;

int  ml_imgline_render_bidi( ml_image_line_t *  line) ;

int  ml_imgline_start_bidi( ml_image_line_t *  line) ;

int  ml_imgline_stop_bidi( ml_image_line_t *  line) ;

int  ml_convert_char_index_normal_to_bidi( ml_image_line_t *  line , int  char_index) ;

int  ml_convert_char_index_bidi_to_normal( ml_image_line_t *  line , int  char_index) ;

int  ml_imgline_copy_str( ml_image_line_t *  line , ml_char_t *  dst , int  beg , u_int  len) ;


#endif
