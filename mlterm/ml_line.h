/*
 *	$Id$
 */

#ifndef  __ML_LINE_H__
#define  __ML_LINE_H__


#include  "ml_str.h"
#include  "ml_bidi.h"	/* ml_bidi_state_t */
#include  "ml_iscii.h"	/* ml_iscii_state_t */
#include  "ml_ot_layout.h"	/* ml_ot_layout_state_t */


enum
{
	WRAPAROUND     = 0x01 ,
	BREAK_BOUNDARY = 0x02 ,
	SCROLL         = 0x04
} ;

enum
{
	VINFO_BIDI = 0x01 ,
	VINFO_ISCII = 0x02 ,
	VINFO_OT_LAYOUT = 0x03 ,
} ;

typedef union  ctl_info
{
	ml_bidi_state_t  bidi ;
	ml_iscii_state_t  iscii ;
	ml_ot_layout_state_t  ot_layout ;

} ctl_info_t ;

/*
 * This object size should be kept as small as possible.
 * (160bit ILP32) (224bit ILP64)
 */
typedef struct  ml_line
{
	/* public(readonly) -- If you access &chars[at], use ml_char_at(). */
	ml_char_t *  chars ;

	/* public(readonly) */
	u_int16_t  num_of_chars ;		/* 0 - 65535 */
	u_int16_t  num_of_filled_chars ;	/* 0 - 65535 */

	/* private */
	/*
	 * Type of col should be int, but u_int16_t is used here to shrink memory
	 * because it is appropriate to assume that change_{beg|end}_col never
	 * becomes minus value.
	 */
	u_int16_t  change_beg_col ;		/* 0 - 65535 */
	u_int16_t  change_end_col ;		/* 0 - 65535 */

#if  ! defined(NO_DYNAMIC_LOAD_CTL) || defined(USE_IND) || defined(USE_FRIBIDI) || defined(USE_OT_LAYOUT)
	/* Don't touch from ml_line.c. ctl_info is used by ml_line_bidi.c and ml_line_iscii.c. */
	ctl_info_t  ctl_info ;
#endif
	u_int8_t  ctl_info_type ;

	int8_t  is_modified ;
	int8_t  is_continued_to_next ;

	/* public */
	int8_t  size_attr ;

} ml_line_t ;


void  ml_set_use_ot_layout( int  flag) ;

int  ml_is_using_ot_layout(void) ;

int  ml_line_init( ml_line_t *  line , u_int  num_of_chars) ;

int  ml_line_clone( ml_line_t *  clone , ml_line_t *  orig , u_int  num_of_chars) ;

int  ml_line_final( ml_line_t *  line) ;

u_int  ml_line_break_boundary( ml_line_t *  line , u_int  size) ;

int  ml_line_assure_boundary( ml_line_t *  line , int  char_index) ;

int  ml_line_reset( ml_line_t *  line) ;

int  ml_line_clear( ml_line_t *  line , int  char_index) ;

int  ml_line_clear_with( ml_line_t *  line , int  char_index , ml_char_t *  ch) ;

int  ml_line_overwrite( ml_line_t *  line , int  beg_char_index , ml_char_t *  chars ,
	u_int  len , u_int  cols) ;

#if  0
int  ml_line_overwrite_all( ml_line_t *  line , ml_char_t *  chars , int  len) ;
#endif

int  ml_line_fill( ml_line_t *  line , ml_char_t *  ch , int  beg , u_int  num) ;

ml_char_t *  ml_char_at( ml_line_t *  line , int  at) ;

int  ml_line_set_modified( ml_line_t *  line , int  beg_char_index , int  end_char_index) ;

int  ml_line_set_modified_all( ml_line_t *  line) ;

int  ml_line_is_cleared_to_end( ml_line_t *  line) ;

int  ml_line_is_modified( ml_line_t *  line) ;

/* XXX Private api for ml_line.c and ml_line_{iscii|bidi}.c */
#define  ml_line_is_real_modified( line)  (ml_line_is_modified( line) == 2)

int  ml_line_get_beg_of_modified( ml_line_t *  line) ;

int  ml_line_get_end_of_modified( ml_line_t *  line) ;

u_int  ml_line_get_num_of_redrawn_chars( ml_line_t *  line , int  to_end) ;

void  ml_line_set_updated( ml_line_t *  line) ;

int  ml_line_is_continued_to_next( ml_line_t *  line) ;

void  ml_line_set_continued_to_next( ml_line_t *  line , int  flag) ;

int  ml_convert_char_index_to_col( ml_line_t *  line , int  char_index , int  flag) ;

int  ml_convert_col_to_char_index( ml_line_t *  line , u_int *  cols_rest , int  col , int  flag) ;

int  ml_line_reverse_color( ml_line_t *  line , int  char_index) ;

int  ml_line_restore_color( ml_line_t *  line , int  char_index) ;

int  ml_line_copy( ml_line_t *  dst , ml_line_t *  src) ;

int  ml_line_swap( ml_line_t *  line1 , ml_line_t *  line2) ;

int  ml_line_share( ml_line_t *  dst , ml_line_t *  src) ;

int  ml_line_is_empty( ml_line_t *  line) ;

u_int  ml_line_get_num_of_filled_cols( ml_line_t *  line) ;

int  ml_line_end_char_index( ml_line_t *  line) ;

int  ml_line_beg_char_index_regarding_rtl( ml_line_t *  line) ;

u_int  ml_line_get_num_of_filled_chars_except_spaces_with_func(
	ml_line_t *  line , int (*func)( ml_char_t * , ml_char_t *)) ;

#define  ml_line_get_num_of_filled_chars_except_spaces(line) \
	ml_line_get_num_of_filled_chars_except_spaces_with_func( (line) , ml_char_code_equal)

void  ml_line_set_size_attr( ml_line_t *  line , int  size_attr) ;


#define  ml_line_is_using_ctl( line)  ((line)->ctl_info_type)

#define  ml_line_has_ot_substitute_glyphs( line) \
	((line)->ctl_info_type == VINFO_OT_LAYOUT && (line)->ctl_info.ot_layout->substituted)

int  ml_line_convert_visual_char_index_to_logical( ml_line_t *  line , int  char_index) ;

int  ml_line_is_rtl( ml_line_t *  line) ;

int  ml_line_copy_logical_str( ml_line_t *  line , ml_char_t *  dst , int  beg , u_int  len) ;

int  ml_line_convert_logical_char_index_to_visual( ml_line_t *  line , int  logical_char_index ,
			int *  meet_pos) ;


ml_line_t *  ml_line_shape( ml_line_t *  line) ;

int  ml_line_unshape( ml_line_t *  line , ml_line_t *  orig) ;

int  ml_line_unuse_ctl( ml_line_t *  line) ;

int  ml_line_ctl_render( ml_line_t *  line , ml_bidi_mode_t  bidi_mode ,
	const char *  separators , void *  term) ;

int  ml_line_ctl_visual( ml_line_t *  line) ;

int  ml_line_ctl_logical( ml_line_t *  line) ;

#ifdef  DEBUG

void  ml_line_dump( ml_line_t *  line) ;

#endif


#endif
