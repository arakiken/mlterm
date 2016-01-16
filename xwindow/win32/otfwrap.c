/*
 *	$Id$
 */

#ifdef  USE_GSUB

#include  "otfwrap.h"

#include  <otf.h>
#include  <kiklib/kik_dlfcn.h>
#include  <kiklib/kik_debug.h>


/* --- static functions --- */

static FT_Error  (*init_freetype)( FT_Library *) ;
static FT_Error  (*done_freetype)( FT_Library) ;
static FT_Error  (*new_memory_face)( FT_Library , const FT_Byte * , FT_Long ,
			FT_Long , FT_Face *) ;
static FT_Error  (*done_face)( FT_Face) ;

static OTF *  (*open)( FT_Face) ;
static void  (*close)( OTF *) ;
static int  (*get_table)( OTF * , const char *) ;
static int  (*drive_cmap)( OTF * , OTF_GlyphString *) ;
static int  (*drive_gsub)( OTF * , OTF_GlyphString * ,
		const char * , const char * , const char *) ;


/* --- global functions --- */

void *
otf_open(
	void *  font_data ,
	u_int  font_data_size
	)
{
	static int  is_tried ;
	FT_Library  ftlib ;
	FT_Face  face ;
	OTF *  otf ;

	if( ! is_tried)
	{
		kik_dl_handle_t  otf_handle ;
		kik_dl_handle_t  ft_handle ;

		is_tried = 1 ;

		if( ! ( otf_handle = kik_dl_open( "" , "otf-0")) ||
		    ! ( open = kik_dl_func_symbol( otf_handle , "OTF_open_ft_face")) ||
		    ! ( close = kik_dl_func_symbol( otf_handle , "OTF_close")) ||
		    ! ( get_table = kik_dl_func_symbol( otf_handle , "OTF_get_table")) ||
		    ! ( drive_cmap = kik_dl_func_symbol( otf_handle , "OTF_drive_cmap")) ||
		    ! ( drive_gsub = kik_dl_func_symbol( otf_handle , "OTF_drive_gsub")))
		{
			kik_error_printf( "libotf: Could not load.\n") ;

			if( otf_handle)
			{
				kik_dl_close( otf_handle) ;
			}

			return  NULL ;
		}

		if( ! ( ft_handle = kik_dl_open( "" , "freetype-6")) ||
		    ! ( init_freetype = kik_dl_func_symbol( ft_handle , "FT_Init_FreeType")) ||
		    ! ( done_freetype = kik_dl_func_symbol( ft_handle , "FT_Done_FreeType")) ||
		    ! ( new_memory_face = kik_dl_func_symbol( ft_handle , "FT_New_Memory_Face")) ||
		    ! ( done_face = kik_dl_func_symbol( ft_handle , "FT_Done_Face")))
		{
			kik_error_printf( "libfreetype: Could not load.\n") ;

			kik_dl_close( otf_handle) ;

			if( ft_handle)
			{
				kik_dl_close( ft_handle) ;
			}

			init_freetype = NULL ;

			return  NULL ;
		}
	}
	else if( ! init_freetype)
	{
		return  NULL ;
	}

	if( (*init_freetype)( &ftlib) != 0)
	{
		return  NULL ;
	}

	if( (*new_memory_face)( ftlib , font_data , font_data_size , 0 , &face) != 0)
	{
		otf = NULL ;
	}
	else if( ( otf = (*open)( face)))
	{
		if( (*get_table)( otf , "GSUB") != 0 ||
		    (*get_table)( otf , "cmap") != 0)
		{
			(*close)( otf) ;
			otf = NULL ;
		}

		(*done_face)( face) ;
	}

	(*done_freetype)( ftlib) ;

	return  otf ;
}

void
otf_close(
	void *  otf
	)
{
	(*close)( otf) ;
}

u_int
otf_convert_text_to_glyphs(
	void *  otf ,
	u_int32_t *  gsub ,
	u_int  gsub_len ,
	u_int32_t *  cmap ,
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

	if( ! ( otfstr.glyphs = realloc( glyphs , otfstr.size * sizeof(*otfstr.glyphs))))
	{
		return  0 ;
	}

	glyphs = otfstr.glyphs ;
	memset( otfstr.glyphs , 0 , otfstr.size * sizeof(*otfstr.glyphs)) ;

	if( src)
	{
		for( count = 0 ; count < src_len ; count++)
		{
			otfstr.glyphs[count].c = src[count] ;
		}

		(*drive_cmap)( otf , &otfstr) ;

		if( cmap)
		{
			for( count = 0 ; count < otfstr.used ; count++)
			{
				cmap[count] = otfstr.glyphs[count].glyph_id ;
			}

			return  otfstr.used ;
		}
	}
	else /* if( cmap) */
	{
		for( count = 0 ; count < src_len ; count++)
		{
			otfstr.glyphs[count].glyph_id = cmap[count] ;
		}
	}

	(*drive_gsub)( otf , &otfstr , script , NULL , features) ;

	for( count = 0 ; count < otfstr.used ; count++)
	{
		gsub[count] = otfstr.glyphs[count].glyph_id ;
	}

	return  otfstr.used ;
}

#endif
