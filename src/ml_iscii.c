/*
 *	$Id$
 */

#include  "ml_iscii.h"

#include  <stdio.h>		/* snprintf */
#include  <ctype.h>		/* isdigit */
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

ml_iscii_lang_t
ml_iscii_get_lang(
	char *  name
	)
{
	ml_iscii_lang_t  lang ;

	for( lang = 0 ; lang < MAX_ISCIILANG ; lang ++)
	{
		if( strcasecmp( iscii_langs[lang] , name) == 0)
		{
			return  lang ;
		}
	}

	return  ISCIILANG_UNKNOWN ;
}


#ifdef  USE_IND

#include  <indian.h>


struct  ml_iscii_state
{
	/*
	 * lang
	 */

	ml_iscii_lang_t  lang ;

	char  font_name_prefix[256] ;
	char *  font_name_suffix ;
	char *  font_name ;
	struct tabl  glyph_map[MAXLEN] ;
	int  glyph_map_size ;

	/*
	 * keyb
	 */
		
	ml_iscii_keyb_t  keyb ;

	struct a2i_tabl a2i_map[A2IMAXLEN] ;
	int  a2i_map_size ;

	/* used for iitkeyb */
	char  prev_key[512] ;
} ;


/* --- global functions --- */

ml_iscii_state_t 
ml_iscii_new(void)
{
	ml_iscii_state_t  iscii_state ;
	
	if( ( iscii_state = malloc( sizeof( struct  ml_iscii_state))) == NULL)
	{
		return  NULL ;
	}

	memset( iscii_state , 0 , sizeof( struct  ml_iscii_state)) ;
	
	return  iscii_state ;
}

int
ml_iscii_delete(
	ml_iscii_state_t  iscii_state
	)
{
	if( iscii_state->font_name)
	{
		free( iscii_state->font_name) ;
	}
	
	free( iscii_state) ;

	return  1 ;
}

