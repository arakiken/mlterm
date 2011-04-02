/*
 *	$Id$
 */

#include  <stdio.h>
#include  <stdlib.h>
#include  <string.h>
#include  <unistd.h>	/* getopt */
#include  <kiklib/kik_debug.h>

#include  <mkf/mkf_iso8859_parser.h>
#include  <mkf/mkf_xct_parser.h>
#include  <mkf/mkf_viscii_parser.h>
#include  <mkf/mkf_8bit_parser.h>
#include  <mkf/mkf_utf32_parser.h>
#include  <mkf/mkf_sjis_parser.h>
#include  <mkf/mkf_eucjp_parser.h>
#include  <mkf/mkf_utf8_parser.h>
#include  <mkf/mkf_iso2022jp_parser.h>
#include  <mkf/mkf_euckr_parser.h>
#include  <mkf/mkf_iso2022kr_parser.h>
#include  <mkf/mkf_johab_parser.h>
#include  <mkf/mkf_euccn_parser.h>
#include  <mkf/mkf_iso2022cn_parser.h>
#include  <mkf/mkf_hz_parser.h>
#include  <mkf/mkf_big5_parser.h>
#include  <mkf/mkf_euctw_parser.h>
#include  <mkf/mkf_utf16_parser.h>

#include  <mkf/mkf_iso8859_conv.h>
#include  <mkf/mkf_xct_conv.h>
#include  <mkf/mkf_viscii_conv.h>
#include  <mkf/mkf_8bit_conv.h>
#include  <mkf/mkf_utf32_conv.h>
#include  <mkf/mkf_sjis_conv.h>
#include  <mkf/mkf_eucjp_conv.h>
#include  <mkf/mkf_utf8_conv.h>
#include  <mkf/mkf_iso2022jp_conv.h>
#include  <mkf/mkf_euckr_conv.h>
#include  <mkf/mkf_iso2022kr_conv.h>
#include  <mkf/mkf_johab_conv.h>
#include  <mkf/mkf_euccn_conv.h>
#include  <mkf/mkf_iso2022cn_conv.h>
#include  <mkf/mkf_hz_conv.h>
#include  <mkf/mkf_big5_conv.h>
#include  <mkf/mkf_euctw_conv.h>
#include  <mkf/mkf_utf16_conv.h>

#include  <mkf/mkf_ucs4_map.h>


typedef struct  mkf_factory_table
{
	char *  encoding ;
	mkf_parser_t *  (*parser_new)(void) ;
	mkf_conv_t *  (*conv_new)(void) ;

} mkf_factory_table_t ;


/* --- static variables --- */

