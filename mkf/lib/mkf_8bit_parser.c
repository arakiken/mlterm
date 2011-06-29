/*
 *	$Id$
 */

#include  "mkf_8bit_parser.h"

#include  <kiklib/kik_mem.h>
#include  <kiklib/kik_debug.h>


typedef struct  mkf_iscii_parser
{
	mkf_parser_t  parser ;
	mkf_charset_t  cs ;

} mkf_iscii_parser_t ;


/* --- static functions --- */

static int
parser_next_char_intern(
	mkf_parser_t *  parser ,
	mkf_char_t *  ch ,
	mkf_charset_t  cs
	)
{
	u_char c ;
	
	if( parser->is_eos)
	{
		return  0 ;
	}

	mkf_parser_mark( parser) ;

	ch->ch[0] = c = *parser->str ;
	ch->size = 1 ;
	ch->property = 0 ;

	if( /* 0x0 <= c && */ c <= 0x7f &&
		(cs != VISCII ||
		(c != 0x02 && c != 0x05 && c != 0x06 && c != 0x14 && c != 0x19 && c != 0x1e)) )
	{
		ch->cs = US_ASCII ;
	}
	else
	{
		if( cs == CP874 &&
		    (c == 0xd1 || ( 0xd4 <= c && c <= 0xda) || ( 0xe7 <= c && c <= 0xee)) )
		{
			ch->property = MKF_COMBINING ;
		}

		ch->cs = cs ;
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

static int
viscii_parser_next_char(
	mkf_parser_t *  parser ,
	mkf_char_t *  ch
	)
{
	return  parser_next_char_intern( parser , ch , VISCII) ;
}

static int
iscii_parser_next_char(
	mkf_parser_t *  parser ,
	mkf_char_t *  ch
	)
{
	return  parser_next_char_intern( parser , ch , ((mkf_iscii_parser_t*)parser)->cs) ;
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

static mkf_parser_t *
iscii_parser_new(
	mkf_charset_t  cs
	)
{
	mkf_iscii_parser_t *  iscii_parser ;
	
	if( ( iscii_parser = malloc( sizeof( mkf_iscii_parser_t))) == NULL)
	{
		return  NULL ;
	}

	mkf_parser_init( &iscii_parser->parser) ;

	iscii_parser->parser.init = mkf_parser_init ;
	iscii_parser->parser.next_char = iscii_parser_next_char ;
	iscii_parser->parser.set_str = parser_set_str ;
	iscii_parser->parser.delete = parser_delete ;
	iscii_parser->cs = cs ;

	return  &iscii_parser->parser ;
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

mkf_parser_t *
mkf_viscii_parser_new(void)
{
	mkf_parser_t *  viscii_parser ;
	
	if( ( viscii_parser = malloc( sizeof( mkf_parser_t))) == NULL)
	{
		return  NULL ;
	}

	mkf_parser_init( viscii_parser) ;

	viscii_parser->init = mkf_parser_init ;
	viscii_parser->next_char = viscii_parser_next_char ;
	viscii_parser->set_str = parser_set_str ;
	viscii_parser->delete = parser_delete ;

	return  viscii_parser ;
}

mkf_parser_t *
mkf_iscii_assamese_parser_new(void)
{
	return  iscii_parser_new( ISCII_ASSAMESE) ;
}

mkf_parser_t *
mkf_iscii_bengali_parser_new(void)
{
	return  iscii_parser_new( ISCII_BENGALI) ;
}

mkf_parser_t *
mkf_iscii_gujarati_parser_new(void)
{
	return  iscii_parser_new( ISCII_GUJARATI) ;
}

mkf_parser_t *
mkf_iscii_hindi_parser_new(void)
{
	return  iscii_parser_new( ISCII_HINDI) ;
}

mkf_parser_t *
mkf_iscii_kannada_parser_new(void)
{
	return  iscii_parser_new( ISCII_KANNADA) ;
}

mkf_parser_t *
mkf_iscii_malayalam_parser_new(void)
{
	return  iscii_parser_new( ISCII_MALAYALAM) ;
}

mkf_parser_t *
mkf_iscii_oriya_parser_new(void)
{
	return  iscii_parser_new( ISCII_ORIYA) ;
}

mkf_parser_t *
mkf_iscii_punjabi_parser_new(void)
{
	return  iscii_parser_new( ISCII_PUNJABI) ;
}

mkf_parser_t *
mkf_iscii_roman_parser_new(void)
{
	return  iscii_parser_new( ISCII_ROMAN) ;
}

mkf_parser_t *
mkf_iscii_tamil_parser_new(void)
{
	return  iscii_parser_new( ISCII_TAMIL) ;
}

mkf_parser_t *
mkf_iscii_telugu_parser_new(void)
{
	return  iscii_parser_new( ISCII_TELUGU) ;
}
