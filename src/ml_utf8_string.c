/*
 *	update: <2001/11/26(02:34:14)>
 *	$Id$
 */

#include  "ml_utf8_string.h"

#include  <stdio.h>
#include  <kiklib/kik_debug.h>
#include  <kiklib/kik_mem.h>		/* alloca */
#include  <mkf/mkf_ucs4_parser.h>
#include  <mkf/mkf_utf8_conv.h>


/* --- static variables --- */

static mkf_parser_t *  ucs4_parser = NULL ;
static mkf_conv_t *  utf8_conv = NULL ;


/* --- static functions --- */

static size_t
convert_to_ucs4(
	u_char *  ucs4_str ,
	size_t  ucs4_len ,
	ml_char_t *  mlchars ,
	u_int  mlchars_len
	)
{
	int  mlch_counter ;
	int  ucs4_counter ;
	ml_char_t *  comb_chars ;
	u_int  comb_size ;

	ucs4_counter = 0 ;
	for( mlch_counter = 0 ; mlch_counter < mlchars_len ; mlch_counter ++)
	{
		if( ucs4_counter + 4 > ucs4_len)
		{
		#ifdef  DEBUG
			kik_warn_printf( KIK_DEBUG_TAG " shortage of buffer !\n") ;
		#endif
		
			return  ucs4_counter ;
		}

		if( ml_char_cs( &mlchars[mlch_counter]) == US_ASCII)
		{
			ucs4_str[ucs4_counter] = 0x0 ;
			ucs4_str[ucs4_counter + 1] = 0x0 ;
			ucs4_str[ucs4_counter + 2] = 0x0 ;
			ucs4_str[ucs4_counter + 3] = ml_char_bytes( &mlchars[mlch_counter])[0] ;
		}
		else if( ml_char_cs( &mlchars[mlch_counter]) == ISO10646_UCS2_1)
		{
			ucs4_str[ucs4_counter] = 0x0 ;
			ucs4_str[ucs4_counter + 1] = 0x0 ;
			memcpy( &ucs4_str[ucs4_counter + 2] , ml_char_bytes( &mlchars[mlch_counter]) , 2) ;
		}
		else if( ml_char_cs( &mlchars[mlch_counter]) == ISO10646_UCS4_1)
		{
			memcpy( ucs4_str , ml_char_bytes( &mlchars[mlch_counter]) , 4) ;
		}
		else
		{
			continue ;
		}
		
		ucs4_counter += 4 ;

		if( ( comb_chars = ml_get_combining_chars( &mlchars[mlch_counter] , &comb_size)))
		{
			ucs4_counter += convert_to_ucs4( &ucs4_str[ucs4_counter] ,
				ucs4_len - ucs4_counter , comb_chars , comb_size) ;
		}
	}

	return  ucs4_counter ;
}


/* --- global functions --- */

/*
 * !!! Notice !!!
 * this converts ml_char_t string to utf8 string , and ml_char_t must be ISO10646.
 * even if ml_char_t-s of other than ISO10646 are given , this doesn't map them to
 * ISO10646 because this is implemented only for UTF8_STRING selection(copy&paste).
 * If you want to copy and paste chars other than ISO10646 , don't use this but use
 * ml_xcompound_text.{ch} and XCOMPOUND_TEXT selection.
 */
size_t
ml_convert_to_utf8(
	u_char *  utf8_str ,
	size_t  utf8_len ,
	ml_char_t *  str ,
	u_int  str_len
	)
{
	u_char *  ucs4_str ;
	size_t  ucs4_len ;
	size_t  filled_len ;

	/*
	 * XXX
	 * on ucs4 , char length is max 4 bytes.
	 * combining chars may be max 2 per char.
	 * so , I think this is enough , but I'm not sure.
	 */
	ucs4_len = (str_len * 4) * 3 ;

	if( ( ucs4_str = alloca( ucs4_len * sizeof( char))) == NULL)
	{
		return  0 ;
	}
	
	filled_len = convert_to_ucs4( ucs4_str , ucs4_len , str , str_len) ;

	if( ucs4_parser == NULL)
	{
		ucs4_parser = mkf_ucs4_parser_new() ;
	}
	else
	{
		(*ucs4_parser->set_str)( ucs4_parser , ucs4_str , filled_len) ;
	}

	if( utf8_conv == NULL)
	{
		utf8_conv = mkf_utf8_conv_new() ;
	}

	filled_len = (*utf8_conv->convert)( utf8_conv , utf8_str , filled_len , ucs4_parser) ;

	return  filled_len ;
}
