/*
 *	update: <2001/11/26(02:42:32)>
 *	$Id$
 */

#include  "mkf_iso2022_conv.h"
#include  "mkf_xct_conv.h"

#include  <kiklib/kik_mem.h>
#include  <kiklib/kik_debug.h>

#include  "mkf_ucs4_map.h"
#include  "mkf_iso2022_intern.h"


typedef struct  mkf_iso2022_conv
{
	mkf_conv_t   conv ;
	mkf_charset_t  gl ;
	mkf_charset_t  gr ;
	
} mkf_iso2022_conv_t ;


/* --- static functions --- */

static size_t
convert_to_iso2022(
	mkf_conv_t *  conv ,
	u_char *  dst ,
	size_t  dst_size ,
	mkf_parser_t *  parser
	)
{
	size_t  filled_size ;
	mkf_char_t  ch ;
	mkf_iso2022_conv_t *  iso2022_conv ;

	iso2022_conv = (mkf_iso2022_conv_t*)conv ;

	filled_size = 0 ;
	while( 1)
	{
		int  counter ;

		mkf_parser_mark( parser) ;

		if( ! (*parser->next_char)( parser , &ch))
		{
			if( parser->is_eos)
			{
			#ifdef  __DEBUG
				kik_debug_printf( KIK_DEBUG_TAG
					" parser reached the end of string.\n") ;
			#endif
			
				return  filled_size ;
			}
			else
			{
			#ifdef  DEBUG
				kik_warn_printf( KIK_DEBUG_TAG
					" parser->init() returns error , but the process is continuing...\n") ;
			#endif

				/*
				 * passing unrecognized byte...
				 */
				if( mkf_parser_increment( parser) == 0)
				{
					return  filled_size ;
				}

				continue ;
			}
		}

		if( IS_CS94SB(ch.cs) || IS_CS94MB(ch.cs))
		{
			if( ch.cs != iso2022_conv->gl)
			{
				if( IS_CS94SB(ch.cs))
				{
					if( filled_size + ch.size + 2 >= dst_size)
					{
						mkf_parser_reset( parser) ;

						return  filled_size ;
					}

					*(dst ++) = '\x1b' ;
					*(dst ++) = '(' ;
					*(dst ++) = CS94SB_FT(ch.cs) ;

					filled_size += 3 ;
				}
				else
				{
					if( filled_size + ch.size + 3 >= dst_size)
					{
						mkf_parser_reset( parser) ;

						return  filled_size ;
					}

					*(dst ++) = '\x1b' ;
					*(dst ++) = '$' ;
					*(dst ++) = '(' ;
					*(dst ++) = CS94MB_FT(ch.cs) ;

					filled_size += 4 ;
				}
				
				iso2022_conv->gl = ch.cs ;
			}
			else
			{
				if( filled_size + ch.size - 1 >= dst_size)
				{
					mkf_parser_reset( parser) ;
			
					return  filled_size ;
				}
			}
			
			for( counter = 0 ; counter < ch.size ; counter++)
			{
				*(dst ++) = ch.ch[counter] ;
			}

			filled_size += ch.size ;
		}
		else if( IS_CS96SB(ch.cs) || IS_CS96MB(ch.cs))
		{
			if( ch.cs != iso2022_conv->gr)
			{
				if( IS_CS96SB(ch.cs))
				{
					if( filled_size + ch.size + 2 >= dst_size)
					{
						mkf_parser_reset( parser) ;

						return  filled_size ;
					}

					*(dst ++) = '\x1b' ;
					*(dst ++) = '-' ;
					*(dst ++) = CS96SB_FT(ch.cs) ;

					filled_size += 3 ;
				}
				else
				{
					if( filled_size + ch.size + 3 >= dst_size)
					{
						mkf_parser_reset( parser) ;

						return  filled_size ;
					}

					*(dst ++) = '\x1b' ;
					*(dst ++) = '$' ;
					*(dst ++) = '-' ;
					*(dst ++) = CS96MB_FT(ch.cs) ;

					filled_size += 4 ;
				}
				
				iso2022_conv->gr = ch.cs ;
			}
			else
			{
				if( filled_size + ch.size - 1 >= dst_size)
				{
					mkf_parser_reset( parser) ;
			
					return  filled_size ;
				}
			}
			
			for( counter = 0 ; counter < ch.size ; counter++)
			{
				*(dst ++) = MAP_TO_GR(ch.ch[counter]) ;
			}

			filled_size += ch.size ;
		}
		else
		{
		#ifdef  DEBUG
			kik_warn_printf( KIK_DEBUG_TAG
				" cs(%x) is not supported by iso2022. char(%x) is discarded.\n" ,
				ch.cs , mkf_char_to_int( &ch)) ;
		#endif

			if( filled_size >= dst_size)
			{
				mkf_parser_reset( parser) ;
			
				return  filled_size ;
			}
			
			*(dst ++) = ' ' ;
			filled_size ++ ;
		}		
	}
}

static void
iso2022_conv_init(
	mkf_conv_t *  conv
	)
{
	mkf_iso2022_conv_t *  iso2022_conv ;

	iso2022_conv = (mkf_iso2022_conv_t*) conv ;
	
	/* GL = G0 / GR = G1 is implicitly invoked , and G0 / G1 is not still set */
	iso2022_conv->gl = UNKNOWN_CS ;
	iso2022_conv->gr = UNKNOWN_CS ;
}

static void
xct_conv_init(
	mkf_conv_t *  conv
	)
{
	mkf_iso2022_conv_t *  iso2022_conv ;

	iso2022_conv = (mkf_iso2022_conv_t*) conv ;
	
	/* GL = G0 / GR = G1 is implicitly invoked */
	iso2022_conv->gl = US_ASCII ;
	iso2022_conv->gr = ISO8859_1_R ;
}

static void
conv_delete(
	mkf_conv_t *  conv
	)
{
	free( conv) ;
}


/* --- global functions --- */

/*
 * 8 bit ISO2022
 */
mkf_conv_t *
mkf_iso2022_conv_new(void)
{
	mkf_iso2022_conv_t *  iso2022_conv ;

	if( ( iso2022_conv = malloc( sizeof( mkf_iso2022_conv_t))) == NULL)
	{
		return  NULL ;
	}

	iso2022_conv->conv.convert = convert_to_iso2022 ;
	iso2022_conv->conv.init = iso2022_conv_init ;
	iso2022_conv->conv.delete = conv_delete ;

	/* GL = G0 / GR = G1 is implicitly invoked , and G0 / G1 is not still set */
	iso2022_conv->gl = UNKNOWN_CS ;
	iso2022_conv->gr = UNKNOWN_CS ;

	return  (mkf_conv_t*)iso2022_conv ;
}

mkf_conv_t *
mkf_xct_conv_new(void)
{
	mkf_iso2022_conv_t *  iso2022_conv ;

	if( ( iso2022_conv = malloc( sizeof( mkf_iso2022_conv_t))) == NULL)
	{
		return  NULL ;
	}

	iso2022_conv->conv.convert = convert_to_iso2022 ;
	iso2022_conv->conv.init = xct_conv_init ;
	iso2022_conv->conv.delete = conv_delete ;

	/* GL = G0 / GR = G1 is implicitly invoked */
	iso2022_conv->gl = US_ASCII ;
	iso2022_conv->gr = ISO8859_1_R ;

	return  (mkf_conv_t*)iso2022_conv ;
}
