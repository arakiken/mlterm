/*
 *	$Id$
 */

#include  "../x_font.h"

#include  <stdio.h>
#include  <fcntl.h>	/* open */
#include  <unistd.h>	/* close */
#include  <sys/mman.h>	/* mmap */
#include  <string.h>	/* memcmp */
#include  <sys/stat.h>	/* fstat */

#include  <kiklib/kik_def.h>	/* WORDS_BIG_ENDIAN */
#include  <kiklib/kik_mem.h>
#include  <kiklib/kik_debug.h>
#include  <kiklib/kik_str.h>	/* strdup */
#include  <mkf/mkf_char.h>	/* mkf_bytes_to_int */


#define  DIVIDE_ROUNDING(a,b)  ( ((int)((a)*10 + (b)*5)) / ((int)((b)*10)) )
#define  DIVIDE_ROUNDINGUP(a,b) ( ((int)((a)*10 + (b)*10 - 1)) / ((int)((b)*10)) )

#ifdef  WORDS_BIG_ENDIAN
#define  _INT32(p,is_be) \
	( (is_be) ? *((u_int32_t*)p) : \
		((p)[3] | ((p)[2] << 8) | ((p)[1] << 16) | ((p)[0] << 24)))
#define  _INT16(p,is_be) \
	( (is_be) ? *((u_int16_t*)p) : ((p)[1] | ((p)[0] << 8)))
#else
#define  _INT32(p,is_be) \
	( (is_be) ? ((p)[3] | ((p)[2] << 8) | ((p)[1] << 16) | ((p)[0] << 24)) : \
		*((u_int32_t*)(p)) )
#define  _INT16(p,is_be) \
	( (is_be) ? ((p)[1] | ((p)[0] << 8)) : *((u_int16_t*)(p)))
#endif

#define  PCF_PROPERTIES		(1<<0)
#define  PCF_ACCELERATORS	(1<<1)
#define  PCF_METRICS		(1<<2)
#define  PCF_BITMAPS		(1<<3)
#define  PCF_INK_METRICS	(1<<4)
#define  PCF_BDF_ENCODINGS	(1<<5)
#define  PCF_SWIDTHS		(1<<6)
#define  PCF_GLYPH_NAMES	(1<<7)
#define  PCF_BDF_ACCELERATORS	(1<<8)


#if  0
#define  __DEBUG
#endif


/* --- static variables --- */

static XFontStruct **  xfonts ;
static u_int  num_of_xfonts ;


/* --- static functions --- */

