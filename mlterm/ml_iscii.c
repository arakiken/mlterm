/*
 *	$Id$
 */

#include  "ml_iscii.h"

#include  <string.h>		/* memset */
#include  <kiklib/kik_mem.h>
#include  <kiklib/kik_debug.h>
#include  <kiklib/kik_util.h>


#if  0
#define  __DEBUG
#endif


/* --- static variables --- */

static char *  iscii_langs[] =
{
	"Assamese" ,
	"Bengali",
	"Gujarati",
	"Hindi",
	"Kannada",
	"Malayalam",
	"Oriya",
	"Punjabi",
	"Roman",
	"Tamil",
	"Telugu",
	
} ;


/* --- global functions --- */

ml_iscii_lang_type_t
ml_iscii_get_lang(
	char *  name
	)
{
	ml_iscii_lang_type_t  lang ;

	for( lang = 0 ; lang < MAX_ISCIILANG ; lang ++)
	{
		if( strcasecmp( iscii_langs[lang] , name) == 0)
		{
			return  lang ;
		}
	}

	return  ISCIILANG_UNKNOWN ;
}

char *
ml_iscii_get_lang_name(
	ml_iscii_lang_type_t  type
	)
{
	if( ISCIILANG_UNKNOWN < type && type < MAX_ISCIILANG)
	{
		return  iscii_langs[type] ;
	}
	else
	{
		return  NULL ;
	}
}


#ifdef  USE_IND

#include  <ctype.h>		/* isdigit */
#include  <indian.h>
#include  <kiklib/kik_str.h>	/* kik_snprintf */


struct  ml_iscii_lang
{
	/*
	 * lang
	 */

	char  font_name_prefix[256] ;
	char *  font_name_suffix ;
	char *  font_name ;
	struct tabl  glyph_map[MAXLEN] ;
	int  glyph_map_size ;
} ;

struct  ml_iscii_keymap
{
	/*
	 * keymap
	 */
	
	struct a2i_tabl a2i_map[A2IMAXLEN] ;
	int  a2i_map_size ;

	/* used for iitkeyb */
	char  prev_key[512] ;

	int8_t  is_inscript ;

} ;


/* --- global functions --- */

ml_iscii_lang_t 
ml_iscii_lang_new(
	ml_iscii_lang_type_t  type
	)
{
	char *  p ;
	int  count ;
	ml_iscii_lang_t  lang ;
	
	if( type < 0 || MAX_ISCIILANG <= type)
	{
		return  NULL ;
	}

	if( ( lang = malloc( sizeof( struct  ml_iscii_lang))) == NULL)
	{
		return  NULL ;
	}

	if( ( lang->glyph_map_size = indian_init( lang->glyph_map , iscii_langs[type] ,
					lang->font_name_prefix , ":")) == -1)
	{
		free( lang) ;
		
		return  NULL ;
	}

#ifdef  DEBUG
	if( lang->glyph_map_size > sizeof( lang->glyph_map))
	{
		kik_warn_printf( KIK_DEBUG_TAG " iscii glyph map size %d is larger than array size %d.\n" ,
			lang->glyph_map_size , sizeof( lang->glyph_map)) ;
	}
#endif
	
	/*
	 * XXX
	 * font name is assumed not to be abbriviated.
	 */
	p = lang->font_name_prefix ;

	for( count = 0 ; count < 7 ; count ++)
	{
		if( ( p = strchr( p , '-')) == NULL || *(++ p) == '\0')
		{
		#ifdef  DEBUG
			kik_warn_printf( KIK_DEBUG_TAG " font name %s is illegal ! \n" ,
				lang->font_name_prefix) ;
		#endif
		
			if( *(p = lang->font_name_prefix) != '\0')
			{
				*(p ++) = '\0' ;
			}
			
			goto  end ;
		}
	}
	
	*(p ++) = '\0' ;

	while( isdigit( *p))
	{
		p ++ ;
	}

end:
	lang->font_name_suffix = p ;
	
#ifdef  __DEBUG
	kik_debug_printf( KIK_DEBUG_TAG " %s?%s\n" ,
		lang->font_name_prefix , lang->font_name_suffix) ;
#endif

	/*
	 * Allocate enough memory to be used in ml_iscii_get_font_name().
	 */
	if( ( lang->font_name = malloc( strlen( lang->font_name_prefix) +
			DIGIT_STR_LEN(u_int) + strlen( lang->font_name_suffix) + 1)) == NULL)
	{
	#ifdef  DEBUG
		kik_warn_printf( KIK_DEBUG_TAG " malloc() failed.\n") ;
	#endif

		free( lang) ;
	
		return  NULL ;
	}
	
	return  lang ;
}

int
ml_iscii_lang_delete(
	ml_iscii_lang_t  lang
	)
{
	if( lang->font_name)
	{
		free( lang->font_name) ;
	}
	
	free( lang) ;

	return  1 ;
}

