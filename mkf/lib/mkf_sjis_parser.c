/*
 *	update: <2001/11/26(02:44:24)>
 *	$Id$
 */

#include  "mkf_sjis_parser.h"

#include  <string.h>			/* memset */
#include  <kiklib/kik_mem.h>
#include  <kiklib/kik_debug.h>

#include  "mkf_iso2022_parser.h"
#include  "mkf_sjis_env.h"
#include  "mkf_jisx0208_1983_property.h"
#include  "mkf_jisx0213_2000_property.h"


/* --- static functions --- */

static int
sjis_parser_next_char_intern(
	mkf_parser_t *  sjis_parser ,
	mkf_char_t *  ch ,
	int  is_sjisx0213
	)
{
	u_char  c1 ;
	
	if( sjis_parser->is_eos)
	{
		return  0 ;
	}
	
	/* initialize */
	mkf_parser_mark( sjis_parser) ;
	memset( ch , 0 , sizeof( mkf_char_t)) ;
	
	c1 = *sjis_parser->str ;
	
	if( c1 <= 0x7E)
	{
		ch->cs = US_ASCII ;
		*ch->ch = c1 ;
		ch->size = 1 ;
		ch->property = 0 ;
	}
	else if( 0xa1 <= c1 && c1 <= 0xdf)
	{
		ch->cs = JISX0201_KATA ;
		*ch->ch = UNSET_MSB(c1) ;
		ch->size = 1 ;
		ch->property = 0 ;
	}
	else
	{
		u_char  c2 ;
		u_int16_t  sjis_ch ;
		u_char  high ;
		u_char  low ;
		mkf_charset_t  cs ;
		
		if( mkf_parser_increment( sjis_parser) == 0)
		{
			mkf_parser_reset( sjis_parser) ;
			
			return  0 ;
		}

		c2 = *sjis_parser->str ;

		/*
		 * specifying character set type.
		 */
		 
		sjis_ch = ((c1 << 8) & 0xff00) + (c2 & 0xff) ;

		if( is_sjisx0213)
		{
			if( c1 >= 0xf0)
			{
				cs = JISX0213_2000_2 ;
			}
			else
			{
				cs = JISX0213_2000_1 ;
			}
		}
		else
		{
			if( mkf_get_sjis_output_type() == APPLE_CS)
			{
				/*
				 * XXX
				 * this check is not exact , but not a problem for practical use.
				 */
				if( 0x00fd <= sjis_ch && sjis_ch <= 0x00ff)
				{
					cs = JISX0208_1983_MAC_EXT ;
				}
				else if( 0x8540 <= sjis_ch && sjis_ch <= 0x886d)
				{
					cs = JISX0208_1983_MAC_EXT ;
				}
				else if( 0xeb41 <= sjis_ch && sjis_ch <= 0xed96)
				{
					cs = JISX0208_1983_MAC_EXT ;
				}
				else
				{
					cs = JISX0208_1983 ;
				}
			}
			else /* if( mkf_get_sjis_output_type() == MICROSOFT_CS) */
			{
				/*
				 * XXX
				 * this check is not exact , but not a problem for practical use.
				 */
				if( 0x8740 <= sjis_ch && sjis_ch <= 0x879c)
				{
					cs = JISC6226_1978_NEC_EXT ;
				}
				else if( 0xed40 <= sjis_ch && sjis_ch <= 0xeefc)
				{
					cs = JISC6226_1978_NECIBM_EXT ;
				}
				else if( 0xfa40 <= sjis_ch && sjis_ch <= 0xfc4b)
				{
					cs = SJIS_IBM_EXT ;
				}
				else
				{
					cs = JISX0208_1983 ;
				}
			}
		}

		/*
		 * converting SJIS -> JIS process.
		 */

		if( cs == SJIS_IBM_EXT)
		{
			/*
			 * SJIS_IBM_EXT
			 * 
			 * IBM extension characters are placed in the empty space of Shift-JIS encoding ,
			 * then these characters cannot be mapped to jisx0208 which uses only 0x20-0x7f
			 * because the decoded byte of them can be 0x93,0x94,0x95,0x96...
			 * So , we keep them sjis encoded bytes in mkf_char_t as
			 * JIS6226_1978_IBM_EXT charset.
			 */

			ch->cs = SJIS_IBM_EXT ;
			ch->ch[0] = c1 ;
			ch->ch[1] = c2 ;
			ch->size = 2 ;
		}
		else if( cs == JISX0213_2000_2)
		{
			u_char sjis_upper_to_jisx02132_map_1[] =
			{
				/* 0xf0 - 0xfc(sjis) */
				0x21 , 0x23 , 0x25 , 0x2d , 0x2f , 0x6f , 0x71 , 0x73 ,
				0x75 , 0x77 , 0x79 , 0x7b , 0x7d ,
			} ;
			
			u_char sjis_upper_to_jisx02132_map_2[] =
			{
				/* 0xf0 - 0xfc(sjis) */
				0x28 , 0x24 , 0x2c , 0x2e , 0x6e , 0x70 , 0x72 , 0x74 ,
				0x76 , 0x78 , 0x7a , 0x7c , 0x7e ,
			} ;

			if( 0xf0 <= c1 && c1 <= 0xfc)
			{
			#ifdef  DEBUG
				kik_warn_printf( KIK_DEBUG_TAG
					" 0x%.2x is illegal upper byte of jisx0213_2.\n" , c1) ;
			#endif
			
				mkf_parser_reset( sjis_parser) ;

				return  0 ;
			}
			
			if( c2 <= 0x9e)
			{
				high = sjis_upper_to_jisx02132_map_1[c1 - 0xf0] ;
				
				if( c2 > 0x7e)
				{
					low = c2 - 0x20 ;
				}
				else
				{
					low = c2 - 0x1f ;
				}
			}
			else
			{
				high = sjis_upper_to_jisx02132_map_2[c1 - 0xf0] ;
				
				c2 -= 0x7e ;
			}
		}
		else
		{
			/*
			 * JISX0213 2000 2
			 */
			
			if( 0x81 <= c1 && c1 <= 0x9f)
			{
				high = c1 - 0x71 ;
			}
			else if( 0xe0 <= c1 && c1 <= 0xfc)
			{
				high = c1 - 0xb1 ;
			}
			else
			{
				/* XXX what's this ? */

				mkf_parser_reset( sjis_parser) ;

				return  0 ;
			}

			high = high * 2 + 1 ;

			if( 0x80 <= c2)
			{
				low = c2 - 1 ;
			}
			else
			{
				low = c2 ;
			}

			if( 0x9e <= low && low <= 0xfb)
			{
				low -= 0x7d ;
				high ++ ;
			}
			else if( 0x40 <= low && low <= 0x9d)
			{
				low -= 0x1f ;
			}
			else
			{
				/* what's this ? */

				mkf_parser_reset( sjis_parser) ;

				return  0 ;
			}

			ch->ch[0] = high ;
			ch->ch[1] = low ;
			ch->size = 2 ;
			ch->cs = cs ;
		}

		if( cs == JISX0208_1983)
		{
			ch->property = mkf_get_jisx0208_1983_property( ch->ch , ch->size) ;
		}
		else if( cs == JISX0213_2000_1)
		{
			ch->property = mkf_get_jisx0213_2000_1_property( ch->ch , ch->size) ;
		}
		else
		{
			ch->property = 0 ;
		}
	}

	mkf_parser_increment( sjis_parser) ;

	return  1 ;
}

