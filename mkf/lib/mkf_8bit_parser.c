/*
 *	$Id$
 */

#include  "mkf_8bit_parser.h"

#include  <kiklib/kik_mem.h>
#include  <kiklib/kik_debug.h>


/* --- static functions --- */

static int
parser_next_char_intern(
	mkf_parser_t *  parser ,
	mkf_char_t *  ch ,
	mkf_charset_t  cs
	)
{
	if( parser->is_eos)
	{
		return  0 ;
	}

	mkf_parser_mark( parser) ;
	
	if( /* 0x0 <= *parser->str && */ *parser->str <= 0x7f)
	{
		ch->ch[0] = *parser->str ;
		ch->size = 1 ;
		ch->cs = US_ASCII ;
	}
	else if( 0x80 <= *parser->str)
	{
		ch->ch[0] = *parser->str ;
		ch->size = 1 ;
		ch->cs = cs ;
	}
	else
	{
	#ifdef  DEBUG
		kik_warn_printf( KIK_DEBUG_TAG " %x char not supported.\n" , *parser->str) ;
	#endif

		return  0 ;
	}

	ch->property = 0 ;

	mkf_parser_increment( parser) ;
	
	return  1 ;
}

static int
koi8_r_parser_next_char(
	mkf_parser_t *  parser ,
	mkf_char_t *  ch
	)
{
	return  parser_next_char_intern( parser , ch , KOI8_R) ;
}

static int
koi8_u_parser_next_char(
	mkf_parser_t *  parser ,
	mkf_char_t *  ch
	)
{
	return  parser_next_char_intern( parser , ch , KOI8_U) ;
}

static int
koi8_t_parser_next_char(
	mkf_parser_t *  parser ,
	mkf_char_t *  ch
	)
{
	return  parser_next_char_intern( parser , ch , KOI8_T) ;
}

static int
georgian_ps_parser_next_char(
	mkf_parser_t *  parser ,
	mkf_char_t *  ch
	)
{
	return  parser_next_char_intern( parser , ch , GEORGIAN_PS) ;
}

static void
parser_set_str(
	mkf_parser_t *  parser ,
	u_char *  str ,
	size_t  size
	)
{
	parser->str = str ;
	parser->left = size ;
	parser->marked_left = 0 ;
	parser->is_eos = 0 ;
}

static void
parser_delete(
	mkf_parser_t *  s
	)
{
	free( s) ;
}


/* --- global functions --- */

mkf_parser_t *
mkf_koi8_r_parser_new(void)
{
	mkf_parser_t *  parser ;
	
	if( ( parser = malloc( sizeof( mkf_parser_t))) == NULL)
	{
		return  NULL ;
	}

	mkf_parser_init( parser) ;

	parser->init = mkf_parser_init ;
	parser->next_char = koi8_r_parser_next_char ;
	parser->set_str = parser_set_str ;
	parser->delete = parser_delete ;

	return  parser ;
}

mkf_parser_t *
mkf_koi8_u_parser_new(void)
{
	mkf_parser_t *  parser ;
	
	if( ( parser = malloc( sizeof( mkf_parser_t))) == NULL)
	{
		return  NULL ;
	}

	mkf_parser_init( parser) ;

	parser->init = mkf_parser_init ;
	parser->next_char = koi8_u_parser_next_char ;
	parser->set_str = parser_set_str ;
	parser->delete = parser_delete ;

	return  parser ;
}

mkf_parser_t *
mkf_koi8_t_parser_new(void)
{
	mkf_parser_t *  parser ;
	
	if( ( parser = malloc( sizeof( mkf_parser_t))) == NULL)
	{
		return  NULL ;
	}

	mkf_parser_init( parser) ;

	parser->init = mkf_parser_init ;
	parser->next_char = koi8_t_parser_next_char ;
	parser->set_str = parser_set_str ;
	parser->delete = parser_delete ;

	return  parser ;
}

mkf_parser_t *
mkf_georgian_ps_parser_new(void)
{
	mkf_parser_t *  parser ;
	
	if( ( parser = malloc( sizeof( mkf_parser_t))) == NULL)
	{
		return  NULL ;
	}

	mkf_parser_init( parser) ;

	parser->init = mkf_parser_init ;
	parser->next_char = georgian_ps_parser_next_char ;
	parser->set_str = parser_set_str ;
	parser->delete = parser_delete ;

	return  parser ;
}
