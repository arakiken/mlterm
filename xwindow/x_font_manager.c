/*
 *	$Id$
 */

#include  "x_font_manager.h"

#include  <string.h>		/* strcat */
#include  <stdio.h>		/* sprintf */
#include  <kiklib/kik_mem.h>	/* malloc/alloca */
#include  <kiklib/kik_str.h>	/* kik_str_dup */
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
 * the order should be the same as ml_char_encoding_t.
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
	
	{ ML_ISCII , ISO8859_1_R } ,
	{ ML_VISCII , VISCII } ,
	{ ML_KOI8_R , KOI8_R } ,
	{ ML_KOI8_U , KOI8_U } ,
	{ ML_UTF8 , ISO10646_UCS4_1 } ,
	
} ;


/* --- static functions --- */

#ifdef  DEBUG

static void
dump_cached_fonts(
	x_font_manager_t *  font_man
	)
{
	int  count ;
	u_int  size ;
	KIK_PAIR( x_font) *  f_array ;
	
	kik_warn_printf( KIK_DEBUG_TAG " cached fonts info\n") ;
	kik_msg_printf( " usascii:\n") ;
	x_font_dump( font_man->usascii_font) ;

	kik_msg_printf( " others:\n") ;
	kik_map_get_pairs_array( font_man->font_cache_table , f_array , size) ;
	for( count = 0 ; count < size ; count++)
	{
		if( f_array[count]->value != NULL)
		{
			x_font_dump( f_array[count]->value) ;
		}
	}
}

#endif

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

static int
is_var_col_width(
	x_font_manager_t *  font_man
	)
{
	return  (font_man->font_custom == font_man->v_font_custom) ||
		(font_man->font_custom == font_man->vaa_font_custom) ;
}

static int
is_aa(
	x_font_manager_t *  font_man
	)
{
	return  (font_man->font_custom == font_man->aa_font_custom) ||
		(font_man->font_custom == font_man->vaa_font_custom) ||
		(font_man->font_custom == font_man->taa_font_custom) ;
}

static int
is_vertical(
	x_font_manager_t *  font_man
	)
{
	return  (font_man->font_custom == font_man->t_font_custom) ||
		(font_man->font_custom == font_man->taa_font_custom) ;
}

static char *
get_font_name(
	x_font_manager_t *  font_man ,
	ml_font_t  id
	)
{
	char *  font_name ;
	
	if( font_man->local_font_custom &&
		( font_name = x_get_font_name( font_man->local_font_custom ,
				font_man->font_size , id)))
	{
		return  font_name ;
	}

	if( ( font_name = x_get_font_name( font_man->font_custom ,
				font_man->font_size , id)))
	{
		return  font_name ;
	}

	if( is_var_col_width( font_man) || is_vertical( font_man))
	{
		/*
		 * searching
		 *  ~/.mlterm/vfont -> ~/.mlterm/font
		 *  ~/.mlterm/tfont -> ~/.mlterm/font
		 *  ~/.mlterm/vaafont -> ~/.mlterm/aafont
		 *  ~/.mlterm/taafont -> ~/.mlterm/aafont
		 */
		if( is_aa( font_man))
		{
			if( font_man->aa_font_custom &&
				( font_name = x_get_font_name( font_man->aa_font_custom ,
					font_man->font_size , id)))
			{
				return  font_name ;
			}
		}
		else
		{
			if( ( font_name = x_get_font_name( font_man->normal_font_custom ,
					font_man->font_size , id)))
			{
				return  font_name ;
			}
		}
	}

	return  NULL ;
}

