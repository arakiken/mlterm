/*
 *	$Id$
 */

#include  "ml_char.h"

#include  <string.h>		/* memset/memcpy */
#include  <kiklib/kik_debug.h>
#include  <kiklib/kik_util.h>	/* K_MIN */

#include  "ml_char_encoding.h"	/* IS_BIWIDTH_CS */


/*
 * !! Notice !!
 * Internal size representation is as follows.
 * 0x0 = size 1 , 0x1 = size 2 , 0x2 = size 3 , 0x3 = size 4.
 */
#define  SIZE(attr) ((((attr) >> 28) & 0x3) + 1)

#define  CHARSET(attr)  (((attr) >> 17) & 0x7ff)

#define  IS_BIWIDTH(attr)  (((attr) >> 16) & 0x1)

#define  IS_REVERSED(attr)  (((attr) >> 15) & 0x1)

#define  REVERSE_COLOR(attr) ((attr) |= (0x1 << 15))

#define  RESTORE_COLOR(attr) ((attr) &= ~(0x1 << 15))

/*
 * !! Notice !!
 * Internal color representation is as follows.
 * ML_BLACK - ML_WHITE: as they are
 * ML_FG_COLOR        : 0x8
 * ML_BG_COLOR        : 0x9
 */
#define  INTERN_COLOR(color) \
	((color) = ((color) == ML_FG_COLOR ? 0x8 : \
		(color) == ML_BG_COLOR ? 0x9 : (color) & ~ML_BOLD_COLOR_MASK))

#define  IS_VALID_INTERN_COLOR(color) ((ML_BLACK <=color) && (color <=0x9))

#define  EXTERN_COLOR(color) \
	((color) = ((color) == 0x8 ? ML_FG_COLOR : \
		(color) == 0x9 ? ML_BG_COLOR : (color)))

#define  FG_COLOR(attr)  (((attr) >> 11) & 0xf)

#define  SET_FG_COLOR(attr,color)  ((attr) = (((attr) & 0xffff87ff) | ((color) << 11)))

#define  BG_COLOR(attr)  (((attr) >> 7) & 0xf)

#define  SET_BG_COLOR(attr,color)  ((attr) = (((attr) & 0xfffff87f) | ((color) << 7)))

#define  IS_BOLD(attr)  (((attr) >> 6) & 0x1)

#define  IS_UNDERLINED(attr)  (((attr) >> 5) & 0x1)

#define  COMB_SIZE(attr)  (((attr) >> 2) & 0x7)

#define  SET_COMB_SIZE(attr,size)  ((attr) = (((attr) & 0xffffffe3) | ((size) << 2)))

#define  IS_COMB(attr)  (((attr) >> 1) & 0x1)

#define  IS_SINGLE_CH(attr)  ((attr) & 0x1)

#define  USE_MULTI_CH(attr)  ((attr) &= 0xfffffffe)

#define  COMPOUND_ATTR(size,charset,is_biwidth,fg_color,bg_color,is_bold,is_underlined,is_comb) \
	( ((size - 1) << 28) | ((charset) << 17) | ((is_biwidth) << 16) | \
	((0x0) << 15) | ((fg_color) << 11) | ((bg_color) << 7) | \
	((is_bold) << 6) | ((is_underlined) << 5) | (0x0 << 2) | \
	(is_comb << 1) | 0x1 )


/* --- static variables --- */

static int  use_char_combining = 1 ;
static int  use_multi_col_char = 1 ;
static ml_char_t  sp_ch ;
static ml_char_t  nl_ch ;


/* --- global functions --- */

int
ml_use_char_combining(void)
{
	use_char_combining = 1 ;

	return  1 ;
}

int
ml_unuse_char_combining(void)
{
	use_char_combining = 0 ;

	return  1 ;
}

int
ml_use_multi_col_char(void)
{
	use_multi_col_char = 1 ;

	return  1 ;
}

int
ml_unuse_multi_col_char(void)
{
	use_multi_col_char = 0 ;

	return  1 ;
}


/*
 * character functions
 */

