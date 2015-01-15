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
#include  <kiklib/kik_debug.h>

#include  <ml_char.h>
#include  <ml_char_encoding.h>	/* ml_parse_unicode_area */


#define  DEFAULT_FONT  0x1ff	/* MAX_CHARSET */

#if  0
#define  __DEBUG
#endif


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


/* --- static variables --- */

#ifdef  USE_FRAMEBUFFER

#define  FONT_FILE  "font"
#define  VFONT_FILE  "vfont"
#define  TFONT_FILE  "tfont"
static char *  font_file = "mlterm/font-fb" ;
static char *  vfont_file = "mlterm/vfont-fb" ;
static char *  tfont_file = "mlterm/tfont-fb" ;

#else

#define  FONT_FILE  (font_file + 7)
#define  VFONT_FILE (vfont_file + 7)
#define  TFONT_FILE (tfont_file + 7)
static char *  font_file = "mlterm/font" ;
static char *  vfont_file = "mlterm/vfont" ;
static char *  tfont_file = "mlterm/tfont" ;

#endif

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
	{ "ISCII_ASSAMESE" , ISCII_ASSAMESE } ,
	{ "ISCII_BENGALI" , ISCII_BENGALI } ,
	{ "ISCII_GUJARATI" , ISCII_GUJARATI } ,
	{ "ISCII_HINDI" , ISCII_HINDI } ,
	{ "ISCII_KANNADA" , ISCII_KANNADA } ,
	{ "ISCII_MALAYALAM" , ISCII_MALAYALAM } ,
	{ "ISCII_ORIYA" , ISCII_ORIYA } ,
	{ "ISCII_PUNJABI" , ISCII_PUNJABI } ,
	{ "ISCII_ROMAN" , ISCII_ROMAN } ,
	{ "ISCII_TAMIL" , ISCII_TAMIL } ,
	{ "ISCII_TELUGU" , ISCII_TELUGU } ,
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

#ifdef  __ANDROID__
static u_int  min_font_size = 10 ;
static u_int  max_font_size = 40 ;
#else
static u_int  min_font_size = 6 ;
static u_int  max_font_size = 30 ;
#endif

/*
 * These will be leaked unless operate_custom_cache( ... , 1 [remove]) deletes them.
 * operate_custom_cache( ... , 1 [remove]) is called only from save_conf, which means
 * that they are deleted when all of them are saved to ~/.mlterm/(vt)(aa)font file.
 */
static custom_cache_t *  custom_cache ;
static u_int  num_of_customs ;


/* --- static functions --- */

#ifdef  KIK_DEBUG
static void  TEST_font_config(void) ;
#endif


static KIK_PAIR( x_font_name)
get_font_name_pair(
	KIK_MAP( x_font_name)  table ,
	ml_font_t  font
	)
{
	KIK_PAIR( x_font_name)  pair ;

	kik_map_get( table , font , pair) ;

	return  pair ;
}

static KIK_PAIR( x_font_name) *
get_font_name_pairs_array(
	u_int *  size ,
	KIK_MAP( x_font_name)  table
	)
{
	KIK_PAIR( x_font_name) *  array ;
	
	kik_map_get_pairs_array( table , array , *size) ;

	return  array ;
}

static int
set_font_name_to_table(
	KIK_MAP( x_font_name)  table ,
	ml_font_t  font ,
	char *  fontname
	)
{
	int  result ;

	kik_map_set( result , table , font , fontname) ;

	return  result ;
}