static int
set_xfont(
	x_font_manager_t *  font_man ,
	x_font_t *  xfont
	)
{
	int  use_medium_for_bold ;
	u_int  col_width ;
	char *  fontname ;
	
	if( xfont->id == DEFAULT_FONT(font_man->usascii_font_cs))
	{
		col_width = 0 ;
	}
	else
	{
		col_width = font_man->usascii_font->width ;
	}
	
	if( ( fontname = get_font_name( font_man , xfont->id)))
	{
		use_medium_for_bold = 0 ;
	}
	else
	{
		/* fontname == NULL */

		use_medium_for_bold = 0 ;

		if( xfont->id & FONT_BOLD)
		{
			if( ( fontname = get_font_name( font_man , xfont->id & ~FONT_BOLD)))
			{
				use_medium_for_bold = 1 ;
			}
		}
	}

	x_font_set_font_present( xfont , x_font_manager_get_font_present( font_man)) ;
	
	if( ! (*font_man->set_xfont)( xfont , fontname , font_man->font_size ,
		col_width , use_medium_for_bold))
	{
		return  0 ;
	}

	if( ! font_man->use_multi_col_char)
	{
		x_change_font_cols( xfont , 1) ;
	}

	return  1 ;
}

static x_font_t *
get_font(
	x_font_manager_t *  font_man ,
	ml_font_t  font
	)
{
	int  result ;
	x_font_t *  xfont ;
	KIK_PAIR( x_font)  fn_pair ;
	char *  fontname ;
	int  use_medium_for_bold ;
	u_int  col_width ;

	if( font == DEFAULT_FONT(font_man->usascii_font_cs))
	{
		col_width = 0 ;
	}
	else
	{
		col_width = font_man->usascii_font->width ;
	}

	kik_map_get( result , font_man->font_cache_table , font , fn_pair) ;
	if( result)
	{
		return  fn_pair->value ;
	}

	if( ( xfont = x_font_new( font_man->display , font)) == NULL)
	{
		return  NULL ;
	}

	if( ( fontname = get_font_name( font_man , font)))
	{
		use_medium_for_bold = 0 ;
	}
	else
	{
		/* fontname == NULL */
		
		use_medium_for_bold = 0 ;
		
		if( font & FONT_BOLD)
		{
			if( ( fontname = get_font_name( font_man , font & ~FONT_BOLD)))
			{
				use_medium_for_bold = 1 ;
			}
		}
	}

	x_font_set_font_present( xfont , x_font_manager_get_font_present( font_man)) ;

	if( ! (*font_man->set_xfont)( xfont , fontname , font_man->font_size ,
		col_width , use_medium_for_bold))
	{
	#ifdef  DEBUG
		kik_warn_printf( KIK_DEBUG_TAG " font for id %x doesn't exist.\n" , font) ;
	#endif
	
		x_font_delete( xfont) ;

		/*
		 * this font doesn't exist , NULL is set.
		 * caching the fact that this font id doesn't exist.
		 */
		xfont = NULL ;
	}
	
	if( ! font_man->use_multi_col_char)
	{
		x_change_font_cols( xfont , 1) ;
	}
	
	kik_map_set( result , font_man->font_cache_table , font , xfont) ;

#ifdef  DEBUG
	kik_warn_printf( KIK_DEBUG_TAG " font for id %x was cached.\n" , font) ;
#endif

	return  xfont ;
}

static int
set_font_present(
	x_font_manager_t *  font_man ,
	x_font_present_t  font_present
	)
{
	if( font_present == FONT_VAR_WIDTH)
	{
		if( font_man->font_custom == font_man->v_font_custom)
		{
			return  0 ;
		}

		font_man->set_xfont = x_font_set_xfont ;
		font_man->font_custom = font_man->v_font_custom ;
	}
	else if( font_present == FONT_AA)
	{
		if( font_man->aa_font_custom == NULL ||
			font_man->font_custom == font_man->aa_font_custom)
		{
			return  0 ;
		}

		font_man->set_xfont = x_font_set_xft_font ;
		font_man->font_custom = font_man->aa_font_custom ;
	}
	else if( font_present == (FONT_AA | FONT_VAR_WIDTH))
	{
		if( font_man->vaa_font_custom == NULL ||
			font_man->font_custom == font_man->vaa_font_custom)
		{
			return  0 ;
		}

		font_man->set_xfont = x_font_set_xft_font ;
		font_man->font_custom = font_man->vaa_font_custom ;
	}
	else if( font_present == FONT_VERTICAL)
	{
		if( font_man->font_custom == font_man->t_font_custom)
		{
			return  0 ;
		}

		font_man->set_xfont = x_font_set_xfont ;
		font_man->font_custom = font_man->t_font_custom ;
	}
	else if( font_present == (FONT_AA | FONT_VERTICAL))
	{
		if( font_man->taa_font_custom == NULL ||
			font_man->font_custom == font_man->taa_font_custom)
		{
			return  0 ;
		}

		font_man->set_xfont = x_font_set_xft_font ;
		font_man->font_custom = font_man->taa_font_custom ;
	}
	else
	{
		if( font_man->font_custom == font_man->normal_font_custom)
		{
			return  0 ;
		}

		font_man->set_xfont = x_font_set_xfont ;
		font_man->font_custom = font_man->normal_font_custom ;
	}

	return  1 ;
}