inline int
ml_char_init(
	ml_char_t *  ch
	)
{
	if( sizeof( ml_char_t *) != sizeof( ml_char_t))
	{
		/*ILP32*/
		memset( ch , 0 , sizeof( ml_char_t)) ;
		/* set u.ch.is_single_ch */
		ch->u.ch.attr = 0x1 ;
	}
	else
	{
		/*LP64*/
		/* LSB of multi_ch must be "is_single_ch"  */
		ch->u.multi_ch =(ml_char_t *)0x1 ;
	}

	return  1 ;
}

inline int
ml_char_final(
	ml_char_t *  ch
	)
{
	if( ! IS_SINGLE_CH(ch->u.ch.attr))
	{
		free( ch->u.multi_ch) ;
	}

	return  1 ;
}

inline int
ml_char_set(
	ml_char_t *  ch ,
	u_char *  bytes ,
	size_t  size ,
	mkf_charset_t  cs ,
	int  is_biwidth ,
	int  is_comb ,
	ml_color_t  fg_color ,
	ml_color_t  bg_color ,
	int  is_bold ,
	int  is_underlined
	)
{
	ml_char_final( ch) ;

	memcpy( ch->u.ch.bytes , bytes , size) ;
	memset( ch->u.ch.bytes + size , 0 , MAX_CHAR_SIZE - size) ;

	INTERN_COLOR(fg_color) ;
	INTERN_COLOR(bg_color) ;

#ifdef  DEBUG
	if( size > MAX_CHAR_SIZE)
	{
		kik_warn_printf(KIK_DEBUG_TAG "size(%d) too large\n", size) ;
	}
	if( cs >= MAX_CHARSET)
	{
		kik_warn_printf(KIK_DEBUG_TAG "charset(%x) is out of range\n", cs) ;
	}
	if( is_biwidth > 1 || is_biwidth < 0)
	{
		kik_warn_printf(KIK_DEBUG_TAG "is_biwidth should be 0/1\n") ;
	}
	if( is_comb > 1 || is_comb < 0)
	{
		kik_warn_printf(KIK_DEBUG_TAG "is_comb should be 0/1\n") ;
	}
	if( !IS_VALID_INTERN_COLOR(fg_color))
	{
		kik_warn_printf(KIK_DEBUG_TAG "fg_color is not valid\n") ;
	}
	if( !IS_VALID_INTERN_COLOR(bg_color))
	{
		kik_warn_printf(KIK_DEBUG_TAG "bg_color is not valid\n") ;
	}
	if( is_bold > 1 || is_bold < 0)
	{
		kik_warn_printf(KIK_DEBUG_TAG "is_bold should be 0/1\n") ;
	}
	if( is_underlined > 1 || is_underlined < 0)
	{
		kik_warn_printf(KIK_DEBUG_TAG "is_underlined should be 0/1\n") ;
	}
#endif

	ch->u.ch.attr =
		COMPOUND_ATTR(size,cs,is_biwidth,fg_color,bg_color,is_bold,is_underlined,is_comb) ;

	return  1 ;
}

inline int
ml_char_combine(
	ml_char_t *  ch ,
	u_char *  bytes ,
	size_t  size ,
	mkf_charset_t  cs ,
	int  is_biwidth ,
	int  is_comb ,
	ml_color_t  fg_color ,
	ml_color_t  bg_color ,
	int  is_bold ,
	int  is_underlined
	)
{
	ml_char_t *  multi_ch ;
	u_int  comb_size ;

	if( ! use_char_combining)
	{
		return  0 ;
	}

	if( IS_SINGLE_CH(ch->u.ch.attr))
	{
		if( ( multi_ch = malloc( sizeof( ml_char_t) * 2)) == NULL)
		{
		#ifdef  DEBUG
			kik_warn_printf( KIK_DEBUG_TAG " malloc() failed.\n") ;
		#endif

			return  0 ;
		}

		if( sizeof( multi_ch) >= 8 && ((int)( multi_ch) & 0x1) != 0)
		{
			kik_msg_printf( "Your malloc() doesn't return 2 byte aligned address."
			                "Character combining is not supported.\n") ;

			return  0 ;
		}

		ml_char_init( multi_ch) ;
		ml_char_copy( multi_ch , ch) ;

		ml_char_init( multi_ch + 1) ;
		if( ml_char_set( multi_ch + 1 , bytes , size , cs , is_biwidth , is_comb ,
			fg_color , bg_color , is_bold , is_underlined) == 0)
		{
			return  0 ;
		}

		comb_size = 1 ;
	}
	else
	{
		if( ( comb_size = COMB_SIZE(ch->u.multi_ch->u.ch.attr)) >= MAX_COMB_SIZE)
		{
			return  0 ;
		}
		
		if( ( multi_ch = realloc( ch->u.multi_ch , sizeof( ml_char_t) * (comb_size + 2))) == NULL)
		{
			return  0 ;
		}

		if( sizeof( multi_ch) >= 8 && ((int)( multi_ch) & 0x1) != 0)
		{
			kik_msg_printf( "Your malloc() doesn't return 2 bytes aligned address."
			                "Character combining is not supported.\n") ;

			return  0 ;
		}
		
		ml_char_init( &multi_ch[comb_size + 1]) ;

		if( ml_char_set( &multi_ch[comb_size + 1] , bytes , size , cs , is_biwidth , is_comb ,
			fg_color , bg_color , is_bold , is_underlined) == 0)
		{
			return  0 ;
		}

		comb_size ++ ;
	}

	SET_COMB_SIZE(multi_ch->u.ch.attr,comb_size) ;

	ch->u.multi_ch = multi_ch ;
	USE_MULTI_CH(ch->u.ch.attr) ;

	return  1 ;
}