static int
load_bitmaps(
	XFontStruct *  xfont ,
	u_char *  p ,
	size_t  size ,
	int  is_be ,
	int  glyph_pad_type
	)
{
	int32_t *  offsets ;
	int32_t  bitmap_sizes[4] ;
	int32_t  count ;

	/* 0 -> byte , 1 -> short , 2 -> int */
	xfont->glyph_width_bytes = ( glyph_pad_type == 2 ? 4 : ( glyph_pad_type == 1 ? 2 : 0)) ;

	xfont->num_of_glyphs = _INT32(p,is_be) ;
	p += 4 ;

	if( size < 8 + sizeof(*offsets) * xfont->num_of_glyphs)
	{
	#ifdef  DEBUG
		kik_debug_printf( KIK_DEBUG_TAG " size %d is too small.\n" , size) ;
	#endif

		return  0 ;
	}

	if( ! ( xfont->glyph_offsets = malloc( sizeof(*offsets) * xfont->num_of_glyphs)))
	{
		return  0 ;
	}

#ifdef  WORDS_BIG_ENDIAN
	if( is_be)
#else
	if( ! is_be)
#endif
	{
		memcpy( xfont->glyph_offsets , p , sizeof(*offsets) * xfont->num_of_glyphs) ;
		p += (sizeof(*offsets) * xfont->num_of_glyphs) ;
	}
	else
	{
		for( count = 0 ; count < xfont->num_of_glyphs ; count++)
		{
			xfont->glyph_offsets[count] = _INT32(p,is_be) ;
			p += 4 ;
		}
	}

	for( count = 0 ; count < 4 ; count++)
	{
		bitmap_sizes[count] = _INT32(p,is_be) ;
		p += 4 ;
	}

	if( size < 8 + sizeof(*offsets) * xfont->num_of_glyphs + 16 + bitmap_sizes[glyph_pad_type])
	{
	#ifdef  DEBUG
		kik_debug_printf( KIK_DEBUG_TAG " size %d is too small.\n" , size) ;
	#endif

		return  0 ;
	}

	if( ! ( xfont->glyphs = malloc( bitmap_sizes[glyph_pad_type])))
	{
		return  0 ;
	}

	memcpy( xfont->glyphs , p , bitmap_sizes[glyph_pad_type]) ;

#ifdef  WORDS_BIG_ENDIAN
	xfont->glyphs_same_endian = is_be ;
#else
	xfont->glyphs_same_endian = ! is_be ;
#endif

#ifdef  __DEBUG
	kik_debug_printf( KIK_DEBUG_TAG "GLYPH COUNT %d x WIDTH BYTE %d = SIZE %d\n" ,
		xfont->num_of_glyphs , xfont->glyph_width_bytes , bitmap_sizes[glyph_pad_type]) ;

	{
		FILE *  fp ;

		p = xfont->glyphs ;

		fp = fopen( "log.txt" , "w") ;

		for( count = 0 ; count < xfont->num_of_glyphs ; count++)
		{
			fprintf( fp , "NUM %x\n" , count) ;
			fprintf( fp , "%x\n" , _INT32(p,is_be)) ;
			p += 4 ;
			fprintf( fp , "%x\n" , _INT32(p,is_be)) ;
			p += 4 ;
			fprintf( fp , "%x\n" , _INT32(p,is_be)) ;
			p += 4 ;
			fprintf( fp , "%x\n" , _INT32(p,is_be)) ;
			p += 4 ;
			fprintf( fp , "%x\n" , _INT32(p,is_be)) ;
			p += 4 ;
			fprintf( fp , "%x\n" , _INT32(p,is_be)) ;
			p += 4 ;
			fprintf( fp , "%x\n" , _INT32(p,is_be)) ;
			p += 4 ;
			fprintf( fp , "%x\n" , _INT32(p,is_be)) ;
			p += 4 ;
			fprintf( fp , "%x\n" , _INT32(p,is_be)) ;
			p += 4 ;
			fprintf( fp , "%x\n" , _INT32(p,is_be)) ;
			p += 4 ;
			fprintf( fp , "%x\n" , _INT32(p,is_be)) ;
			p += 4 ;
			fprintf( fp , "%x\n" , _INT32(p,is_be)) ;
			p += 4 ;
			fprintf( fp , "%x\n" , _INT32(p,is_be)) ;
			p += 4 ;
			fprintf( fp , "%x\n" , _INT32(p,is_be)) ;
			p += 4 ;
			fprintf( fp , "%x\n" , _INT32(p,is_be)) ;
			p += 4 ;
			fprintf( fp , "%x\n\n" , _INT32(p,is_be)) ;
			p += 4 ;
		}

		fclose( fp) ;
	}
#endif

	return  1 ;
}

