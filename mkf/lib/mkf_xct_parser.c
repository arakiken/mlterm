/*
 *	$Id$
 */

#include  "mkf_xct_parser.h"

#include  <stdio.h>		/* NULL */
#include  <string.h>		/* strncmp */
#include  <kiklib/kik_debug.h>
#include  <kiklib/kik_mem.h>	/* malloc */
#include  <kiklib/kik_types.h>	/* size_t */

#include  "mkf_iso2022_parser.h"


typedef struct mkf_xct_parser
{
	mkf_iso2022_parser_t  iso2022_parser ;
	size_t  left ;
	mkf_charset_t  cs ;
	int8_t  big5_buggy ;
	
} mkf_xct_parser_t ;


/* --- static functions --- */

static int
xct_non_iso2022_is_started(
	mkf_iso2022_parser_t *  iso2022_parser
	)
{
	mkf_xct_parser_t *  xct_parser ;
	u_char  m ;
	u_char  l ;
	size_t  len ;
	u_char *  cs_str ;
	size_t  cs_len ;
	
#ifdef  __DEBUG
	{
		int  i ;
		
		fprintf( stderr , "non iso2022 sequence -->\n") ;
		for( i = 0 ; i < iso2022_parser->parser.left ; i ++)
		{
			fprintf( stderr , "%.2x " , iso2022_parser->parser.str[i]) ;
		}
		fprintf( stderr , "\n") ;
	}
#endif

	xct_parser = (mkf_xct_parser_t*)iso2022_parser ;

	/*
	 * parsing string len.
	 */
	 
	m = *xct_parser->iso2022_parser.parser.str ;

	if( mkf_parser_increment( xct_parser) == 0)
	{
		mkf_parser_reset( xct_parser) ;

		return  0 ;
	}

	l = *xct_parser->iso2022_parser.parser.str ;

	len = ((m - 128) * 128) + (l - 128) ;

	/*
	 * parsing charset name.
	 */
	 
	cs_str = xct_parser->iso2022_parser.parser.str + 1 ;
	cs_len = 0 ;

	while( 1)
	{
		if( len == 0)
		{
			return  1 ;
		}
		
		if( mkf_parser_increment( xct_parser) == 0)
		{
			mkf_parser_reset( xct_parser) ;

			return  0 ;
		}

		len -- ;
		
		if( *xct_parser->iso2022_parser.parser.str == 0x02)
		{
			break ;
		}

		cs_len ++ ;
	}

	if( xct_parser->iso2022_parser.non_iso2022_cs == XCT_NON_ISO2022_CS_1)
	{
		if( cs_len == 6 && strncmp( cs_str , "koi8-r" , cs_len) == 0)
		{
			xct_parser->cs = KOI8_R ;
		}
		else if( cs_len == 6 && strncmp( cs_str , "koi8-u" , cs_len) == 0)
		{
			xct_parser->cs = KOI8_U ;
		}
		else if( cs_len == 11 && strncmp( cs_str , "viscii1.1-1" , cs_len) == 0)
		{
			xct_parser->cs = VISCII ;
		}
		else
		{
			/* unknown cs */
			return  0 ;
		}
	}
	else if( xct_parser->iso2022_parser.non_iso2022_cs == XCT_NON_ISO2022_CS_2)
	{
		if( cs_len == 6 && strncmp( cs_str , "big5-0" , cs_len) == 0)
		{
			xct_parser->cs = BIG5 ;
		}
		else if( cs_len == 6 && strncmp( cs_str , "BIG5-0" , cs_len) == 0)
		{
			/*
			 * !! Notice !!
			 * Big5 CTEXT implementation of XFree86 4.1.0 or before is very BUGGY!
			 */
			if( xct_parser->iso2022_parser.parser.left >= 10 &&
				memcmp( xct_parser->iso2022_parser.parser.str ,
					"\x02\x80\x89" "BIG5-0" "\x02" , 10) == 0)
			{
				/* skip to next 0x2 */
				xct_parser->iso2022_parser.parser.str += 9 ;
				xct_parser->iso2022_parser.parser.left -= 9 ;
				xct_parser->big5_buggy = 1 ;
			}
			
			xct_parser->cs = BIG5 ;
		}
		else if( cs_len == 5 && strncmp( cs_str , "gbk-0" , cs_len) == 0)
		{
			xct_parser->cs = GBK ;
		}
		else
		{
			/* unknown cs */
			return  0 ;
		}
	}
	else
	{
		/* unknown cs */
		return  0 ;
	}

	xct_parser->left = len ;
	
	/*
	 * ok.
	 */
	
#ifdef  __DEBUG
	kik_debug_printf( KIK_DEBUG_TAG " parsing str of cs %x len %d.\n" , xct_parser->cs , len) ;
#endif

	mkf_parser_increment( xct_parser) ;

	return  1 ;
}

