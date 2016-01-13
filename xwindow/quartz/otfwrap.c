/*
 *	$Id$
 */

#ifdef  USE_GSUB

#include  "otfwrap.h"

#include  <otf.h>
#include  <dlfcn.h>
#include  <kiklib/kik_debug.h>


/* --- static functions --- */

static OTF *  (*open)( const char *) ;
static void  (*close)( OTF *) ;
static int  (*check_table)( OTF * , const char *) ;
static int  (*drive_cmap)( OTF * , OTF_GlyphString *) ;
static int  (*drive_gsub)( OTF * , OTF_GlyphString * ,
		const char * , const char * , const char *) ;


/* --- global functions --- */

void *
otf_open(
	const char *  name
	)
{
	static int  is_tried ;
	OTF *  otf ;

	if( ! is_tried)
	{
		void *  handle ;

		is_tried = 1 ;

		if( ! ( handle = dlopen( "@executable_path/lib/gtk/libotf.0.dylib" , RTLD_LAZY)) &&
		    ! ( handle = dlopen( "libotf.0.dylib" , RTLD_LAZY)))
		{
			kik_error_printf( "libotf: Could not load.\n") ;

			return  NULL ;
		}

	#ifdef  DEBUG
		kik_debug_printf( KIK_DEBUG_TAG " Loading libotf.0.dylib\n") ;
	#endif

		if( ! ( open = dlsym( handle , "OTF_open")) ||
		    ! ( close = dlsym( handle , "OTF_close")) ||
		    ! ( check_table = dlsym( handle , "OTF_check_table")) ||
		    ! ( drive_cmap = dlsym( handle , "OTF_drive_cmap")) ||
		    ! ( drive_gsub = dlsym( handle , "OTF_drive_gsub")))
		{
			open = NULL ;
			dlclose( handle) ;
		}
	}
	else if( ! open)
	{
		return  NULL ;
	}

	if( ( otf = (*open)( name)))
	{
		if( (*check_table)( otf , "GSUB") == 0 &&
		    (*check_table)( otf , "cmap") == 0)
		{
			return  otf ;
		}

		(*close)( otf) ;
	}

	return  NULL ;
}

void
otf_close(
	void *  otf
	)
{
	return  (*close)( otf) ;
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
