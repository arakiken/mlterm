/*
 *	$Id$
 */

#include  "ml_char.h"

#include  <stdio.h>		/* fprintf */
#include  <string.h>		/* memset */
#include  <kiklib/kik_debug.h>
#include  <kiklib/kik_mem.h>

#include  "ml_font.h"


/*
 * !Notice!
 * internal representation.
 * 0x0 = size 1 , 0x1 = size 2 , 0x2 = size 3 , 0x3 = size 4.
 */
#define  SIZE(attr) ((((attr) >> 14) & 0x3) + 1)

#define  COMB_SIZE(attr)  (((attr) >> 12) & 0x3)

#define  IS_REVERSED(attr)  (((attr) >> 11) & 0x01)

#define  FG_COLOR(attr)  (((attr) >> 7) & 0xf)

#define  BG_COLOR(attr)  (((attr) >> 3) & 0xf)

#define  FONT_DECOR(attr)  ((attr) & 0x7)

#define  COMPOUND_ATTR(size,comb_size,is_reversed,fg_color,bg_color,decor) \
	((((size - 1) << 14) & 0xc000) | (((comb_size) << 12) & 0x3000) | (((is_reversed) << 11) & 0x800) | \
	(((fg_color) << 7) & 0x780) | (((bg_color) << 3) & 0x78) | decor)


/* --- static variables --- */

static int  is_char_combining = 0 ;


/* --- global functions --- */

int
ml_use_char_combining(void)
{
	is_char_combining = 1 ;

	return  1 ;
}

int
ml_stop_char_combining(void)
{
	is_char_combining = 0 ;

	return  1 ;
}

int
ml_is_char_combining(void)
{
	return  is_char_combining ;
}

int
ml_str_init(
	ml_char_t *  str ,
	u_int  size
	)
{
	memset( str , 0 , sizeof( ml_char_t) * size) ;

	return  1 ;
}

ml_char_t *
__ml_str_init(
	ml_char_t *  str ,	/* alloca()-ed memory (see ml_char.h) */
	u_int  size
	)
{
	if( str == NULL)
	{
		/* alloca() failed. */
		
		return  NULL ;
	}

	if( ! ( ml_str_init( str , size)))
	{
		return  NULL ;
	}

	return  str ;
}
	

ml_char_t *
ml_str_new(
	u_int  size
	)
{
	ml_char_t *  str ;

	if( ( str = malloc( sizeof( ml_char_t) * size)) == NULL)
	{
	#ifdef  DEBUG
		kik_warn_printf( KIK_DEBUG_TAG " malloc() failed.\n") ;
	#endif
	
		return  NULL ;
	}

	if( ml_str_init( str , size) == 0)
	{
		free( str) ;
		
		return  NULL ;
	}

	return  str ;
}

int
ml_str_final(
	ml_char_t *  str ,
	u_int  size
	)
{
	int  counter ;

	for( counter = 0 ; counter < size ; counter ++)
	{
		ml_char_final( &str[counter]) ;
	}

	return  1 ;
}

int
ml_str_delete(
	ml_char_t *  str ,
	u_int  size
	)
{
	if( ml_str_final( str , size))
	{
		free( str) ;

		return  1 ;
	}
	else
	{
		free( str) ;

		return  0 ;
	}
}

/*
 * dst and src may overlap.
 */
inline int
ml_str_copy(
	ml_char_t *  dst ,
	ml_char_t *  src ,
	u_int  size
	)
{
	int  counter ;

	if( size == 0 || dst == src)
	{
		return  0 ;
	}

	if( dst < src)
	{
		for( counter = 0 ; counter < size ; counter ++)
		{
			ml_char_copy( dst++ , src++) ;
		}
	}
	else if( dst > src)
	{
		dst += size ;
		src += size ;
		for( counter = 0 ; counter < size ; counter ++)
		{
			ml_char_copy( --dst  , --src) ;
		}
	}

	return  1 ;
}

#ifdef  DEBUG

void
ml_str_dump(
	ml_char_t *  chars ,
	u_int  len
	)
{
	int  counter ;

	for( counter = 0 ; counter < len ; counter ++)
	{
		ml_char_dump( &chars[counter]) ;
	}

	fprintf( stderr , "\n") ;
}

