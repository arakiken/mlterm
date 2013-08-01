/*
 *	$Id$
 */

#include  "x_font_manager.h"

#include  <string.h>		/* strcat */
#include  <stdio.h>		/* sprintf */
#include  <kiklib/kik_mem.h>	/* malloc/alloca */
#include  <kiklib/kik_util.h>	/* DIGIT_STR_LEN */


#if  0
#define  __DEBUG
#endif


typedef struct encoding_to_cs_table
{
	ml_char_encoding_t  encoding ;
	mkf_charset_t  cs ;

} encoding_to_cs_table_t ;


/* --- static variables --- */

/*
 * !!! Notice !!!
 * The order should be the same as ml_char_encoding_t.
 * US_ASCII font for encodings after ML_UTF8 is ISO8859_1_R. (see x_get_usascii_font_cs())
 */
static encoding_to_cs_table_t  usascii_font_cs_table[] =
{
	{ ML_ISO8859_1 , ISO8859_1_R } ,
	{ ML_ISO8859_2 , ISO8859_2_R } ,
	{ ML_ISO8859_3 , ISO8859_3_R } ,
	{ ML_ISO8859_4 , ISO8859_4_R } ,
	{ ML_ISO8859_5 , ISO8859_5_R } ,
	{ ML_ISO8859_6 , ISO8859_6_R } ,
	{ ML_ISO8859_7 , ISO8859_7_R } ,
	{ ML_ISO8859_8 , ISO8859_8_R } ,
	{ ML_ISO8859_9 , ISO8859_9_R } ,
	{ ML_ISO8859_10 , ISO8859_10_R } ,
	{ ML_TIS620 , TIS620_2533 } ,
	{ ML_ISO8859_13 , ISO8859_13_R } ,
	{ ML_ISO8859_14 , ISO8859_14_R } ,
	{ ML_ISO8859_15 , ISO8859_15_R } ,
	{ ML_ISO8859_16 , ISO8859_16_R } ,
	{ ML_TCVN5712 , TCVN5712_3_1993 } ,
	
	{ ML_ISCII_ASSAMESE , ISO8859_1_R } ,
	{ ML_ISCII_BENGALI , ISO8859_1_R } ,
	{ ML_ISCII_GUJARATI , ISO8859_1_R } ,
	{ ML_ISCII_HINDI , ISO8859_1_R } ,
	{ ML_ISCII_KANNADA , ISO8859_1_R } ,
	{ ML_ISCII_MALAYALAM , ISO8859_1_R } ,
	{ ML_ISCII_ORIYA , ISO8859_1_R } ,
	{ ML_ISCII_PUNJABI , ISO8859_1_R } ,
	{ ML_ISCII_ROMAN , ISO8859_1_R } ,
	{ ML_ISCII_TAMIL , ISO8859_1_R } ,
	{ ML_ISCII_TELUGU , ISO8859_1_R } ,
	{ ML_VISCII , VISCII } ,
	{ ML_KOI8_R , KOI8_R } ,
	{ ML_KOI8_U , KOI8_U } ,
	{ ML_KOI8_T , KOI8_T } ,
	{ ML_GEORGIAN_PS , GEORGIAN_PS } ,
	{ ML_CP1250 , CP1250 } ,
	{ ML_CP1251 , CP1251 } ,
	{ ML_CP1252 , CP1252 } ,
	{ ML_CP1253 , CP1253 } ,
	{ ML_CP1254 , CP1254 } ,
	{ ML_CP1255 , CP1255 } ,
	{ ML_CP1256 , CP1256 } ,
	{ ML_CP1257 , CP1257 } ,
	{ ML_CP1258 , CP1258 } ,
	{ ML_CP874 , CP874 } ,

	{ ML_UTF8 , ISO10646_UCS4_1 } ,
	
} ;


/* --- static functions --- */

static int
change_font_cache(
	x_font_manager_t *  font_man ,
	x_font_cache_t *  font_cache
	)
{
	x_release_font_cache( font_man->font_cache) ;
	font_man->font_cache = font_cache ;

	return  1 ;
}


/* --- global functions --- */

