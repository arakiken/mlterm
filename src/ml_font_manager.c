/*
 *	$Id$
 */

#include  "ml_font_manager.h"

#include  <string.h>		/* strcat/memset */
#include  <stdio.h>		/* sprintf */
#include  <kiklib/kik_mem.h>	/* malloc/alloca */
#include  <kiklib/kik_str.h>	/* kik_str_dup */
#include  <kiklib/kik_util.h>	/* DIGIT_STR_LEN */
#include  <kiklib/kik_conf_io.h>
#include  <kiklib/kik_file.h>

#include  "ml_font_intern.h"


#if  0
#define  __DEBUG
#endif


typedef struct encoding_to_cs_table
{
	ml_char_encoding_t  encoding ;
	mkf_charset_t  cs ;

} encoding_to_cs_table_t ;


/* --- static variables --- */

/*
 * !!! Notice !!!
 * the order should be the same as ml_char_encoding_t in ml_char_encoding.h
 */
static encoding_to_cs_table_t  usascii_font_cs_table[] =
{
	{ ML_ISO8859_1 , ISO8859_1_R } ,
	{ ML_ISO8859_2 , ISO8859_2_R } ,
	{ ML_ISO8859_3 , ISO8859_3_R } ,
	{ ML_ISO8859_4 , ISO8859_4_R } ,
	{ ML_ISO8859_5 , ISO8859_5_R } ,
	{ ML_ISO8859_6 , ISO8859_6_R } ,
	{ ML_ISO8859_7 , ISO8859_7_R } ,
	{ ML_ISO8859_8 , ISO8859_8_R } ,
	{ ML_ISO8859_9 , ISO8859_9_R } ,
	{ ML_ISO8859_10 , ISO8859_10_R } ,
	{ ML_TIS620 , TIS620_2533 } ,
	{ ML_ISO8859_13 , ISO8859_13_R } ,
	{ ML_ISO8859_14 , ISO8859_14_R } ,
	{ ML_ISO8859_15 , ISO8859_15_R } ,
	{ ML_ISO8859_16 , ISO8859_16_R } ,
	{ ML_TCVN5712 , TCVN5712_3_1993 } ,
	
	{ ML_VISCII , VISCII } ,
	{ ML_KOI8_R , KOI8_R } ,
	{ ML_KOI8_U , KOI8_U } ,
#ifdef  USE_UCS4
	{ ML_UTF8 , ISO10646_UCS4_1 } ,
#else
	{ ML_UTF8 , ISO10646_UCS2_1 } ,
#endif
	
} ;


/* --- static functions --- */

#ifdef  DEBUG

static void
dump_cached_fonts(
	ml_font_manager_t *  font_man
	)
{
	int  counter ;
	u_int  size ;
	KIK_PAIR( ml_font) *  f_array ;
	
	fprintf( stderr , "these fonts are cached ->\n") ;
#ifdef  ANTI_ALIAS
	fprintf( stderr , "  usascii: XftFont %p\n" , font_man->usascii_font->xft_font) ;
#endif
	fprintf( stderr , "  usascii: XFont %li\n" , font_man->usascii_font->xfont->fid) ;
	
	kik_map_get_pairs_array( font_man->font_cache_table , f_array , size) ;
	for( counter = 0 ; counter < size ; counter++)
	{
		if( f_array[counter]->value != NULL)
		{
		#ifdef  ANTI_ALIAS
			fprintf( stderr , "  attr %x: XftFont %p\n" , f_array[counter]->value->attr ,
				f_array[counter]->value->xft_font) ;
		#endif
			fprintf( stderr , "  attr %x: XFont %li\n" , f_array[counter]->value->attr ,
				f_array[counter]->value->xfont->fid) ;
		}
	}
}

#endif

static int
fontattr_hash(
	ml_font_attr_t  fontattr ,
	u_int  size
	)
{
	return  fontattr % size ;
}

static int
fontattr_compare(
	ml_font_attr_t  key1 ,
	ml_font_attr_t  key2
	)
{
	return  (key1 == key2) ;
}

static mkf_charset_t
get_usascii_font_cs(
	ml_char_encoding_t  encoding
	)
{
	if( encoding < 0 || sizeof( usascii_font_cs_table) / sizeof( usascii_font_cs_table[0]) <= encoding)
	{
		return  ISO8859_1_R ;
	}
	else if( encoding != usascii_font_cs_table[encoding].encoding)
	{
	#ifdef  DEBUG
		kik_warn_printf( KIK_DEBUG_TAG " %x is illegal encoding.\n") ;
	#endif

		return  ISO8859_1_R ;
	}
	else
	{
	#ifdef  __DEBUG
		kik_debug_printf( KIK_DEBUG_TAG " us ascii font is %x cs\n" ,
			usascii_font_cs_table[encoding].cs) ;
	#endif
	
		return  usascii_font_cs_table[encoding].cs ;
	}
}