static int
load_encodings(
	XFontStruct *  xfont ,
	u_char *  p ,
	size_t  size ,
	int  is_be
	)
{
	size_t  idx_size ;

	xfont->min_char_or_byte2 = _INT16(p,is_be) ;
	p += 2 ;
	xfont->max_char_or_byte2 = _INT16(p,is_be) ;
	p += 2 ;
	xfont->min_byte1 = _INT16(p,is_be) ;
	p += 2 ;
	xfont->max_byte1 = _INT16(p,is_be) ;
	p += 2 ;

	/* skip default_char */
	p += 2 ;

	idx_size = ( xfont->max_char_or_byte2 - xfont->min_char_or_byte2 + 1) *
			( xfont->max_byte1 - xfont->min_byte1 + 1) * sizeof(int16_t) ;

	if( size < 14 + idx_size)
	{
	#ifdef  DEBUG
		kik_debug_printf( KIK_DEBUG_TAG " size %d is too small.\n" , size) ;
	#endif

		return  0 ;
	}

	if( ! ( xfont->glyph_indeces = malloc( idx_size)))
	{
		return  0 ;
	}

#ifdef  WORDS_BIG_ENDIAN
	if( is_be)
#else
	if( ! is_be)
#endif
	{
		memcpy( xfont->glyph_indeces , p , idx_size) ;
	}
	else
	{
		size_t  count ;

		for( count = 0 ; count < (idx_size / sizeof(int16_t)) ; count++)
		{
			xfont->glyph_indeces[count] = _INT16(p,is_be) ;
			p += 2 ;
		}
	}

#ifdef  __DEBUG
	kik_debug_printf( KIK_DEBUG_TAG "GLYPH INDEX %d %d %d %d\n" ,
		xfont->min_char_or_byte2 ,
		xfont->max_char_or_byte2 ,
		xfont->max_byte1 ,
		xfont->min_byte1) ;

	{
		FILE *  fp ;
		int  count ;
		int16_t *  p ;

		p = xfont->glyph_indeces ;

		fp = fopen( "log2.txt" , "w") ;

		for( count = xfont->min_char_or_byte2 ;
		     count <= xfont->max_char_or_byte2 ; count++)
		{
			fprintf( fp , "%d %x\n" , count , (int)*p) ;
			p ++ ;
		}

		fclose( fp) ;
	}
#endif

	return  1 ;
}

static int
get_metrics(
	u_int8_t *  width ,
	u_int8_t *  height ,
	u_int8_t *  ascent ,
	u_char *  p ,
	size_t  size ,
	int  is_be ,
	int  is_compressed
	)
{
	int16_t  num_of_metrics ;

	/* XXX Proportional font is not considered. */

	if( is_compressed)
	{
		num_of_metrics = *((u_int16_t*)p) ;
		p += 2 ;

		*width = p[2] - 0x80 ;
		*ascent = p[3] - 0x80 ;
		*height = *ascent + (p[4] - 0x80) ;

	#ifdef  __DEBUG
		kik_debug_printf( KIK_DEBUG_TAG " COMPRESSED METRICS %d %d %d %d\n" ,
			num_of_metrics , *width , *height , *ascent) ;
	#endif
	}
	else
	{
		num_of_metrics = _INT32(p,is_be) ;
		p += 4 ;

		p += 4 ;

		*width = _INT16(p,is_be) ;
		p += 2 ;

		*ascent = _INT16(p,is_be) ;
		p += 2 ;

		*height = *ascent + _INT16(p,is_be) ;

	#ifdef  __DEBUG
		kik_debug_printf( KIK_DEBUG_TAG " NOT COMPRESSED METRICS %d %d %d %d\n" ,
			num_of_metrics , *width , *height , *ascent) ;
	#endif
	}

	return  1 ;
}