/* Always returns Not-NULL value. */
static KIK_MAP( x_font_name)
get_font_name_table(
	x_font_config_t *  font_config ,
	int  font_size				/* Check if valid before call this function. */
	)
{
	if( font_config->font_name_table[font_size - min_font_size] == NULL)
	{
		kik_map_new_with_size( ml_font_t , char * ,
			font_config->font_name_table[font_size - min_font_size] ,
			kik_map_hash_int , kik_map_compare_int , 16) ;
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

	if( key_len >= 7 && strncmp( key , "DEFAULT" , 7) == 0)
	{
		font = DEFAULT_FONT ;

		goto  check_style ;
	}

	if( key_len >= 3 && strncmp( key , "U+" , 2) == 0)
	{
		u_int  min ;
		u_int  max ;

		if( ml_parse_unicode_area( key , &min , &max) &&
		    ( font = ml_char_get_unicode_area_font( min , max)) != UNKNOWN_CS)
		{
			goto  check_style ;
		}
		else
		{
			return  UNKNOWN_CS ;
		}
	}
	
	for( count = 0 ; count < sizeof( cs_table) / sizeof( cs_table[0]) ; count ++)
	{
		size_t  nlen ;

		nlen = strlen( cs_table[count].name) ;

		if( key_len >= nlen && strncmp( cs_table[count].name , key , nlen) == 0 &&
			( key[nlen] == '\0' ||
				/* "_BOLD" or "_FULLWIDTH" is trailing */ key[nlen] == '_'))
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

	font = NORMAL_FONT_OF(cs) ;

	if( ! ( font & FONT_FULLWIDTH) &&
	    ( strstr( key , "_BIWIDTH") || /* compat with 3.2.2 or before. */
	      strstr( key , "_FULLWIDTH")))
	{
		font |= FONT_FULLWIDTH ;
	}

check_style:
	if( strstr( key , "_BOLD"))
	{
		font |= FONT_BOLD ;
	}

	if( strstr( key , "_ITALIC"))
	{
		font |= FONT_ITALIC ;
	}

	return  font ;
}

/*
 * If entry == "" or ";", font_name is "" and font_size is 0.
 */
static int
parse_entry(
	char **  font_name ,	/* if entry is "" or illegal format, not changed. */
	u_int *  font_size ,	/* if entry is "" or illegal format, not changed. */
	char *  entry		/* Don't specify NULL. */
	)
{
#ifdef  __DEBUG
	kik_debug_printf( KIK_DEBUG_TAG " Parsing %s => " , entry) ;
#endif

	if( strchr( entry , ','))
	{
		/*
		 * for each size.
		 * [size],[font name]
		 */
		char *  size_str ;

		/*
		 * kik_str_sep() never returns NULL and and entry never becomes NULL
		 * because strchr( entry , ',') is succeeded.
		 */
		size_str = kik_str_sep( &entry , ",") ;
		
		if( ! kik_str_to_uint( font_size , size_str))
		{
			kik_msg_printf( "font size %s is not valid.\n" , size_str) ;

			return  0 ;
		}
	}
	else
	{
		*font_size = 0 ;
	}
	
	*font_name = entry ;
	
#ifdef  __DEBUG
	kik_msg_printf( "size %d name %s\n" , *font_size , *font_name) ;
#endif

	return  1 ;
}

/*
 * value = "" or ";" => Reset default font name.
 * value = ";12,b;" => Reset deafult font name and set "b" as font name for size 12.
 */
static int
parse_conf(
	x_font_config_t *  font_config ,
	const char *  key,
	char *  value		/* Includes multiple entries. Destroyed in this function. */
	)
{
	ml_font_t  font ;
	char *  entry ;

	if( ( font = parse_key( key)) == UNKNOWN_CS)
	{
		return  0 ;
	}

	if( *value == '\0')
	{
		/* Remove current setting. */
		x_customize_default_font_name( font_config , font , value) ;

		return  1 ;
	}
	
	/*
	 * [entry];[entry];[entry];....
	 * kik_str_sep() returns NULL only if value == NULL.
	 */
	while( ( entry = kik_str_sep( &value , ";")) != NULL)
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
		
		if( value && *value == '\0')
		{
			/* Last ';' of "....;" was parsed. Be careful in kik_str_sep( val , ";") */
			break ;
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

		#ifdef  __DEBUG
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

#ifdef  __DEBUG
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
	#ifdef  __DEBUG
		kik_debug_printf( KIK_DEBUG_TAG " Read line from %s => %s = %s\n" ,
			filename , key , value) ;
	#endif
	
		parse_conf( font_config , key , value) ;
	}
	
	kik_file_close( from) ;
	
	return  1 ;
}

static int
read_all_conf(
	x_font_config_t *  font_config ,
	const char *  changed_font_file	/* If this function is called after a font file is
	                                 * changed, specify it here to avoid re-read font files.
					 * Otherwise specify NULL. */
	)
{
	char *  font_rcfile ;
	char *  font_rcfile2 ;	/* prior to font_rcfile */
	char *  rcpath ;
	
	/* '>= XFT' means XFT or Cairo */
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
		font_rcfile = font_file ;

		switch( font_config->font_present & ~FONT_AA)
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

/*
 * <Return value>
 * "<default font name>;<font size>,<font name>;...;"
 * NULL: error(including the case of ow_value having no valid entry) happen.
 *       In this case, orig_value is not destroyed.
 * ""  : Removing succesfully ow_value from orig_value results in no entries.
 * ";" : Default font name == "".
 *
 * <Note>
 * If ow_value or orig_value has invalid format entries, they are ignored.
 */
static char *
create_value(
	int *  is_changed ,	/* can be NULL. If orig_value is changed, *is_changed becomes 1. */
	char *  ow_value ,	/* Overwriting value. Destroyed in this function. */
	char *  orig_value ,	/* Original value(can be NULL). Destroyed in this function. */
	int  operate		/* 0=add, 1=remove. */
	)
{
	char *  new_value ;
	size_t  new_value_len ;
	struct
	{
		char *  font_name ;
		u_int  font_size ;
	} *  ow_values , *  orig_values ;
	u_int  num ;	/* for ow_value */
	u_int  _num ;	/* for orig_value */
	int  count ;	/* for ow_value */
	int  _count ;	/* for orig_value */
	char *  p ;

#ifdef  __DEBUG
	kik_debug_printf( "%s %s with %s => " ,
		operate ? "Removing" : "Overwriting" , orig_value , ow_value) ;
#endif

	if( ( ow_values = alloca( sizeof( *ow_values) *
		( kik_count_char_in_str( ow_value , ';') + 1))) == NULL ||
	    ( orig_values = alloca( sizeof( *orig_values) *
		( ( orig_value ? kik_count_char_in_str( orig_value , ';') : 0)  + 1))) == NULL)
	{
		return  NULL ;
	}

	/* If ow_value = "a" and orig_value = "b", new_value = "a;b;" */
	new_value_len = strlen( ow_value) + (orig_value ? strlen( orig_value) : 0) + 3 ;

	num = 0 ;
	/* kik_str_sep() returns NULL only if ow_value == NULL. */
	while( ( p = kik_str_sep( &ow_value , ";")) != NULL)
	{
		if( parse_entry( &(ow_values[num].font_name) , &(ow_values[num].font_size) , p))
		{
			num ++ ;
		}
		
		if( ow_value && *ow_value == '\0')
		{
			/* Last ';' of "....;" was parsed. Be careful in kik_str_sep( val , ";") */
			break ;
		}
	}

	if( num == 0)
	{
		/* ow_value doesn't have valid entry. */
		return  NULL ;
	}

	if( ( new_value = malloc( new_value_len)) == NULL)
	{
		return  NULL ;
	}
	
	_num = 0 ;
	/* kik_str_sep() returns NULL only if orig_value == NULL. */
	while( ( p = kik_str_sep( &orig_value , ";")) != NULL)
	{
		if( parse_entry( &(orig_values[_num].font_name) ,
				&(orig_values[_num].font_size) , p))
		{
			_num ++ ;
		}

		if( orig_value && *orig_value == '\0')
		{
			/* Last ';' of "....;" was parsed. Be careful in kik_str_sep( val , ";") */
			break ;
		}
	}

	*(p = new_value) = '\0' ;
	
	for( _count = 0 ; _count < _num ; _count++)	/* Original value */
	{
		for( count = 0 ; count < num ; count++)	/* Overwriting value */
		{
			if( ow_values[count].font_name &&
				orig_values[_count].font_size == ow_values[count].font_size)
			{
				if( operate == 0 && strcmp( orig_values[_count].font_name ,
							ow_values[count].font_name) == 0)
				{
					/* Overwriting value is ignored. */
					ow_values[count].font_name = NULL ;
				}
				else
				{
					/* Original value is overwritten. */
					orig_values[_count].font_name = NULL ;
				}

				/* In case same font_size exist in orig_values, don't break here */
			}
		}

		/* If default font name isn't overwritten above, copy original one. */
		if( orig_values[_count].font_size == 0 && orig_values[_count].font_name)
		{
			sprintf( p , "%s;" , orig_values[_count].font_name) ;
			p += strlen(p) ;
		}
	}

	if( is_changed)
	{
		*is_changed = 0 ;
	}
	
	if( operate == 0)
	{
		/* Adding overwriting values. */
		for( count = 0 ; count < num ; count++)
		{
			/*
			 * If ow_values[count].font_name is NULL, original value is not changed.
			 */
			if( ow_values[count].font_name)
			{
				if( ow_values[count].font_size == 0)
				{
					/* default font name. */

					if( new_value == p)
					{
						sprintf( new_value , "%s;" ,
							ow_values[count].font_name) ;
					}
					else
					{
						size_t  ow_len ;
						
						ow_len = strlen( ow_values[count].font_name) ;
						memmove( new_value + ow_len + 1 , new_value ,
							strlen( new_value) + 1) ; 
						memcpy( new_value , ow_values[count].font_name ,
							ow_len) ;
						memcpy( new_value + ow_len , ";" , 1) ;
					}
				}
				else /* if( ow_values[count].font_size > 0) */
				{
					sprintf( p , "%d,%s;" , ow_values[count].font_size ,
						ow_values[count].font_name) ;
				}
				
				p += strlen(p) ;
				
				if( is_changed)
				{
					*is_changed = 1 ;
				}
			}
		}
	}

	/* Copy original values which are not overwritten. */
	for( _count = 0 ; _count < _num ; _count++)
	{
		/* If orig_values[count].font_name is NULL , this is overwritten above. */
		if( orig_values[_count].font_name)
		{
			/*
			 * That of orig_values[_count].font_size == 0 is added to new_line in
			 * for loop above.
			 */
			if( orig_values[_count].font_size > 0)
			{
				sprintf( p , "%d,%s;" , orig_values[_count].font_size ,
					orig_values[_count].font_name) ;
				p += strlen(p) ;
			}
		}
		else if( is_changed)
		{
			*is_changed = 1 ;
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
	char *  value ,	/* Destroyed in this function. */
	int  operate	/* 0=add, 1=remove (same as create_value()). */
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
			
			if( ( new_value = create_value( &is_changed , value ,
						custom_cache[count].value , operate)) == NULL)
			{
			#ifdef  DEBUG
				kik_debug_printf( KIK_DEBUG_TAG
					" Failed to create value in operate_custom_cache.\n") ;
			#endif
			
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
				 * value is removed completely in create_value().
				 */

				free( new_value) ;
				free( custom_cache[count].key) ;
				free( custom_cache[count].value) ;
				custom_cache[count] = custom_cache[--num_of_customs] ;
				if( num_of_customs == 0)
				{
					free( custom_cache) ;
					custom_cache = NULL ;
					
				#ifdef  __DEBUG
					kik_debug_printf( "Custom cache is completely freed.\n") ;
				#endif
				}
			}

			if( ! is_changed)
			{
			#ifdef  DEBUG
				kik_debug_printf( KIK_DEBUG_TAG
					" custom_cache is not changed.\n") ;
			#endif
			
				return  0 ;
			}

		#ifdef  __DEBUG
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

	if( ( value = create_value( NULL , value , NULL , 0)) == NULL)
	{
		return  0 ;
	}
	
	if( ( custom_cache[num_of_customs].key = strdup( key)) == NULL)
	{
		free( value) ;

		return  0 ;
	}
	
	custom_cache[num_of_customs].value = value ;
	custom_cache[num_of_customs++].file = file ;

#ifdef  __DEBUG
	kik_debug_printf( "%s=%s is newly added to custom cache.\n" , key , value) ;
#endif

	return  1 ;
}

static int
write_conf(
	char *  path ,		/* Can be destroyed in this function. */
	const char *  key ,
	char *  value		/* Includes multiple entries. Destroyed in this function. */
	)
{
	char *  cleaned_value ;
	char *  cv_p ;
	char *  v_p ;
	kik_conf_write_t *  conf ;

#ifdef  __DEBUG
	kik_debug_printf( KIK_DEBUG_TAG " Clean value %s => " , value) ;
#endif

	/*
	 * ";10,;11,a11" => "11,a11"
	 */
	cv_p = cleaned_value = value ;
	while( 1)
	{
		int  do_break ;

		if( ( v_p = strchr( value , ';')))
		{
			*v_p = '\0' ;
			if( *(v_p + 1) == '\0')
			{
				do_break = 1 ;
			}
			else
			{
				do_break = 0 ;
			}
		}
		else
		{
			v_p = value + strlen(value) ;
			do_break = 1 ;
		}

		if( v_p != value)
		{
			if( *(v_p - 1) != ',')
			{
				size_t  len ;

				len = strlen( value) ;
				memmove( cv_p , value , len) ;
				cv_p += len ;
				if( ! do_break)
				{
					/*
					 * If value is not terminated by ';' like "12,a12",
					 * don't append ';'. Appending ';' can break
					 * memory of 'value'.
					 */
					memcpy( cv_p , ";" , 1) ;
					cv_p ++ ;
				}
			}
		}

		if( do_break)
		{
			break ;
		}
		else
		{
			/* goto next entry. */
			value = v_p + 1 ;
		}
	}

	*cv_p = '\0' ;

#ifdef  __DEBUG
	kik_msg_printf( "%s\n" , cleaned_value) ;
#endif

	conf = kik_conf_write_open( path) ;
	if( conf == NULL)
	{
		return  0 ;
	}

	kik_conf_io_write( conf , key , cleaned_value) ;

	kik_conf_write_close( conf) ;

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
	char *  p ;

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

			if( ( new_value = create_value( &is_changed , value , _value , 0))
						== NULL || ! is_changed)
			{
				free( new_value) ;
				free( path) ;
				kik_file_close( kfile) ;

				operate_custom_cache( file , key , value , 1 /* remove */) ;

			#ifdef  DEBUG
				if( ! is_changed)
				{
					kik_debug_printf( KIK_DEBUG_TAG " Not changed.") ;
				}
				else
				{
					kik_debug_printf( KIK_DEBUG_TAG " create_value failed.") ;
				}
				kik_msg_printf( " => Not saved.\n") ;
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

	p = kik_str_alloca_dup( value) ;
	
	/* Remove from custom_cache */
	operate_custom_cache( file , key , value , 1 /* remove */) ;
	free( new_value) ;

	if( p)
	{
		int  ret ;
		
		ret = write_conf( path , key , p) ;
		free( path) ;

		return  ret ;
	}
	else
	{
	#ifdef  DEBUG
		kik_debug_printf( KIK_DEBUG_TAG
			" kik_str_alloca_dup failed and configuration is not written.\n") ;
	#endif
	
		return  0 ;
	}
}

/*
 * <Return value>
 * 1: Valid "%d" or no '%' is found.
 * 0: Invalid '%' is found.
 */
static int
is_valid_default_font_format(
	const char *  format
	)
{
	char *  p ;

	if( ( p = strchr( format, '%')))
	{
		/* force to be '%d' */
		if( p[1] != 'd')
		{
			return  0 ;
		}

		/* '%' can happen only once at most */
		if( p != strrchr( format, '%'))
		{
			return  0 ;
		}
	}
	
	return  1 ;
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
				/* '>= XFT' means XFT or Cairo */
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

static x_font_config_t *
create_shared_font_config(
	x_type_engine_t  type_engine ,
	x_font_present_t  font_present
	)
{
	int  count ;
	
	for( count = 0 ; count < num_of_configs ; count ++)
	{
		if( ( type_engine == TYPE_XCORE ? font_configs[count]->type_engine == TYPE_XCORE :
						/* '>= XFT' means XFT or Cairo */
						font_configs[count]->type_engine >= TYPE_XFT) &&
		    ( (font_configs[count]->font_present & ~FONT_AA) == (font_present & ~FONT_AA)))
		{
		#ifdef  __DEBUG
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
		kik_warn_printf( KIK_DEBUG_TAG
			" max_font_size %d should be larger than min_font_size %d\n" ,
			max_fsize , min_fsize) ;
	#endif

		return  0 ;
	}

	min_font_size = min_fsize ;
	max_font_size = max_fsize ;

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

	KIK_TESTIT_ONCE(font_config, ()) ;

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
	u_int  count ;
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
				font_configs[count]->type_engine == TYPE_XCORE :
				/* '>= XFT' means XFT or Cairo */
				font_configs[count]->type_engine >= TYPE_XFT) &&
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
	#ifdef  __DEBUG
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
		font_config->default_font_name_table ,
		kik_map_hash_int , kik_map_compare_int , 16) ;

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

			fn_array = get_font_name_pairs_array( &size ,
					font_config->font_name_table[count]) ;
			
			for( __count = 0 ; __count < size ; __count ++)
			{
				free( fn_array[__count]->value) ;
			}
			
			kik_map_delete( font_config->font_name_table[count]) ;
		}
	}

	free( font_config->font_name_table) ;

	fn_array = get_font_name_pairs_array( &size , font_config->default_font_name_table) ;

	for( count = 0 ; count < size ; count ++)
	{
		free( fn_array[count]->value) ;
	}

	kik_map_delete( font_config->default_font_name_table) ;

	free( font_config) ;
	
	return  1 ;
}

/*
 * <Return value>
 * 0: Not changed(including the case of failure).
 * 1: Succeeded.
 */
int
x_customize_font_name(
	x_font_config_t *  font_config ,
	ml_font_t  font ,
	char *  fontname ,
	u_int  font_size
	)
{
	KIK_MAP( x_font_name)  map ;
	KIK_PAIR( x_font_name)  pair ;

	if( font_size < min_font_size || max_font_size < font_size)
	{
		return  0 ;
	}

	map = get_font_name_table( font_config , font_size) ;
	if( ( pair = get_font_name_pair( map , font)))
	{
		if( *fontname == '\0')
		{
			int  result ;
			
			/* Curent setting in font_config is removed. */
			free( pair->value) ;
			kik_map_erase_simple( result , map , font) ;
		}
		else if( strcmp( pair->value , fontname) != 0)
		{
			if( ( fontname = strdup( fontname)) == NULL)
			{
				return  0 ;
			}
			
			free( pair->value) ;
			pair->value = fontname ;
		}
	#if  0
		else
		{
			/* If new fontname is the same as current one, nothing is done. */
		}
	#endif
	}
	else
	{
		if( *fontname == '\0' || ( fontname = strdup( fontname)) == NULL)
		{
			return  0 ;
		}

		set_font_name_to_table( map , font , fontname) ;
	}

#ifdef  DEBUG
	kik_debug_printf( KIK_DEBUG_TAG " Set %x font size %d => fontname %s.\n" ,
		font , font_size , fontname) ;
#endif

	return  1 ;
}

/*
 * <Return value>
 * 0: Not changed(including the case of failure).
 * 1: Succeeded.
 */
int
x_customize_default_font_name(
	x_font_config_t *  font_config ,
	ml_font_t  font ,
	char *  fontname
	)
{
	KIK_PAIR( x_font_name)  pair ;

	if( is_valid_default_font_format( fontname) == 0)
	{
		kik_msg_printf( "%s is invalid format for font name.\n") ;

		return  0 ;
	}

	if( ( pair = get_font_name_pair( font_config->default_font_name_table , font)))
	{
		if( *fontname == '\0')
		{
			int  result ;
			
			/* Curent setting in font_config is removed. */
			free( pair->value) ;
			kik_map_erase_simple( result , font_config->default_font_name_table , font) ;
		}
		else if( strcmp( pair->value , fontname) != 0)
		{
			if( ( fontname = strdup( fontname)) == NULL)
			{
				return  0 ;
			}
			
			free( pair->value) ;
			pair->value = fontname ;
		}
	#if  0
		else
		{
			/* If new fontname is the same as current one, nothing is done. */
		}
	#endif
	}
	else
	{
		if( *fontname == '\0' || ( fontname = strdup( fontname)) == NULL)
		{
			return  0 ;
		}

		set_font_name_to_table( font_config->default_font_name_table , font , fontname) ;
	}

#ifdef  DEBUG
	kik_debug_printf( KIK_DEBUG_TAG " Set %x default font => fontname %s.\n" ,
		font , fontname) ;
#endif

	return  1 ;
}

/*
 * 0 => customization is failed.
 * -1 => customization is succeeded but saving is failed.
 * 1 => succeeded.
 */
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
	u_int  count ;

	if( file == NULL || strcmp( file, FONT_FILE) == 0)
	{
		file = font_file ;
		num_of_targets = match_font_configs( targets , 6 , /* is xcore */ 1 , 0) ;
	}
	else if( strcmp( file, aafont_file + 7) == 0)
	{
		file = aafont_file ;
		num_of_targets = match_font_configs( targets , 6 , /* is not xcore */ 0 , 0) ;
	}
	else if( strcmp( file, VFONT_FILE) == 0)
	{
		file = vfont_file ;
		num_of_targets = match_font_configs( targets , 6 , /* is xcore */ 1 ,
							FONT_VAR_WIDTH) ;
	}
	else if( strcmp( file, TFONT_FILE) == 0)
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

#ifdef  __DEBUG
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
		char *  p ;
		int  ret ;
		
		if( ( p = kik_str_alloca_dup( value)) &&
			/* Overwrite custom_cache. */
			operate_custom_cache( file , key , p , 0 /* overwrite */))
		{
			ret = 1 ;
			for( count = 0 ; count < num_of_targets ; count++)
			{
				read_all_conf( targets[count] , file) ;
			}
		}
		else
		{
			ret = 0 ;
		}
		
		if( ! save_conf( file , key , value))
		{
			return  ret ? -1 : 0 ;
		}
		else
		{
			return  ret ;
		}
	}
	else
	{
		/* Overwrite custom_cache. */
		if( ! operate_custom_cache( file , key , value , 0 /* overwrite */))
		{
			return  0 ;
		}
		
		for( count = 0 ; count < num_of_targets ; count++)
		{
			read_all_conf( targets[count] , file) ;
		}
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
	KIK_MAP( x_font_name)  map ;
	KIK_PAIR( x_font_name)  pair ;
	char *  font_name ;
	char *  encoding ;
	size_t  encoding_len ;
	int  has_percentd ;
#if ! defined(USE_WIN32GUI) && ! defined(USE_FRAMEBUFFER)
	static char *  orig_style[] = { "-medium-" , "-r-" , "-medium-r-" } ;
	static char *  new_style[] = { "-bold-" , "-i-" , "-bold-i-" } ;
#endif

	if( font_size < min_font_size || max_font_size < font_size)
	{
		return  NULL ;
	}

	map = get_font_name_table( font_config , font_size) ;

	if( HAS_UNICODE_AREA(font))
	{
		font &= ~FONT_FULLWIDTH ;
	}

	if( ( pair = get_font_name_pair( map , font)))
	{
	#if  1
		if( *(pair->value) == '&' &&
		    ( font = parse_key( pair->value + 1)) != UNKNOWN_CS)
		{
			/*
			 * XXX (Undocumented)
			 *
			 * JISX0213_2000_1 = &JISX0208_1983 in font configuration files.
			 * => try to get a font name of JISX0208_1983 instead of
			 *    JISX0213_2000_1 recursively.
			 */
			return  x_get_config_font_name( font_config , font_size , font) ;
		}
	#endif

		return  strdup( pair->value) ;
	}
	else
	{
	#if ! defined(USE_WIN32GUI) && ! defined(USE_FRAMEBUFFER)
		int  idx ;

		if( font_config->type_engine == TYPE_XCORE &&
		    ( idx = FONT_STYLE_INDEX(font)) >= 0 &&
		    ( pair = get_font_name_pair( map , font & ~(FONT_BOLD|FONT_ITALIC))) &&
		    ( font_name = kik_str_replace( pair->value ,
					orig_style[idx] , new_style[idx])))
		{
		#ifdef  DEBUG
			kik_debug_printf( KIK_DEBUG_TAG "Set font %s for %x\n" , font_name , font) ;
		#endif

			set_font_name_to_table( map , font , font_name) ;

			if( ( pair = get_font_name_pair( map , font)))
			{
				return  strdup( pair->value) ;
			}
		}
	#endif
	}

	encoding = NULL ;
	if( ( pair = get_font_name_pair( font_config->default_font_name_table , font)) == NULL)
	{
		while( ! ( pair = get_font_name_pair( map ,
					DEFAULT_FONT | (font & (FONT_BOLD|FONT_ITALIC)))) &&
		       ! ( pair = get_font_name_pair( font_config->default_font_name_table ,
					DEFAULT_FONT | (font & (FONT_BOLD|FONT_ITALIC)))))
		{
		#if ! defined(USE_WIN32GUI) && ! defined(USE_FRAMEBUFFER)
			int  idx ;

			if( font_config->type_engine == TYPE_XCORE &&
			    ( idx = FONT_STYLE_INDEX(font)) >= 0 &&
			    ( ( ( pair = get_font_name_pair( map , DEFAULT_FONT)) &&
			        ( font_name = kik_str_replace( pair->value ,
						orig_style[idx] , new_style[idx]))) ||
			      ( ( pair = get_font_name_pair(
						font_config->default_font_name_table ,
						DEFAULT_FONT)) &&
				( font_name = kik_str_replace( pair->value ,
						orig_style[idx] , new_style[idx])))) )
			{
			#ifdef  DEBUG
				kik_debug_printf( KIK_DEBUG_TAG
					"Set default font %s for %x\n" , font_name , font) ;
			#endif

				set_font_name_to_table( map ,
					DEFAULT_FONT | (font & (FONT_BOLD|FONT_ITALIC)) ,
					font_name) ;
			}
			else
		#endif
			{
				return  NULL ;
			}
		}

	#if ! defined(USE_WIN32GUI) && ! defined(USE_FRAMEBUFFER)
		if( font_config->type_engine == TYPE_XCORE &&
		    /* encoding is appended if font_name is XLFD (not alias name). */
		    ( strchr( pair->value , '*') || strchr( pair->value , '-')))
		{
			char **  names ;

			if( ! ( names = x_font_get_encoding_names( FONT_CS(font))) ||
			    ! names[0])
			{
				return  NULL ;
			}

			encoding = names[0] ;
		}
	#endif
	}

#if  1
	if( *(pair->value) == '&' &&
	    /* XXX font variable is overwritten. */
	    ( font = parse_key( pair->value + 1)) != UNKNOWN_CS)
	{
		/*
		 * XXX (Undocumented)
		 *
		 * JISX0213_2000_1 = &JISX0208_1983 in font configuration files.
		 * => try to get a font name of JISX0208_1983 instead of
		 *    JISX0213_2000_1 recursively.
		 */
		return  x_get_config_font_name( font_config , font_size , font) ;
	}
#endif

	/*
	 * If pair->value is valid format or not is checked by is_valid_default_font_format()
	 * in x_customize_default_font_name().
	 * So all you have to do here is strchr( ... , '%') alone.
	 */
	if( strchr( pair->value , '%'))
	{
		has_percentd = 1 ;
	}
	else if( encoding == NULL)
	{
		return  strdup( pair->value) ;
	}
	else
	{
		has_percentd = 0 ;
	}

	if( ! ( font_name = malloc( strlen( pair->value) +
				/* -2 is for "%d" */
				(has_percentd ? DIGIT_STR_LEN(font_size) - 2 : 0) +
				(encoding_len = encoding ? strlen(encoding) : 0) + 1)))
	{
		return  NULL ;
	}

	if( has_percentd)
	{
		sprintf( font_name , pair->value , font_size) ;
	}
	else
	{
		strcpy( font_name , pair->value) ;
	}

	if( encoding)
	{
		char *  percent ;

		if( ( percent = strchr( font_name , ':')))
		{
			/* -*-:200 -> -*-iso8859-1:200 */

			memmove( percent + encoding_len , percent , strlen( percent) + 1) ;
			memcpy( percent , encoding , encoding_len) ;
		}
		else
		{
			strcat( font_name , encoding) ;
		}
	}

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

	if( file == NULL || strcmp( file, FONT_FILE) == 0)
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
	else if( strcmp( file, VFONT_FILE) == 0)
	{
		engine = TYPE_XCORE ;
		present = FONT_VAR_WIDTH ;
	}
	else if( strcmp( file, TFONT_FILE) == 0)
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
	u_int  count ;

	array = get_font_name_pairs_array( &size , get_font_name_table( font_config , font_size)) ;
	d_array = get_font_name_pairs_array( &d_size , font_config->default_font_name_table) ;

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
		/*
		 * XXX
		 * Ignore DEFAULT_FONT setting because it doesn't have encoding name.
		 */
		if( FONT_CS(array[count]->key) != DEFAULT_FONT)
		{
			strcpy( p , array[count]->value) ;
			p += strlen( p) ;
			*(p ++) = ',' ;
		}
	}

	for( count = 0 ; count < d_size ; count ++)
	{
		/*
		 * XXX
		 * Ignore DEFAULT_FONT setting because it doesn't have encoding name.
		 */
		if( FONT_CS(d_array[count]->key) != DEFAULT_FONT)
		{
			sprintf( p , d_array[count]->value , font_size) ;
			p += strlen( p) ;
			*(p ++) = ',' ;
		}
	}

	if( p > font_name_list)
	{
		--p ;
	}
	*p = '\0' ;

#ifdef  DEBUG
	kik_debug_printf( KIK_DEBUG_TAG " Font list is %s\n" , font_name_list) ;
#endif

	return  font_name_list ;
}

char *
x_get_charset_name(
	mkf_charset_t  cs
	)
{
	int  count ;

	for( count = 0 ; count < sizeof(cs_table) / sizeof(cs_table[0]) ; count++)
	{
		if( cs_table[count].cs == cs)
		{
			return  cs_table[count].name ;
		}
	}

	return  NULL ;
}


#ifdef  KIK_DEBUG

#include  <assert.h>

static void
TEST_write_conf(void)
{
	struct
	{
		char *  data1 ;
		char *  data2 ;
		
	} data[] =
	{
		{ ";11,a11" , "11,a11" } ,
		{ ";10,;11,a11" , "11,a11" } ,
		{ ";;12,;12,;11,a11" , "11,a11" } ,
		{ ";12,a12" , "12,a12" } ,
	} ;
	int  count ;

	for( count = 0 ; count < sizeof(data)  / sizeof(data[0]) ; count++)
	{
		char *  p = kik_str_alloca_dup( data[count].data1) ;
		write_conf( "" , "" , p) ;
		if( strcmp( p , data[count].data2) != 0)
		{
			kik_debug_printf( KIK_DEBUG_TAG " Test failed(write conf(%s) => %s).\n" ,
				data[count].data1 , p) ;
			abort() ;
		}
	}
}

static void
TEST_create_value(void)
{
	struct
	{
		char *  data1 ;
		char *  data2 ;
		char *  ow_result  ;
		int  ow_changed ;
		char *  rm_result ;
		int  rm_changed ;
		
	} data[] =
	{
		{ "a10;12,a12;14,a14" , "12,a13" , "a10;12,a13;14,a14;" , 1 , "a10;14,a14;" , 1 } ,
		{ "a10;12,a12;14,a14" , "a12" , "a12;12,a12;14,a14;" , 1 , "12,a12;14,a14;" , 1 } ,
		{ "a10;a12;a13" , "a14" , "a14;" , 1 , "" , 1 } ,
		{ "a10;12,a12;12,a14" , "12,a16" , "a10;12,a16;" , 1 , "a10;" , 1 } ,
		{ "12,a12" , "14,a14" , "14,a14;12,a12;" , 1 , "12,a12;" , 0 } ,
		{ "a10" , "12,a12" , "a10;12,a12;" , 1 , "a10;" , 0 } ,
		{ "a10;a11;a12" , "a13" , "a13;" , 1 , "" , 1 } ,
		{ "10,a10;10,a12" , "11,a12" , "11,a12;10,a10;10,a12;" , 1 , "10,a10;10,a12;" , 0 } ,
		{ "10,a10;10,a12" , "10,a14" , "10,a14;" , 1 , "" , 1 } ,
		{ "10,a10;10,a12;11,a11" , "10,a14" , "10,a14;11,a11;" , 1 , "11,a11;" , 1 } ,
		{ "a10" , "" , ";" , 1 , "" , 1 } ,
		{ NULL , ",a14;" , NULL , 0 , NULL , 0 } ,
		{ "" , ";;;a14" , "a14;" , 1 , "" , 1 /* XXX It can't be helped... */ } ,
		{ NULL , ";;;a14" , "a14;;;;" /* XXX It can't be helped... */ , 1 , "" , 0 } ,
	} ;
	int  count ;

	for( count = 0 ; count < sizeof(data)  / sizeof(data[0]) ; count++)
	{
		char * ret ;
		int  is_changed ;
		
		ret = create_value( &is_changed ,
			kik_str_alloca_dup( data[count].data2) ,
			data[count].data1 ? kik_str_alloca_dup( data[count].data1) : NULL , 0) ;

		if( ret == NULL || data[count].ow_result == NULL)
		{
			if( ret != data[count].ow_result)
			{
				kik_debug_printf( KIK_DEBUG_TAG
					" Test failed(create_value(%s + %s) => %s).\n" ,
					data[count].data1 , data[count].data2 , ret) ;
				abort() ;
			}
		}
		else if( strcmp( data[count].ow_result , ret) != 0)
		{
			kik_debug_printf( KIK_DEBUG_TAG
				" Test failed(create_value(%s + %s) => %s).\n" ,
				data[count].data1 , data[count].data2 , ret) ;
			abort() ;
		}
		else if( is_changed != data[count].ow_changed)
		{
			kik_debug_printf( KIK_DEBUG_TAG
				" Test failed(create_value(%s + %s) => %s : %s ?).\n" ,
				data[count].data1 , data[count].data2 , ret ,
				is_changed ? "is changed" : "is not changed") ;
			abort() ;
		}

		free( ret) ;
		
		ret = create_value( &is_changed ,
			kik_str_alloca_dup( data[count].data2) ,
			data[count].data1 ? kik_str_alloca_dup( data[count].data1) : NULL , 1) ;

		if( ret == NULL || data[count].ow_result == NULL)
		{
			if( ret != data[count].ow_result)
			{
				kik_debug_printf( KIK_DEBUG_TAG
					" Test failed(create_value(%s - %s) => %s).\n" ,
					data[count].data1 , data[count].data2 , ret) ;
				abort() ;
			}
		}
		else if( strcmp( data[count].rm_result , ret) != 0)
		{
			kik_debug_printf( KIK_DEBUG_TAG
				" Test failed(create_value(%s - %s) => %s).\n" ,
				data[count].data1 , data[count].data2 , ret) ;
			abort() ;
		}
		else if( is_changed != data[count].rm_changed)
		{
			kik_debug_printf( KIK_DEBUG_TAG
				" Test failed(create_value(%s - %s) => %s : %s ?).\n" ,
				data[count].data1 , data[count].data2 , ret ,
				is_changed ? "is changed" : "is not changed") ;
			abort() ;
		}

		free( ret) ;
	}
}

static void
TEST_font_config(void)
{
#if ! defined(USE_FRAMEBUFFER) && ! defined(USE_WIN32GUI)
	x_font_config_t *  font_config ;
	char *  value ;

	font_config = x_font_config_new( TYPE_XCORE , 0) ;
	x_customize_font_name( font_config , ISO8859_1_R , "-hoge-medium-r-fuga-" , 12) ;

	value = x_get_config_font_name( font_config , 12 , ISO8859_1_R|FONT_BOLD) ;
	assert( strcmp( "-hoge-bold-r-fuga-" , value) == 0) ;
	free( value) ;

	value = x_get_config_font_name( font_config , 12 , ISO8859_1_R|FONT_ITALIC) ;
	assert( strcmp( "-hoge-medium-i-fuga-" , value) == 0) ;
	free( value) ;

	value = x_get_config_font_name( font_config , 12 , ISO8859_1_R|FONT_BOLD|FONT_ITALIC) ;
	assert( strcmp( "-hoge-bold-i-fuga-" , value) == 0) ;
	free( value) ;

	x_font_config_delete( font_config) ;

	TEST_create_value() ;
	TEST_write_conf() ;
#endif
}

#endif
