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

	if( ch->cs == CP874 &&
	   (ch->ch[0] == 0xd1 || ( 0xd4 <= ch->ch[0] && ch->ch[0] <= 0xda) ||
			( 0xe7 <= ch->ch[0] && ch->ch[0] <= 0xee)) )
	{
		ch->property = MKF_COMBINING ;
	}
	else
	{
		ch->property = 0 ;
	}

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

static int
cp1250_parser_next_char(
	mkf_parser_t *  parser ,
	mkf_char_t *  ch
	)
{
	return  parser_next_char_intern( parser , ch , CP1250) ;
}

static int
cp1251_parser_next_char(
	mkf_parser_t *  parser ,
	mkf_char_t *  ch
	)
{
	return  parser_next_char_intern( parser , ch , CP1251) ;
}

static int
cp1252_parser_next_char(
	mkf_parser_t *  parser ,
	mkf_char_t *  ch
	)
{
	return  parser_next_char_intern( parser , ch , CP1252) ;
}

static int
cp1253_parser_next_char(
	mkf_parser_t *  parser ,
	mkf_char_t *  ch
	)
{
	return  parser_next_char_intern( parser , ch , CP1253) ;
}

static int
cp1254_parser_next_char(
	mkf_parser_t *  parser ,
	mkf_char_t *  ch
	)
{
	return  parser_next_char_intern( parser , ch , CP1254) ;
}

static int
cp1255_parser_next_char(
	mkf_parser_t *  parser ,
	mkf_char_t *  ch
	)
{
	return  parser_next_char_intern( parser , ch , CP1255) ;
}

static int
cp1256_parser_next_char(
	mkf_parser_t *  parser ,
	mkf_char_t *  ch
	)
{
	return  parser_next_char_intern( parser , ch , CP1256) ;
}

static int
cp1257_parser_next_char(
	mkf_parser_t *  parser ,
	mkf_char_t *  ch
	)
{
	return  parser_next_char_intern( parser , ch , CP1257) ;
}

static int
cp1258_parser_next_char(
	mkf_parser_t *  parser ,
	mkf_char_t *  ch
	)
{
	return  parser_next_char_intern( parser , ch , CP1258) ;
}

static int
cp874_parser_next_char(
	mkf_parser_t *  parser ,
	mkf_char_t *  ch
	)
{
	return  parser_next_char_intern( parser , ch , CP874) ;
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

mkf_parser_t *
mkf_cp1250_parser_new(void)
{
	mkf_parser_t *  parser ;
	
	if( ( parser = malloc( sizeof( mkf_parser_t))) == NULL)
	{
		return  NULL ;
	}

	mkf_parser_init( parser) ;

	parser->init = mkf_parser_init ;
	parser->next_char = cp1250_parser_next_char ;
	parser->set_str = parser_set_str ;
	parser->delete = parser_delete ;

	return  parser ;
}

mkf_parser_t *
mkf_cp1251_parser_new(void)
{
	mkf_parser_t *  parser ;
	
	if( ( parser = malloc( sizeof( mkf_parser_t))) == NULL)
	{
		return  NULL ;
	}

	mkf_parser_init( parser) ;

	parser->init = mkf_parser_init ;
	parser->next_char = cp1251_parser_next_char ;
	parser->set_str = parser_set_str ;
	parser->delete = parser_delete ;

	return  parser ;
}

mkf_parser_t *
mkf_cp1252_parser_new(void)
{
	mkf_parser_t *  parser ;
	
	if( ( parser = malloc( sizeof( mkf_parser_t))) == NULL)
	{
		return  NULL ;
	}

	mkf_parser_init( parser) ;

	parser->init = mkf_parser_init ;
	parser->next_char = cp1252_parser_next_char ;
	parser->set_str = parser_set_str ;
	parser->delete = parser_delete ;

	return  parser ;
}

mkf_parser_t *
mkf_cp1253_parser_new(void)
{
	mkf_parser_t *  parser ;
	
	if( ( parser = malloc( sizeof( mkf_parser_t))) == NULL)
	{
		return  NULL ;
	}

	mkf_parser_init( parser) ;

	parser->init = mkf_parser_init ;
	parser->next_char = cp1253_parser_next_char ;
	parser->set_str = parser_set_str ;
	parser->delete = parser_delete ;

	return  parser ;
}

mkf_parser_t *
mkf_cp1254_parser_new(void)
{
	mkf_parser_t *  parser ;
	
	if( ( parser = malloc( sizeof( mkf_parser_t))) == NULL)
	{
		return  NULL ;
	}

	mkf_parser_init( parser) ;

	parser->init = mkf_parser_init ;
	parser->next_char = cp1254_parser_next_char ;
	parser->set_str = parser_set_str ;
	parser->delete = parser_delete ;

	return  parser ;
}

mkf_parser_t *
mkf_cp1255_parser_new(void)
{
	mkf_parser_t *  parser ;
	
	if( ( parser = malloc( sizeof( mkf_parser_t))) == NULL)
	{
		return  NULL ;
	}

	mkf_parser_init( parser) ;

	parser->init = mkf_parser_init ;
	parser->next_char = cp1255_parser_next_char ;
	parser->set_str = parser_set_str ;
	parser->delete = parser_delete ;

	return  parser ;
}

mkf_parser_t *
mkf_cp1256_parser_new(void)
{
	mkf_parser_t *  parser ;
	
	if( ( parser = malloc( sizeof( mkf_parser_t))) == NULL)
	{
		return  NULL ;
	}

	mkf_parser_init( parser) ;

	parser->init = mkf_parser_init ;
	parser->next_char = cp1256_parser_next_char ;
	parser->set_str = parser_set_str ;
	parser->delete = parser_delete ;

	return  parser ;
}

mkf_parser_t *
mkf_cp1257_parser_new(void)
{
	mkf_parser_t *  parser ;
	
	if( ( parser = malloc( sizeof( mkf_parser_t))) == NULL)
	{
		return  NULL ;
	}

	mkf_parser_init( parser) ;

	parser->init = mkf_parser_init ;
	parser->next_char = cp1257_parser_next_char ;
	parser->set_str = parser_set_str ;
	parser->delete = parser_delete ;

	return  parser ;
}

mkf_parser_t *
mkf_cp1258_parser_new(void)
{
	mkf_parser_t *  parser ;
	
	if( ( parser = malloc( sizeof( mkf_parser_t))) == NULL)
	{
		return  NULL ;
	}

	mkf_parser_init( parser) ;

	parser->init = mkf_parser_init ;
	parser->next_char = cp1258_parser_next_char ;
	parser->set_str = parser_set_str ;
	parser->delete = parser_delete ;

	return  parser ;
}

mkf_parser_t *
mkf_cp874_parser_new(void)
{
	mkf_parser_t *  parser ;
	
	if( ( parser = malloc( sizeof( mkf_parser_t))) == NULL)
	{
		return  NULL ;
	}

	mkf_parser_init( parser) ;

	parser->init = mkf_parser_init ;
	parser->next_char = cp874_parser_next_char ;
	parser->set_str = parser_set_str ;
	parser->delete = parser_delete ;

	return  parser ;
}