#endif


inline int
ml_char_init(
	ml_char_t *  ch
	)
{
	memset( ch , 0 , sizeof( ml_char_t)) ;

	return  1 ;
}

inline int
ml_char_final(
	ml_char_t *  ch
	)
{
	if( is_char_combining && COMB_SIZE(ch->attr) > 0)
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
	ml_font_t *  font ,
	ml_font_decor_t  font_decor ,
	ml_color_t  fg_color ,
	ml_color_t  bg_color
	)
{
	ml_char_final( ch) ;

	ch->u.font = font ;

	memcpy( ch->bytes , bytes , size) ;

	ch->attr = COMPOUND_ATTR(size,0,0,fg_color,bg_color,font_decor) ;

	return  1 ;
}

inline int
ml_char_combine(
	ml_char_t *  ch ,
	u_char *  bytes ,
	size_t  size ,
	ml_font_t *  font ,
	ml_font_decor_t  font_decor ,
	ml_color_t  fg_color ,
	ml_color_t  bg_color
	)
{
	ml_char_t *  multi_ch ;
	u_int  comb_size ;

	if( ! is_char_combining || COMB_SIZE(ch->attr) >= 3)
	{
		return  0 ;
	}

	if( COMB_SIZE(ch->attr) == 0)
	{
		if( ( multi_ch = malloc( sizeof( ml_char_t) * 2)) == NULL)
		{
		#ifdef  DEBUG
			kik_warn_printf( KIK_DEBUG_TAG " malloc() failed.\n") ;
		#endif

			return  0 ;
		}

		ml_str_init( multi_ch , 2) ;

		ml_char_copy( &multi_ch[0] , ch) ;

		if( ml_char_set( &multi_ch[1] , bytes , size , font , font_decor ,
			fg_color , bg_color) == 0)
		{
			return  0 ;
		}

		comb_size = 1 ;
	}
	else
	{
		comb_size = COMB_SIZE(ch->attr) ;
		
		if( ( multi_ch = realloc( ch->u.multi_ch , sizeof( ml_char_t) * (comb_size + 2)))
			== NULL)
		{
			return  0 ;
		}

		ml_char_init( &multi_ch[comb_size + 1]) ;

		if( ml_char_set( &multi_ch[comb_size + 1] , bytes , size ,
			font , font_decor , fg_color , bg_color) == 0)
		{
			return  0 ;
		}

		comb_size ++ ;
	}

	ch->u.multi_ch = multi_ch ;
	
	ch->attr = COMPOUND_ATTR(SIZE(ch->attr),comb_size,IS_REVERSED(ch->attr),
		FG_COLOR(ch->attr),BG_COLOR(ch->attr),FONT_DECOR(ch->attr)) ;

	return  1 ;
}

inline ml_char_t *
ml_get_combining_chars(
	ml_char_t *  ch ,
	u_int *  size
	)
{
	if( is_char_combining && ( *size = COMB_SIZE(ch->attr)) > 0)
	{
		return  &ch->u.multi_ch[1] ;
	}
	else
	{
		return  NULL ;
	}
}

inline int
ml_char_copy(
	ml_char_t *  dst ,
	ml_char_t *  src
	)
{
	ml_char_final( dst) ;

	if( dst == src)
	{
		return  0 ;
	}
	
	memcpy( dst , src , sizeof( ml_char_t)) ;

	if( is_char_combining && COMB_SIZE(src->attr) > 0)
	{
		ml_char_t *  multi_ch ;
		
		if( ( multi_ch = malloc( sizeof( ml_char_t) * (COMB_SIZE(src->attr) + 1))) == NULL)
		{
		#ifdef  DEBUG
			kik_warn_printf( KIK_DEBUG_TAG " failed to malloc.\n") ;
		#endif

			return  0 ;
		}

		memcpy( multi_ch , src->u.multi_ch , sizeof( ml_char_t) * (COMB_SIZE(src->attr) + 1)) ;

		dst->u.multi_ch = multi_ch ;
	}
	
	return  1 ;
}

inline u_char *
ml_char_bytes(
	ml_char_t *  ch
	)
{
	return  ch->bytes ;
}