static int
load_pcf(
	XFontStruct *  xfont ,
	const char *  file_path
	)
{
	int  fd ;
	struct stat  st ;
	u_char *  pcf = NULL ;
	u_char *  p ;
	int32_t  num_of_tables ;
	int  table_load_count ;
	int32_t  count ;

	if( ( fd = open( file_path , O_RDONLY)) == -1)
	{
	#ifdef  DEBUG
		kik_debug_printf( KIK_DEBUG_TAG " Failed to open %s." , file_path) ;
	#endif

		return  0 ;
	}

	table_load_count = 0 ;

	if( fstat( fd , &st) == -1)
	{
		goto  end ;
	}

	if( ! ( p = pcf = mmap( NULL , st.st_size , PROT_READ , MAP_PRIVATE , fd , 0)))
	{
		goto  end ;
	}

	if( memcmp( p , "\1fcp" , 4) != 0)
	{
		goto  end ;
	}
	p += 4 ;

	num_of_tables = _INT32(p,0) ;
	p += 4 ;

	for( count = 0 ; count < num_of_tables ; count++)
	{
		int32_t  type ;
		int32_t  format ;
		int32_t  size ;
		int32_t  offset ;

		type = _INT32(p,0) ;
		p += 4 ;
		format = _INT32(p,0) ;
		p += 4 ;
		size = _INT32(p,0) ;
		p += 4 ;
		offset = _INT32(p,0) ;
		p += 4 ;

		if( /* (format & 8) != 0 || */ /* MSBit first */
		    ((format >> 4) & 3) != 0 || /* the bits aren't stored in
		                                   bytes(0) but in short(1) or int(2). */
		    offset + size > st.st_size ||
		    format != _INT32(pcf + offset,0) )
		{
		#ifdef  DEBUG
			kik_debug_printf( KIK_DEBUG_TAG
				" %s is unsupported pcf format.\n" , file_path) ;
		#endif
		}
		else if( type == PCF_BITMAPS)
		{
			if( ! load_bitmaps( xfont , pcf + offset + 4 , size ,
					format & 4 , format & 3))
			{
				goto  end ;
			}

			table_load_count ++ ;
		}
		else if( type == PCF_BDF_ENCODINGS)
		{
			if( ! load_encodings( xfont , pcf + offset + 4 , size , format & 4))
			{
				goto  end ;
			}

			table_load_count ++ ;
		}
		else if( type == PCF_METRICS)
		{
			if( ! get_metrics( &xfont->width , &xfont->height , &xfont->ascent ,
					pcf + offset + 4 , size , format & 4 , format & 0x100))
			{
				goto  end ;
			}

			table_load_count ++ ;
		}
	}

#ifdef  __DEBUG
	{
	#if  0
		u_char  ch[] = "\x21\x29" ;
	#elif  1
		u_char  ch[] = "a" ;
	#else
		u_char  ch[] = "\x06\x22" ;	/* UCS2 */
	#endif
		u_char *  bitmap ;
		int  i ;
		int  j ;

		if( ( bitmap = x_get_bitmap( xfont , ch , sizeof(ch) - 1)))
		{
			for( j = 0 ; j < xfont->height ; j++)
			{
				for( i = 0 ; i < xfont->width ; i++)
				{
					kik_msg_printf( "%d" ,
						x_get_bitmap_cell( xfont ,
							bitmap , i , j) ? 1 : 0) ;
				}
				kik_msg_printf( "\n") ;
			}
		}
	}
#endif

end:
	close( fd) ;

	if( pcf)
	{
		munmap( pcf , st.st_size) ;
	}

	if( table_load_count == 3 && ( xfont->file = strdup( file_path)))
	{
		return  1 ;
	}
	else
	{
		return  0 ;
	}
}

static int
unload_pcf(
	XFontStruct *  xfont
	)
{
	if( xfont->file)
	{
		free( xfont->file) ;
	}

	if( xfont->glyphs)
	{
		free( xfont->glyphs) ;
	}

	if( xfont->glyph_offsets)
	{
		free( xfont->glyph_offsets) ;
	}

	if( xfont->glyph_indeces)
	{
		free( xfont->glyph_indeces) ;
	}

	return  1 ;
}


/* --- global functions --- */

int
x_compose_dec_special_font(void)
{
	/* Do nothing for now in fb. */
	return  0 ;
}