x_font_manager_t *
x_font_manager_new(
	Display *  display ,
	x_type_engine_t  type_engine ,
	x_font_present_t  font_present ,
	u_int  font_size ,
	mkf_charset_t  usascii_font_cs ,
	int  use_multi_col_char ,
	u_int  step_in_changing_font_size ,
	u_int  letter_space ,
	int  use_bold_font
	)
{
	x_font_manager_t *  font_man ;
	
	if( ! ( font_man = malloc( sizeof( x_font_manager_t))))
	{
	#ifdef  DEBUG
		kik_warn_printf( KIK_DEBUG_TAG " malloc() failed.\n") ;
	#endif
	
		return  NULL ;
	}

	if( ( ! ( font_man->font_config = x_acquire_font_config( type_engine , font_present)) ||
	      ! ( font_man->font_cache = x_acquire_font_cache( display , font_size ,
						usascii_font_cs , font_man->font_config ,
						use_multi_col_char , letter_space))))
	{
		x_type_engine_t  engine ;

	#ifdef  DEBUG
		kik_debug_printf( KIK_DEBUG_TAG " Could not handle %s fonts.\n" ,
			x_get_type_engine_name( type_engine)) ;
	#endif

		for( engine = TYPE_XCORE ; ; engine ++)
		{
			if( engine == type_engine)
			{
				continue ;
			}

			if( font_man->font_config)
			{
				x_release_font_config( font_man->font_config) ;
			}

			if( engine >= TYPE_ENGINE_MAX)
			{
				free( font_man) ;

				return  NULL ;
			}

			if( ( font_man->font_config = x_acquire_font_config(
								engine , font_present)) &&
			    ( font_man->font_cache = x_acquire_font_cache( display , font_size ,
						usascii_font_cs , font_man->font_config ,
						use_multi_col_char , letter_space)) )
			{
				break ;
			}
		}

		kik_msg_printf( "Fall back to %s.\n" , x_get_type_engine_name( engine)) ;
	}

	if(  x_get_max_font_size() - x_get_min_font_size() >= step_in_changing_font_size)
	{
		font_man->step_in_changing_font_size = step_in_changing_font_size ;
	}
	else
	{
		 font_man->step_in_changing_font_size = x_get_max_font_size() - x_get_min_font_size() ;
	}

	font_man->use_bold_font = use_bold_font ;

	return  font_man ;
}

int
x_font_manager_delete(
	x_font_manager_t *  font_man
	)
{
	x_release_font_cache( font_man->font_cache) ;

	x_release_font_config( font_man->font_config) ;
	font_man->font_config = NULL ;
	
	free( font_man) ;

	return  1 ;
}

x_font_t *
x_get_font(
	x_font_manager_t *  font_man ,
	ml_font_t  font
	)
{
	x_font_t *  xfont ;

	if( ! font_man->use_bold_font)
	{
		font &= ~FONT_BOLD ;
	}

	if( ( xfont = x_font_cache_get_xfont( font_man->font_cache , font)))
	{
		return  xfont ;
	}
	else
	{
		return  font_man->font_cache->usascii_font ;
	}
}

int
x_font_manager_usascii_font_cs_changed(
	x_font_manager_t *  font_man ,
	mkf_charset_t  usascii_font_cs
	)
{
	x_font_cache_t *  font_cache ;

	if( usascii_font_cs == font_man->font_cache->usascii_font_cs)
	{
		return  0 ;
	}

	if( ( font_cache = x_acquire_font_cache( font_man->font_cache->display ,
				font_man->font_cache->font_size , usascii_font_cs ,
				font_man->font_config ,
				font_man->font_cache->use_multi_col_char ,
				font_man->font_cache->letter_space)) == NULL)
	{
		return  0 ;
	}

	change_font_cache( font_man , font_cache) ;

	return  1 ;
}

/*
 * Return 1 if font present is successfully changed.
 * Return 0 if not changed.
 */