inline int
ml_combine_chars(
	ml_char_t *  ch ,
	ml_char_t *  comb
	)
{
	return  ml_char_combine( ch , ml_char_bytes( comb) , SIZE(comb->u.ch.attr) ,
			CHARSET(comb->u.ch.attr) ,
			IS_BIWIDTH(comb->u.ch.attr) , IS_COMB(comb->u.ch.attr) ,
			FG_COLOR(comb->u.ch.attr) , BG_COLOR(comb->u.ch.attr) ,
			IS_BOLD(comb->u.ch.attr) , IS_UNDERLINED(comb->u.ch.attr)) ;
}

inline ml_char_t *
ml_get_base_char(
	ml_char_t *  ch
	)
{
	if( IS_SINGLE_CH(ch->u.ch.attr))
	{
		return  ch ;
	}
	else
	{
		return  ch->u.multi_ch ;
	}
}

inline ml_char_t *
ml_get_combining_chars(
	ml_char_t *  ch ,
	u_int *  size
	)
{
	if( IS_SINGLE_CH(ch->u.ch.attr))
	{
		*size = 0 ;

		return  NULL ;
	}
	else
	{
		*size = COMB_SIZE(ch->u.multi_ch->u.ch.attr) ;

		return  ch->u.multi_ch + 1 ;
	}
}

inline int
ml_char_move(
	ml_char_t *  dst ,
	ml_char_t *  src
	)
{
	if( dst == src)
	{
		return  0 ;
	}
	
	ml_char_final( dst) ;

	memcpy( dst , src , sizeof( ml_char_t)) ;

#if  0
	/* invalidated src */
	ml_char_init( src) ;
#endif

	return  1 ;
}

inline int
ml_char_copy(
	ml_char_t *  dst ,
	ml_char_t *  src
	)
{
	if( dst == src)
	{
		return  0 ;
	}
	
	ml_char_final( dst) ;

	memcpy( dst , src , sizeof( ml_char_t)) ;

	if( ! IS_SINGLE_CH(src->u.ch.attr))
	{
		ml_char_t *  multi_ch ;
		u_int  comb_size ;

		comb_size = COMB_SIZE(src->u.multi_ch->u.ch.attr) ;
		
		if( ( multi_ch = malloc( sizeof( ml_char_t) * (comb_size + 1))) == NULL)
		{
		#ifdef  DEBUG
			kik_warn_printf( KIK_DEBUG_TAG " failed to malloc.\n") ;
		#endif

			return  0 ;
		}

		memcpy( multi_ch , src->u.multi_ch , sizeof( ml_char_t) * (comb_size + 1)) ;

		dst->u.multi_ch = multi_ch ;
		USE_MULTI_CH(dst->u.ch.attr) ;
	}
	
	return  1 ;
}