static x_font_t *
search_usascii_font(
	x_font_manager_t *  font_man
	)
{
	x_font_t *  xfont ;
	
	if( ( xfont = get_font( font_man ,
			DEFAULT_FONT(font_man->usascii_font_cs))) == NULL)
	{
		u_int  beg_size ;

		beg_size = font_man->font_size ;

		while( 1)
		{
			if( ++ font_man->font_size > font_man->font_custom->max_font_size)
			{
				font_man->font_size = font_man->font_custom->min_font_size ;
			}

			if( font_man->font_size == beg_size)
			{
			#ifdef  DEBUG
				kik_warn_printf( KIK_DEBUG_TAG
					"a font for displaying us ascii chars is not found.\n") ;
			#endif
			
				return  NULL ;
			}

			if( ( xfont = get_font( font_man ,
					DEFAULT_FONT(font_man->usascii_font_cs))))
			{
				break ;
			}
		}
	}

	return  xfont ;
}


/* --- global functions --- */

x_font_manager_t *
x_font_manager_new(
	Display *  display ,
	x_font_custom_t *  normal_font_custom ,
	x_font_custom_t *  v_font_custom ,
	x_font_custom_t *  t_font_custom ,
	x_font_custom_t *  aa_font_custom ,	/* may be NULL */
	x_font_custom_t *  vaa_font_custom ,
	x_font_custom_t *  taa_font_custom ,
	x_font_present_t  font_present ,
	u_int  font_size ,
	mkf_charset_t  usascii_font_cs ,
	int  usascii_font_cs_changable ,
	int  use_multi_col_char ,
	int  step_in_changing_font_size
	)
{
	x_font_manager_t *  font_man ;
	
	if( ( font_man = malloc( sizeof( x_font_manager_t))) == NULL)
	{
	#ifdef  DEBUG
		kik_warn_printf( KIK_DEBUG_TAG " malloc() failed.\n") ;
	#endif
	
		return  NULL ;
	}

	font_man->normal_font_custom = normal_font_custom ;
	font_man->v_font_custom = v_font_custom ;
	font_man->t_font_custom = t_font_custom ;
	font_man->aa_font_custom = aa_font_custom ;
	font_man->vaa_font_custom = vaa_font_custom ;
	font_man->taa_font_custom = taa_font_custom ;

	font_man->font_custom = NULL ;	
	font_man->local_font_custom = NULL ;

	font_man->font_size = font_size ;

	kik_map_new( ml_font_t , x_font_t * , font_man->font_cache_table , 
		font_hash , font_compare) ;

	font_man->display = display ;

	font_man->prev_cache.font = 0 ;
	font_man->prev_cache.xfont = NULL ;

	font_man->usascii_font_cs = usascii_font_cs ;
	font_man->usascii_font_cs_changable = usascii_font_cs_changable ;

	font_man->use_multi_col_char = use_multi_col_char ;

	font_man->step_in_changing_font_size = step_in_changing_font_size ;

	set_font_present( font_man , font_present) ;

	if( ( font_man->usascii_font = search_usascii_font( font_man)) == NULL)
	{
	#ifdef  DEBUG
		kik_warn_printf( KIK_DEBUG_TAG " Not found usascii font. Bye...\n") ;
	#endif

		exit(1) ;
	}
	
	return  font_man ;
}