static int
set_usascii_xfont(
	ml_font_manager_t *  font_man ,
	ml_font_t *  font ,
	int  use_medium_for_bold
	)
{
	char *  fontname ;
	ml_font_attr_t  attr ;
	ml_font_attr_t  orig_attr ;

	attr = DEFAULT_FONT_ATTR(get_usascii_font_cs(font_man->encoding)) ;

	fontname = ml_get_font_name_for_attr( font_man->font_custom , font_man->font_size , attr) ;

	orig_attr = font->attr ;
	font->attr = attr ;

	if( ! (*font_man->set_xfont)( font , fontname , font_man->font_size ,
		0 , use_medium_for_bold))
	{
		font->attr = orig_attr ;
		
		return  0 ;
	}
	else
	{
		font->attr = orig_attr ;

		return  1 ;
	}
}

static ml_font_t *
get_font_intern(
	ml_font_manager_t *  font_man ,
	ml_font_attr_t  fontattr ,
	int  cache_null_if_not_found
	)
{
	int  result ;
	ml_font_t *  font ;
	KIK_PAIR( ml_font)  fn_pair ;
	char *  fontname ;
	int  use_medium_for_bold ;
	u_int  col_width ;

	/* mapping US_ASCII to appropriate charset */
	if( FONT_CS(fontattr) == US_ASCII)
	{
		fontattr &= ~US_ASCII ;
		fontattr |= get_usascii_font_cs( font_man->encoding) ;
		col_width = 0 ;
	}
	else
	{
		col_width = ml_col_width( font_man) ;
	}
	
	kik_map_get( result , font_man->font_cache_table , fontattr , fn_pair) ;
	if( result)
	{
		return  fn_pair->value ;
	}

	if( ( font = ml_font_new( font_man->display , fontattr)) == NULL)
	{
		return  NULL ;
	}

	if( ( fontname = ml_get_font_name_for_attr( font_man->font_custom , font_man->font_size , fontattr)))
	{
		use_medium_for_bold = 0 ;
	}
	else
	{
		/* fontname == NULL */
		
		use_medium_for_bold = 0 ;
		
		if( fontattr & FONT_BOLD)
		{
			if( ( fontname = ml_get_font_name_for_attr( font_man->font_custom ,
				font_man->font_size , (fontattr & ~FONT_BOLD) | FONT_MEDIUM)))
			{
				use_medium_for_bold = 1 ;
			}
		}
	}

	if( (*font_man->set_xfont)( font , fontname , font_man->font_size ,
		col_width , use_medium_for_bold) == 0)
	{
	#ifdef  DEBUG
		kik_warn_printf( KIK_DEBUG_TAG " font for attr %x doesn't exist.\n" , font->attr) ;
	#endif
	
		ml_font_delete( font) ;

		if( cache_null_if_not_found)
		{
			/*
			 * this font doesn't exist , NULL is set.
			 * caching the fact that this font attr doesn't exist.
			 */

			font = NULL ;
		}
		else
		{
			return  NULL ;
		}
	}
	
	kik_map_set( result , font_man->font_cache_table , fontattr , font) ;

#ifdef  DEBUG
	kik_warn_printf( KIK_DEBUG_TAG " font for attr %x was cached.\n" , fontattr) ;
#endif

	return  font ;
}	


/* --- global functions --- */

ml_font_manager_t *
ml_font_manager_new(
	Display *  display ,
	ml_font_custom_t *  normal_font_custom ,
	ml_font_custom_t *  aa_font_custom ,
	u_int  font_size ,
	ml_char_encoding_t  encoding
	)
{
	ml_font_manager_t *  font_man ;
	
	if( ( font_man = malloc( sizeof( ml_font_manager_t))) == NULL)
	{
	#ifdef  DEBUG
		kik_warn_printf( KIK_DEBUG_TAG " malloc() failed.\n") ;
	#endif
	
		return  NULL ;
	}

	font_man->normal_font_custom = normal_font_custom ;
	font_man->aa_font_custom = aa_font_custom ;

	font_man->font_custom = font_man->normal_font_custom ;

	font_man->font_size = font_size ;

	kik_map_new( ml_font_attr_t , ml_font_t * , font_man->font_cache_table , 
		fontattr_hash , fontattr_compare) ;

	font_man->display = display ;

	font_man->usascii_font = NULL ;
	font_man->encoding = encoding ;

	font_man->set_xfont = ml_font_set_xfont ;

	return  font_man ;
}

