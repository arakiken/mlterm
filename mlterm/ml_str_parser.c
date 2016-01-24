/*
 *	$Id$
 */

#include  "ml_str_parser.h"

#include  <string.h>			/* memcpy */
#include  <kiklib/kik_mem.h>
#include  <kiklib/kik_debug.h>

#include  "ml_char_encoding.h"		/* ml_is_msb_set */
#include  "ml_drcs.h"


typedef struct ml_str_parser
{
	mkf_parser_t  parser ;

	/*
	 * !! Notice !!
	 * mkf_parser_reset() and mkf_parser_mark() don't recognize these members.
	 */
	ml_char_t *  str ;
	u_int  left ;
	u_int  comb_left ;

} ml_str_parser_t ;


/* --- static functions --- */

static int
next_char(
	mkf_parser_t *  parser ,
	mkf_char_t *  ch
	)
{
	ml_str_parser_t *  ml_str_parser ;
	ml_char_t *  ml_ch ;
	u_int  comb_size ;

	ml_str_parser = (ml_str_parser_t*) parser ;

	/* hack for mkf_parser_reset */
	ml_str_parser->str -= (parser->left - ml_str_parser->left) ;
	ml_str_parser->left = parser->left ;

	while( 1)
	{
		if( ml_str_parser->parser.is_eos)
		{
			goto  err ;
		}

		mkf_parser_mark( parser) ;

		/*
		 * skipping NULL
		 */
		if( ! ml_char_is_null( ml_str_parser->str))
		{
			break ;
		}

		ml_str_parser->left -- ;
		ml_str_parser->str ++ ;

		if( ml_str_parser->left == 0)
		{
			ml_str_parser->parser.is_eos = 1 ;
		}
	}

	ml_ch = ml_str_parser->str ;
	
	if( ml_str_parser->comb_left > 0)
	{
		ml_char_t *  combs ;
		
		if( ( combs = ml_get_combining_chars( ml_ch , &comb_size)) == NULL ||
			comb_size < ml_str_parser->comb_left)
		{
			/* strange ! */
			
			ml_str_parser->comb_left = 0 ;

			goto  err ;
		}
		
		ml_ch = &combs[ comb_size - ml_str_parser->comb_left] ;
		
		if( -- ml_str_parser->comb_left == 0)
		{
			ml_str_parser->left -- ;
			ml_str_parser->str ++ ;
		}
	}
	else
	{
		if( ml_get_combining_chars( ml_ch , &comb_size))
		{
			ml_str_parser->comb_left = comb_size ;
		}
		else
		{
			ml_str_parser->left -- ;
			ml_str_parser->str ++ ;
		}
	}

	ch->cs = ml_char_cs( ml_ch) ;
	ch->size = CS_SIZE(ch->cs) ;

	mkf_int_to_bytes( ch->ch , ch->size , ml_char_code( ml_ch)) ;

	/*
	 * Android doesn't support PUA as follows. (tested on Android 4.0)
	 *
	 * e.g.) UTF8:0x4f88819a (U+10805a)
	 * W/dalvikvm( 4527): JNI WARNING: input is not valid Modified UTF-8: illegal start byte 0xf4
	 * I/dalvikvm( 4527):   at dalvik.system.NativeStart.run(Native Method)
	 * E/dalvikvm( 4527): VM aborting
	 */
#ifndef  __ANDROID__
	if( ! ml_convert_drcs_to_unicode_pua( ch))
#endif
	{
		/* XXX */
		ch->property = 0 ;

		if( ml_is_msb_set( ch->cs))
		{
			UNSET_MSB(ch->ch[0]) ;
		}
	}

	if( ml_str_parser->left == 0)
	{
		ml_str_parser->parser.is_eos = 1 ;
	}

	/* hack for mkf_parser_reset */
	parser->left = ml_str_parser->left ;

	if( ml_char_is_null( ml_ch))
	{
		return  next_char( parser , ch) ;
	}

	return  1 ;

err:
	/* hack for mkf_parser_reset */
	parser->left = ml_str_parser->left ;

	return  0 ;
}

static void
init(
	mkf_parser_t *  mkf_parser
	)
{
	ml_str_parser_t *  ml_str_parser ;

	ml_str_parser = (ml_str_parser_t*) mkf_parser ;

	mkf_parser_init( mkf_parser) ;

	ml_str_parser->str = NULL ;
	ml_str_parser->left = 0 ;
	ml_str_parser->comb_left = 0 ;
}

static void
set_str(
	mkf_parser_t *  mkf_parser ,
	u_char *  str ,
	size_t  size
	)
{
	/* do nothing */
}

static void
delete(
	mkf_parser_t *  s
	)
{
	free( s) ;
}


/* --- global functions --- */

mkf_parser_t *
ml_str_parser_new(void)
{
	ml_str_parser_t *  ml_str_parser ;
	
	if( ( ml_str_parser = malloc( sizeof( ml_str_parser_t))) == NULL)
	{
		return  NULL ;
	}

	init( (mkf_parser_t*) ml_str_parser) ;

	ml_str_parser->parser.init = init ;
	ml_str_parser->parser.set_str = set_str ;
	ml_str_parser->parser.delete = delete ;
	ml_str_parser->parser.next_char = next_char ;

	return  (mkf_parser_t*)ml_str_parser ;
}

void
ml_str_parser_set_str(
	mkf_parser_t *  mkf_parser ,
	ml_char_t *  str ,
	u_int  size
	)
{
	ml_str_parser_t *  ml_str_parser ;

	ml_str_parser = (ml_str_parser_t*) mkf_parser ;

	ml_str_parser->parser.is_eos = 0 ;
	ml_str_parser->parser.left = size ;
	
	ml_str_parser->str = str ;
	ml_str_parser->left = size ;
}