inline int
ml_char_set_bytes(
	ml_char_t *  ch ,
	u_char *  bytes ,
	size_t  size
	)
{
	memcpy( ch->bytes , bytes , size) ;
	
	ch->attr = COMPOUND_ATTR(size,COMB_SIZE(ch->attr),IS_REVERSED(ch->attr),
		FG_COLOR(ch->attr),BG_COLOR(ch->attr),FONT_DECOR(ch->attr)) ;

	if( COMB_SIZE(ch->attr) > 0)
	{
		ml_char_set_bytes( &ch->u.multi_ch[0] , bytes , size) ;
	}
	
	return  1 ;
}

inline ml_font_t *
ml_char_font(
	ml_char_t *  ch
	)
{
	if( COMB_SIZE(ch->attr) == 0)
	{
	#ifdef  DEBUG
		if( ch->u.font == NULL)
		{
			kik_warn_printf( KIK_DEBUG_TAG " font of the char [") ;
			ml_char_dump( ch) ;
			fprintf( stderr , "] is NULL.\n") ;
		}
	#endif
	
		return  ch->u.font ;
	}
	else
	{
	#ifdef  DEBUG
		if( ch->u.multi_ch == NULL)
		{
			kik_warn_printf( KIK_DEBUG_TAG " multi_ch of char [") ;
			ml_char_dump( ch) ;
			fprintf( stderr , "] is NULL.\n") ;
		}
	#endif
	
		return  ch->u.multi_ch[0].u.font ;
	}
}

inline u_int
ml_char_height(
	ml_char_t *  ch
	)
{
	if( ml_char_font(ch) == NULL)
	{
		kik_error_printf( "ml_font_t is NULL.\n") ;

		/* XXX avoiding zero division */
		return  1 ;
	}
	
	return  ml_char_font(ch)->height ;
}

inline u_int
ml_char_height_to_baseline(
	ml_char_t *  ch
	)
{
	if( ml_char_font(ch) == NULL)
	{
		kik_error_printf( "ml_font_t is NULL.\n") ;

		/* XXX avoiding zero division */
		return  1 ;
	}
	
	return  ml_char_font(ch)->height_to_baseline ;
}

/*
 * return actual width.
 */
inline u_int
ml_char_width(
	ml_char_t *  ch
	)
{
	if( ml_char_font(ch) == NULL)
	{
		kik_error_printf( "ml_font_t is NULL.\n") ;
	
		/* XXX avoiding zero division */
		return  1 ;
	}
	
	return  ml_char_font( ch)->width ;
}

/*
 * for ml_image
 */
inline u_int
ml_char_cols(
	ml_char_t *  ch
	)
{
	if( ml_char_font(ch) == NULL)
	{
		kik_error_printf( "ml_font_t is NULL.\n") ;

		/* XXX avoiding zero division */
		return  1 ;
	}
	
	return  ml_char_font(ch)->cols ;
}

inline mkf_charset_t
ml_char_cs(
	ml_char_t *  ch
	)
{
	/*
	 * !!! Notice !!!
	 * FONT_CS is not necessarily equal to the ch's cs , because US ASCII cs is included
	 * by other cs e.g. ISO8859_1_R or so(see cs_table in main.c).
	 */
	if( ml_char_size(ch) == 1 && ml_char_bytes( ch)[0] <= 0x7f)
	{
		return  US_ASCII ;
	}
	else
	{
		if( ml_char_font(ch) == NULL)
		{
			kik_error_printf( "ml_font_t is NULL.\n") ;
			
			return  UNKNOWN_CS ;
		}
		
		return  ml_font_cs( ml_char_font(ch)) ;
	}
}

inline int
ml_char_is_reversed(
	ml_char_t *  ch
	)
{
	return  IS_REVERSED(ch->attr) ;
}

inline ml_color_t
ml_char_fg_color(
	ml_char_t *  ch
	)
{
	return  FG_COLOR(ch->attr) ;
}

