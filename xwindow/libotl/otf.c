/*
 *	$Id$
 */

#include  <otf.h>
#include  <kiklib/kik_debug.h>
#include  <kiklib/kik_mem.h>


/* --- static functions --- */

#ifdef  USE_WIN32GUI
static void *
OTF_open_win32(
	void *  obj ,
	u_int  size
	)
{
}
#endif


/* --- global functions --- */

#ifdef  NO_DYNAMIC_LOAD_OTL
static
#endif
void *
otl_open(
	void *  obj ,
	u_int  size
	)
{
	OTF *  otf ;

#if  defined(USE_WIN32GUI)
	FT_Library  ftlib ;
	FT_Face  face ;

	if( FT_Init_FreeType( &ftlib) != 0)
	{
		return  NULL ;
	}

	if( FT_New_Memory_Face)( ftlib , font_data , font_data_size , 0 , &face) != 0)
	{
		otf = NULL ;
	}
	else if( ( otf = OTF_open( face)))
	{
		if( OTF_get_table( otf , "GSUB") != 0 ||
		    OTF_get_table( otf , "cmap") != 0)
		{
			(*close)( otf) ;
			otf = NULL ;
		}

		FT_Done_Face( face) ;
	}

	FT_Done_FreeType( ftlib) ;
#else
#if  defined(USE_QUARTZ)
	if( ( otf = OTF_open( obj)))
#else
	if( ( otf = OTF_open_ft_face( obj)))
#endif
	{
		if( OTF_check_table( otf , "GSUB") != 0 ||
		    OTF_check_table( otf , "cmap") != 0)
		{
			OTF_close( otf) ;
			otf = NULL ;
		}
	}
#endif

	return  otf ;
}

#ifdef  NO_DYNAMIC_LOAD_OTL
static
#endif
void
otl_close(
	void *  otf
	)
{
	OTF_close( otf) ;
}

#ifdef  NO_DYNAMIC_LOAD_OTL
static
#endif
u_int
otl_convert_text_to_glyphs(
	void *  otf ,
	u_int32_t *  shaped ,
	u_int  shaped_len ,
	int8_t *  offsets ,
	u_int8_t *  widths ,
	u_int32_t *  cmapped ,
	u_int32_t *  src ,
	u_int  src_len ,
	const char *  script ,
	const char *  features
	)
{
	static OTF_Glyph *  glyphs ;
	OTF_GlyphString  otfstr ;
	u_int  count ;

	otfstr.size = otfstr.used = src_len ;

/* Avoid kik_mem memory management */
#undef  realloc
	if( ! ( otfstr.glyphs = realloc( glyphs , otfstr.size * sizeof(*otfstr.glyphs))))
	{
		return  0 ;
	}
#ifdef  KIK_DEBUG
#define  realloc( ptr , size)  kik_mem_realloc( ptr , size , __FILE__ , __LINE__ , __FUNCTION)
#endif

	glyphs = otfstr.glyphs ;
	memset( otfstr.glyphs , 0 , otfstr.size * sizeof(*otfstr.glyphs)) ;

	if( src)
	{
		for( count = 0 ; count < src_len ; count++)
		{
			otfstr.glyphs[count].c = src[count] ;
		}

		OTF_drive_cmap( otf , &otfstr) ;

		if( cmapped)
		{
			for( count = 0 ; count < otfstr.used ; count++)
			{
				cmapped[count] = otfstr.glyphs[count].glyph_id ;
			}

			return  otfstr.used ;
		}
		else
		{
		#if  0
			if( ! offsets || ! widths)
			{
				/* do nothing */
			}
			else
		#endif
			{
				memcpy( offsets , 0 , shaped_len * sizeof(*offsets)) ;
				memcpy( widths , 0 , shaped_len * sizeof(*widths)) ;
			}
		}
	}
	else /* if( cmapped) */
	{
		for( count = 0 ; count < src_len ; count++)
		{
			otfstr.glyphs[count].glyph_id = cmapped[count] ;
		}
	}

	OTF_drive_gsub( otf , &otfstr , script , NULL , features) ;

	for( count = 0 ; count < otfstr.used ; count++)
	{
		shaped[count] = otfstr.glyphs[count].glyph_id ;
	}

	return  otfstr.used ;
}