int
x_change_font_present(
	x_font_manager_t *  font_man ,
	x_type_engine_t  type_engine ,
	x_font_present_t  font_present
	)
{
	x_font_config_t *  font_config ;
	x_font_cache_t *  font_cache ;

#if ! defined(USE_WIN32GUI) && ! defined(USE_FRAMEBUFFER)
	/*
	 * FONT_AA is effective in xft, so following hack is necessary in xlib.
	 */
	if( ( type_engine == TYPE_XCORE) && ( font_man->font_config->font_present & FONT_AA))
	{
		font_present &= ~FONT_AA ;
	}
	else if( ( font_present & FONT_AA) &&
		font_man->font_config->type_engine == TYPE_XCORE && type_engine == TYPE_XCORE)
	{
	#if  ! defined(USE_TYPE_XFT) && defined(USE_TYPE_CAIRO)
		type_engine = TYPE_CAIRO ;
	#else
		type_engine = TYPE_XFT ;
	#endif
	}
#endif

	if( font_present == font_man->font_config->font_present &&
		type_engine == font_man->font_config->type_engine)
	{
		/* Same as current settings. */
		
		return  0 ;
	}

	if( ( font_config = x_acquire_font_config( type_engine , font_present)) == NULL)
	{
		return  0 ;
	}

	if( ( font_cache = x_acquire_font_cache( font_man->font_cache->display ,
				font_man->font_cache->font_size ,
				font_man->font_cache->usascii_font_cs ,
				font_config , font_man->font_cache->use_multi_col_char ,
				font_man->font_cache->letter_space)) == NULL)
	{
		x_release_font_config( font_config) ;

		return  0 ;
	}

	change_font_cache( font_man , font_cache) ;

	x_release_font_config( font_man->font_config) ;
	font_man->font_config = font_config ;

	return  1 ;
}

x_type_engine_t
x_get_type_engine(
	x_font_manager_t *  font_man
	)
{
	return  font_man->font_config->type_engine ;
}

x_font_present_t
x_get_font_present(
	x_font_manager_t *  font_man
	)
{
	return  font_man->font_config->font_present ;
}

int
x_change_font_size(
	x_font_manager_t *  font_man ,
	u_int  font_size
	)
{
	x_font_cache_t *  font_cache ;
		
	if( font_size == font_man->font_cache->font_size)
	{
		/* not changed (pretending to succeed) */
		
		return  1 ;
	}

	if( font_size < x_get_min_font_size() || x_get_max_font_size() < font_size)
	{
		return  0 ;
	}

	if( ( font_cache = x_acquire_font_cache( font_man->font_cache->display ,
				font_size , font_man->font_cache->usascii_font_cs , 
				font_man->font_config ,
				font_man->font_cache->use_multi_col_char ,
				font_man->font_cache->letter_space)) == NULL)
	{
		return  0 ;
	}

	change_font_cache( font_man , font_cache) ;

	return  1 ;
}

int
x_larger_font(
	x_font_manager_t *  font_man
	)
{
	u_int  font_size ;
	x_font_cache_t *  font_cache ;

	if( font_man->font_cache->font_size + font_man->step_in_changing_font_size > x_get_max_font_size())
	{
		font_size = x_get_min_font_size() ;
	}
	else
	{
		font_size = font_man->font_cache->font_size + font_man->step_in_changing_font_size ;
	}
	
	if( ( font_cache = x_acquire_font_cache( font_man->font_cache->display ,
				font_size , font_man->font_cache->usascii_font_cs , 
				font_man->font_config ,
				font_man->font_cache->use_multi_col_char ,
				font_man->font_cache->letter_space)) == NULL)
	{
		return  0 ;
	}
	
	change_font_cache( font_man , font_cache) ;

	return  1 ;
}

int
x_smaller_font(
	x_font_manager_t *  font_man
	)
{
	u_int  font_size ;
	x_font_cache_t *  font_cache ;

	if( font_man->font_cache->font_size < x_get_min_font_size() + font_man->step_in_changing_font_size)
	{
		font_size = x_get_max_font_size() ;
	}
	else
	{
		font_size = font_man->font_cache->font_size - font_man->step_in_changing_font_size ;
	}
	
	if( ( font_cache = x_acquire_font_cache( font_man->font_cache->display ,
				font_size , font_man->font_cache->usascii_font_cs , 
				font_man->font_config ,
				font_man->font_cache->use_multi_col_char ,
				font_man->font_cache->letter_space)) == NULL)
	{
		return  0 ;
	}

	change_font_cache( font_man , font_cache) ;

	return  1 ;
}