x_font_t *
x_font_new(
	Display *  display ,
	ml_font_t  id ,
	x_type_engine_t  type_engine ,
	x_font_present_t  font_present ,
	const char *  fontname ,
	u_int  fontsize ,
	u_int  col_width ,
	int  use_medium_for_bold ,
	u_int  letter_space	/* Ignored for now. */
	)
{
	x_font_t *  font ;
	void *  p ;
	u_int  count ;

	if( ! fontname)
	{
		kik_msg_printf( "Font file is not specified.\n") ;

		return  NULL ;
	}

	if( type_engine != TYPE_XCORE || ! ( font = calloc( 1 , sizeof(x_font_t))))
	{
		return  NULL ;
	}

	for( count = 0 ; count < num_of_xfonts ; count++)
	{
		if( strcmp( xfonts[count]->file , fontname) == 0)
		{
			font->xfont = xfonts[count] ;
			xfonts[count]->ref_count ++ ;

			goto  xfont_loaded ;
		}
	}

	if( ! ( font->xfont = calloc( 1 , sizeof(XFontStruct))))
	{
		free( font) ;

		return  NULL ;
	}

	font->display = display ;

	if( ! load_pcf( font->xfont , fontname) ||
	    ! ( p = realloc( xfonts , sizeof(XFontStruct*) * (num_of_xfonts + 1))))
	{
		unload_pcf( font->xfont) ;
		free( font->xfont) ;
		free( font) ;

		return  NULL ;
	}

	xfonts = p ;
	xfonts[num_of_xfonts++] = font->xfont ;
	font->xfont->ref_count = 1 ;

xfont_loaded:
	/* Following is almost the same processing as xlib. */

	font->id = id ;

	if( font->id & FONT_BIWIDTH)
	{
		font->cols = 2 ;
	}
	else
	{
		font->cols = 1 ;
	}

	/* XXX is_var_col_width is not supported. */
#if  0
	if( ( font_present & FONT_VAR_WIDTH) || IS_ISCII(FONT_CS(font->id)))
	{
		font->is_var_col_width = 1 ;
	}
	else
#endif
	{
		font->is_var_col_width = 0 ;
	}

	if( font_present & FONT_VERTICAL)
	{
		font->is_vertical = 1 ;
	}
	else
	{
		font->is_vertical = 0 ;
	}

	if( use_medium_for_bold)
	{
		font->is_double_drawing = 1 ;
	}
	else
	{
		font->is_double_drawing = 0 ;
	}

	font->width = font->xfont->width ;
	font->height = font->xfont->height ;
	font->ascent = font->xfont->ascent ;

	font->x_off = 0 ;

	if( col_width == 0)
	{
		/* standard(usascii) font */

	#if  0
		if( percent > 0)
		{
			u_int  ch_width ;

			if( font->is_vertical)
			{
				/*
				 * !! Notice !!
				 * The width of full and half character font is the same.
				 */
				ch_width = DIVIDE_ROUNDING( fontsize * percent , 100) ;
			}
			else
			{
				ch_width = DIVIDE_ROUNDING( fontsize * percent , 200) ;
			}

			if( font->width != ch_width)
			{
				if( ! font->is_var_col_width)
				{
					/*
					 * If width(2) of '1' doesn't match ch_width(4)
					 * x_off = (4-2)/2 = 1.
					 * It means that starting position of drawing '1' is 1
					 * as follows.
					 *
					 *  0123
					 * +----+
					 * | ** |
					 * |  * |
					 * |  * |
					 * +----+
					 */
					if( font->width < ch_width)
					{
						font->x_off = (ch_width - font->width) / 2 ;
					}

					font->width = ch_width ;
				}
			}
		}
		else
	#endif
		if( font->is_vertical)
		{
			/*
			 * !! Notice !!
			 * The width of full and half character font is the same.
			 */

			font->x_off = font->width / 2 ;
			font->width *= 2 ;
		}

		if( letter_space > 0)
		{
			font->width += letter_space ;
			font->x_off += (letter_space / 2) ;
		}
	}
	else
	{
		/* not a standard(usascii) font */

		/*
		 * XXX hack
		 * forcibly conforming non standard font width to standard font width.
		 */

		if( font->is_vertical)
		{
			/*
			 * !! Notice !!
			 * The width of full and half character font is the same.
			 */

			if( font->width != col_width)
			{
				kik_msg_printf( "Font(id %x) width(%d) is not matched with "
					"standard width(%d).\n" ,
					font->id , font->width , col_width) ;

				/* is_var_col_width is always false if is_vertical is true. */
			#if  0
				if( ! font->is_var_col_width)
			#endif
				{
					if( font->width < col_width)
					{
						font->x_off = (col_width - font->width) / 2 ;
					}

					font->width = col_width ;
				}
			}
		}
		else
		{
			if( font->width != col_width * font->cols)
			{
				kik_msg_printf( "Font(id %x) width(%d) is not matched with "
					"standard width(%d).\n" ,
					font->id , font->width , col_width * font->cols) ;

				if( ! font->is_var_col_width)
				{
					if( font->width < col_width * font->cols)
					{
						font->x_off = (col_width * font->cols -
								font->width) / 2 ;
					}

					font->width = col_width * font->cols ;
				}
			}
		}
	}


	/*
	 * checking if font width/height/ascent member is sane.
	 */

	if( font->width == 0)
	{
	#ifdef  DEBUG
		kik_warn_printf( KIK_DEBUG_TAG " font width is 0.\n") ;
	#endif

		/* XXX this may be inaccurate. */
		font->width = DIVIDE_ROUNDINGUP( fontsize * font->cols , 2) ;
	}

	if( font->height == 0)
	{
		/* XXX this may be inaccurate. */
		font->height = fontsize ;
	}

	if( font->ascent == 0)
	{
		/* XXX this may be inaccurate. */
		font->ascent = fontsize ;
	}

#ifdef  DEBUG
	kik_debug_printf( KIK_DEBUG_TAG " %s font is loaded.\n" , fontname) ;
#endif

#ifdef  __DEBUG
	x_font_dump( font) ;
#endif

	return  font ;
}

