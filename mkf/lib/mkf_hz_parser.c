/*
 *	update: <2001/11/26(02:42:25)>
 *	$Id$
 */

#include  "mkf_hz_parser.h"

#include  <kiklib/kik_debug.h>
#include  <kiklib/kik_mem.h>


typedef struct  mkf_hz_parser
{
	mkf_parser_t  parser ;
	mkf_charset_t  cur_cs ;

} mkf_hz_parser_t ;


/* --- static functions --- */

static void
hz_parser_init(
	mkf_parser_t *  parser
	)
{
	mkf_hz_parser_t *  hz_parser ;

	mkf_parser_init( parser) ;

	hz_parser = (mkf_hz_parser_t*) parser ;

	hz_parser->cur_cs = US_ASCII ;
}

static void
hz_parser_set_str(
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
hz_parser_delete(
	mkf_parser_t *  parser
	)
{
	free( parser) ;
}

static int
hz_parser_next_char(
	mkf_parser_t *  parser ,
	mkf_char_t *  ch
	)
{
	mkf_hz_parser_t *  hz_parser ;

	hz_parser = (mkf_hz_parser_t*) parser ;

	if( parser->is_eos)
	{
		return  0 ;
	}

	while( 1)
	{
		mkf_parser_mark( parser) ;

		if( *parser->str != '~')
		{
			/* is not tilda */

			break ;
		}
		else
		{
			if( mkf_parser_increment( parser) == 0)
			{
				mkf_parser_reset( parser) ;

				return  0 ;
			}
		}

		/* is tilda */
		
		if( *parser->str == '~')
		{
			/* double tilda */
			
			ch->ch[0] = *parser->str ;
			ch->size = 1 ;
			ch->cs = US_ASCII ;
			ch->property = 0 ;

			mkf_parser_increment( parser) ;

			return  1 ;
		}
		else if( *parser->str == '{')
		{
			/* ~{ */
			
			hz_parser->cur_cs = GB2312_80 ;
		}
		else if( *parser->str == '}')
		{
			/* ~} */
			
			hz_parser->cur_cs = US_ASCII ;
		}
		else if( *parser->str == '\n')
		{
			/* line continuation */
		}
		else
		{
			/*
			 * XXX
			 * this is an illegal format(see rfc-1843) , but for the time being
			 * the char of parser->str[-1](=='~') is output.
			 */
			
			ch->ch[0] = '~' ;
			ch->size = 1 ;
			ch->cs = US_ASCII ;
			ch->property = 0 ;

			/* already incremented. */

			return  1 ;
		}

		if( mkf_parser_increment( parser) == 0)
		{
			/*
			 * a set of hz sequence was completely parsed ,
			 * so mkf_parser_reset() is not executed here.
			 */

			return  0 ;
		}
	}

	if( /* 0x0 <= *parser->str && */ *parser->str <= 0x1f || hz_parser->cur_cs == US_ASCII)
	{
		/* control char */
		
		ch->ch[0] = *parser->str ;
		ch->size = 1 ;
		ch->cs = US_ASCII ;
	}
	else if( hz_parser->cur_cs == GB2312_80)
	{
		ch->ch[0] = *parser->str ;

		if( mkf_parser_increment( parser) == 0)
		{
			mkf_parser_reset( parser) ;

			return  0 ;
		}

		ch->ch[1] = *parser->str ;
		ch->size = 2 ;
		ch->cs = GB2312_80 ;
	}
	else
	{
	#ifdef  DEBUG
		kik_warn_printf( KIK_DEBUG_TAG
			" hz does not support charsets but GB2312 and US_ASCII.\n") ;
	#endif

		ch->ch[0] = ' ' ;
		ch->size = 1 ;
		ch->cs = US_ASCII ;

		hz_parser->cur_cs = US_ASCII ;
	}

	ch->property = 0 ;

	mkf_parser_increment( parser) ;

	return  1 ;
}


/* --- global functions --- */

mkf_parser_t *
mkf_hz_parser_new(void)
{
	mkf_hz_parser_t *  hz_parser ;

	if( ( hz_parser = malloc( sizeof( mkf_hz_parser_t))) == NULL)
	{
		return  NULL ;
	}

	hz_parser_init( ( mkf_parser_t*) hz_parser) ;	

	hz_parser->parser.init = hz_parser_init ;
	hz_parser->parser.set_str = hz_parser_set_str ;
	hz_parser->parser.delete = hz_parser_delete ;
	hz_parser->parser.next_char = hz_parser_next_char ;

	return  (mkf_parser_t*) hz_parser ;
}