u_int
x_get_font_size(
	x_font_manager_t *  font_man
	)
{
	return  font_man->font_cache->font_size ;
}

int
x_set_use_multi_col_char(
	x_font_manager_t *  font_man ,
	int  flag
	)
{
	x_font_cache_t *  font_cache ;

	if( font_man->font_cache->use_multi_col_char == flag)
	{
		return  0 ;
	}

	if( ( font_cache = x_acquire_font_cache( font_man->font_cache->display ,
				font_man->font_cache->font_size ,
				font_man->font_cache->usascii_font_cs ,
				font_man->font_config , flag ,
				font_man->font_cache->letter_space)) == NULL)
	{
		return  0 ;
	}

	change_font_cache( font_man , font_cache) ;

	return  1 ;
}

int
x_set_letter_space(
	x_font_manager_t *  font_man ,
	u_int  letter_space
	)
{
	x_font_cache_t *  font_cache ;

	if( font_man->font_cache->letter_space == letter_space)
	{
		return  0 ;
	}

	if( ( font_cache = x_acquire_font_cache( font_man->font_cache->display ,
				font_man->font_cache->font_size ,
				font_man->font_cache->usascii_font_cs ,
				font_man->font_config , font_man->font_cache->use_multi_col_char ,
				letter_space)) == NULL)
	{
		return  0 ;
	}

	change_font_cache( font_man , font_cache) ;

	return  1 ;
}

int
x_set_use_bold_font(
	x_font_manager_t *  font_man ,
	int  use_bold_font
	)
{
	if( font_man->use_bold_font == use_bold_font)
	{
		return  0 ;
	}

	font_man->use_bold_font = use_bold_font ;

	return  1 ;
}

XFontSet
x_get_fontset(
	x_font_manager_t *  font_man
	)
{
#if  defined(USE_FRAMEBUFFER)

	return  None ;

#elif  defined(USE_WIN32GUI)

	static LOGFONT  logfont ;

	ZeroMemory( &logfont , sizeof(logfont)) ;
	GetObject( font_man->font_cache->usascii_font->fid , sizeof(logfont) , &logfont) ;

	return  &logfont ;

#else

	XFontSet  fontset ;
	char *  list_str ;
	char **  missing ;
	int  miss_num ;
	char *  def_str ;

	if( ( list_str = x_get_font_name_list_for_fontset( font_man->font_cache)) == NULL)
	{
		return  None ;
	}
	
#ifdef  __DEBUG
	kik_debug_printf( KIK_DEBUG_TAG " font set list -> %s\n" , list_str) ;
#endif

	fontset = XCreateFontSet( font_man->font_cache->display , list_str ,
			&missing , &miss_num , &def_str) ;

	free( list_str) ;
	
#ifdef  DEBUG
	if( miss_num)
	{
		int  count ;
		
		kik_warn_printf( KIK_DEBUG_TAG " missing charsets ...\n") ;
		for( count = 0 ; count < miss_num ; count ++)
		{
			kik_msg_printf( " %s\n" , missing[count]) ;
		}
	}
#endif

	XFreeStringList( missing) ;

	return  fontset ;
#endif
}

mkf_charset_t
x_get_usascii_font_cs(
	ml_char_encoding_t  encoding
	)
{
	if( encoding < 0 ||
		sizeof( usascii_font_cs_table) / sizeof( usascii_font_cs_table[0]) <= encoding)
	{
		return  ISO8859_1_R ;
	}
#ifdef  DEBUG
	else if( encoding != usascii_font_cs_table[encoding].encoding)
	{
		kik_warn_printf( KIK_DEBUG_TAG " %x is illegal encoding.\n" , encoding) ;

		return  ISO8859_1_R ;
	}
#endif
	else
	{
	#ifdef  __DEBUG
		kik_debug_printf( KIK_DEBUG_TAG " us ascii font is %x cs\n" ,
			usascii_font_cs_table[encoding].cs) ;
	#endif
	
		return  usascii_font_cs_table[encoding].cs ;
	}
}
