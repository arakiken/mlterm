/*
 *	$Id$
 */

#ifndef  __ML_ENCODING_H__
#define  __ML_ENCODING_H__


#include  <mkf/mkf_parser.h>
#include  <mkf/mkf_conv.h>


/*
 * supported encodings are those which are not conflicted with US_ASCII.
 * so , UCS4 is not supported.
 */
typedef enum  ml_encoding_type
{
	ML_UNKNOWN_ENCODING = -1 ,
	ML_ISO8859_1 = 0 ,
	ML_ISO8859_2 ,
	ML_ISO8859_3 ,
	ML_ISO8859_4 ,
	ML_ISO8859_5 ,
	ML_ISO8859_6 ,
	ML_ISO8859_7 ,
	ML_ISO8859_8 ,
	ML_ISO8859_9 ,
	ML_ISO8859_10 ,
	ML_TIS620 ,
	ML_ISO8859_13 ,
	ML_ISO8859_14 ,
	ML_ISO8859_15 ,
	ML_ISO8859_16 ,
	ML_TCVN5712 ,
	
	ML_VISCII ,
	ML_KOI8_R ,
	ML_KOI8_U ,
	
	ML_UTF8 ,

	ML_EUCJP ,
	ML_EUCJISX0213 ,
	ML_ISO2022JP ,
	ML_ISO2022JP2 ,
	ML_ISO2022JP3 ,
	ML_SJIS ,
	ML_SJISX0213 ,
	
	ML_EUCKR ,
	ML_UHC ,
	ML_JOHAB ,
	ML_ISO2022KR ,

	ML_BIG5 ,
	ML_EUCTW ,

	ML_BIG5HKSCS ,
	
	ML_EUCCN ,
	ML_GBK ,
	ML_GB18030 ,
	ML_HZ ,

	ML_ISO2022CN ,
	
	MAX_CHAR_ENCODINGS ,
	
}  ml_char_encoding_t ;


#define  IS_ISO8859_VARIANT(encoding)  (ML_ISO8859_1 <= (encoding) && (encoding) <= ML_TCVN5712)

#define  IS_ENCODING_BASED_ON_ISO2022(encoding) \
	(IS_ISO8859_VARIANT(encoding) || (ML_EUCJP <= (encoding) && (encoding) <= ML_ISO2022JP3) || \
		ML_EUCKR == (encoding) || ML_ISO2022KR == (encoding) || ML_EUCTW == (encoding) || \
		ML_ISO2022CN == (encoding) || ML_EUCCN == (encoding))

#define  IS_UTF8_SUBSET_ENCODING(encoding) \
	( (encoding) != ML_ISO2022JP && (encoding) != ML_ISO2022JP2 && (encoding) != ML_ISO2022JP3 && \
		(encoding) != ML_ISO2022CN && (encoding) != ML_ISO2022KR )

ml_char_encoding_t  ml_get_encoding( char *  name) ;

mkf_parser_t *  ml_parser_new( ml_char_encoding_t  encoding) ;

mkf_conv_t *  ml_conv_new( ml_char_encoding_t  encoding) ;


#endif
