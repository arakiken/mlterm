/*
 *	$Id$
 */

#ifndef  __ML_FONT_H__
#define  __ML_FONT_H__


#include  <mkf/mkf_charset.h>


#undef  MAX_CHARSET
#define  MAX_CHARSET  0x1ff
#define  FONT_CS(font)  ((font) & MAX_CHARSET)
#define  FONT_STYLE_INDEX(font)  ((((font) & (FONT_BOLD|FONT_ITALIC)) >> 10) - 1)
#define  HAS_UNICODE_AREA(font)  ((font) >= 0x1000)
#define  NORMAL_FONT_OF(cs)  (IS_FULLWIDTH_CS(cs) ? (cs) | FONT_FULLWIDTH : (cs))
#define  SIZE_ATTR_FONT(font , size_attr)  ((font) | ((size_attr) << 12))
#define  SIZE_ATTR_OF(font)  (((font) >> 12) & 0x3)

enum
{
	DOUBLE_WIDTH = 0x1 ,
	DOUBLE_HEIGHT_TOP = 0x2 ,
	DOUBLE_HEIGHT_BOTTOM = 0x3 ,
} ;


typedef enum ml_font
{
	/* 0x00 - MAX_CHARSET(0x1ff) is reserved for mkf_charset_t */

	/* for unicode half or full width tag */
	FONT_FULLWIDTH = 0x200u ,	/* (default) half width */

	/* for font thickness */
	FONT_BOLD    = 0x400u ,	/* (default) medium */

	/* for font slant */
	FONT_ITALIC  = 0x800u ,	/* (default) roman */

#if  0
	/* font width */
	FONT_SEMICONDENSED	/* (default) normal */
#endif

	/*
	 * 0x1000 - is used for Unicode range mark (see ml_char_get_unicode_area_font)
	 * or size_attr (see x_font_manager.c)
	 */

} ml_font_t ;


#endif