inline u_char *
ml_char_bytes(
	ml_char_t *  ch
	)
{
	if( IS_SINGLE_CH(ch->u.ch.attr))
	{
		return  ch->u.ch.bytes ;
	}
	else
	{
		return  ml_char_bytes( ch->u.multi_ch) ;
	}
}

inline size_t
ml_char_size(
	ml_char_t *  ch
	)
{
	if( IS_SINGLE_CH(ch->u.ch.attr))
	{
		return  SIZE(ch->u.ch.attr) ;
	}
	else
	{
		return  ml_char_size( ch->u.multi_ch) ;
	}
}

inline int
ml_char_set_bytes(
	ml_char_t *  ch ,
	u_char *  bytes
	)
{
	if( IS_SINGLE_CH(ch->u.ch.attr))
	{
		memcpy( ch->u.ch.bytes , bytes , SIZE(ch->u.ch.attr)) ;
	}
	else
	{
		ml_char_set_bytes( ch->u.multi_ch , bytes) ;
	}
	
	return  1 ;
}

inline mkf_charset_t
ml_char_cs(
	ml_char_t *  ch
	)
{
	if( IS_SINGLE_CH(ch->u.ch.attr))
	{
		return  CHARSET(ch->u.ch.attr) ;
	}
	else
	{
		return  ml_char_cs( ch->u.multi_ch) ;
	}
}

inline ml_font_t
ml_char_font(
	ml_char_t *  ch
	)
{
	if( IS_SINGLE_CH(ch->u.ch.attr))
	{
		ml_font_t  font ;

		font = CHARSET(ch->u.ch.attr) ;

		if( IS_BOLD(ch->u.ch.attr))
		{
			font |= FONT_BOLD ;
		}

		if( IS_BIWIDTH(ch->u.ch.attr))
		{
			font |= FONT_BIWIDTH ;
		}

		return  font ;
	}
	else
	{
		return  ml_char_font( ch->u.multi_ch) ;
	}
}

inline u_int
ml_char_cols(
	ml_char_t *  ch
	)
{
	if( use_multi_col_char && ml_char_is_biwidth(ch))
	{
		return  2 ;
	}
	else
	{
		return  1 ;
	}
}

/*
 * 'use_multi_col_char' not concerned.
 */
inline u_int
ml_char_is_biwidth(
	ml_char_t *  ch
	)
{
	if( IS_SINGLE_CH(ch->u.ch.attr))
	{
		return  IS_BIWIDTH(ch->u.ch.attr) || IS_BIWIDTH_CS( CHARSET(ch->u.ch.attr)) ;
	}
	else
	{
		return  ml_char_is_biwidth( ch->u.multi_ch) ;
	}
}

inline int
ml_char_is_comb(
	ml_char_t *  ch
	)
{
	if( IS_SINGLE_CH(ch->u.ch.attr))
	{
		return  IS_COMB(ch->u.ch.attr) ;
	}
	else
	{
		return  ml_char_is_comb( ch->u.multi_ch) ;
	}
}

inline ml_color_t
ml_char_fg_color(
	ml_char_t *  ch
	)
{
	if( IS_SINGLE_CH(ch->u.ch.attr))
	{
		ml_color_t  color ;

		if( IS_REVERSED(ch->u.ch.attr))
		{
			color = BG_COLOR(ch->u.ch.attr) ;
		}
		else
		{
			color = FG_COLOR(ch->u.ch.attr) ;
		}

		EXTERN_COLOR(color) ;

		if( color < MAX_VT_COLORS && IS_BOLD(ch->u.ch.attr))
		{
			color |= ML_BOLD_COLOR_MASK ;
		}

		return  color ;
	}
	else
	{
		return  ml_char_fg_color( ch->u.multi_ch) ;
	}
}

inline int
ml_char_set_fg_color(
	ml_char_t *  ch ,
	ml_color_t  color
	)
{
	if( IS_SINGLE_CH(ch->u.ch.attr))
	{
		INTERN_COLOR(color) ;

		SET_FG_COLOR(ch->u.ch.attr,color) ;

		return  1 ;
	}
	else
	{
		int  count ;

		for( count = 0 ; count < COMB_SIZE(ch->u.multi_ch->u.ch.attr) + 1 ; count ++)
		{
			ml_char_set_fg_color( ch->u.multi_ch + count , color) ;
		}

		return  1 ;
	}
}

