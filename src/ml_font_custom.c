/*
 *	$Id$
 */

#include  "ml_font_custom.h"

#include  <string.h>		/* memset */
#include  <kiklib/kik_mem.h>	/* malloc */
#include  <kiklib/kik_str.h>	/* strdup */
#include  <kiklib/kik_util.h>	/* K_MIN */
#include  <kiklib/kik_conf_io.h>
#include  <kiklib/kik_file.h>

#include  "ml_font_intern.h"


#define  DEFAULT_MAX_FONT_SIZE  30
#define  DEFAULT_MIN_FONT_SIZE  6


typedef struct  cs_table
{
	char *  name ;
	mkf_charset_t  cs ;

} cs_table_t ;


/* --- static variables --- */

static cs_table_t  cs_table[] =
{
	{ "DEC_SPECIAL" , DEC_SPECIAL } ,
	{ "ISO8859_1" , ISO8859_1_R } ,
	{ "ISO8859_2" , ISO8859_2_R } ,
	{ "ISO8859_3" , ISO8859_3_R } ,
	{ "ISO8859_4" , ISO8859_4_R } ,
	{ "ISO8859_5" , ISO8859_5_R } ,
	{ "ISO8859_6" , ISO8859_6_R } ,
	{ "ISO8859_7" , ISO8859_7_R } ,
	{ "ISO8859_8" , ISO8859_8_R } ,
	{ "ISO8859_9" , ISO8859_9_R } ,
	{ "ISO8859_10" , ISO8859_10_R } ,
	{ "TIS620" , TIS620_2533 } ,
	{ "ISO8859_13" , ISO8859_13_R } ,
	{ "ISO8859_14" , ISO8859_14_R } ,
	{ "ISO8859_15" , ISO8859_15_R } ,
	{ "ISO8859_16" , ISO8859_16_R } ,
	{ "TCVN5712" , TCVN5712_3_1993 } ,
	{ "ISCII" , ISCII } ,
	{ "VISCII" , VISCII } ,
	{ "KOI8_R" , KOI8_R } ,
	{ "KOI8_U" , KOI8_U } ,
	{ "JISX0201_KATA" , JISX0201_KATA } ,
	{ "JISX0201_ROMAN" , JISX0201_ROMAN } ,
	{ "JISX0208_1978" , JISC6226_1978 } ,
	{ "JISC6226_1978" , JISC6226_1978 } ,
	{ "JISX0208_1983" , JISX0208_1983 } ,
	{ "JISX0208_1990" , JISX0208_1990 } ,
	{ "JISX0212_1990" , JISX0212_1990 } ,
	{ "JISX0213_2000_1" , JISX0213_2000_1 } ,
	{ "JISX0213_2000_2" , JISX0213_2000_2 } ,
	{ "KSC5601_1987" , KSC5601_1987 } ,
	{ "KSX1001_1997" , KSC5601_1987 } ,
	{ "UHC" , UHC } ,
	{ "JOHAB" , JOHAB } ,
	{ "GB2312_80" , GB2312_80 } ,
	{ "GBK" , GBK } ,
	{ "BIG5" , BIG5 } ,
	{ "HKSCS" , HKSCS } ,
	{ "CNS11643_1992_1" , CNS11643_1992_1 } ,
	{ "CNS11643_1992_2" , CNS11643_1992_2 } ,
	{ "CNS11643_1992_3" , CNS11643_1992_3 } ,
	{ "CNS11643_1992_4" , CNS11643_1992_4 } ,
	{ "CNS11643_1992_5" , CNS11643_1992_5 } ,
	{ "CNS11643_1992_6" , CNS11643_1992_6 } ,
	{ "CNS11643_1992_7" , CNS11643_1992_7 } ,
	{ "ISO10646_UCS2_1" , ISO10646_UCS2_1 } ,
	{ "ISO10646_UCS4_1" , ISO10646_UCS4_1 } ,

} ;


/* --- static functions --- */

static int
fontattr_hash(
	ml_font_attr_t  fontattr ,
	u_int  size
	)
{
	return  fontattr % size ;
}

static int
fontattr_compare(
	ml_font_attr_t  key1 ,
	ml_font_attr_t  key2
	)
{
	return  (key1 == key2) ;
}

static KIK_MAP( ml_font_name)
get_font_name_table(
	ml_font_custom_t *  font_custom ,
	int  font_size
	)
{
	if( font_custom->font_name_table[font_size - font_custom->min_font_size] == NULL)
	{
		kik_map_new( ml_font_attr_t , char * ,
			font_custom->font_name_table[font_size - font_custom->min_font_size] ,
			fontattr_hash , fontattr_compare) ;
	}

	return  font_custom->font_name_table[font_size - font_custom->min_font_size] ;
}


/* --- global functions --- */