static int
xct_next_non_iso2022_byte(
	mkf_iso2022_parser_t *  iso2022_parser ,
	mkf_char_t *  ch
	)
{
	mkf_xct_parser_t *  xct_parser ;

	xct_parser = (mkf_xct_parser_t*)iso2022_parser ;

	if( xct_parser->left == 0)
	{
		/*
		 * !! Notice !!
		 * Big5 CTEXT implementation of XFree86 4.1.0 or before is very BUGGY!
		 */
		if( xct_parser->big5_buggy && xct_parser->cs == BIG5 &&
			0xa1 <= *xct_parser->iso2022_parser.parser.str &&
				*xct_parser->iso2022_parser.parser.str <= 0xf9)
		{
			xct_parser->left = 2 ;
		}
		else
		{
			xct_parser->cs = UNKNOWN_CS ;
			xct_parser->big5_buggy = 0 ;
			
			return  0 ;
		}
	}

	ch->ch[ch->size ++] = *xct_parser->iso2022_parser.parser.str ;
	xct_parser->left -- ;
	ch->cs = xct_parser->cs ;
	
	mkf_parser_increment( xct_parser) ;

	return  1 ;
}

static void
xct_parser_init(
	mkf_parser_t *  parser
	)
{
	mkf_xct_parser_t *  xct_parser ;
	
	mkf_parser_init( parser) ;

	xct_parser = (mkf_xct_parser_t*) parser ;

	xct_parser->iso2022_parser.g0 = US_ASCII ;
	xct_parser->iso2022_parser.g1 = ISO8859_1_R ;
	xct_parser->iso2022_parser.g2 = UNKNOWN_CS ;
	xct_parser->iso2022_parser.g3 = UNKNOWN_CS ;

	xct_parser->iso2022_parser.gl = &xct_parser->iso2022_parser.g0 ;
	xct_parser->iso2022_parser.gr = &xct_parser->iso2022_parser.g1 ;

	xct_parser->iso2022_parser.non_iso2022_cs = UNKNOWN_CS ;

	xct_parser->iso2022_parser.is_single_shifted = 0 ;
}


/* --- global functions --- */

mkf_parser_t *
mkf_xct_parser_new(void)
{
	mkf_xct_parser_t *  xct_parser ;

	if( ( xct_parser = malloc(sizeof( mkf_xct_parser_t))) == NULL)
	{
		return  NULL ;
	}

	mkf_iso2022_parser_init_func( &xct_parser->iso2022_parser) ;

	xct_parser_init( &xct_parser->iso2022_parser.parser) ;

	xct_parser->left = 0 ;
	xct_parser->cs = UNKNOWN_CS ;
	xct_parser->big5_buggy = 0 ;
	
	/* overrride */
	xct_parser->iso2022_parser.non_iso2022_is_started = xct_non_iso2022_is_started ;
	xct_parser->iso2022_parser.next_non_iso2022_byte = xct_next_non_iso2022_byte ;
	xct_parser->iso2022_parser.parser.init = xct_parser_init ;

	return  (mkf_parser_t*) xct_parser ;
}
