/*
 *	$Id$
 */

#include  "x_font_config.h"

#include  <string.h>		/* memset */
#include  <kiklib/kik_mem.h>	/* malloc */
#include  <kiklib/kik_str.h>	/* strdup */
#include  <kiklib/kik_util.h>	/* DIGIT_STR_LEN */
#include  <kiklib/kik_conf_io.h>
#include  <kiklib/kik_file.h>


typedef struct  cs_table
{
	char *  name ;
	mkf_charset_t  cs ;

} cs_table_t ;


#if  0
#define  __DEBUG
#endif


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
	{ "ISO10646_UCS2_1" , ISO10646_UCS4_1 } ,
	{ "ISO10646_UCS4_1" , ISO10646_UCS4_1 } ,

} ;

static x_font_config_t **  font_configs ;
static u_int  num_of_configs ;

static u_int  min_font_size = 6 ;
static u_int  max_font_size = 30 ;


/* --- static functions --- */

static int
font_hash(
	ml_font_t  font ,
	u_int  size
	)
{
	return  font % size ;
}

static int
font_compare(
	ml_font_t  key1 ,
	ml_font_t  key2
	)
{
	return  (key1 == key2) ;
}

static KIK_MAP( x_font_name)
get_font_name_table(
	x_font_config_t *  font_config ,
	int  font_size
	)
{
	if( font_config->font_name_table[font_size - min_font_size] == NULL)
	{
		kik_map_new_with_size( ml_font_t , char * ,
			font_config->font_name_table[font_size - min_font_size] ,
			font_hash , font_compare , 16) ;
	}

	return  font_config->font_name_table[font_size - min_font_size] ;
}

static int
read_conf(
	x_font_config_t *  font_config ,
	char *  filename
	)
{
	kik_file_t *  from ;
	char *  key ;
	char *  value ;
	int  count ;

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
		ml_font_t  font ;
		size_t  key_len ;
		char *  entry ;

		key_len = strlen( key) ;
		
		count = 0 ;
		for( count = 0 ; count < sizeof( cs_table) / sizeof( cs_table[0]) ; count ++)
		{
			size_t  nlen ;

			nlen = strlen( cs_table[count].name) ;
			
			if( key_len >= nlen &&
				strncmp( cs_table[count].name , key , nlen) == 0 &&
				( key[nlen] == '\0' ||
					/* "_BOLD" or "_BIWIDTH" is trailing */
					key[nlen] == '_'))
			{
				cs = cs_table[count].cs ;

				break ;
			}
		}

		if( count == sizeof( cs_table) / sizeof( cs_table[0]))
		{
			continue ;
		}

		font = DEFAULT_FONT(cs) ;
		
		if( key_len >= 8 && strstr( key , "_BIWIDTH"))
		{
			font |= FONT_BIWIDTH ;
		}

		if( key_len >= 5 && strstr( key , "_BOLD"))
		{
			font |= FONT_BOLD ;
		}

		/*
		 * [entry];[entry];[entry];....
		 */
		while( value != NULL && ( entry = kik_str_sep( &value , ";")) != NULL)
		{
			char *  fontname ;
			u_int  size ;

			if( *entry == '\0')
			{
				continue ;
			}
			else if( strchr( entry , ','))
			{
				/*
				 * for each size.
				 * [size],[font name]
				 */
				 
				char *  size_str ;
				
				if( ( size_str = kik_str_sep( &entry , ",")) == NULL ||
					( fontname = entry) == NULL)
				{
					continue ;
				}
				
				if( ! kik_str_to_uint( &size , size_str))
				{
					kik_msg_printf( "a font size %s is not valid.\n" , size_str) ;

					continue ;
				}
				
			#ifdef  __DEBUG
				kik_debug_printf( KIK_DEBUG_TAG
					" setting font [font %x size %d name %s]\n" ,
					font , size , fontname) ;
			#endif

				x_customize_font_name( font_config , font , fontname , size) ;
			}
			else
			{
				/*
				 * default font.
				 * [font name]
				 */

				x_customize_default_font_name( font_config , font , entry , size) ;
			}
		}
	}
	
	kik_file_close( from) ;
	
	return  1 ;
}


/* --- global functions --- */

int
x_set_font_size_range(
	u_int  min_fsize ,
	u_int  max_fsize
	)
{
	if( min_fsize == 0)
	{
	#ifdef  DEBUG
		kik_warn_printf( KIK_DEBUG_TAG " min_font_size must not be 0.\n") ;
	#endif

		return  0 ;
	}

	if( max_fsize < min_fsize)
	{
	#ifdef  DEBUG
		kik_warn_printf( KIK_DEBUG_TAG " max_font_size %d should be larger than min_font_size %d\n" ,
			max_fsize , min_fsize) ;
	#endif

		return  0 ;
	}

	min_font_size = min_fsize;
	max_font_size = max_fsize;
	return  1 ;
}

u_int
x_get_min_font_size(void)
{
	return  min_font_size ;
}