int
ml_font_custom_init(
	ml_font_custom_t *  font_custom ,
	u_int  min_font_size ,
	u_int  max_font_size
	)
{
	if( min_font_size == 0)
	{
		min_font_size = DEFAULT_MIN_FONT_SIZE ;
	}
	
	if( max_font_size == 0)
	{
		max_font_size = DEFAULT_MAX_FONT_SIZE ;
	}
	
	if( max_font_size < min_font_size)
	{
	#ifdef  DEBUG
		kik_warn_printf( KIK_DEBUG_TAG " max_font_size %d is smaller than min_font_size %d\n" ,
			max_font_size , min_font_size) ;
	#endif
	
		return  0 ;
	}
	
	if( ( font_custom->font_name_table = malloc(
		sizeof( KIK_MAP( ml_font_name)) * (max_font_size - min_font_size + 1))) == NULL)
	{
	#ifdef  DEBUG
		kik_warn_printf( KIK_DEBUG_TAG " malloc() failed.\n") ;
	#endif

		return  0 ;
	}
	
	memset( font_custom->font_name_table , 0 ,
		sizeof( KIK_MAP( ml_font_name)) * (max_font_size - min_font_size + 1)) ;

	font_custom->max_font_size = max_font_size ;
	font_custom->min_font_size = min_font_size ;

	return  1 ;
}

int
ml_font_custom_final(
	ml_font_custom_t *  font_custom
	)
{
	int  counter ;
	u_int  size ;
	KIK_PAIR( ml_font_name) *  fn_array ;
	
	for( counter = 0 ; counter <= (font_custom->max_font_size - font_custom->min_font_size) ; counter ++)
	{
		if( font_custom->font_name_table[counter])
		{
			int  __counter ;

			kik_map_get_pairs_array( font_custom->font_name_table[counter] ,
				fn_array , size) ;
				
			for( __counter = 0 ; __counter < size ; __counter ++)
			{
				free( fn_array[__counter]->value) ;
			}
			
			kik_map_delete( font_custom->font_name_table[counter]) ;
		}
	}

	free( font_custom->font_name_table) ;

	return  1 ;
}

int
ml_font_custom_read_conf(
	ml_font_custom_t *  font_custom ,
	char *  filename
	)
{
	kik_file_t *  from ;
	char *  key ;
	char *  value ;
	int  counter ;

	if( ! ( from = kik_file_open( filename , "r")))
	{
	#ifdef  DEBUG
		kik_warn_printf( KIK_DEBUG_TAG " %s couldn't be opened.\n" , filename) ;
	#endif
	
		return  0 ;
	}

	while( kik_conf_io_read( from , &key , &value))
	{
		mkf_charset_t  cs ;
		ml_font_attr_t  attr ;
		char *  size_str ;
		size_t  key_len ;

		key_len = strlen( key) ;
		
		counter = 0 ;
		for( counter = 0 ; counter < sizeof( cs_table) / sizeof( cs_table[0]) ; counter ++)
		{
			if( strncmp( cs_table[counter].name , key ,
				K_MIN( strlen( cs_table[counter].name) , key_len)) == 0)
			{
				cs = cs_table[counter].cs ;

				break ;
			}
		}

		if( counter == sizeof( cs_table) / sizeof( cs_table[0]))
		{
			continue ;
		}

		attr = DEFAULT_FONT_ATTR(cs) ;
		
		if( key)
		{
			if( key_len >= 8 && strstr( key , "_BIWIDTH"))
			{
				attr |= FONT_BIWIDTH ;
			}

			if( key_len >= 5 && strstr( key , "_BOLD"))
			{
				attr &= ~FONT_MEDIUM ;
				attr |= FONT_BOLD ;
			}
		}
		
		while( value != NULL && ( size_str = kik_str_sep( &value , ",")) != NULL)
		{
			u_int  size ;
			char *  fontname ;

			if( value == NULL || ( fontname = kik_str_sep( &value , ";")) == NULL)
			{
				break ;
			}

			if( ! kik_str_to_int( &size , size_str))
			{
				kik_msg_printf( "a font size %s is not digit.\n" , size_str) ;
			}
			else
			{
			#ifdef  __DEBUG
				kik_debug_printf( KIK_DEBUG_TAG
					" setting font [attr %x size %s name %s]\n" ,
					attr , size_str , fontname) ;
			#endif

				ml_set_font_name( font_custom , attr , fontname , size) ;
			}
		}
	}
	
	kik_file_close( from) ;
	
	return  1 ;
}

#ifdef  ANTI_ALIAS