static mkf_factory_table_t  factories[] =
{
	{ "iso8859-1" , mkf_iso8859_1_parser_new , mkf_iso8859_1_conv_new } ,
	{ "iso8859-2" , mkf_iso8859_2_parser_new , mkf_iso8859_2_conv_new } ,
	{ "iso8859-3" , mkf_iso8859_3_parser_new , mkf_iso8859_3_conv_new } ,
	{ "iso8859-4" , mkf_iso8859_4_parser_new , mkf_iso8859_4_conv_new } ,
	{ "iso8859-5" , mkf_iso8859_5_parser_new , mkf_iso8859_5_conv_new } ,
	{ "iso8859-6" , mkf_iso8859_6_parser_new , mkf_iso8859_6_conv_new } ,
	{ "iso8859-7" , mkf_iso8859_7_parser_new , mkf_iso8859_7_conv_new } ,
	{ "iso8859-8" , mkf_iso8859_8_parser_new , mkf_iso8859_8_conv_new } ,
	{ "iso8859-9" , mkf_iso8859_9_parser_new , mkf_iso8859_9_conv_new } ,
	{ "iso8859-10" , mkf_iso8859_10_parser_new , mkf_iso8859_10_conv_new } ,
	{ "tis620" , mkf_tis620_2533_parser_new , mkf_tis620_2533_conv_new } ,
	{ "iso8859-13" , mkf_iso8859_13_parser_new , mkf_iso8859_13_conv_new } ,
	{ "iso8859-14" , mkf_iso8859_14_parser_new , mkf_iso8859_14_conv_new } ,
	{ "iso8859-15" , mkf_iso8859_15_parser_new , mkf_iso8859_15_conv_new } ,
	{ "iso8859-16" , mkf_iso8859_16_parser_new , mkf_iso8859_16_conv_new } ,
	{ "tcvn5712" , mkf_tcvn5712_3_1993_parser_new , mkf_tcvn5712_3_1993_conv_new } ,
	{ "xct" , mkf_xct_parser_new , mkf_xct_conv_new } ,
	{ "viscii" , mkf_viscii_parser_new , mkf_viscii_conv_new } ,
	{ "koi8-r" , mkf_koi8_r_parser_new , mkf_koi8_r_conv_new } ,
	{ "koi8-u" , mkf_koi8_u_parser_new , mkf_koi8_u_conv_new } ,
	{ "cp1250" , mkf_cp1250_parser_new , mkf_cp1250_conv_new } ,
	{ "cp1251" , mkf_cp1251_parser_new , mkf_cp1251_conv_new } ,
	{ "cp1252" , mkf_cp1252_parser_new , mkf_cp1252_conv_new } ,
	{ "cp1253" , mkf_cp1253_parser_new , mkf_cp1253_conv_new } ,
	{ "cp1254" , mkf_cp1250_parser_new , mkf_cp1254_conv_new } ,
	{ "cp1255" , mkf_cp1250_parser_new , mkf_cp1255_conv_new } ,
	{ "cp1256" , mkf_cp1250_parser_new , mkf_cp1256_conv_new } ,
	{ "cp1257" , mkf_cp1250_parser_new , mkf_cp1257_conv_new } ,
	{ "cp1258" , mkf_cp1250_parser_new , mkf_cp1258_conv_new } ,
	{ "cp874" , mkf_cp874_parser_new , mkf_cp874_conv_new } ,
	{ "eucjp" , mkf_eucjp_parser_new , mkf_eucjp_conv_new } ,
	{ "eucjisx0213" , mkf_eucjisx0213_parser_new , mkf_eucjisx0213_conv_new } ,
	{ "sjis" , mkf_sjis_parser_new , mkf_sjis_conv_new } ,
	{ "sjisx0213" , mkf_sjisx0213_parser_new , mkf_sjisx0213_conv_new } ,
	{ "utf8" , mkf_utf8_parser_new , mkf_utf8_conv_new } ,
	{ "utf16" , mkf_utf16_parser_new , mkf_utf16_conv_new } ,
	{ "utf16le" , mkf_utf16le_parser_new , mkf_utf16le_conv_new } ,
	{ "utf32" , mkf_utf32_parser_new , mkf_utf32_conv_new } ,
	{ "junet8" , mkf_iso2022jp_8_parser_new , mkf_iso2022jp_8_conv_new } ,
	{ "junet7" , mkf_iso2022jp_7_parser_new , mkf_iso2022jp_7_conv_new } ,
	{ "iso2022jp2" , mkf_iso2022jp2_parser_new , mkf_iso2022jp2_conv_new } ,
	{ "iso2022jp3" , mkf_iso2022jp3_parser_new , mkf_iso2022jp3_conv_new } ,
	{ "euckr" , mkf_euckr_parser_new , mkf_euckr_conv_new } ,
	{ "uhc" , mkf_uhc_parser_new , mkf_uhc_conv_new } ,
	{ "iso2022kr" , mkf_iso2022kr_parser_new , mkf_iso2022kr_conv_new } ,
	{ "johab" , mkf_johab_parser_new , mkf_johab_conv_new } ,
	{ "euccn" , mkf_euccn_parser_new , mkf_euccn_conv_new } ,
	{ "gbk" , mkf_gbk_parser_new , mkf_gbk_conv_new } ,
	{ "gb18030" , mkf_gb18030_2000_parser_new , mkf_gb18030_2000_conv_new } ,
	{ "iso2022cn" , mkf_iso2022cn_parser_new , mkf_iso2022cn_conv_new } ,
	{ "hz" , mkf_hz_parser_new , mkf_hz_conv_new } ,
	{ "big5" , mkf_big5_parser_new , mkf_big5_conv_new } ,
	{ "big5hkscs" , mkf_big5hkscs_parser_new , mkf_big5hkscs_conv_new } ,
	{ "euctw" , mkf_euctw_parser_new , mkf_euctw_conv_new } ,
} ;


