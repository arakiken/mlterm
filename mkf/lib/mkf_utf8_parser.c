/*
 *	$Id$
 */

#include  "mkf_utf8_parser.h"

#include  <kiklib/kik_mem.h>
#include  <kiklib/kik_debug.h>

#include  "mkf_ucs_property.h"


/* --- static functions --- */

static int
utf8_parser_next_char(
	mkf_parser_t *  utf8_parser ,
	mkf_char_t *  ucs4_ch
	)
{
	u_char *  utf8_ch ;
	mkf_ucs_property_t  prop ;

	if( utf8_parser->is_eos)
	{
		return  0 ;
	}

	utf8_ch = utf8_parser->str ;
	
	if( ( utf8_ch[0] & 0xc0) == 0x80)
	{
	#ifdef  DEBUG
		kik_warn_printf( KIK_DEBUG_TAG
			" %x is not the first byte of utf8 character.\n" , utf8_ch[0]) ;
	#endif

		mkf_parser_increment( utf8_parser) ;

		return  0 ;
	}
	
	if( ( utf8_ch[0] & 0x80) == 0)
	{
		/* 0x00 - 0x7f */
		ucs4_ch->ch[0] = '\x00' ;
		ucs4_ch->ch[1] = '\x00' ;
		ucs4_ch->ch[2] = '\x00' ;
		ucs4_ch->ch[3] = utf8_ch[0] ;
		
		mkf_parser_increment( utf8_parser) ;
	}
	else
	{
		u_int32_t  ucs4_int ;
		size_t  bytes ;

		if( ( utf8_ch[0] & 0xe0) == 0xc0)
		{
			bytes = 2 ;
			if( utf8_parser->left < bytes)
			{
				utf8_parser->is_eos = 1 ;
				
				return  0 ;
			}
			
			ucs4_int = (((utf8_ch[0] & 0x1f) << 6) & 0xffffffc0) |
				(utf8_ch[1] & 0x3f) ;


			if( ucs4_int < 0x80)
			{
				goto  utf8_err ;
			}
		}
		else if( ( utf8_ch[0] & 0xf0) == 0xe0)
		{
			bytes = 3 ;
			if( utf8_parser->left < bytes)
			{
				utf8_parser->is_eos = 1 ;

				return  0 ;
			}

			ucs4_int = (((utf8_ch[0] & 0x0f) << 12) & 0xffff000) |
				(((utf8_ch[1] & 0x3f) << 6) & 0xffffffc0) |
				(utf8_ch[2] & 0x3f) ;

			if( ucs4_int < 0x800)
			{
				goto  utf8_err ;
			}
		}
		else if( ( utf8_ch[0] & 0xf8) == 0xf0)
		{
			bytes = 4 ;
			if( utf8_parser->left < bytes)
			{
				utf8_parser->is_eos = 1 ;

				return  0 ;
			}

			ucs4_int = (((utf8_ch[0] & 0x07) << 18) & 0xfffc0000) |
				(((utf8_ch[1] & 0x3f) << 12) & 0xffff000) |
				(((utf8_ch[2] & 0x3f) << 6) & 0xffffffc0) |
				(utf8_ch[3] & 0x3f) ;

			if( ucs4_int < 0x10000)
			{
				goto  utf8_err ;
			}
		}
		else if( ( utf8_ch[0] & 0xfc) == 0xf8)
		{
			bytes = 5 ;
			if( utf8_parser->left < bytes)
			{
				utf8_parser->is_eos = 1 ;

				return  0 ;
			}

			ucs4_int = (((utf8_ch[0] & 0x03) << 24) & 0xff000000) |
				(((utf8_ch[1] & 0x3f) << 18) & 0xfffc0000) |
				(((utf8_ch[2] & 0x3f) << 12) & 0xffff000) |
				(((utf8_ch[3] & 0x3f) << 6) & 0xffffffc0) |
				(utf8_ch[4] & 0x3f) ;

			if( ucs4_int < 0x200000)
			{
				goto  utf8_err ;
			}
		}
		else if( ( utf8_ch[0] & 0xfe) == 0xfc)
		{
			bytes = 6 ;
			if( utf8_parser->left < bytes)
			{
				utf8_parser->is_eos = 1 ;

				return  0 ;
			}

			ucs4_int = (((utf8_ch[0] & 0x01 << 30) & 0xc0000000)) |
				(((utf8_ch[1] & 0x3f) << 24) & 0xff000000) |
				(((utf8_ch[2] & 0x3f) << 18) & 0xfffc0000) |
				(((utf8_ch[3] & 0x3f) << 12) & 0xffff000) |
				(((utf8_ch[4] & 0x3f) << 6) & 0xffffffc0) |
				(utf8_ch[4] & 0x3f) ;

			if( ucs4_int < 0x4000000)
			{
				goto  utf8_err ;
			}
		}
		else
		{
		#ifdef  DEBUG
			kik_warn_printf( KIK_DEBUG_TAG
				" 0x%x is a strange as the first utf8 byte.\n" , utf8_ch[0]) ;
		#endif

			goto  utf8_err ;
		}

		ucs4_ch->ch[0] = ((ucs4_int >> 24) & 0xff) ;
		ucs4_ch->ch[1] = ((ucs4_int >> 16) & 0xff) ;
		ucs4_ch->ch[2] = ((ucs4_int >> 8) & 0xff) ;
		ucs4_ch->ch[3] = (ucs4_int & 0xff) ;
		
		mkf_parser_n_increment( utf8_parser , bytes) ;
	}

	ucs4_ch->size = 4 ;
	ucs4_ch->cs = ISO10646_UCS4_1 ;
	
	prop = mkf_get_ucs_property( ucs4_ch->ch , ucs4_ch->size) ;
	if( UCSPROP_GENERAL_CATEGORY(prop) == MKF_UCSPROP_MC ||
		UCSPROP_GENERAL_CATEGORY(prop) == MKF_UCSPROP_ME ||
		UCSPROP_GENERAL_CATEGORY(prop) == MKF_UCSPROP_MN)
	{
		ucs4_ch->property = MKF_COMBINING ;
	}
	else
	{
		ucs4_ch->property = 0 ;
	}
	
	return  1 ;
	
utf8_err:
#ifdef  DEBUG
	kik_warn_printf( KIK_DEBUG_TAG " illegal utf8 format.\n") ;
#endif

	mkf_parser_n_increment( utf8_parser , 1) ;

	return  0 ;
}

static void
utf8_parser_set_str(
	mkf_parser_t *  utf8_parser ,
	u_char *  str ,
	size_t  size
	)
{
	utf8_parser->str = str ;
	utf8_parser->left = size ;
	utf8_parser->marked_left = 0 ;
	utf8_parser->is_eos = 0 ;
}

static void
utf8_parser_delete(
	mkf_parser_t *  s
	)
{
	free( s) ;
}


/* --- global functions --- */

mkf_parser_t *
mkf_utf8_parser_new(void)
{
	mkf_parser_t *  utf8_parser ;

	if( ( utf8_parser = malloc( sizeof( mkf_parser_t))) == NULL)
	{
		return  NULL ;
	}

	mkf_parser_init( utf8_parser) ;

	utf8_parser->init = mkf_parser_init ;
	utf8_parser->set_str = utf8_parser_set_str ;
	utf8_parser->delete = utf8_parser_delete ;
	utf8_parser->next_char = utf8_parser_next_char ;

	return  utf8_parser ;
}