int
ml_font_custom_read_aa_conf(
	ml_font_custom_t *  font_custom ,
	char *  filename
	)
{
	kik_file_t *  from ;
	char *  key ;
	char *  value ;
	int  counter ;

	if( ! ( from = kik_file_open( filename , "r")))
	{
	#ifdef  DEBUG
		kik_warn_printf( KIK_DEBUG_TAG " %s couldn't be opened.\n" , filename) ;
	#endif
	
		return  0 ;
	}

	while( kik_conf_io_read( from , &key , &value))
	{
		mkf_charset_t  cs ;
		ml_font_attr_t  attr ;
		char *  size_str ;
		u_int  size ;
		size_t  key_len ;
		char *  default_fontname ;

		key_len = strlen( key) ;
		
		counter = 0 ;
		for( counter = 0 ; counter < sizeof( cs_table) / sizeof( cs_table[0]) ; counter ++)
		{
			if( strncmp( cs_table[counter].name , key ,
				K_MIN( strlen( cs_table[counter].name) , key_len)) == 0)
			{
				cs = cs_table[counter].cs ;

				break ;
			}
		}

		if( counter == sizeof( cs_table) / sizeof( cs_table[0]))
		{
			continue ;
		}

		attr = DEFAULT_FONT_ATTR(cs) ;
		
		if( key)
		{
			if( key_len >= 8 && strstr( key , "_BIWIDTH"))
			{
				attr |= FONT_BIWIDTH ;
			}

			if( key_len >= 5 && strstr( key , "_BOLD"))
			{
				attr &= ~FONT_MEDIUM ;
				attr |= FONT_BOLD ;
			}
		}

		if( ( default_fontname = kik_str_sep( &value , ";")) == NULL)
		{
			continue ;
		}

		for( size = font_custom->min_font_size ; size <= font_custom->max_font_size ; size ++)
		{
			ml_set_font_name( font_custom , attr , default_fontname , size) ;
		}
		
		while( value != NULL && ( size_str = kik_str_sep( &value , ",")) != NULL)
		{
			char *  fontname ;

			if( value == NULL || ( fontname = kik_str_sep( &value , ";")) == NULL)
			{
				break ;
			}

			if( ! kik_str_to_int( &size , size_str))
			{
				kik_msg_printf( "a font size %s is not digit.\n" , size_str) ;
			}
			else
			{
			#ifdef  __DEBUG
				kik_debug_printf( KIK_DEBUG_TAG
					" setting font [attr %x size %s name %s]\n" ,
					attr , size_str , fontname) ;
			#endif

				ml_set_font_name( font_custom , attr , fontname , size) ;
			}
		}
	}
	
	kik_file_close( from) ;
	
	return  1 ;
}

#endif

int
ml_set_font_name(
	ml_font_custom_t *  font_custom ,
	ml_font_attr_t  fontattr ,
	char *  fontname ,
	u_int  font_size
	)
{
	int  result ;
	KIK_PAIR( ml_font_name)  pair ;

	if( font_size < font_custom->min_font_size || font_custom->max_font_size < font_size)
	{
		return  0 ;
	}

	fontname = strdup( fontname) ;

	kik_map_get( result , get_font_name_table( font_custom , font_size) ,
		fontattr , pair) ;
	if( result)
	{
		free( pair->value) ;
		pair->value = fontname ;
	}
	else
	{
		kik_map_set( result , get_font_name_table( font_custom , font_size) ,
			fontattr , fontname) ;
	}

#ifdef  __DEBUG
	kik_warn_printf( "fontname %s for size %d\n" , fontname , font_size) ;
#endif

	return  result ;
}

char *
ml_get_font_name_for_attr(
	ml_font_custom_t *  font_custom ,
	u_int  font_size ,
	ml_font_attr_t  attr
	)
{
	KIK_PAIR( ml_font_name)  fa_pair ;
	int  result ;
	
	if( font_size < font_custom->min_font_size || font_custom->max_font_size < font_size)
	{
		return  NULL ;
	}

	kik_map_get( result , get_font_name_table( font_custom , font_size) , attr , fa_pair) ;
	if( result)
	{
		return  fa_pair->value ;
	}
	else
	{
		return  NULL ;
	}
}

u_int
ml_get_all_font_names(
	ml_font_custom_t *  font_custom ,
	char ***  font_names ,
	u_int  font_size
	)
{
	KIK_PAIR( ml_font_name) *  array ;
	u_int  size ;
	char **  str_array ;
	int  counter ;
	
	kik_map_get_pairs_array( get_font_name_table( font_custom , font_size) ,
		array , size) ;

	if( size == 0)
	{
		*font_names = NULL ;

		return  0 ;
	}
	
	if( ( str_array = malloc( sizeof( char *) * size)) == NULL)
	{
		return  0 ;
	}
	
	for( counter = 0 ; counter < size ; counter ++)
	{
		str_array[counter] = array[counter]->value ;
	}

	*font_names = str_array ;

	return  size ;
}