int
ml_iscii_select_lang(
	ml_iscii_state_t  iscii_state ,
	ml_iscii_lang_t  lang
	)
{
	char *  p ;
	int  counter ;

	if( lang < 0 || MAX_ISCIILANG <= lang)
	{
		return  0 ;
	}

	if( ( iscii_state->glyph_map_size = indian_init( iscii_state->glyph_map , iscii_langs[lang] ,
					iscii_state->font_name_prefix , ":")) == -1)
	{
		return  0 ;
	}

#ifdef  DEBUG
	if( iscii_state->glyph_map_size > sizeof( iscii_state->glyph_map))
	{
		kik_warn_printf( KIK_DEBUG_TAG " iscii glyph map size %d is larger than array size %d.\n" ,
			iscii_state->glyph_map_size , sizeof( iscii_state->glyph_map)) ;
	}
#endif
	
	/*
	 * XXX
	 * font name is assumed not to be abbriviated.
	 */
	p = iscii_state->font_name_prefix ;

	for( counter = 0 ; counter < 7 ; counter ++)
	{
		if( ( p = strchr( p , '-')) == NULL || *(++ p) == '\0')
		{
		#ifdef  DEBUG
			kik_warn_printf( KIK_DEBUG_TAG " font name %s is illegal ! \n" ,
				iscii_state->font_name_prefix) ;
		#endif
		
			if( *(p = iscii_state->font_name_prefix) != '\0')
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
	iscii_state->font_name_suffix = p ;
	
#ifdef  __DEBUG
	kik_debug_printf( KIK_DEBUG_TAG " %s?%s\n" ,
		iscii_state->font_name_prefix , iscii_state->font_name_suffix) ;
#endif

	if( iscii_state->font_name)
	{
		free( iscii_state->font_name) ;
	}
	
	if( ( iscii_state->font_name =
		malloc( strlen( iscii_state->font_name_prefix) +
			DIGIT_STR_LEN(u_int) + strlen( iscii_state->font_name_suffix) + 1)) == NULL)
	{
	#ifdef  DEBUG
		kik_warn_printf( KIK_DEBUG_TAG " malloc() failed.\n") ;
	#endif
	
		return  0 ;
	}
	
	return  1 ;
}

char *
ml_iscii_get_font_name(
	ml_iscii_state_t  iscii_state ,
	u_int  font_size
	)
{
	/*
	 * XXX
	 * font_size + 2 seems appropriate.
	 */
	sprintf( iscii_state->font_name , "%s%d%s" ,
		iscii_state->font_name_prefix , font_size + 2 , iscii_state->font_name_suffix) ;

#ifdef  __DEBUG
	kik_debug_printf( KIK_DEBUG_TAG " %s\n" , iscii_state->font_name) ;
#endif
	
	return  iscii_state->font_name ;
}

u_int
ml_iscii_shape(
	ml_iscii_state_t  iscii_state ,
	u_char *  dst ,
	size_t  dst_size ,
	u_char *  src
	)
{
	return  iscii2font( iscii_state->glyph_map , src , dst , iscii_state->glyph_map_size) ;
}

int
ml_iscii_select_keyb(
	ml_iscii_state_t  iscii_state ,
	ml_iscii_keyb_t  keyb
	)
{
	if( keyb != ISCIIKEYB_NONE)
	{
		if( keyb == ISCIIKEYB_INSCRIPT)
		{
			if( ( iscii_state->a2i_map_size =
				readkeymap( iscii_state->a2i_map , "inscript" , ",")) == -1)
			{
				keyb = ISCIIKEYB_NONE ;
			}
		}
		else /* if( keyb == ISCIIKEYB_IITKEYB) */
		{
			if( ( iscii_state->a2i_map_size =
				readkeymap( iscii_state->a2i_map , "iitkeyb" , ":")) == -1)
			{
				keyb = ISCIIKEYB_NONE ;
			}
			else
			{
				memset( iscii_state->prev_key , 0 , sizeof( iscii_state->prev_key)) ;
			}
		}

	#ifdef  DEBUG
		if( iscii_state->a2i_map_size > sizeof( iscii_state->a2i_map))
		{
			kik_warn_printf( KIK_DEBUG_TAG
				" ascii2iscii map size %d is larger than array size %d.\n" ,
				iscii_state->a2i_map_size , sizeof( iscii_state->a2i_map)) ;
		}
	#endif
	}

	iscii_state->keyb = keyb ;

	return  1 ;
}

ml_iscii_keyb_t
ml_iscii_current_keyb(
	ml_iscii_state_t  iscii_state
	)
{
	return  iscii_state->keyb ;
}

size_t
ml_convert_ascii_to_iscii(
	ml_iscii_state_t  iscii_state ,
	u_char *  iscii ,
	size_t  iscii_len ,
	u_char *  ascii ,
	size_t  ascii_len
	)
{
	if( iscii_state->keyb != ISCIIKEYB_NONE)
	{
		u_char *  dup ;
		
		if( ( dup = alloca( ascii_len + 1)) == NULL)
		{
			goto  no_conv ;
		}

		strncpy( dup , ascii , ascii_len) ;
		dup[ascii_len] = '\0' ;

		if( iscii_state->keyb == ISCIIKEYB_INSCRIPT)
		{
			snprintf( iscii , ascii_len + 1 , "%s" ,
				ins2iscii( iscii_state->a2i_map , dup , iscii_state->a2i_map_size)) ;
		}
		else /* if( keyb->keyb == ISCIIKEYB_IITKEYB) */
		{
			iitk2iscii( iscii_state->a2i_map , dup ,
				iscii_state->prev_key , iscii_state->a2i_map_size) ;
			snprintf( iscii , strlen( dup) + 1 , "%s" , dup) ;

			iscii_state->prev_key[0] = ascii[0] ;
			iscii_state->prev_key[1] = '\0' ;
		}

		return  strlen( iscii) ;
	}
	
no_conv:
	memmove( iscii , ascii , iscii_len) ;

	return  ascii_len ;
}


#else  /* USE_IND */


struct  ml_iscii_state
{
	ml_iscii_lang_t  lang ;
	ml_iscii_keyb_t  keyb ;
} ;


/* --- global functions --- */

ml_iscii_state_t
ml_iscii_new(void)
{
	ml_iscii_state_t  iscii_state ;

	if( ( iscii_state = malloc( sizeof( struct ml_iscii_state))) == NULL)
	{
		return  NULL ;
	}

	memset( iscii_state , 0 , sizeof( struct ml_iscii_state)) ;

	return  iscii_state ;
}

int
ml_iscii_delete(
	ml_iscii_state_t  iscii_state
	)
{
	free( iscii_state) ;

	return  1 ;
}

int
ml_iscii_select_lang(
	ml_iscii_state_t  iscii_state ,
	ml_iscii_lang_t  lang
	)
{
	iscii_state->lang = lang ;

	return  1 ;
}

char *
ml_iscii_get_font_name(
	ml_iscii_state_t  iscii_state ,
	u_int  font_size
	)
{
	return  NULL ;
}

u_int
ml_iscii_shape(
	ml_iscii_state_t  iscii_state ,
	u_char *  dst ,
	size_t  dst_size ,
	u_char *  src
	)
{
	size_t  len ;

	len = K_MIN(dst_size,strlen(src)) ;

	memcpy( dst , src , len) ;

	return  len ;
}

int
ml_iscii_select_keyb(
	ml_iscii_state_t  iscii_state ,
	ml_iscii_keyb_t  keyb
	)
{
	iscii_state->keyb = keyb ;

	return  1 ;
}

ml_iscii_keyb_t
ml_iscii_current_keyb(
	ml_iscii_state_t  iscii_state
	)
{
	return  iscii_state->keyb ;
}

size_t
ml_convert_ascii_to_iscii(
	ml_iscii_state_t  iscii_state ,
	u_char *  iscii ,
	size_t  iscii_len ,
	u_char *  ascii ,
	size_t  ascii_len
	)
{
	size_t  len ;

	len = K_MIN(iscii_len,ascii_len) ;
	
	memcpy( iscii , ascii , len) ;

	return  len ;
}


#endif	/* USE_IND */
