/*
 *	$Id$
 */

#ifndef  __ML_CHAR_H__
#define  __ML_CHAR_H__


#include  <X11/Xlib.h>
#include  <kiklib/kik_types.h>
#include  <kiklib/kik_mem.h>	/* alloca */
#include  <mkf/mkf_charset.h>	/* mkf_charset_t */

#include  "ml_font.h"
#include  "ml_color.h"


#ifdef  USE_UCS4
#define  MAX_CHAR_SIZE  4
#else
#define  MAX_CHAR_SIZE  2
#endif


typedef enum  ml_font_decor
{
	FONT_UNDERLINE = 0x1u ,
	FONT_OVERLINE = 0x2u ,

} ml_font_decor_t ;

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
		/* 32 bit(on ILP32) */
		ml_font_t *  font ;

		struct ml_char *  multi_ch ;
	} u ;

	/* 16 bit or 32 bit(on ILP32) */
	u_char  bytes[MAX_CHAR_SIZE] ;
	
	/*
	 * 16 bit
	 * 2 bit : size(0x0 - 0x3)
	 * 2 bit : comb_size(0x0 - 0x3)
	 * 1 bit : is_reversed flag(0 or 1)
	 * 4 bit : fg_color(0x0 - 0xf)
	 * 4 bit : bg_color(0x0 - 0xf)
	 * 3 bit : font_decor(0x0 - 0x7)
	 */
	u_int16_t  attr ;

} ml_char_t ;


int  ml_use_char_combining(void) ;

int  ml_stop_char_combining(void) ;

int  ml_is_char_combining(void) ;


int  ml_str_init( ml_char_t *  str , u_int  size) ;

ml_char_t *  __ml_str_init( ml_char_t *  str , u_int  size) ;

#define  ml_str_alloca(size) __ml_str_init( alloca(sizeof(ml_char_t) * (size)) , (size) )

ml_char_t *  ml_str_new( u_int  size) ;

int  ml_str_final( ml_char_t *  str , u_int  size) ;

int  ml_str_delete( ml_char_t *  str , u_int  size) ;

inline int  ml_str_copy( ml_char_t *  dst , ml_char_t *  src , u_int  size) ; 

#ifdef  DEBUG

void  ml_str_dump( ml_char_t *  chars , u_int  len) ;

#endif


inline int  ml_char_init( ml_char_t *  ch) ;

inline int  ml_char_final( ml_char_t *  ch) ;

inline int  ml_char_set( ml_char_t *  ch , u_char *  bytes , size_t  ch_size ,
	ml_font_t *  font , ml_font_decor_t  font_decor , ml_color_t  fg_color , ml_color_t  bg_color) ;

inline int  ml_char_combine( ml_char_t *  ch , u_char *  bytes , size_t  ch_size ,
	ml_font_t *  font , ml_font_decor_t  font_decor , ml_color_t  fg_color , ml_color_t  bg_color) ;
	
inline ml_char_t *  ml_get_combining_chars( ml_char_t *  ch , u_int *  size) ;
	
inline int  ml_char_copy( ml_char_t *  dst , ml_char_t *  src) ;

inline u_char *  ml_char_bytes( ml_char_t *  ch) ;
	
inline int  ml_char_set_bytes( ml_char_t *  ch , u_char *  bytes , size_t  size) ;

inline ml_font_t *  ml_char_font( ml_char_t *  ch) ;

inline u_int  ml_char_height( ml_char_t *  ch) ;

inline u_int  ml_char_height_to_baseline( ml_char_t *  ch) ;

inline u_int  ml_char_width( ml_char_t *  ch) ;

inline u_int  ml_char_cols( ml_char_t *  ch) ;

inline mkf_charset_t  ml_char_cs( ml_char_t *  ch) ;

inline int  ml_char_is_reversed( ml_char_t *  ch) ;

inline ml_color_t  ml_char_fg_color( ml_char_t *  ch) ;

inline int  ml_char_set_fg_color( ml_char_t *  ch , ml_color_t  fg_color) ;

inline ml_color_t  ml_char_bg_color( ml_char_t *  ch) ;

inline int  ml_char_set_bg_color( ml_char_t *  ch , ml_color_t  bg_color) ;

inline ml_color_t  ml_char_font_decor( ml_char_t *  ch) ;

inline size_t  ml_char_size( ml_char_t *  ch) ;

inline int  ml_char_reverse_color( ml_char_t *  ch) ;

inline int  ml_char_restore_color( ml_char_t *  ch) ;

#ifdef  DEBUG

void  ml_char_dump( ml_char_t *  ch) ;

#endif


#endif
