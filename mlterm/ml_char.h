/*
 *	$Id$
 */

#ifndef  __ML_CHAR_H__
#define  __ML_CHAR_H__


#include  <kiklib/kik_types.h>
#include  <kiklib/kik_mem.h>	/* alloca */
#include  <mkf/mkf_charset.h>	/* mkf_charset_t */

#include  "ml_font.h"
#include  "ml_color.h"


#define  MAX_CHAR_SIZE  4
#define  MAX_COMB_SIZE  7


/*
 * this object size should be kept as small as possible.
 * (ILP32: 64bit) (LP64: 96bit)
 *
 * [combining state]
 * ml_char_t.[all_fields except u] are invalid.
 * ml_char_t.u.multi_ch -> ml_char_t [main char]
 *                      -> ml_char_t [first combining char]
 *                      -> ml_char_t [second combining char]
 *                      .....
 */
typedef struct ml_char
{
	union
	{
		/* 32 bit */
		u_char  bytes[MAX_CHAR_SIZE] ;

		/* 32(ILP32) or 64(LP64) bit */
		struct ml_char *  multi_ch ;

	} u ;
	
	/*
	 * total 32 bit
	 * 4 bit : free
	 * 2 bit : size(0x0 - 0x3)
	 * 11 bit: charset(0x0 - 0x7ff)
	 * 1 bit : is_biwidth(0 or 1)
	 * 3 bit : comb_size(0x0 - 0x7)
	 * 1 bit : is_comb(0 or 1)
	 * 4 bit : fg_color(0x0 - 0xf)
	 * 4 bit : bg_color(0x0 - 0xf)
	 * 1 bit : is_bold(0 or 1)
	 * 1 bit : is_underlined(0 or 1)
	 * 1 bit : is_reversed(0 or 1)	... used for X Selection
	 */
	u_int32_t  attr ;

} ml_char_t ;


int  ml_use_char_combining(void) ;

int  ml_unuse_char_combining(void) ;

int  ml_use_multi_col_char(void) ;

int  ml_unuse_multi_col_char(void) ;


int  ml_str_init( ml_char_t *  str , u_int  size) ;

ml_char_t *  __ml_str_init( ml_char_t *  str , u_int  size) ;

#define  ml_str_alloca(size) __ml_str_init( alloca(sizeof(ml_char_t) * (size)) , (size) )

ml_char_t *  ml_str_new( u_int  size) ;

int  ml_str_final( ml_char_t *  str , u_int  size) ;

int  ml_str_delete( ml_char_t *  str , u_int  size) ;

inline int  ml_str_copy( ml_char_t *  dst , ml_char_t *  src , u_int  size) ; 

inline u_int  ml_str_cols( ml_char_t *  chars , u_int  len) ;

inline int  ml_str_bytes_equal( ml_char_t *  str1 , ml_char_t *  str2 , u_int  len) ;

#ifdef  DEBUG

void  ml_str_dump( ml_char_t *  chars , u_int  len) ;

#endif


inline int  ml_char_init( ml_char_t *  ch) ;

inline int  ml_char_final( ml_char_t *  ch) ;

inline int  ml_char_set( ml_char_t *  ch , u_char *  bytes , size_t  size ,
	mkf_charset_t  cs , int  is_biwidth , int  is_comb ,
	ml_color_t  fg_color , ml_color_t  bg_color , int  is_bold , int  is_underlined) ;

inline int  ml_char_combine( ml_char_t *  ch , u_char *  bytes , size_t  size ,
	mkf_charset_t  cs , int  is_biwidth , int  is_comb ,
	ml_color_t  fg_color , ml_color_t  bg_color , int  is_bold , int  is_underlined) ;

inline int  ml_combine_chars( ml_char_t *  ch , ml_char_t *  comb) ;

inline int  ml_remove_combining_char( ml_char_t *  ch) ;

inline ml_char_t *  ml_get_base_char( ml_char_t *  ch) ;

inline ml_char_t *  ml_get_combining_chars( ml_char_t *  ch , u_int *  size) ;

inline int  ml_char_move( ml_char_t *  dst , ml_char_t *  src) ;

inline int  ml_char_copy( ml_char_t *  dst , ml_char_t *  src) ;

inline u_char *  ml_char_bytes( ml_char_t *  ch) ;

inline int  ml_char_set_bytes( ml_char_t *  ch , u_char *  bytes) ;

inline size_t  ml_char_size( ml_char_t *  ch) ;

inline mkf_charset_t  ml_char_cs( ml_char_t *  ch) ;

inline int  ml_char_is_comb( ml_char_t *  ch) ;

inline ml_font_t  ml_char_font( ml_char_t *  ch) ;

inline u_int  ml_char_cols( ml_char_t *  ch) ;

inline ml_color_t  ml_char_fg_color( ml_char_t *  ch) ;

inline int  ml_char_set_fg_color( ml_char_t *  ch , ml_color_t  color) ;

inline ml_color_t  ml_char_bg_color( ml_char_t *  ch) ;

inline int  ml_char_set_bg_color( ml_char_t *  ch , ml_color_t  color) ;

inline int  ml_char_is_underlined( ml_char_t *  ch) ;

inline int  ml_char_reverse_color( ml_char_t *  ch) ;

inline int  ml_char_restore_color( ml_char_t *  ch) ;

int  ml_char_is_null( ml_char_t *  ch) ;

inline int  ml_char_bytes_is( ml_char_t *  ch , char *  bytes , size_t  size , mkf_charset_t  cs) ;

inline int  ml_char_bytes_equal( ml_char_t *  ch1 , ml_char_t *  ch2) ;

#ifdef  DEBUG

void  ml_char_dump( ml_char_t *  ch) ;

#endif


#endif