int
x_font_manager_delete(
	x_font_manager_t *  font_man
	)
{
	int  count ;
	u_int  size ;
	KIK_PAIR( x_font) *  f_array ;

	kik_map_get_pairs_array( font_man->font_cache_table , f_array , size) ;
	for( count = 0 ; count < size ; count++)
	{
		if( f_array[count]->value != NULL)
		{
			x_font_delete( f_array[count]->value) ;
		}
	}

	kik_map_delete( font_man->font_cache_table) ;

	if( font_man->local_font_custom)
	{
		x_font_custom_final( font_man->local_font_custom) ;

		free( font_man->local_font_custom) ;
	}

	free( font_man) ;

	return  1 ;
}

int
x_font_manager_change_font_present(
	x_font_manager_t *  font_man ,
	x_font_present_t  font_present
	)
{
	set_font_present( font_man , font_present) ;
	
	x_font_manager_reload( font_man) ;
	
	return  1 ;
}

x_font_present_t
x_font_manager_get_font_present(
	x_font_manager_t *  font_man
	)
{
	x_font_present_t  font_present ;

	font_present = 0 ;
	
	if( is_aa( font_man))
	{
		font_present |= FONT_AA ;
	}

	if( is_var_col_width( font_man))
	{
		font_present |= FONT_VAR_WIDTH ;
	}
	
	if( is_vertical( font_man))
	{
		font_present |= FONT_VERTICAL ;
	}

	return  font_present ;	
}

int
x_font_manager_set_local_font_name(
	x_font_manager_t *  font_man ,
	ml_font_t  font ,
	char *  font_name ,
	u_int  font_size
	)
{
	if( font_man->local_font_custom == NULL)
	{
		if( ( font_man->local_font_custom = malloc( sizeof( x_font_custom_t))) == NULL)
		{
			return  0 ;
		}

		x_font_custom_init( font_man->local_font_custom ,
			font_man->font_custom->min_font_size , font_man->font_custom->max_font_size) ;
	}

	x_set_font_name( font_man->local_font_custom , font , font_name , font_size) ;

	return  1 ;
}

x_font_t *
x_get_font(
	x_font_manager_t *  font_man ,
	ml_font_t  font
	)
{
	x_font_t *  xfont ;

	if( font_man->prev_cache.font == font)
	{
		return  font_man->prev_cache.xfont ;
	}

	if( FONT_CS(font) == US_ASCII)
	{
		font &= ~US_ASCII ;
		font |= font_man->usascii_font_cs ;
	}
	
	if( ( xfont = get_font( font_man , font)))
	{
		font_man->prev_cache.font = font ;
		font_man->prev_cache.xfont = xfont ;
		
		return  xfont ;
	}
	else
	{
		return  font_man->usascii_font ;
	}
}

x_font_t *
x_get_usascii_font(
	x_font_manager_t *  font_man
	)
{
	return  font_man->usascii_font ;
}

int
x_font_manager_reload(
	x_font_manager_t *  font_man
	)
{
	KIK_PAIR( x_font) *  array ;
	u_int  size ;
	int  count ;
	
	if( ! set_xfont( font_man , font_man->usascii_font))
	{
		return  0 ;
	}
	
	kik_map_get_pairs_array( font_man->font_cache_table , array , size) ;

	for( count = 0 ; count < size ; count ++)
	{
		ml_font_t  font ;
		x_font_t *  xfont ;

		font = array[count]->key ;
		if( (xfont = array[count]->value) == NULL)
		{
			if( ( xfont = x_font_new( font_man->display , font)) == NULL)
			{
			#ifdef  DEBUG
				kik_warn_printf( KIK_DEBUG_TAG " x_font_new() failed.\n") ;
			#endif
			
				continue ;
			}

			array[count]->value = xfont ;
		}

		if( font == DEFAULT_FONT(font_man->usascii_font_cs))
		{
			/* usascii default font is already loaded. */
			
			continue ;
		}

		if( set_xfont( font_man , xfont) == 0)
		{
		#ifdef  DEBUG
			kik_warn_printf( KIK_DEBUG_TAG " larger font for id %x is not found.\n" ,
				xfont->id) ;
		#endif

			x_font_delete( xfont) ;
			array[count]->value = NULL ;
		}
	}

#ifdef  DEBUG
	dump_cached_fonts( font_man) ;
#endif

	return  1 ;
}