u_int
x_get_max_font_size(void)
{
	return  max_font_size ;
}

x_font_config_t *
x_acquire_font_config(
	x_type_engine_t  type_engine ,
	x_font_present_t  font_present
	)
{
	x_font_config_t *  font_config ;
	void *  p ;
	char *  font_rcfile ;
	char *  font_rcfile2 ;	/* prior to font_rcfile */
	char *  rcpath ;
	
	if( font_configs)
	{
		int  count ;

		for( count = 0 ; count < num_of_configs ; count ++)
		{
			/*
			 * 'type_engine' isn't compared because TYPE_XFT and TYPE_STSF
			 * config files are the same.
			 */
			if( font_configs[count]->font_present == font_present
				/* && font_configs[count]->type_engine == type_engine */)
			{
				font_configs[count]->ref_count ++ ;

				return  font_configs[count] ;
			}
		}
	}

	if( ( p = realloc( font_configs , sizeof( x_font_config_t*) * (num_of_configs + 1))) == NULL)
	{
		return  NULL ;
	}

	font_configs = p ;

	if( ( font_config = x_font_config_new( type_engine , font_present)) == NULL)
	{
		return  NULL ;
	}

	if( type_engine == TYPE_XFT || type_engine == TYPE_STSF)
	{
		switch( font_present & ~FONT_AA)
		{
		default:
			font_rcfile = "mlterm/aafont" ;
			font_rcfile2 = NULL ;
			break ;

		case FONT_VAR_WIDTH:
			font_rcfile = "mlterm/aafont" ;
			font_rcfile2 = "mlterm/vaafont" ;
			break ;

		case FONT_VERTICAL:
			font_rcfile = "mlterm/aafont" ;
			font_rcfile2 = "mlterm/taafont" ;
			break ;
		}
	}
	else
	{
		if( font_present & FONT_AA)
		{
			return  NULL ;
		}
		else
		{
			switch( font_present)
			{
			default:
				font_rcfile = "mlterm/font" ;
				font_rcfile2 = NULL ;
				break ;

			case FONT_VAR_WIDTH:
				font_rcfile = "mlterm/font" ;
				font_rcfile2 = "mlterm/vfont" ;
				break ;

			case FONT_VERTICAL:
				font_rcfile = "mlterm/font" ;
				font_rcfile2 = "mlterm/tfont" ;
				break ;
			}
		}
	}
		
	if( ( rcpath = kik_get_sys_rc_path( font_rcfile)))
	{
		read_conf( font_config , rcpath) ;
		free( rcpath) ;
	}

	if( ( rcpath = kik_get_user_rc_path( font_rcfile)))
	{
		read_conf( font_config , rcpath) ;
		free( rcpath) ;
	}

	if( font_rcfile2)
	{
		if( ( rcpath = kik_get_sys_rc_path( font_rcfile2)))
		{
			read_conf( font_config , rcpath) ;
			free( rcpath) ;
		}

		if( ( rcpath = kik_get_user_rc_path( font_rcfile2)))
		{
			read_conf( font_config , rcpath) ;
			free( rcpath) ;
		}
	}

	font_config->ref_count ++ ;
	
	return  font_configs[num_of_configs ++] = font_config ;
}

int
x_release_font_config(
	x_font_config_t *  font_config
	)
{
	int  count ;
	
	if( -- font_config->ref_count > 0)
	{
		return  1 ;
	}

	for( count = 0 ; count < num_of_configs ; count ++)
	{
		if( font_configs[count] == font_config)
		{
			font_configs[count] = font_configs[--num_of_configs] ;
			x_font_config_delete( font_config) ;

			if( num_of_configs == 0)
			{
				free( font_configs) ;
				font_configs = NULL ;
			}
			
			return  1 ;
		}
	}

	return  0 ;
}

x_font_config_t *
x_font_config_new(
	x_type_engine_t  type_engine ,
	x_font_present_t  font_present
	)
{
	x_font_config_t *  font_config ;
	
	if( ( font_config = malloc( sizeof( x_font_config_t))) == NULL)
	{
		return  NULL ;
	}
	
	if( ( font_config->font_name_table = malloc(
		sizeof( KIK_MAP( x_font_name)) * (max_font_size - min_font_size + 1))) == NULL)
	{
	#ifdef  DEBUG
		kik_warn_printf( KIK_DEBUG_TAG " malloc() failed.\n") ;
	#endif

		free( font_config) ;
		
		return  NULL ;
	}
	
	memset( font_config->font_name_table , 0 ,
		sizeof( KIK_MAP( x_font_name)) * (max_font_size - min_font_size + 1)) ;

	kik_map_new_with_size( ml_font_t , char * ,
		font_config->default_font_name_table , font_hash , font_compare , 8) ;

	font_config->type_engine = type_engine ;
	font_config->font_present = font_present ;
	font_config->ref_count = 0 ;
	
	return  font_config ;
}

