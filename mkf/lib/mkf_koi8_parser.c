/*
 *	$Id$
 */

#include  "mkf_koi8_parser.h"

#include  <kiklib/kik_mem.h>
#include  <kiklib/kik_debug.h>


/* --- static functions --- */

static int
koi8_parser_next_char_intern(
	mkf_parser_t *  koi8_parser ,
	mkf_char_t *  ch ,
	mkf_charset_t  cs
	)
{
	if( koi8_parser->is_eos)
	{
		return  0 ;
	}

	mkf_parser_mark( koi8_parser) ;
	
	if( /* 0x0 <= *koi8_parser->str && */ *koi8_parser->str <= 0x7f)
	{
		ch->ch[0] = *koi8_parser->str ;
		ch->size = 1 ;
		ch->cs = US_ASCII ;
	}
	else if( 0x80 <= *koi8_parser->str)
	{
		ch->ch[0] = *koi8_parser->str ;
		ch->size = 1 ;
		ch->cs = cs ;
	}
	else
	{
	#ifdef  DEBUG
		kik_warn_printf( KIK_DEBUG_TAG " koi8 doesn't support %x char.\n" ,
			*koi8_parser->str) ;
	#endif

		return  0 ;
	}

	ch->property = 0 ;

	mkf_parser_increment( koi8_parser) ;
	
	return  1 ;
}

static int
koi8_r_parser_next_char(
	mkf_parser_t *  koi8_parser ,
	mkf_char_t *  ch
	)
{
	return  koi8_parser_next_char_intern( koi8_parser , ch , KOI8_R) ;
}

static int
koi8_u_parser_next_char(
	mkf_parser_t *  koi8_parser ,
	mkf_char_t *  ch
	)
{
	return  koi8_parser_next_char_intern( koi8_parser , ch , KOI8_U) ;
}

static void
koi8_parser_set_str(
	mkf_parser_t *  koi8_parser ,
	u_char *  str ,
	size_t  size
	)
{
	koi8_parser->str = str ;
	koi8_parser->left = size ;
	koi8_parser->marked_left = 0 ;
	koi8_parser->is_eos = 0 ;
}

static void
koi8_parser_delete(
	mkf_parser_t *  s
	)
{
	free( s) ;
}


/* --- global functions --- */

mkf_parser_t *
mkf_koi8_r_parser_new(void)
{
	mkf_parser_t *  koi8_parser ;
	
	if( ( koi8_parser = malloc( sizeof( mkf_parser_t))) == NULL)
	{
		return  NULL ;
	}

	mkf_parser_init( koi8_parser) ;

	koi8_parser->init = mkf_parser_init ;
	koi8_parser->next_char = koi8_r_parser_next_char ;
	koi8_parser->set_str = koi8_parser_set_str ;
	koi8_parser->delete = koi8_parser_delete ;

	return  koi8_parser ;
}

mkf_parser_t *
mkf_koi8_u_parser_new(void)
{
	mkf_parser_t *  koi8_parser ;
	
	if( ( koi8_parser = malloc( sizeof( mkf_parser_t))) == NULL)
	{
		return  NULL ;
	}

	mkf_parser_init( koi8_parser) ;

	koi8_parser->init = mkf_parser_init ;
	koi8_parser->next_char = koi8_u_parser_next_char ;
	koi8_parser->set_str = koi8_parser_set_str ;
	koi8_parser->delete = koi8_parser_delete ;

	return  koi8_parser ;
}