int
ml_font_manager_delete(
	ml_font_manager_t *  font_man
	)
{
	int  counter ;
	u_int  size ;
	KIK_PAIR( ml_font) *  f_array ;

	kik_map_get_pairs_array( font_man->font_cache_table , f_array , size) ;
	for( counter = 0 ; counter < size ; counter++)
	{
		if( f_array[counter]->value != NULL)
		{
			ml_font_delete( f_array[counter]->value) ;
		}
	}

	kik_map_delete( font_man->font_cache_table) ;

	free( font_man) ;

	return  1 ;
}

int
ml_font_manager_use_aa(
	ml_font_manager_t *  font_man
	)
{
#ifdef  ANTI_ALIAS
	if( font_man->font_custom == font_man->aa_font_custom)
	{
		return  0 ;
	}
	
	font_man->set_xfont = ml_font_set_xft_font ;
	font_man->font_custom = font_man->aa_font_custom ;

	return  1 ;
#else
	/* always false */
	
	return  0 ;
#endif
}

int
ml_font_manager_unuse_aa(
	ml_font_manager_t *  font_man
	)
{
#ifdef  ANTI_ALIAS
	if( font_man->font_custom == font_man->normal_font_custom)
	{
		return  0 ;
	}
	
	font_man->set_xfont = ml_font_set_xfont ;
	font_man->font_custom = font_man->normal_font_custom ;

	return  1 ;
#else
	/* always false */
	
	return  0 ;
#endif
}

ml_font_t *
ml_get_font(
	ml_font_manager_t *  font_man ,
	ml_font_attr_t  fontattr
	)
{
	return  get_font_intern( font_man , fontattr , 1 /* cache NULL if the font not found */) ;
}

ml_font_t *
ml_get_usascii_font(
	ml_font_manager_t *  font_man
	)
{
	if( font_man->usascii_font == NULL)
	{
		ml_font_attr_t  fontattr ;

		fontattr = DEFAULT_FONT_ATTR(US_ASCII) ;
		
		if( ( font_man->usascii_font = get_font_intern( font_man , fontattr ,
			0 /* don't cache NULL if the font not found */)) == NULL)
		{
			u_int  beg_size ;

			beg_size = font_man->font_size ;

			while( 1)
			{
				if( ++ font_man->font_size > font_man->font_custom->max_font_size)
				{
					font_man->font_size = font_man->font_custom->min_font_size ;
				}

				if( font_man->font_size == beg_size)
				{
					kik_msg_printf(
						"a font for displaying us ascii chars is not found.\n") ;
					exit(1) ;
				}
				
				if( ( font_man->usascii_font =
					get_font_intern( font_man , fontattr ,
					0 /* don't cache NULL if the font not found */)))
				{
					break ;
				}
			}
		}
	}

	return  font_man->usascii_font ;
}

u_int
ml_col_width(
	ml_font_manager_t *  font_man
	)
{
	ml_font_t *  usascii_font ;
	
	if( ( usascii_font = ml_get_usascii_font( font_man)) == NULL)
	{
		kik_error_printf( "usascii font is not set.\n") ;
		
		/* XXX avoiding zero division */
		return  1 ;
	}
	
	return  usascii_font->width ;
}

u_int
ml_line_height(
	ml_font_manager_t *  font_man
	)
{
	ml_font_t *  usascii_font ;
	
	if( ( usascii_font = ml_get_usascii_font( font_man)) == NULL)
	{
		kik_error_printf( "usascii font is not set.\n") ;
		
		/* XXX avoiding zero division */
		return  1 ;
	}

	return  usascii_font->height ;
}

u_int
ml_line_height_to_baseline(
	ml_font_manager_t *  font_man
	)
{
	ml_font_t *  usascii_font ;
	
	if( ( usascii_font = ml_get_usascii_font( font_man)) == NULL)
	{
		kik_error_printf( "usascii font is not set.\n") ;
		
		/* XXX avoiding zero division */
		return  1 ;
	}

	return  usascii_font->height_to_baseline ;
}

