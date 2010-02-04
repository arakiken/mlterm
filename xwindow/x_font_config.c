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

typedef struct  custom_cache
{
	const char *  file ;
	char *  key ;
	char *  value ;

} custom_cache_t ;


#if  0
#define  __DEBUG
#endif


/* --- static variables --- */

static char *  font_file = "mlterm/font" ;
static char *  vfont_file = "mlterm/vfont" ;
static char *  tfont_file = "mlterm/tfont" ;
static char *  aafont_file = "mlterm/aafont" ;
static char *  vaafont_file = "mlterm/vaafont" ;
static char *  taafont_file = "mlterm/taafont" ;

/*
 * If this table is changed, x_font.c:cs_info_table and mc_font.c:cs_info_table
 * shoule be also changed.
 */
static cs_table_t  cs_table[] =
{
	{ "ISO10646_UCS4_1" , ISO10646_UCS4_1 } ,
	{ "ISO10646_UCS2_1" , ISO10646_UCS2_1 } ,

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
#if  0
	/*
	 * Koi8_t and georgian_ps charsets can be shown by unicode font only.
	 */
	{ "KOI8_T" , KOI8_T } ,
	{ "GEORGIAN_PS" , GEORGIAN_PS } ,
#endif
#ifdef  USE_WIN32GUI
	{ "CP1250" , CP1250 } ,
	{ "CP1251" , CP1251 } ,
	{ "CP1252" , CP1252 } ,
	{ "CP1253" , CP1253 } ,
	{ "CP1254" , CP1254 } ,
	{ "CP1255" , CP1255 } ,
	{ "CP1256" , CP1256 } ,
	{ "CP1257" , CP1257 } ,
	{ "CP1258" , CP1258 } ,
#endif
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
#if  0
	/*
	 * XXX
	 * UHC and JOHAB fonts are not used at the present time.
	 * see ml_vt100_parser.c:ml_parse_vt100_sequence().
	 */
	{ "UHC" , UHC } ,
	{ "JOHAB" , JOHAB } ,
#endif
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

} ;

static x_font_config_t **  font_configs ;
static u_int  num_of_configs ;

static u_int  min_font_size = 6 ;
static u_int  max_font_size = 30 ;

/*
 * These will be leaked unless operate_custom_cache( ... , 1 [remove]) free them.
 */
static custom_cache_t *  custom_cache ;
static u_int  num_of_customs ;


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