int
x_font_manager_usascii_font_cs_changed(
	x_font_manager_t *  font_man ,
	mkf_charset_t  usascii_font_cs
	)
{
	mkf_charset_t  old_cs ;
	x_font_t *  font ;

	if( ! font_man->usascii_font_cs_changable || usascii_font_cs == font_man->usascii_font_cs)
	{
		return  0 ;
	}

	old_cs = font_man->usascii_font_cs ;
	font_man->usascii_font_cs = usascii_font_cs ;

	if( ( font = search_usascii_font( font_man)) == NULL)
	{
	#ifdef  DEBUG
		kik_warn_printf( KIK_DEBUG_TAG " Not found usascii font for %x cs.\n") ;
	#endif
	
		font_man->usascii_font_cs = old_cs ;

		return  0 ;
	}
	else
	{
		font_man->usascii_font = font ;

		return  1 ;
	}
}

int
x_change_font_size(
	x_font_manager_t *  font_man ,
	u_int  font_size
	)
{
	u_int  orig_font_size ;
	
	if( font_size == font_man->font_size)
	{
		/* not changed (pretending to succeed) */
		
		return  1 ;
	}

#ifdef  __DEBUG
	kik_debug_printf( KIK_DEBUG_TAG " changing font size to %d\n" , font_size) ;
#endif

	if( font_size < font_man->font_custom->min_font_size ||
		font_man->font_custom->max_font_size < font_size)
	{
	#ifdef  DEBUG
		kik_warn_printf( KIK_DEBUG_TAG " font_size %d is illegal.\n" , font_size) ;
	#endif
	
		return  0 ;
	}

	orig_font_size = font_man->font_size ;
	font_man->font_size = font_size ;

	if( ! x_font_manager_reload( font_man))
	{
		font_man->font_size = orig_font_size ;
		
		return  0 ;
	}
	else
	{
		return  1 ;
	}
}

int
x_larger_font(
	x_font_manager_t *  font_man
	)
{
	u_int  beg_size ;
	u_int  size ;

	beg_size = font_man->font_size ;
	size = font_man->font_size ;
	
	while( 1)
	{
		if( size + font_man->step_in_changing_font_size > font_man->font_custom->max_font_size)
		{
			if( font_man->font_custom->min_font_size == beg_size)
			{
				return  0 ;
			}

			size = font_man->font_custom->min_font_size ;
		}
		else
		{
			if( size < beg_size && beg_size <= size + font_man->step_in_changing_font_size)
			{
				return  0 ;
			}

			size += font_man->step_in_changing_font_size ;
		}
		
		if( x_change_font_size( font_man , size))
		{
			return  1 ;
		}
	}
}

int
x_smaller_font(
	x_font_manager_t *  font_man
	)
{
	u_int  beg_size ;
	u_int  size ;

	beg_size = font_man->font_size ;
	size = font_man->font_size ;

	while( 1)
	{
		if( size == font_man->font_custom->min_font_size)
		{
			if( font_man->font_custom->max_font_size == beg_size)
			{
				return  0 ;
			}
			
			size = font_man->font_custom->max_font_size ;
		}
		else
		{
			if( size - font_man->step_in_changing_font_size <= beg_size && beg_size < size)
			{
				return  0 ;
			}
			
			size -= font_man->step_in_changing_font_size ;
		}
		
		if( x_change_font_size( font_man , size))
		{
			return  1 ;
		}
	}
}