int
ml_font_manager_change_encoding(
	ml_font_manager_t *  font_man ,
	ml_char_encoding_t  encoding
	)
{
	mkf_charset_t  cs ;
	ml_font_t *  usascii_font ;
	ml_font_attr_t  orig_attr ;
	ml_char_encoding_t  orig_encoding ;
	int  result ;
	
	if( ( cs = get_usascii_font_cs( encoding)) == get_usascii_font_cs( font_man->encoding))
	{
		/* usascii font size never changed */
		
		return  0 ;
	}

	if( ( usascii_font = ml_get_usascii_font( font_man)) == NULL)
	{
		/* critical error */
		
		return  0 ;
	}
	
	orig_encoding = font_man->encoding ;
	font_man->encoding = encoding ;

	orig_attr = usascii_font->attr ;
	ml_change_font_cs( usascii_font , get_usascii_font_cs( encoding)) ;

	while( ! set_usascii_xfont( font_man , usascii_font , 0))
	{
		/*
		 * searching this cs font of other sizes...
		 */
		 
		if( ! ml_larger_font( font_man))
		{
			/* failed */

			font_man->encoding = orig_encoding ;
			usascii_font->attr = orig_attr ;

			return  -1 ;
		}
	}

	kik_map_erase( result , font_man->font_cache_table , orig_attr) ;
	kik_map_set( result , font_man->font_cache_table , usascii_font->attr , usascii_font) ;
	
	/* us ascii font size may be changed. */

#ifdef  DEBUG
	kik_warn_printf( KIK_DEBUG_TAG) ;
	dump_cached_fonts( font_man) ;
#endif
	
	return  1 ;
}

int
ml_change_font_size(
	ml_font_manager_t *  font_man ,
	u_int  font_size
	)
{
	KIK_PAIR( ml_font) *  array ;
	ml_font_t *  usascii_font ;
	u_int  size ;
	int  counter ;

#if  0
	/*
	 * !! Notice !!
	 * this check should be done by the caller , because if "font_size" is not changed ,
	 * *actual* font size may be changed.(e.g. XftFont <=> XFontStruct)
	 */
	if( font_size == font_man->font_size)
	{
		/* not changed */
		
		return  1 ;
	}
#endif

#ifdef  __DEBUG
	kik_debug_printf( KIK_DEBUG_TAG " changing font size to %d\n" , font_size) ;
#endif

	if( font_size < font_man->font_custom->min_font_size ||
		font_man->font_custom->max_font_size < font_size)
	{
	#ifdef  DEBUG
		kik_warn_printf( KIK_DEBUG_TAG " font_size %d is illegal.\n" , font_size) ;
	#endif
	
		return  0 ;
	}

	font_man->font_size = font_size ;
	
	if( ( usascii_font = ml_get_usascii_font( font_man)) == NULL)
	{
		/* critical error */
		
	#ifdef  DEBUG
		kik_warn_printf( KIK_DEBUG_TAG
			" usascii font of size %d is not found. font size not changed.\n" ,
			font_man->font_size) ;
	#endif

		return  0 ;
	}
	
	if( set_usascii_xfont( font_man , usascii_font , 0) == 0)
	{
	#ifdef  DEBUG
		kik_warn_printf( KIK_DEBUG_TAG
			" usascii font of size %d is not found. font size not changed.\n" ,
			font_size) ;
	#endif

		return  0 ;
	}
	
	kik_map_get_pairs_array( font_man->font_cache_table , array , size) ;

	for( counter = 0 ; counter < size ; counter ++)
	{
		ml_font_attr_t  attr ;
		ml_font_t *  font ;
		char *  fontname ;
		int  use_medium_for_bold ;

		attr = array[counter]->key ;
		if( (font = array[counter]->value) == NULL)
		{
			if( ( font = ml_font_new( font_man->display , attr)) == NULL)
			{
			#ifdef  DEBUG
				kik_warn_printf( KIK_DEBUG_TAG " ml_font_new() failed.\n") ;
			#endif
			
				continue ;
			}

			array[counter]->value = font ;
		}

		if( attr == DEFAULT_FONT_ATTR(get_usascii_font_cs( font_man->encoding)))
		{
			/* usascii default font is already loaded. */
			
			continue ;
		}

		if( ( fontname = ml_get_font_name_for_attr( font_man->font_custom ,
			font_man->font_size ,attr)))
		{
			use_medium_for_bold = 0 ;
		}
		else
		{
			/* fontname == NULL */
			
			use_medium_for_bold = 0 ;

			if( attr & FONT_BOLD)
			{
				if( ( fontname = ml_get_font_name_for_attr( font_man->font_custom ,
					font_man->font_size , (attr & ~FONT_BOLD) | FONT_MEDIUM)))
				{
					use_medium_for_bold = 1 ;
				}
			}
		}
		
		if( (*font_man->set_xfont)( font , fontname , font_man->font_size ,
			ml_col_width( font_man) , use_medium_for_bold) == 0)
		{
		#ifdef  DEBUG
			kik_warn_printf( KIK_DEBUG_TAG
				" larger font for attr %x is not found. us ascii font is used instead.\n" ,
				font->attr) ;
		#endif

			if( attr & FONT_BOLD)
			{
				use_medium_for_bold = 1 ;
			}
			else
			{
				use_medium_for_bold = 0 ;
			}

			if( set_usascii_xfont( font_man , font , use_medium_for_bold) == 0)
			{
			#ifdef  DEBUG
				kik_warn_printf( KIK_DEBUG_TAG " this should never happen.\n") ;
			#endif
			}
		}
	}
	
#ifdef  DEBUG
	dump_cached_fonts( font_man) ;
#endif

	return  1 ;
}