inline ml_color_t
ml_char_bg_color(
	ml_char_t *  ch
	)
{
	if( IS_SINGLE_CH(ch->u.ch.attr))
	{
		ml_color_t  color ;

		if( IS_REVERSED(ch->u.ch.attr))
		{
			color = FG_COLOR(ch->u.ch.attr) ;
		}
		else
		{
			color = BG_COLOR(ch->u.ch.attr) ;
		}

		EXTERN_COLOR(color) ;

		return  color ;
	}
	else
	{
		return  ml_char_bg_color( ch->u.multi_ch) ;
	}
}

inline int
ml_char_set_bg_color(
	ml_char_t *  ch ,
	ml_color_t  color
	)
{
	if( IS_SINGLE_CH(ch->u.ch.attr))
	{
		INTERN_COLOR(color) ;

		SET_BG_COLOR(ch->u.ch.attr,color) ;

		return  1 ;
	}
	else
	{
		int  count ;

		for( count = 0 ; count < COMB_SIZE(ch->u.multi_ch->u.ch.attr) + 1 ; count ++)
		{
			ml_char_set_bg_color( ch->u.multi_ch + count , color) ;
		}

		return  1 ;
	}
}

inline int
ml_char_is_underlined(
	ml_char_t *  ch
	)
{
	if( IS_SINGLE_CH(ch->u.ch.attr))
	{
		return  IS_UNDERLINED(ch->u.ch.attr) ;
	}
	else
	{
		return  ml_char_is_underlined( ch->u.multi_ch) ;
	}
}

inline int
ml_char_reverse_color(
	ml_char_t *  ch
	)
{
	if( IS_SINGLE_CH(ch->u.ch.attr))
	{
		if( IS_REVERSED(ch->u.ch.attr))
		{
			return  0 ;
		}

		REVERSE_COLOR(ch->u.ch.attr) ;

		return  1 ;
	}
	else
	{
		int  count ;

		for( count = 0 ; count < COMB_SIZE(ch->u.multi_ch->u.ch.attr) + 1 ; count ++)
		{
			ml_char_reverse_color( ch->u.multi_ch + count) ;
		}

		return  1 ;
	}
}

inline int
ml_char_restore_color(
	ml_char_t *  ch
	)
{
	if( IS_SINGLE_CH(ch->u.ch.attr))
	{
		if( ! IS_REVERSED(ch->u.ch.attr))
		{
			return  0 ;
		}

		RESTORE_COLOR(ch->u.ch.attr) ;

		return  1 ;
	}
	else
	{
		int  count ;

		for( count = 0 ; count < COMB_SIZE(ch->u.multi_ch->u.ch.attr) + 1 ; count ++)
		{
			ml_char_restore_color( ch->u.multi_ch + count) ;
		}

		return  1 ;
	}
}

/* XXX only used in iscii_logical() */
inline int
ml_char_copy_color_reversed_flag(
	ml_char_t *  dst ,
	ml_char_t *  src
	)
{
	if( IS_SINGLE_CH(src->u.ch.attr))
	{
		if( IS_REVERSED(src->u.ch.attr))
		{
			return  ml_char_reverse_color( dst) ;
		}
		else
		{
			return  ml_char_restore_color( dst) ;
		}
	}
	else
	{
		/* See the first character only */
		if( IS_REVERSED(src->u.multi_ch->u.ch.attr))
		{
			return  ml_char_reverse_color( dst) ;
		}
		else
		{
			return  ml_char_restore_color( dst) ;
		}
	}
}

inline int
ml_char_is_null(
	ml_char_t *  ch
	)
{
	if( IS_SINGLE_CH( ch->u.ch.attr))
	{
		return  (SIZE(ch->u.ch.attr) == 1 && ch->u.ch.bytes[0] == '\0') ;
	}
	else
	{
		return  ml_char_is_null( ch->u.multi_ch) ;
	}
}

/*
 * XXX
 * Returns inaccurate result in dealing with combined characters.
 * Even if they have the same bytes, false is returned since
 * ml_char_t:multi_ch-s never point the same address.)
 */
inline int
ml_char_equal(
	ml_char_t *  ch1 ,
	ml_char_t *  ch2
	)
{
	return  memcmp( ch1 , ch2 , sizeof( ml_char_t)) == 0 ;
}

