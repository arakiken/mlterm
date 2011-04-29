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

mkf_iscii_lang_t
ml_iscii_get_lang(
	char *  name
	)
{
	mkf_iscii_lang_t  lang ;

	for( lang = 0 ; lang < MAX_ISCIILANGS ; lang ++)
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
	mkf_iscii_lang_t  type
	)
{
	if( ISCIILANG_UNKNOWN < type && type < MAX_ISCIILANGS)
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

	/* Max length of font name is assumed to be 256 in indian_init(). (see indian.c) */
	char  font_name_format[256] ;
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
	mkf_iscii_lang_t  type
	)
{
	ml_iscii_lang_t  lang ;
	
	if( type < 0 || MAX_ISCIILANGS <= type)
	{
		return  NULL ;
	}

	if( ( lang = malloc( sizeof( struct  ml_iscii_lang))) == NULL)
	{
		return  NULL ;
	}

	if( ( lang->glyph_map_size = indian_init( lang->glyph_map , iscii_langs[type] ,
					lang->font_name_format , ":")) == -1)
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
	 * Allocate enough memory to be used in ml_iscii_get_font_name().
	 */
	if( ( lang->font_name = malloc( strlen( lang->font_name_format) +
					DIGIT_STR_LEN(u_int) + 1)) == NULL)
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

	/* XXX lang->glyph_map[N].iscii and lang->glyph_map[N].font should be free'ed. */

	free( lang) ;

	return  1 ;
}

char *
ml_iscii_get_font_name(
	ml_iscii_lang_t  lang ,
	u_int  font_size
	)
{
	/* If font_name_format contains '%d', it is replaced by specified font_size */
	sprintf( lang->font_name , lang->font_name_format , font_size) ;

#ifdef  __DEBUG
	kik_debug_printf( KIK_DEBUG_TAG " font name %s\n" , lang->font_name) ;
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
	/*
	 * XXX
	 * iscii2font() expects dst to be terminated by zero.
	 * int iscii2font(struct tabl table[MAXLEN], char *input, char *output, int sz) {
	 *	...
	 *	bzero(output,strlen(output));
	 *	...          ^^^^^^^^^^^^^^
	 * }
	 */
	dst[0] = '\0' ;

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

	/*
	 * ins2iscii() and iitk2iscii() return 2nd argument variable whose memory
	 * is modified by converted iscii bytes.
	 * So, enough memory (* MAXBUFF) should be allocated here.
	 */
	if( ( dup = alloca( ascii_len * MAXBUFF)) == NULL)
	{
		goto  no_conv ;
	}

	strncpy( dup , ascii , ascii_len) ;
	dup[ascii_len] = '\0' ;

	if( keymap->is_inscript)
	{
		kik_snprintf( iscii , iscii_len , "%s" ,
			ins2iscii( keymap->a2i_map , dup , keymap->a2i_map_size)) ;
	}
	else
	{
		kik_snprintf( iscii , iscii_len , "%s" ,
			iitk2iscii( keymap->a2i_map , dup , keymap->prev_key ,
				keymap->a2i_map_size)) ;

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
ml_iscii_lang_new(
	mkf_iscii_lang_t  type
	)
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