static int
sjis_parser_next_char(
	mkf_parser_t *  sjis_parser ,
	mkf_char_t *  ch
	)
{
	return  sjis_parser_next_char_intern( sjis_parser , ch , 0) ;
}

static int
sjisx0213_parser_next_char(
	mkf_parser_t *  sjis_parser ,
	mkf_char_t *  ch
	)
{
	return  sjis_parser_next_char_intern( sjis_parser , ch , 1) ;
}

static void
sjis_parser_set_str(
	mkf_parser_t *  sjis_parser ,
	u_char *  str ,
	size_t  size
	)
{
	sjis_parser->str = str ;
	sjis_parser->left = size ;
	sjis_parser->marked_left = 0 ;
	sjis_parser->is_eos = 0 ;
}

static void
sjis_parser_delete(
	mkf_parser_t *  s
	)
{
	free( s) ;
}


/* --- global functions --- */

mkf_parser_t *
mkf_sjis_parser_new(void)
{
	mkf_parser_t *  sjis_parser ;
	
	if( ( sjis_parser = malloc( sizeof( mkf_parser_t))) == NULL)
	{
		return  NULL ;
	}

	mkf_parser_init( sjis_parser) ;

	sjis_parser->init = mkf_parser_init ;
	sjis_parser->set_str = sjis_parser_set_str ;
	sjis_parser->delete = sjis_parser_delete ;
	sjis_parser->next_char = sjis_parser_next_char ;

	return  sjis_parser ;
}

mkf_parser_t *
mkf_sjisx0213_parser_new(void)
{
	mkf_parser_t *  sjis_parser ;
	
	if( ( sjis_parser = malloc( sizeof( mkf_parser_t))) == NULL)
	{
		return  NULL ;
	}

	mkf_parser_init( sjis_parser) ;

	sjis_parser->init = mkf_parser_init ;
	sjis_parser->set_str = sjis_parser_set_str ;
	sjis_parser->delete = sjis_parser_delete ;
	sjis_parser->next_char = sjisx0213_parser_next_char ;

	return  sjis_parser ;
}