inline int
ml_char_bytes_is(
	ml_char_t *  ch ,
	char *  bytes ,
	size_t  size ,
	mkf_charset_t  cs
	)
{
	if( IS_SINGLE_CH(ch->u.ch.attr))
	{
		if( CHARSET(ch->u.ch.attr) == cs &&
			SIZE(ch->u.ch.attr) == size &&
			memcmp( ml_char_bytes( ch) , bytes , size) == 0)
		{
			return  1 ;
		}
		else
		{
			return  0 ;
		}
	}
	else
	{
		return  ml_char_bytes_is( ch->u.multi_ch , bytes , size , cs) ;
	}
}

inline int
ml_char_bytes_equal(
	ml_char_t *  ch1 ,
	ml_char_t *  ch2
	)
{
	u_int  size1 ;
	u_int  size2 ;
	ml_char_t *  comb1 ;
	ml_char_t *  comb2 ;
	u_int  comb1_size ;
	u_int  comb2_size ;
	int  count ;

	size1 = ml_char_size( ch1) ;
	size2 = ml_char_size( ch2) ;

	if( size1 != size2)
	{
		return  0 ;
	}

	if( memcmp( ml_char_bytes( ch1) , ml_char_bytes( ch2) , size1) != 0)
	{
		return  0 ;
	}
	
	comb1 = ml_get_combining_chars( ch1 , &comb1_size) ;
	comb2 = ml_get_combining_chars( ch2 , &comb2_size) ;

	if( comb1_size != comb2_size)
	{
		return  0 ;
	}

	for( count = 0 ; count < comb1_size ; count ++)
	{
		if( ! ml_char_bytes_equal( &comb1[count] , &comb2[count]))
		{
			return  0 ;
		}
	}

	return  1 ;
}

inline ml_char_t *
ml_sp_ch(void)
{
	if( sp_ch.u.ch.attr == 0)
	{
		ml_char_init( &sp_ch) ;
		ml_char_set( &sp_ch , " " , 1 , US_ASCII , 0 , 0 , ML_FG_COLOR , ML_BG_COLOR , 0 , 0) ;
	}

	return  &sp_ch ;
}

inline ml_char_t *
ml_nl_ch(void)
{
	if( nl_ch.u.ch.attr == 0)
	{
		ml_char_init( &nl_ch) ;
		ml_char_set( &nl_ch , "\n" , 1 , US_ASCII , 0 , 0 , ML_FG_COLOR , ML_BG_COLOR ,
			0 , 0) ;
	}

	return  &nl_ch ;
}


#ifdef  DEBUG

#if  0
#define  DUMP_HEX
#endif

/*
 * for debugging.
 */
void
ml_char_dump(
	ml_char_t *  ch
	)
{
	u_int  comb_size ;
	ml_char_t *  comb_chars ;
	
#ifdef  DUMP_HEX
	int  i ;
	
	kik_msg_printf( "[") ;
	for( i = 0 ; i < ml_char_size(ch) ; i ++)
	{
		kik_msg_printf( "%.2x" , ml_char_bytes(ch)[i]) ;
	}
	kik_msg_printf( "]") ;
#else
	if( ml_char_size(ch) == 2)
	{
		if( ml_char_cs(ch) == JISX0208_1983)
		{
			/* only eucjp */

			kik_msg_printf( "%c%c" , ml_char_bytes(ch)[0] | 0x80 ,
				ml_char_bytes(ch)[1] | 0x80) ;
		}
		else
		{
			kik_msg_printf( "**") ;
		}
	}
	else if( ml_char_size(ch) == 1)
	{
		kik_msg_printf( "%c" , ml_char_bytes(ch)[0]) ;
	}
	else
	{
		kik_msg_printf( "!!! unsupported char[0x%.2x len %d] !!!" ,
			ml_char_bytes(ch)[0] , SIZE(ch->u.ch.attr)) ;
	}
#endif

	if( ( comb_chars = ml_get_combining_chars( ch , &comb_size)) != NULL)
	{
		int  count ;

		for( count = 0 ; count < comb_size ; count ++)
		{
			ml_char_dump( &comb_chars[count]) ;
		}
	}
}

#endif