int
x_use_multi_col_char(
	x_font_manager_t *  font_man
	)
{
	KIK_PAIR( x_font) *  array ;
	u_int  size ;
	int  count ;

	if( font_man->use_multi_col_char)
	{
		return  0 ;
	}

	kik_map_get_pairs_array( font_man->font_cache_table , array , size) ;

	for( count = 0 ; count < size ; count ++)
	{
		x_font_t *  font ;

		if( ( font = array[count]->value) != NULL)
		{
			x_change_font_cols( font , 0) ;
		}
	}

	font_man->use_multi_col_char = 1 ;

	return  1 ;
}

int
x_unuse_multi_col_char(
	x_font_manager_t *  font_man
	)
{
	KIK_PAIR( x_font) *  array ;
	u_int  size ;
	int  count ;

	if( ! font_man->use_multi_col_char)
	{
		return  0 ;
	}
	
	kik_map_get_pairs_array( font_man->font_cache_table , array , size) ;

	for( count = 0 ; count < size ; count ++)
	{
		x_font_t *  font ;

		if( ( font = array[count]->value) != NULL)
		{
			x_change_font_cols( font , 1) ;
		}
	}

	font_man->use_multi_col_char = 0 ;

	return  1 ;
}

XFontSet
x_get_fontset(
	x_font_manager_t *  font_man
	)
{
	XFontSet  fontset ;
	char *  default_font_name ;
	char *  list_str ;
	u_int  list_str_len ;
	char **  missing ;
	int  miss_num ;
	char *  def_str ;
	char **  fontnames ;
	u_int  size ;
	int  count ;

	/* "+ 1" is for '\0' */
	if( ( default_font_name = alloca( 22 + DIGIT_STR_LEN(font_man->font_size) + 1)) == NULL)
	{
		return  NULL ;
	}

	sprintf( default_font_name , "-*-*-medium-r-*--%d-*-*-*-*-*" , font_man->font_size) ;

	list_str_len = strlen( default_font_name) ;

	if( is_aa( font_man) || is_vertical( font_man))
	{
		if( is_var_col_width( font_man))
		{
			size = x_get_all_font_names( font_man->v_font_custom ,
				&fontnames , font_man->font_size) ;
		}
		else
		{
			size = x_get_all_font_names( font_man->normal_font_custom ,
				&fontnames , font_man->font_size) ;
		}
	}
	else
	{
		size = x_get_all_font_names( font_man->font_custom ,  &fontnames ,
			font_man->font_size) ;
	}
	
	for( count = 0 ; count < size ; count ++)
	{
		list_str_len += (1 + strlen( fontnames[count])) ;
	}
	
	if( ( list_str = alloca( sizeof( char) * list_str_len)) == NULL)
	{
		return  NULL ;
	}
	*list_str = '\0' ;

	for( count = 0 ; count < size ; count ++)
	{
		if( strstr( list_str , fontnames[count]))
		{
			/* removing the same name fonts. */

			continue ;
		}

		strcat( list_str , fontnames[count]) ;
		strcat( list_str , ",") ;
	}

	if( size > 0)
	{
		free( fontnames) ;
	}

	strcat( list_str , default_font_name) ;

#ifdef  __DEBUG
	kik_debug_printf( KIK_DEBUG_TAG " font set list -> %s\n" , list_str) ;
#endif

	fontset = XCreateFontSet( font_man->display , list_str , &missing , &miss_num , &def_str) ;

#ifdef  DEBUG
	if( miss_num)
	{
		kik_warn_printf( KIK_DEBUG_TAG " missing these fonts ...\n") ;
		for( count = 0 ; count < miss_num ; count ++)
		{
			kik_msg_printf( " %s\n" , missing[count]) ;
		}
	}
#endif

	XFreeStringList( missing) ;

	return  fontset ;
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