int
x_font_delete(
	x_font_t *  font
	)
{
	if( -- font->xfont->ref_count == 0)
	{
		u_int  count ;

		for( count = 0 ; count < num_of_xfonts ; count++)
		{
			if( strcmp( xfonts[count]->file , font->xfont->file) == 0)
			{
				if( -- num_of_xfonts > 0)
				{
					xfonts[count] = xfonts[--num_of_xfonts] ;
				}
				else
				{
					free( xfonts) ;
					xfonts = NULL ;
				}
			}
		}

	#ifdef  DEBUG
		kik_debug_printf( KIK_DEBUG_TAG
			" %s font is unloaded. => CURRENT NUM OF XFONTS %d\n" ,
			font->xfont->file , num_of_xfonts) ;
	#endif

		unload_pcf( font->xfont) ;
		free( font->xfont) ;
	}

	free( font) ;

	return  1 ;
}

int
x_change_font_cols(
	x_font_t *  font ,
	u_int  cols	/* 0 means default value */
	)
{
	if( cols == 0)
	{
		if( font->id & FONT_BIWIDTH)
		{
			font->cols = 2 ;
		}
		else
		{
			font->cols = 1 ;
		}
	}
	else
	{
		font->cols = cols ;
	}

	return  1 ;
}

u_int
x_calculate_char_width(
	x_font_t *  font ,
	const u_char *  ch ,
	size_t  len ,
	mkf_charset_t  cs
	)
{
	return  font->width ;
}

char **
x_font_get_encoding_names(
	mkf_charset_t  cs
	)
{
	return  NULL ;
}