int
x_font_config_delete(
	x_font_config_t *  font_config
	)
{
	int  count ;
	u_int  size ;
	KIK_PAIR( x_font_name) *  fn_array ;
	
	for( count = 0 ; count <= (max_font_size - min_font_size) ; count ++)
	{
		if( font_config->font_name_table[count])
		{
			int  __count ;

			kik_map_get_pairs_array( font_config->font_name_table[count] ,
				fn_array , size) ;
				
			for( __count = 0 ; __count < size ; __count ++)
			{
				free( fn_array[__count]->value) ;
			}
			
			kik_map_delete( font_config->font_name_table[count]) ;
		}
	}

	free( font_config->font_name_table) ;

	kik_map_get_pairs_array( font_config->default_font_name_table ,
		fn_array , size) ;

	for( count = 0 ; count < size ; count ++)
	{
		free( fn_array[count]->value) ;
	}

	kik_map_delete( font_config->default_font_name_table) ;

	free( font_config) ;
	
	return  1 ;
}

int
x_customize_font_name(
	x_font_config_t *  font_config ,
	ml_font_t  font ,
	char *  fontname ,
	u_int  font_size
	)
{
	int  result ;
	KIK_PAIR( x_font_name)  pair ;

	if( font_size < min_font_size || max_font_size < font_size)
	{
		return  0 ;
	}

	fontname = strdup( fontname) ;

	kik_map_get( result , get_font_name_table( font_config , font_size) , font , pair) ;
	if( result)
	{
		free( pair->value) ;
		pair->value = fontname ;
	}
	else
	{
		kik_map_set( result , get_font_name_table( font_config , font_size) ,
			font , fontname) ;
	}

#ifdef  __DEBUG
	kik_warn_printf( "fontname %s for size %d\n" , fontname , font_size) ;
#endif

	return  result ;
}

int
x_customize_default_font_name(
	x_font_config_t *  font_config ,
	ml_font_t  font ,
	char *  fontname ,
	u_int  font_size
	)
{
	int  result ;
	KIK_PAIR( x_font_name)  pair ;

	fontname = strdup( fontname) ;

	kik_map_get( result , font_config->default_font_name_table , font , pair) ;
	if( result)
	{
		free( pair->value) ;
		pair->value = fontname ;
	}
	else
	{
		kik_map_set( result , font_config->default_font_name_table ,
			font , fontname) ;
	}

	return  result ;
}

char *
x_get_config_font_name(
	x_font_config_t *  font_config ,
	u_int  font_size ,
	ml_font_t  font
	)
{
	KIK_PAIR( x_font_name)  fa_pair ;
	int  result ;
	char *  font_name ;
	
	if( font_size < min_font_size || max_font_size < font_size)
	{
		return  NULL ;
	}

	kik_map_get( result , get_font_name_table( font_config , font_size) , font , fa_pair) ;
	if( result)
	{
		return  strdup( fa_pair->value) ;
	}

	kik_map_get( result , font_config->default_font_name_table , font , fa_pair) ;
	if( ! result)
	{
		return  NULL ;
	}

	/* -2 is for "%d" */
	if( ( font_name = malloc( strlen( fa_pair->value) - 2 + DIGIT_STR_LEN(font_size) + 1)) == NULL)
	{
		return  NULL ;
	}

	sprintf( font_name , fa_pair->value , font_size) ;

	return  font_name ;
}

char *
x_get_all_config_font_names(
	x_font_config_t *  font_config ,
	u_int  font_size
	)
{
	KIK_PAIR( x_font_name) *  array ;
	u_int  size ;
	KIK_PAIR( x_font_name) *  d_array ;
	u_int  d_size ;
	char *  font_name_list ;
	size_t  list_len ;
	char *  p ;
	int  count ;
	
	kik_map_get_pairs_array( get_font_name_table( font_config , font_size) , array , size) ;
	kik_map_get_pairs_array( font_config->default_font_name_table , d_array , d_size) ;

	if( d_size + size == 0)
	{
		return  NULL ;
	}

	list_len = 0 ;
	
	for( count = 0 ; count < size ; count ++)
	{
		list_len += (strlen( array[count]->value) + 1) ;
	}

	for( count = 0 ; count < d_size ; count ++)
	{
		list_len += (strlen( d_array[count]->value) - 2 + DIGIT_STR_LEN(font_size) + 1) ;
	}

	if( ( font_name_list = malloc( list_len)) == NULL)
	{
		return  NULL ;
	}

	p = font_name_list ;

	for( count = 0 ; count < size ; count ++)
	{
		strcpy( p , array[count]->value) ;
		p += strlen( p) ;
		*(p ++) = ',' ;
	}

	for( count = 0 ; count < d_size ; count ++)
	{
		sprintf( p , d_array[count]->value , font_size) ;
		p += strlen( p) ;
		*(p ++) = ',' ;
	}

	*(p - 1) = '\0' ;

	return  font_name_list ;
}
