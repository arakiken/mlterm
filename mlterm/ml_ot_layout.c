/*
 *	$Id$
 */

#ifdef  USE_OT_LAYOUT

#include  "ml_ot_layout.h"

#include  <kiklib/kik_str.h>	/* kik_snprintf */
#include  <kiklib/kik_dlfcn.h>
#include  <kiklib/kik_mem.h>
#include  <kiklib/kik_debug.h>
#include  <kiklib/kik_util.h>	/* K_MAX */

#include  "ml_ctl_loader.h"


#if  0
#define  __DEBUG
#endif


/* --- static variables --- */

static u_int  (*shape_func)( void * , u_int32_t * , u_int , int8_t * , u_int8_t * ,
			u_int32_t * , u_int32_t * , u_int , const char * , const char *) ;
static void *  (*get_font_func)( void * , ml_font_t) ;
static char *  ot_layout_attrs[] = { "latn" , "liga,clig,dlig,hlig,rlig" } ;
static int8_t  ot_layout_attr_changed[2] ;


/* --- static functions --- */

#ifndef  NO_DYNAMIC_LOAD_CTL

static int
ml_is_rtl_char(
	u_int32_t  code
	)
{
	int (*func)( u_int32_t) ;

	if( ! ( func = ml_load_ctl_bidi_func( ML_IS_RTL_CHAR)))
	{
		return  0 ;
	}

	return  (*func)( code) ;
}

#elif  defined(USE_FRIBIDI)

/* Defined in libctl/ml_bidi.c */
int  ml_is_rtl_char( u_int32_t  code) ;

#else

#define  ml_is_rtl_char(code)  (0)

#endif


/* --- global functions --- */

void
ml_set_ot_layout_attr(
	char *  value ,
	ml_ot_layout_attr_t  attr
	)
{
	if( 0 <= attr && attr < MAX_OT_ATTRS)
	{
		if( ot_layout_attr_changed[attr])
		{
			free( ot_layout_attrs[attr]) ;
		}
		else
		{
			ot_layout_attr_changed[attr] = 1 ;
		}

		if( ! value || (attr == OT_SCRIPT && strlen(value) != 4) ||
		    ! ( ot_layout_attrs[attr] = strdup( value)))
		{
			ot_layout_attrs[attr] = (attr == OT_SCRIPT) ?
						"latn" :
						"liga,clig,dlig,hlig,rlig" ;
		}
	}
}

void
ml_ot_layout_set_shape_func(
	u_int  (*func1)( void * , u_int32_t * , u_int , int8_t * , u_int8_t * ,
		u_int32_t * , u_int32_t * , u_int , const char * , const char *) ,
	void *  (*func2)( void * , ml_font_t)
	)
{
	shape_func = func1 ;
	get_font_func = func2 ;
}

u_int
ml_ot_layout_shape(
	void *  font ,
	u_int32_t *  shaped ,
	u_int  shaped_len ,
	int8_t *  offsets ,
	u_int8_t *  widths ,
	u_int32_t *  cmapped ,
	u_int32_t *  src ,
	u_int  src_len
	)
{
	if( ! shape_func)
	{
		return  0 ;
	}

	return  (*shape_func)( font , shaped , shaped_len , offsets , widths ,
			cmapped , src , src_len ,
			ot_layout_attrs[OT_SCRIPT] , ot_layout_attrs[OT_FEATURES]) ;
}

void *
ml_ot_layout_get_font(
	void *  term ,
	ml_font_t  font
	)
{
	if( ! get_font_func)
	{
		return  0 ;
	}

	return  (*get_font_func)( term , font) ;
}

ml_ot_layout_state_t
ml_ot_layout_new(void)
{
	return  calloc( 1 , sizeof( struct ml_ot_layout_state)) ;
}

int
ml_ot_layout_delete(
	ml_ot_layout_state_t  state
	)
{
	free( state->num_of_chars_array) ;
	free( state) ;

	return  1 ;
}