/* Return written size */
size_t
x_convert_ucs4_to_utf16(
	u_char *  dst ,	/* 4 bytes. Big endian. */
	u_char *  src	/* 4 bytes. Big endian. */
	)
{
#if  0
	kik_debug_printf( KIK_DEBUG_TAG "%.8x => " , mkf_bytes_to_int( src , 4)) ;
#endif

	if( src[0] > 0x0 || src[1] > 0x10)
	{
		return  0 ;
	}
	else if( src[1] == 0x0)
	{
		dst[0] = src[2] ;
		dst[1] = src[3] ;

		return  2 ;
	}
	else /* if( src[1] <= 0x10) */
	{
		/* surrogate pair */

		u_int32_t  linear ;
		u_char  c ;

		linear = mkf_bytes_to_int( src , 4) - 0x10000 ;

		c = (u_char)( linear / (0x100 * 0x400)) ;
		linear -= (c * 0x100 * 0x400) ;
		dst[0] = c + 0xd8 ;

		c = (u_char)( linear / 0x400) ;
		linear -= (c * 0x400) ;
		dst[1] = c ;

		c = (u_char)( linear / 0x100) ;
		linear -= (c * 0x100) ;
		dst[2] = c + 0xdc ;
		dst[3] = (u_char) linear ;

	#if  0
		kik_msg_printf( "%.2x%.2x%.2x%.2x\n" , dst[0] , dst[1] , dst[2] , dst[3]) ;
	#endif

		return  4 ;
	}
}


#ifdef  DEBUG

int
x_font_dump(
	x_font_t *  font
	)
{
	kik_msg_printf( "Font id %x: XFont %p (width %d, height %d, ascent %d, x_off %d)" ,
		font->id , font->xfont , font->width , font->height , font->ascent , font->x_off) ;

	if( font->is_proportional)
	{
		kik_msg_printf( " (proportional)") ;
	}

	if( font->is_var_col_width)
	{
		kik_msg_printf( " (var col width)") ;
	}

	if( font->is_vertical)
	{
		kik_msg_printf( " (vertical)") ;
	}

	if( font->is_double_drawing)
	{
		kik_msg_printf( " (double drawing)") ;
	}

	kik_msg_printf( "\n") ;

	return  1 ;
}

#endif


u_char *
x_get_bitmap(
	XFontStruct *  xfont ,
	u_char *  ch ,
	size_t  len
	)
{
	size_t  ch_idx ;
	int16_t  glyph_idx ;
	int32_t  glyph_offset ;

	if( len == 2)
	{
		ch_idx = (ch[0] - xfont->min_byte1) *
			 (xfont->max_char_or_byte2 - xfont->min_char_or_byte2 + 1) +
			ch[1] - xfont->min_char_or_byte2 ;
	}
	else /* if( len == 1) */
	{
		ch_idx = ch[0] - xfont->min_char_or_byte2 ;
	}

	if( ch_idx >= ( xfont->max_char_or_byte2 - xfont->min_char_or_byte2 + 1) *
			( xfont->max_byte1 - xfont->min_byte1 + 1) ||
	    ( glyph_idx = xfont->glyph_indeces[ ch_idx]) == -1)
	{
		return  NULL ;
	}

	glyph_offset = xfont->glyph_offsets[ glyph_idx] ;

#if  0
	kik_debug_printf( KIK_DEBUG_TAG " chindex %d glindex %d glyph offset %d\n" ,
		ch_idx , glyph_idx , glyph_offset) ;
#endif

	return  xfont->glyphs + glyph_offset ;
}

int
x_get_bitmap_cell(
	XFontStruct *  xfont ,
	u_char *  bitmap ,
	int  x ,
	int  y		/* y < height must be checked by the caller. */
	)
{
	if( bitmap && x < xfont->width)
	{
		if( xfont->glyphs_same_endian) /* XXX ? */
		{
			return  bitmap[y * xfont->glyph_width_bytes + x / 8] & (1 << (x % 8)) ;
		}
		else
		{
			return  bitmap[y * xfont->glyph_width_bytes + x / 8] &
					(1 << (8 - (x % 8) - 1)) ;
		}
	}
	else
	{
		return  0 ;
	}
}