/* --- static functions --- */

static void
usage()
{
	kik_msg_printf( "usage: mkf -i [input code] -o [output code] [file]\n") ;
	kik_msg_printf( "supported codes: iso8859-[1-10] tis620 iso8859-[13-16] tcvn5712 xct viscii koi8-r koi8-u cp1250 cp1251 cp1252 cp1253 cp1254 cp1255 cp1256 cp1257 cp1258 cp874 eucjp eucjisx0213 sjis sjisx0213 utf8 utf16 utf16le utf32 junet8 junet7 iso2022jp2 iso2022jp3 euckr uhc iso2022kr johab euccn gbk gb18030 iso2022cn hz big5 big5hkscs euctw\n") ;
}


/* --- global functions --- */

int
main( int  argc , char **  argv)
{
	extern char *  optarg ;
	extern int  optind ;
	
	int  c ;
	char *  in ;
	char *  out ;
	int  count ;
	FILE *  fp ;
	u_char  output[1024] ;
	u_char  input[1024] ;
	u_char *  input_p ;
	mkf_parser_t *  parser ;
	mkf_conv_t *  conv ;
	size_t  size ;

	if( argc != 6)
	{
		usage() ;

		return  1 ;
	}
	
	in = NULL ;
	out = NULL ;
	while( ( c = getopt( argc , argv , "i:o:")) != -1)
	{
		switch( c)
		{
		case 'i' :
			in = optarg ;
			break ;

		case 'o' :
			out = optarg ;
			break ;

		default:
			usage() ;

			return  1 ;
		}
	}

	argc -= optind ;
	argv += optind ;

	if( argc == 0)
	{
		fp = stdin ;
	}
	else if( argc == 1)
	{
		if( ( fp = fopen( *argv , "r")) == NULL)
		{
			kik_error_printf( "%s not found.\n" , *argv) ;
			usage() ;

			return  1 ;
		}
	}
	else
	{
		kik_error_printf( "too many arguments.\n") ;
		usage() ;

		return  1 ;
	}

	parser = NULL ;
	
	for( count = 0 ; count < sizeof( factories) / sizeof( factories[0]) ; count ++)
	{
		if( strcmp( factories[count].encoding , in) == 0)
		{
			parser = (*factories[count].parser_new)() ;
		}
	}

	if( parser == NULL)
	{
		kik_error_printf( "input encoding %s is illegal.\n" , in) ;
		usage() ;
		
		return  1 ;
	}
	
	conv = NULL ;
	
	for( count = 0 ; count < sizeof( factories) / sizeof( factories[0]) ; count ++)
	{
		if( strcmp( factories[count].encoding , out) == 0)
		{
			conv = (*factories[count].conv_new)() ;
		}
	}

	if( conv == NULL)
	{
		kik_error_printf( "output encoding %s is illegal.\n" , out) ;
		usage() ;
		
		return  1 ;
	}

	input_p = input ;
	while( ( size = fread( input_p , 1 , 1024 - parser->left , fp)) > 0)
	{
		(*parser->set_str)( parser , input , size + parser->left) ;
		size = (*conv->convert)( conv , output , 1024 , parser) ;
		
		fwrite( output , 1 , size , stdout) ;
		
		if( parser->left > 0)
		{
			memcpy( input , parser->str , parser->left) ;
			input_p = input + parser->left ;
		}
		else
		{
			input_p = input ;
		}
	}

	if( parser->left > 0)
	{
		(*parser->set_str)( parser , input , parser->left) ;
		size = (*conv->convert)( conv , output , 1024 , parser) ;

		fwrite( output , 1 , size , stdout) ;
	}
	
	(*parser->delete)( parser) ;
	(*conv->delete)( conv) ;
	
	return  0 ;
}