int
ml_ot_layout(
	ml_ot_layout_state_t  state ,
	ml_char_t *  src ,
	u_int  src_len
	)
{
	int  dst_pos ;
	int  src_pos ;
	u_int32_t *  ucs_buf ;
	u_int32_t *  shaped_buf ;
	u_int8_t *  num_of_chars_array ;
	u_int  shaped_buf_len ;
	u_int  prev_shaped_filled ;
	u_int  ucs_filled ;
	ml_font_t  font ;
	ml_font_t  prev_font ;
	void *  xfont ;

	if( ( ucs_buf = alloca( ( src_len * MAX_COMB_SIZE + 1) * sizeof(*ucs_buf))) == NULL)
	{
		return  0 ;
	}

	shaped_buf_len = src_len * MAX_COMB_SIZE + 1 ;
	if( ( shaped_buf = alloca( shaped_buf_len * sizeof(*shaped_buf))) == NULL)
	{
		return  0 ;
	}

	if( ( num_of_chars_array = alloca( shaped_buf_len * sizeof(*num_of_chars_array))) == NULL)
	{
		return  0 ;
	}

	state->substituted = 0 ;
	dst_pos = -1 ;
	prev_font = font = UNKNOWN_CS ;
	xfont = NULL ;
	for( src_pos = 0 ; src_pos < src_len ; src_pos ++)
	{
		font = ml_char_font( src + src_pos) ;
		if( FONT_CS(font) == US_ASCII && ml_char_code( src + src_pos) != ' ')
		{
			font &= ~US_ASCII ;
			font |= ISO10646_UCS4_1 ;
		}

		if( prev_font != font)
		{
			if( xfont &&
			    ( prev_shaped_filled != ucs_filled ||
			      memcmp( shaped_buf , ucs_buf ,
					prev_shaped_filled * sizeof(*shaped_buf)) != 0))
			{
				state->substituted = 1 ;
			}

			prev_shaped_filled = ucs_filled = 0 ;
			prev_font = font ;

			if( FONT_CS(font) == ISO10646_UCS4_1)
			{
				xfont = ml_ot_layout_get_font( state->term , font) ;
			}
			else
			{
				xfont = NULL ;
			}
		}

		if( xfont)
		{
			u_int  shaped_filled ;
			u_int  count ;
			ml_char_t *  comb ;
			u_int  num ;

			ucs_buf[ucs_filled] = ml_char_code( src + src_pos) ;
			if( ml_is_rtl_char( ucs_buf[ucs_filled]))
			{
				return  -1 ;
			}
			/* Don't do it in ml_is_rtl_char() which may be replaced by (0). */
			ucs_filled ++ ;

			comb = ml_get_combining_chars( src + src_pos , &num) ;
			for( ; num > 0 ; num--)
			{
				ucs_buf[ucs_filled] = ml_char_code( comb++) ;
				if( ml_is_rtl_char( ucs_buf[ucs_filled]))
				{
					return  -1 ;
				}
				/* Don't do it in ml_is_rtl_char() which may be replaced by (0). */
				ucs_filled ++ ;
			}

			/* store glyph index in ucs_buf. */
			ml_ot_layout_shape( xfont , NULL , 0 , NULL , NULL ,
				ucs_buf + ucs_filled - num - 1 ,
				ucs_buf + ucs_filled - num - 1 , num + 1) ;

			/* apply ot_layout to glyph indeces in ucs_buf. */
			shaped_filled = ml_ot_layout_shape( xfont , shaped_buf , shaped_buf_len ,
						NULL , NULL , ucs_buf , NULL , ucs_filled) ;

			if( shaped_filled < prev_shaped_filled)
			{
				if( shaped_filled == 0)
				{
					return  0 ;
				}

				count = prev_shaped_filled - shaped_filled ;
				dst_pos -= count ;

				for( ; count > 0 ; count--)
				{
					num_of_chars_array[dst_pos] +=
						num_of_chars_array[dst_pos + count] ;
				}

				prev_shaped_filled = shaped_filled ; /* goto to the next if block */
			}

			if( dst_pos >= 0 && shaped_filled == prev_shaped_filled)
			{
				num_of_chars_array[dst_pos] ++ ;
			}
			else
			{
				num_of_chars_array[++dst_pos] = 1 ;

				for( count = shaped_filled - prev_shaped_filled ;
				     count > 1 ; count--)
				{
					num_of_chars_array[++dst_pos] = 0 ;
				}
			}

			prev_shaped_filled = shaped_filled ;
		}
		else if( IS_ISCII(FONT_CS(font)))
		{
			return  -2 ;
		}
		else
		{
			num_of_chars_array[++dst_pos] = 1 ;
		}
	}

	if( xfont &&
	    ( prev_shaped_filled != ucs_filled ||
	      memcmp( shaped_buf , ucs_buf , prev_shaped_filled * sizeof(*shaped_buf)) != 0))
	{
		state->substituted = 1 ;
	}

	if( state->size != dst_pos + 1)
	{
		void *  p ;

		if( ! ( p = realloc( state->num_of_chars_array ,
				K_MAX(dst_pos + 1,src_len) * sizeof(*num_of_chars_array))))
		{
			return  0 ;
		}

	#ifdef  __DEBUG
		if( p != state->num_of_chars_array)
		{
			kik_debug_printf( KIK_DEBUG_TAG
				" REALLOC array %d(%p) -> %d(%p)\n" ,
				state->size , state->num_of_chars_array ,
				dst_pos + 1 , p) ;
		}
	#endif

		state->num_of_chars_array = p ;
		state->size = dst_pos + 1 ;
	}

	memcpy( state->num_of_chars_array , num_of_chars_array ,
		state->size * sizeof(*num_of_chars_array)) ;

	return  1 ;
}

int
ml_ot_layout_copy(
	ml_ot_layout_state_t  dst ,
	ml_ot_layout_state_t  src ,
	int  optimize
	)
{
	u_int8_t *  p ;

	if( optimize && ! src->substituted)
	{
		ml_ot_layout_delete( dst) ;

		return  -1 ;
	}
	else if( src->size == 0)
	{
		free( dst->num_of_chars_array) ;
		p = NULL ;
	}
	else if( ( p = realloc( dst->num_of_chars_array , sizeof( u_int8_t) * src->size)))
	{
		memcpy( p , src->num_of_chars_array , sizeof( u_int8_t) * src->size) ;
	}
	else
	{
		return  0 ;
	}

	dst->num_of_chars_array = p ;
	dst->term = src->term ;
	dst->size = src->size ;
	dst->substituted = src->substituted ;

	return  1 ;
}

int
ml_ot_layout_reset(
	ml_ot_layout_state_t  state
	)
{
	state->size = 0 ;

	return  1 ;
}

#endif