int
ml_larger_font(
	ml_font_manager_t *  font_man
	)
{
	u_int  beg_size ;
	u_int  size ;

	beg_size = font_man->font_size ;
	size = font_man->font_size ;
	
	while( 1)
	{
		if( ++ size > font_man->font_custom->max_font_size)
		{
			size = font_man->font_custom->min_font_size ;
		}

		if( size == beg_size)
		{
			return  0 ;
		}

		if( ml_change_font_size( font_man , size))
		{
			return  1 ;
		}
	}
}

int
ml_smaller_font(
	ml_font_manager_t *  font_man
	)
{
	u_int  beg_size ;
	u_int  size ;

	beg_size = font_man->font_size ;
	size = font_man->font_size ;

	while( 1)
	{
		if( size == font_man->font_custom->min_font_size)
		{
			size = font_man->font_custom->max_font_size ;
		}
		else
		{
			size -- ;
		}

		if( size == beg_size)
		{
			return  0 ;
		}
		
		if( ml_change_font_size( font_man , size))
		{
			return  1 ;
		}
	}
}

XFontSet
ml_get_fontset(
	ml_font_manager_t *  font_man
	)
{
	XFontSet  fontset ;
	char *  default_font_name ;
	char *  list_str ;
	u_int  list_str_len ;
	char **  missing ;
	int  miss_num ;
	char *  def_str ;
	char **  fontnames ;
	u_int  size ;
	int  counter ;

	/* "+ 1" is for '\0' */
	if( ( default_font_name = alloca( 22 + DIGIT_STR_LEN(font_man->font_size) + 1)) == NULL)
	{
		return  NULL ;
	}

	sprintf( default_font_name , "-*-*-*-*-*--%d-*-*-*-*-*" , font_man->font_size) ;

	list_str_len = strlen( default_font_name) ;

	if( font_man->font_custom != font_man->aa_font_custom)
	{
		if( ( size = ml_get_all_font_names( font_man->font_custom , 
			&fontnames , font_man->font_size)) > 0)
		{
			for( counter = 0 ; counter < size ; counter ++)
			{
				list_str_len += (1 + strlen( fontnames[counter])) ;
			}
		}
	}
	else
	{
		size = 0 ;
	}
	
	if( ( list_str = alloca( sizeof( char) * list_str_len)) == NULL)
	{
		return  NULL ;
	}
	*list_str = '\0' ;

	for( counter = 0 ; counter < size ; counter ++)
	{
		if( strstr( list_str , fontnames[counter]))
		{
			/* removing the same name fonts. */

			continue ;
		}

		strcat( list_str , fontnames[counter]) ;
		strcat( list_str , ",") ;
	}

	if( size > 0)
	{
		free( fontnames) ;
	}

	strcat( list_str , default_font_name) ;

#ifdef  __DEBUG
	kik_debug_printf( KIK_DEBUG_TAG " font set list -> %s\n" , list_str) ;
#endif

	fontset = XCreateFontSet( font_man->display , list_str , &missing , &miss_num , &def_str) ;

#ifdef  DEBUG
	if( miss_num)
	{
	#ifdef  ANTI_ALIAS
		int  counter ;
	#endif
	
		kik_warn_printf( KIK_DEBUG_TAG " missing these fonts ...\n") ;
		for( counter = 0 ; counter < miss_num ; counter ++)
		{
			fprintf( stderr , " %s\n" , missing[counter]) ;
		}
	}
#endif

	XFreeStringList( missing) ;

	return  fontset ;
}