static ml_font_t
parse_key(
	const char *  key
	)
{
	int  count ;
	size_t  key_len ;
	mkf_charset_t  cs ;
	ml_font_t  font ;
	
	key_len = strlen( key) ;

	for( count = 0 ; count < sizeof( cs_table) / sizeof( cs_table[0]) ; count ++)
	{
		size_t  nlen ;

		nlen = strlen( cs_table[count].name) ;

		if( key_len >= nlen && strncmp( cs_table[count].name , key , nlen) == 0 &&
			( key[nlen] == '\0' ||
				/* "_BOLD" or "_BIWIDTH" is trailing */ key[nlen] == '_'))
		{
			cs = cs_table[count].cs ;

			break ;
		}
	}

	if( count == sizeof( cs_table) / sizeof( cs_table[0]))
	{
	#ifdef  DEBUG
		kik_debug_printf( KIK_DEBUG_TAG " %s is not valid charset.\n", key) ;
	#endif
	
		return  UNKNOWN_CS ;
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

	return  font ;
}

static int
parse_entry(
	char **  font_name ,	/* if entry is "" or illegal format, not changed. */
	u_int *  font_size ,	/* if entry is "" or illegal format, not changed. */
	char *  entry
	)
{
	if( *entry == '\0')
	{
		return  0 ;
	}
	else if( strchr( entry , ','))
	{
		/*
		 * for each size.
		 * [size],[font name]
		 */

		char *  size_str ;

		if( ( size_str = kik_str_sep( &entry , ",")) == NULL || entry == NULL)
		{
			return  0 ;
		}

		if( ! kik_str_to_uint( font_size , size_str))
		{
			kik_msg_printf( "a font size %s is not valid.\n" , size_str) ;

			return  0 ;
		}
	}
	else
	{
		*font_size = 0 ;
	}
	
	*font_name = entry ;
	
#ifdef  __DEBUG
	kik_debug_printf( KIK_DEBUG_TAG " setting font [size %d name %s]\n" ,
		*font_size , *font_name) ;
#endif

	return  1 ;
}

static int
parse_conf(
	x_font_config_t *  font_config ,
	const char *  key,
	char *  value	/* Includes multiple entries. Destroyed in this function. */
	)
{
	ml_font_t  font ;
	char *  entry ;

	if( ( font = parse_key( key)) == UNKNOWN_CS)
	{
		return  0 ;
	}

	/*
	 * [entry];[entry];[entry];....
	 */
	while( value != NULL && ( entry = kik_str_sep( &value , ";")) != NULL)
	{
		char *  font_name ;
		u_int  font_size ;

		if( parse_entry( &font_name , &font_size , entry))
		{
			if( font_size == 0)
			{
				/*
				 * default font.
				 * [font name]
				 */
				x_customize_default_font_name( font_config , font , font_name) ;
			}
			else
			{
				/*
				 * [font size],[font name]
				 */
				x_customize_font_name( font_config , font , font_name , font_size) ;
			}
		}
	}

	return  1 ;
}

static int
apply_custom_cache(
	x_font_config_t *  font_config ,
	const char *  filename
	)
{
	int  count ;

	for( count = 0 ; count < num_of_customs ; count++)
	{
		if( filename == custom_cache[count].file)
		{
			char *  p ;

		#ifdef  DEBUG
			kik_debug_printf( "Appling customization %s=%s\n" ,
				custom_cache[count].key , custom_cache[count].value) ;
		#endif
		
			if( ( p = kik_str_alloca_dup( custom_cache[count].value)))
			{
				parse_conf( font_config , custom_cache[count].key , p) ;
			}
		}
	}

	return  1 ;
}

static int
read_conf(
	x_font_config_t *  font_config ,
	const char *  filename
	)
{
	kik_file_t *  from ;
	char *  key ;
	char *  value ;

#ifdef  DEBUG
	kik_debug_printf( KIK_DEBUG_TAG " read_conf( %s)\n" , filename) ;
#endif

	if( ! ( from = kik_file_open( filename , "r")))
	{
	#ifdef  DEBUG
		kik_warn_printf( KIK_DEBUG_TAG " %s couldn't be opened.\n" , filename) ;
	#endif
	
		return  0 ;
	}

	while( kik_conf_io_read( from , &key , &value))
	{
		parse_conf( font_config, key, value) ;
	}
	
	kik_file_close( from) ;
	
	return  1 ;
}

static int
read_all_conf(
	x_font_config_t *  font_config ,
	const char *  changed_font_file	/* This function is called after a font file is changed,
					 * specify it. Otherwise specify NULL. */
	)
{
	char *  font_rcfile ;
	char *  font_rcfile2 ;	/* prior to font_rcfile */
	char *  rcpath ;
	
	/* '>= XFT' means XFT or STSF */
	if( font_config->type_engine >= TYPE_XFT)
	{
		font_rcfile = aafont_file ;
		
		switch( font_config->font_present & ~FONT_AA)
		{
		default:
			font_rcfile2 = NULL ;
			break ;

		case FONT_VAR_WIDTH:
			font_rcfile2 = vaafont_file ;
			break ;

		case FONT_VERTICAL:
			font_rcfile2 = taafont_file ;
			break ;
		}
	}
	else
	{
	#ifndef  USE_WIN32GUI
		if( font_config->font_present & FONT_AA)
		{
			/* Never reached. */
			return  0 ;
		}
		else
	#endif
		{
			font_rcfile = font_file ;

			switch( font_config->font_present)
			{
			default:
				font_rcfile2 = NULL ;
				break ;

			case FONT_VAR_WIDTH:
				font_rcfile2 = vfont_file ;
				break ;

			case FONT_VERTICAL:
				font_rcfile2 = tfont_file ;
				break ;
			}
		}
	}

	if( ! changed_font_file)
	{
		if( ( rcpath = kik_get_sys_rc_path( font_rcfile)))
		{
			read_conf( font_config , rcpath) ;
			free( rcpath) ;
		}
	}

	if( ! changed_font_file || changed_font_file == font_rcfile)
	{
		if( ( rcpath = kik_get_user_rc_path( font_rcfile)))
		{
			read_conf( font_config , rcpath) ;
			free( rcpath) ;
		}
	}

	apply_custom_cache( font_config , font_rcfile) ;

	if( font_rcfile2)
	{
		if( ! changed_font_file)
		{
			if( ( rcpath = kik_get_sys_rc_path( font_rcfile2)))
			{
				read_conf( font_config , rcpath) ;
				free( rcpath) ;
			}
		}

		if( ( rcpath = kik_get_user_rc_path( font_rcfile2)))
		{
			read_conf( font_config , rcpath) ;
			free( rcpath) ;
		}
		
		apply_custom_cache( font_config , font_rcfile2) ;
	}

	return  1 ;
}

static char *
overwrite_value(
	int *  is_changed ,	/* can be NULL. If _value is changed, *is_changed becomes 1. */
	char *  value ,		/* Overwriting value. Destroyed in this function. */
	char *  _value ,	/* Original value. Destroyed in this function. */
	int  do_remove		/* If 1, overwritten value is removed. */
	)
{
	char *  new_value ;
	struct
	{
		char *  font_name ;
		u_int  font_size ;
	} *  values , * _values ;
	u_int  num ;
	u_int  _num ;
	char *  p ;
	int  count ;
	int  _count ;

#ifdef  __DEBUG
	kik_debug_printf( "Overwriting %s with %s => " , _value , value) ;
#endif

	if( *value == '\0')
	{
		/* _value is completely removed. */
		
		if( is_changed)
		{
			*is_changed = 1 ;
		}

		return  strdup( value) ;
	}

	if( ( values = alloca( sizeof( *values) *
			( kik_count_char_in_str( value , ';') + 1))) == NULL ||
	    ( _values = alloca( sizeof( *_values) *
			( kik_count_char_in_str( _value , ';') + 1))) == NULL ||
	    /* If value = "a" and _value = "b", new_value = "a;b;" */
	    ( new_value = malloc( strlen( value) + strlen( _value) + 3)) == NULL)
	{
		return  NULL ;
	}

	_num = 0 ;
	while( _value != NULL && ( p = kik_str_sep( &_value , ";")) != NULL)
	{
		if( parse_entry( &(_values[_num].font_name) , &(_values[_num].font_size) , p))
		{
			_num ++ ;
		}
	}

	num = 0 ;
	while( value != NULL && ( p = kik_str_sep( &value , ";")) != NULL)
	{
		if( parse_entry( &(values[num].font_name) , &(values[num].font_size) , p))
		{
			num ++ ;
		}
	}

	*new_value = '\0' ;
	
	for( _count = 0 ; _count < _num ; _count++)	/* Original value */
	{
		for( count = 0 ; count < num ; count++)	/* Overwriting value */
		{
			if( values[count].font_name &&
				_values[_count].font_size == values[count].font_size)
			{
				if( ! do_remove && strcmp( _values[_count].font_name ,
							values[count].font_name) == 0)
				{
					/* Overwriting value is ignored. */
					values[count].font_name = NULL ;
				}
				else
				{
					/* Original value is overwritten(removed) */
					_values[_count].font_name = NULL ;
					
					if( _count == 0 && ! do_remove &&
						values[count].font_size == 0)
					{
						/* Overwriting default font name. */
						sprintf( new_value , "%s;" ,
							values[count].font_name) ;
					}
				}

				/* In case same font_size exists in _values, don't break here. */
			}
		}

		/* If default font name isn't overwritten(removed) above, copy original one. */
		if( _values[_count].font_size == 0 && _values[_count].font_name)
		{
			sprintf( new_value , "%s;" , _values[_count].font_name) ;
		}
	}

	p = new_value + strlen(new_value) ;

	if( is_changed)
	{
		*is_changed = 0 ;
	}
	
	/* Copy original values which are not overwritten(removed). */
	for( _count = 0 ; _count < _num ; _count++)
	{
		/* If _values[count].font_name is NULL , this is overwritten(removed) above. */
		if( _values[_count].font_name)
		{
			if( _values[_count].font_size > 0)
			{
				sprintf( p , "%d,%s;" , _values[_count].font_size ,
					_values[_count].font_name) ;
				p += strlen(p) ;
			}
		}
		else if( is_changed)
		{
			*is_changed = 1 ;
		}
	}

	if( ! do_remove)
	{
		/* Adding overwriting values. */
		for( count = 0 ; count < num ; count++)
		{
			/*
			 * If values[count].font_name is NULL, original value is not changed.
			 * If values[count].font_name is "", original value is not overwritten
			 * but removed.
			 */
			if( values[count].font_name && *values[count].font_name &&
				values[count].font_size > 0)
			{
				sprintf( p , "%d,%s;" , values[count].font_size ,
					values[count].font_name) ;
				p += strlen(p) ;

				if( is_changed)
				{
					*is_changed = 1 ;
				}
			}
		}
	}

#ifdef  __DEBUG
	kik_msg_printf( "%s\n" , new_value) ;
#endif

	return  new_value ;
}

static int
operate_custom_cache(
	const char *  file ,
	const char *  key ,
	char *  value ,
	int  operate	/* 0=add, 1=remove (according to argument of overwrite_value()). */
	)
{
	void *  p ;
	int  count ;

	for( count = 0 ; count < num_of_customs ; count++)
	{
		if( custom_cache[count].file == file &&
			strcmp( custom_cache[count].key , key) == 0)
		{
			int  is_changed ;
			char *  new_value ;
			
			if( ( new_value = overwrite_value( &is_changed , value ,
						custom_cache[count].value , operate)) == NULL)
			{
				return  0 ;
			}

			if( *new_value)
			{
				free( custom_cache[count].value) ;
				custom_cache[count].value = new_value ;
			}
			else
			{
				/*
				 * value is removed completely in overwrite_value().
				 */
				
				free( new_value) ;
				free( custom_cache[count].key) ;
				free( custom_cache[count].value) ;
				custom_cache[count] = custom_cache[--num_of_customs] ;
				if( num_of_customs == 0)
				{
					free( custom_cache) ;
					custom_cache = NULL ;
				}
			}

			if( ! is_changed)
			{
			#ifdef  DEBUG
				kik_debug_printf( KIK_DEBUG_TAG " Not changed\n") ;
			#endif
			
				return  0 ;
			}

		#if  0
			kik_debug_printf( "%s=%s\n" , key , new_value) ;
		#endif
		
			return  1 ;
		}
	}

	if( operate == 1)
	{
		/* Nothing to remove. */
		
		return  0 ;
	}

	if( ( p = realloc( custom_cache , sizeof(custom_cache_t) * (num_of_customs + 1))) == NULL)
	{
		return  0 ;
	}

	custom_cache = p ;
	
	custom_cache[num_of_customs].file = file ;
	custom_cache[num_of_customs].key = strdup( key) ;
	custom_cache[num_of_customs++].value = strdup( value) ;

#if  0
	kik_debug_printf( "%s=%s\n" , key , value) ;
#endif

	return  1 ;
}

static int
save_conf(
	const char *  file ,
	const char *  key ,
	char *  value		/* Includes multiple entries. Destroyed in this function. */
	)
{
	char *  path ;
	kik_file_t *  kfile ;
	char *  new_value ;
	kik_conf_write_t *  conf ;

	if( ( path = kik_get_user_rc_path( file)) == NULL)
	{
		return  0 ;
	}

	new_value = NULL ;

	kfile = kik_file_open( path , "r") ;

	while( kfile)
	{
		char *  _key ;
		char *  _value ;

		if( ! kik_conf_io_read( kfile , &_key , &_value))
		{
			break ;
		}
		else if( strcmp( key , _key) == 0)
		{
			int  is_changed ;

			if( ( new_value = overwrite_value( &is_changed , value , _value , 0))
						== NULL || ! is_changed)
			{
				free( new_value) ;
				free( path) ;
				kik_file_close( kfile) ;

				#ifdef  DEBUG
				if( ! is_changed)
				{
					kik_debug_printf( KIK_DEBUG_TAG " Not changed.\n") ;
				}
				#endif
				
				return  0 ;
			}
			
			break ;
		}
	}

	if( kfile)
	{
		kik_file_close( kfile) ;
	}

	if( new_value)
	{
		value = new_value ;
	}

	conf = kik_conf_write_open( path) ;
	free( path) ;
	if( conf == NULL)
	{
		return  0 ;
	}

	kik_conf_io_write( conf , key , value) ;

	kik_conf_write_close( conf) ;

	/* Remove from config_cache */
	operate_custom_cache( file , key , value, 1 /* remove */) ;

	if( new_value)
	{
		free( new_value) ;
	}

	return  1 ;
}

static int
is_valid_format(
	const char *  format
	)
{
	char *  p;

	if( ( p = strchr( format, '%')))
	{
		/* '%' can happen only once at most */
		if( p != strrchr( format, '%'))
		{
			return  0 ;
		}

		/* force to be '%d' */
		if( p[1] != 'd')
		{
			return  0 ;
		}
	}

	return 1;
}

static x_font_config_t *
find_font_config(
	x_type_engine_t  type_engine ,
	x_font_present_t  font_present
	)
{
	if( font_configs)
	{
		int  count ;

		for( count = 0 ; count < num_of_configs ; count ++)
		{
			if( font_configs[count]->font_present == font_present &&
				font_configs[count]->type_engine == type_engine)
			{
				return  font_configs[count] ;
			}
		}
	}

	return  NULL ;
}

static u_int
match_font_configs(
	x_font_config_t **  matched_configs ,
	u_int  max_size ,	/* must be over 0. */
	int  is_xcore ,
	x_font_present_t  present_mask
	)
{
	int  count ;
	u_int  size ;

	size = 0 ;
	for( count = 0 ; count < num_of_configs ; count++)
	{
		if( (is_xcore ? font_configs[count]->type_engine == TYPE_XCORE :
				/* '>= XFT' means XFT or STSF */
				font_configs[count]->type_engine >= TYPE_XFT) &&
		    (present_mask ? (font_configs[count]->font_present & present_mask) : 1) )
		{
			matched_configs[size++] = font_configs[count] ;
			if( size >= max_size)
			{
				break ;
			}
		}
	}

	return  size ;
}

x_font_config_t *
create_shared_font_config(
	x_type_engine_t  type_engine ,
	x_font_present_t  font_present
	)
{
	int  count ;
	
	for( count = 0 ; count < num_of_configs ; count ++)
	{
		if( ( type_engine == TYPE_XCORE ? font_configs[count]->type_engine == type_engine :
						font_configs[count]->type_engine >= type_engine) &&
		    ( (font_configs[count]->font_present & ~FONT_AA) == (font_present & ~FONT_AA)))
		{
		#ifdef  DEBUG
			kik_debug_printf( KIK_DEBUG_TAG " Found sharable font_config.\n") ;
		#endif

			x_font_config_t *  font_config ;
			
			if( ( font_config = malloc( sizeof( x_font_config_t))) == NULL)
			{
				return  NULL ;
			}

			font_config->type_engine = type_engine ;
			font_config->font_present = font_present ;
			font_config->font_name_table = font_configs[count]->font_name_table ;
			font_config->default_font_name_table =
				font_configs[count]->default_font_name_table ;
			font_config->ref_count = 0 ;

			return   font_config ;
		}
	}

	return  NULL ;
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

	min_font_size = min_fsize ;
	max_font_size = max_fsize ;

#ifdef  DEBUG
	TEST_overwrite_value() ;
#endif
	
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

	if( ( font_config = find_font_config( type_engine , font_present)))
	{
		font_config->ref_count ++ ;

		return  font_config ;
	}
	
	if( ( p = realloc( font_configs , sizeof( x_font_config_t*) * (num_of_configs + 1))) == NULL)
	{
		return  NULL ;
	}

	font_configs = p ;

	if( ( font_config = create_shared_font_config( type_engine , font_present)) == NULL)
	{
		if( ( font_config = x_font_config_new( type_engine , font_present)) == NULL ||
			! read_all_conf( font_config , NULL) )
		{
			return  NULL ;
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
	int  has_share ;
	int  found ;
	
	if( -- font_config->ref_count > 0)
	{
		return  1 ;
	}

	has_share = 0 ;
	found = 0 ;
	count = 0 ;
	while( count < num_of_configs)
	{
		if( font_configs[count] == font_config)
		{
			font_configs[count] = font_configs[--num_of_configs] ;
			found = 1 ;

			continue ;
		}
		else if( ( font_config->type_engine == TYPE_XCORE ?
				font_configs[count]->type_engine == font_config->type_engine :
				font_configs[count]->type_engine >= font_config->type_engine) &&
			( (font_configs[count]->font_present & ~FONT_AA) ==
				(font_config->font_present & ~FONT_AA)) )
		{
			has_share = 1 ;
		}

		count ++ ;
	}

	if( ! found)
	{
	#ifdef  DEBUG
		kik_debug_printf( KIK_DEBUG_TAG " font_config is not found in font_configs.\n") ;
	#endif
	
		return  0 ;
	}

	if( has_share /* && num_of_configs > 0 */)
	{
	#ifdef  DEBUG
		kik_debug_printf( KIK_DEBUG_TAG " Sharable font_config exists.\n") ;
	#endif
	
		free( font_config) ;

		return  1 ;
	}

	x_font_config_delete( font_config) ;
	
	if( num_of_configs == 0)
	{
		free( font_configs) ;
		font_configs = NULL ;
	}

	return  1 ;
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

	kik_map_get( result , get_font_name_table( font_config , font_size) , font , pair) ;
	if( result)
	{
		/* If new fontname is the same as current one, nothing is done. */
		if( strcmp( pair->value , fontname) != 0)
		{
			if( ( fontname = strdup( fontname)) == NULL)
			{
				return  0 ;
			}
			
			free( pair->value) ;
			pair->value = fontname ;
		}
	}
	else
	{
		if( ( fontname = strdup( fontname)) == NULL)
		{
			return  0 ;
		}
		
		kik_map_set( result , get_font_name_table( font_config , font_size) ,
			font , fontname) ;
	}

#ifdef  __DEBUG
	kik_warn_printf( "fontname %s for size %d\n" , fontname , font_size) ;
#endif

	return  1 ;
}

int
x_customize_default_font_name(
	x_font_config_t *  font_config ,
	ml_font_t  font ,
	char *  fontname
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

int
x_customize_font_file(
	const char *  file ,	/* if null, use "mlterm/font" file. */
	char *  key ,	/* charset name */
	char *  value ,	/* font list */
	int  save
	)
{
	/*
	 * Max number of target font_config is 6.
	 * [file == aafont_file]
	 * TYPE_XFT, TYPE_XFT & FONT_VAR_WIDTH , TYPE_XFT & FONT_VERTICAL , TYPE_XFT & FONT_AA ,
	 * TYPE_XFT & FONT_VAR_WIDTH & FONT_AA , TYPE_XFT & FONT_VERTICAL & FONT_AA
	 */
	x_font_config_t *  targets[6] ;
	u_int  num_of_targets ;
	int  count ;

	if( file == NULL || strcmp( file, font_file + 7) == 0)
	{
		file = font_file ;
		num_of_targets = match_font_configs( targets , 6 , /* is xcore */ 1 , 0) ;
	}
	else if( strcmp( file, aafont_file + 7) == 0)
	{
		file = aafont_file ;
		num_of_targets = match_font_configs( targets , 6 , /* is not xcore */ 0 , 0) ;
	}
	else if( strcmp( file, vfont_file + 7) == 0)
	{
		file = vfont_file ;
		num_of_targets = match_font_configs( targets , 6 , /* is xcore */ 1 ,
							FONT_VAR_WIDTH) ;
	}
	else if( strcmp( file, tfont_file + 7) == 0)
	{
		file = tfont_file ;
		num_of_targets = match_font_configs( targets , 6 , /* is xcore */ 1 ,
							FONT_VERTICAL) ;
	}
	else if( strcmp( file, vaafont_file + 7) == 0)
	{
		file = vaafont_file ;
		num_of_targets = match_font_configs( targets , 6 , /* is not xcore */ 0 ,
							FONT_VAR_WIDTH) ;
	}
	else if( strcmp( file, taafont_file + 7) == 0)
	{
		file = taafont_file ;
		num_of_targets = match_font_configs( targets , 6 , /* is not xcore */ 0 ,
							FONT_VERTICAL) ;
	}
	else
	{
	#ifdef  DEBUG
		kik_debug_printf( KIK_DEBUG_TAG " font file %s is not found.\n", file) ;
	#endif
	
		return  0 ;
	}

#ifdef  DEBUG
	if( num_of_targets)
	{
		kik_debug_printf( "customize font file %s %s %s\n", file, key, value) ;
	}
	else
	{
		kik_debug_printf( "customize font file %s %s %s(not changed in run time)\n",
			file, key, value) ;
	}
#endif

	if( save)
	{
	#ifdef  DEBUG
		kik_debug_printf( "customization is saved.\n") ;
	#endif
		
		if( ! save_conf( file , key , value))
		{
			return  0 ;
		}
	}
	else
	{
		/* Add to custom_cache. */
		if( ! operate_custom_cache( file , key , value , 0 /* add */))
		{
			return  0 ;
		}
	}

	for( count = 0 ; count < num_of_targets ; count++)
	{
		read_all_conf( targets[count] , file) ;
	}

	return  1 ;
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

	if( !is_valid_format( fa_pair->value))
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
x_get_config_font_name2(
	const char *  file ,		/* can be NULL */
	u_int  font_size ,
	char *  font_cs
	)
{
	ml_font_t  font ;
	x_font_config_t *  font_config ;
	x_type_engine_t  engine ;
	x_font_present_t  present ;
	char *  font_name ;

	if( file == NULL || strcmp( file, font_file + 7) == 0)
	{
		engine = TYPE_XCORE ;
		present = 0 ;
	}
	else if( strcmp( file, aafont_file + 7) == 0)
	{
		engine = TYPE_XFT ;
		
		/*
		 * font_config::default_font_name_table and ::font_name_table are
		 * shared with font_configs whose difference is only FONT_AA.
		 */
		present = 0 ;
	}
	else if( strcmp( file, vfont_file + 7) == 0)
	{
		engine = TYPE_XCORE ;
		present = FONT_VAR_WIDTH ;
	}
	else if( strcmp( file, tfont_file + 7) == 0)
	{
		engine = TYPE_XCORE ;
		present = FONT_VERTICAL ;
	}
	else if( strcmp( file, vaafont_file + 7) == 0)
	{
		engine = TYPE_XFT ;
		present = FONT_VAR_WIDTH ;
	}
	else if( strcmp( file, taafont_file + 7) == 0)
	{
		engine = TYPE_XFT ;
		present = FONT_VERTICAL ;
	}
	else
	{
	#ifdef  DEBUG
		kik_debug_printf( KIK_DEBUG_TAG " font file %s is not found.\n", file) ;
	#endif
	
		return  NULL ;
	}

	if( ( font_config = x_acquire_font_config( engine , present)) == NULL)
	{
	#ifdef  DEBUG
		kik_debug_printf( KIK_DEBUG_TAG " x_font_config_t is not found.\n") ;
	#endif
	
		return  NULL ;
	}
	
	if( ( font = parse_key( font_cs)) == UNKNOWN_CS)
	{
		return  NULL ;
	}

	font_name = x_get_config_font_name( font_config , font_size , font) ;

	x_release_font_config( font_config) ;

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
		if( is_valid_format( d_array[count]->value))
		{
			sprintf( p , d_array[count]->value , font_size) ;
			p += strlen( p) ;
			*(p ++) = ',' ;
		}
	}

	*(p - 1) = '\0' ;

	return  font_name_list ;
}

#ifdef  DEBUG
static int
TEST_overwrite_value(void)
{
	struct
	{
		char *  data1 ;
		char *  data2 ;
		char *  data3 ;
		char *  data4 ;
	} data[] =
	{
		{ "a10;12,a12;14,a14" , "12,a13" , "a10;14,a14;12,a13;" , "a10;14,a14;" } ,
		{ "a10;12,a12;14,a14" , "a12" , "a12;12,a12;14,a14;" , "12,a12;14,a14;" } ,
		{ "a10;a12;a13" , "a14" , "a14;" , "" } ,
		{ "a10;12,a12;12,a14" , "12,a16" , "a10;12,a16;" , "a10;" } ,
		{ "12,a12" , "14,a14" , "12,a12;14,a14;" , "12,a12;" } ,
		{ "a10" , "12,a12" , "a10;12,a12;" , "a10;" } ,
	} ;
	int  count ;

	for( count = 0 ; count < sizeof(data)  / sizeof(data[0]) ; count++)
	{
		char * ret ;
		int  is_changed ;
		
		ret = overwrite_value( &is_changed ,
			kik_str_alloca_dup( data[count].data2) ,
			kik_str_alloca_dup( data[count].data1) , 0) ;

		if( strcmp( data[count].data3 , ret) != 0)
		{
			kik_debug_printf( KIK_DEBUG_TAG " Test failed(%s + %s => %s).\n" ,
				data[count].data1 , data[count].data2 , ret) ;
		}
		else if( is_changed == 0)
		{
			kik_debug_printf( KIK_DEBUG_TAG
				" Test failed(%s + %s => %s is *not* changed?).\n" ,
				data[count].data1 , data[count].data2 , ret) ;
		}

		free( ret) ;
		
		ret = overwrite_value( &is_changed ,
			kik_str_alloca_dup( data[count].data2) ,
			kik_str_alloca_dup( data[count].data1) , 1) ;

		if( strcmp( data[count].data4 , ret) != 0)
		{
			kik_debug_printf( KIK_DEBUG_TAG " Test failed(%s - %s => %s).\n" ,
				data[count].data1 , data[count].data2 , ret) ;
		}
		else if( count < 4 && is_changed == 0)
		{
			kik_debug_printf( KIK_DEBUG_TAG
				" Test failed(%s - %s => %s is *not* changed?).\n" ,
				data[count].data1 , data[count].data2 , ret) ;
		}

		free( ret) ;
	}

	return  1 ;
}
#endif