char *
ml_iscii_get_font_name(
	ml_iscii_lang_t  lang ,
	u_int  font_size
	)
{
	/*
	 * XXX
	 * font_size + 2 seems appropriate.
	 */
	sprintf( lang->font_name , "%s%d%s" ,
		lang->font_name_prefix , font_size + 2 , lang->font_name_suffix) ;

#ifdef  __DEBUG
	kik_debug_printf( KIK_DEBUG_TAG " %s\n" , lang->font_name) ;
#endif
	
	return  lang->font_name ;
}

u_int
ml_iscii_shape(
	ml_iscii_lang_t  lang ,
	u_char *  dst ,
	size_t  dst_size ,
	u_char *  src
	)
{
	return  iscii2font( lang->glyph_map , src , dst , lang->glyph_map_size) ;
}

ml_iscii_keymap_t
ml_iscii_keymap_new(
	int  is_inscript
	)
{
	ml_iscii_keymap_t  keymap ;

	if( ( keymap = malloc( sizeof( struct ml_iscii_keymap))) == NULL)
	{
		return  NULL ;
	}

	if( is_inscript)
	{
		if( ( keymap->a2i_map_size = readkeymap( keymap->a2i_map , "inscript" , ",")) == -1)
		{
			free( keymap) ;

			return  NULL ;
		}
	}
	else
	{
		if( ( keymap->a2i_map_size = readkeymap( keymap->a2i_map , "iitkeyb" , ":")) == -1)
		{
			free( keymap) ;

			return  NULL ;
		}
		
		memset( keymap->prev_key , 0 , sizeof( keymap->prev_key)) ;
	}
		
#ifdef  DEBUG
	if( keymap->a2i_map_size > sizeof( keymap->a2i_map))
	{
		kik_warn_printf( KIK_DEBUG_TAG
			" ascii2iscii map size %d is larger than array size %d.\n" ,
			keymap->a2i_map_size , sizeof( keymap->a2i_map)) ;
	}
#endif

	keymap->is_inscript = is_inscript ;
	
	return  keymap ;
}

int
ml_iscii_keymap_delete(
	ml_iscii_keymap_t  keymap
	)
{
	free( keymap) ;

	return  1 ;
}

size_t
ml_convert_ascii_to_iscii(
	ml_iscii_keymap_t  keymap ,
	u_char *  iscii ,
	size_t  iscii_len ,
	u_char *  ascii ,
	size_t  ascii_len
	)
{
	u_char *  dup ;

	if( ( dup = alloca( ascii_len + 1)) == NULL)
	{
		goto  no_conv ;
	}

	strncpy( dup , ascii , ascii_len) ;
	dup[ascii_len] = '\0' ;

	if( keymap->is_inscript)
	{
		kik_snprintf( iscii , ascii_len + 1 , "%s" ,
			ins2iscii( keymap->a2i_map , dup , keymap->a2i_map_size)) ;
	}
	else
	{
		iitk2iscii( keymap->a2i_map , dup , keymap->prev_key , keymap->a2i_map_size) ;
		kik_snprintf( iscii , strlen( dup) + 1 , "%s" , dup) ;

		keymap->prev_key[0] = ascii[0] ;
		keymap->prev_key[1] = '\0' ;
	}

	return  strlen( iscii) ;
	
no_conv:
	memmove( iscii , ascii , iscii_len) ;

	return  ascii_len ;
}


#else  /* USE_IND */


/* --- global functions --- */

ml_iscii_lang_t
ml_iscii_lang_new(void)
{
	return  NULL ;
}

int
ml_iscii_lang_delete(
	ml_iscii_lang_t  lang
	)
{
	return  0 ;
}

int
ml_iscii_select_lang(
	ml_iscii_lang_t  lang ,
	ml_iscii_lang_t  type
	)
{
	return  0 ;
}

char *
ml_iscii_get_font_name(
	ml_iscii_lang_t  lang ,
	u_int  font_size
	)
{
	return  NULL ;
}

u_int
ml_iscii_shape(
	ml_iscii_lang_t  lang ,
	u_char *  dst ,
	size_t  dst_size ,
	u_char *  src
	)
{
	return   0 ;
}

ml_iscii_keymap_t
ml_iscii_keymap_new(
	int  is_inscript
	)
{
	return  NULL ;
}

int
ml_iscii_keymap_delete(
	ml_iscii_keymap_t  keymap
	)
{
	return  0 ;
}

size_t
ml_convert_ascii_to_iscii(
	ml_iscii_keymap_t  keymap ,
	u_char *  iscii ,
	size_t  iscii_len ,
	u_char *  ascii ,
	size_t  ascii_len
	)
{
	return  0 ;
}


#endif	/* USE_IND */