inline int
ml_char_set_fg_color(
	ml_char_t *  ch ,
	ml_color_t  fg_color
	)
{
	if( is_char_combining)
	{
		int  counter ;

		for( counter = 0 ; counter < COMB_SIZE(ch->attr) ; counter ++)
		{
			ml_char_set_fg_color( &ch->u.multi_ch[counter + 1] , fg_color) ;
		}
	}
	
	ch->attr = COMPOUND_ATTR(SIZE(ch->attr),COMB_SIZE(ch->attr),0,
		fg_color,BG_COLOR(ch->attr),FONT_DECOR(ch->attr)) ;

	return  1 ;
}

inline ml_color_t
ml_char_bg_color(
	ml_char_t *  ch
	)
{
	return  BG_COLOR(ch->attr) ;
}

inline int
ml_char_set_bg_color(
	ml_char_t *  ch ,
	ml_color_t  bg_color
	)
{
	if( is_char_combining)
	{
		int  counter ;

		for( counter = 0 ; counter < COMB_SIZE(ch->attr) ; counter ++)
		{
			ml_char_set_bg_color( &ch->u.multi_ch[counter + 1] , bg_color) ;
		}
	}
	
	ch->attr = COMPOUND_ATTR(SIZE(ch->attr),COMB_SIZE(ch->attr),0,
		FG_COLOR(ch->attr),bg_color,FONT_DECOR(ch->attr)) ;

	return  1 ;
}

inline ml_color_t
ml_char_font_decor(
	ml_char_t *  ch
	)
{
	return  FONT_DECOR(ch->attr) ;
}

inline size_t
ml_char_size(
	ml_char_t *  ch
	)
{
	return  SIZE(ch->attr) ;
}

inline int
ml_char_reverse_color(
	ml_char_t *  ch
	)
{
	int  counter ;

	if( IS_REVERSED(ch->attr))
	{
		return  0 ;
	}
	
	if( is_char_combining)
	{
		for( counter = 0 ; counter < COMB_SIZE(ch->attr) ; counter ++)
		{
			ml_char_reverse_color( &ch->u.multi_ch[counter + 1]) ;
		}
	}
	
	ch->attr = COMPOUND_ATTR(SIZE(ch->attr),COMB_SIZE(ch->attr),1,
		BG_COLOR(ch->attr),FG_COLOR(ch->attr),FONT_DECOR(ch->attr)) ;
	
	return  1 ;
}

inline int
ml_char_restore_color(
	ml_char_t *  ch
	)
{
	int  counter ;
	
	if( ! IS_REVERSED(ch->attr))
	{
		return  0 ;
	}
	
	if( is_char_combining)
	{
		for( counter = 0 ; counter < COMB_SIZE(ch->attr) ; counter ++)
		{
			ml_char_restore_color( &ch->u.multi_ch[counter + 1]) ;
		}
	}
	
	ch->attr = COMPOUND_ATTR(SIZE(ch->attr),COMB_SIZE(ch->attr),0,
		BG_COLOR(ch->attr),FG_COLOR(ch->attr),FONT_DECOR(ch->attr)) ;
	
	return  1 ;
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
	
	for( i = 0 ; i < ml_char_size(ch) ; i ++)
	{
		fprintf( stderr , "%.2x" , ml_char_bytes(ch)[i]) ;
	}
#else
	if( ml_char_size(ch) == 2)
	{
		if( ml_char_cs(ch) == JISX0208_1983)
		{
			/* only eucjp */

			fprintf( stderr , "%c%c" , ml_char_bytes(ch)[0] | 0x80 ,
				ml_char_bytes(ch)[1] | 0x80) ;
		}
		else
		{
			fprintf( stderr , "**") ;
		}
	}
	else if( ml_char_size(ch) == 1)
	{
		fprintf( stderr , "%c" , ml_char_bytes(ch)[0]) ;
	}
	else
	{
		fprintf( stderr , "!!! unsupported char[0x%.2x len %d] !!!" ,
			ml_char_bytes(ch)[0] , SIZE(ch->attr)) ;
	}
#endif

	if( ( comb_chars = ml_get_combining_chars( ch , &comb_size)) != NULL)
	{
		int  counter ;

		for( counter = 0 ; counter < comb_size ; counter ++)
		{
			ml_char_dump( &comb_chars[counter]) ;
		}
	}
}

#endif
