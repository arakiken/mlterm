/*
 *	$Id$
 */

#ifdef  USE_GSUB

#include  "ml_gsub.h"

#include  <kiklib/kik_str.h>	/* kik_snprintf */
#include  <kiklib/kik_dlfcn.h>
#include  <kiklib/kik_mem.h>
#include  <kiklib/kik_debug.h>


#if  0
#define  __DEBUG
#endif


/* --- static variables --- */

static u_int  (*shape_func)( void * , u_int32_t * , u_int , u_int32_t * , u_int32_t * , u_int ,
			const char * , const char *) ;
static void *  (*get_font_func)( void * , ml_font_t) ;
static char *  gsub_attrs[] = { "latn" , "liga,clig,dlig,hlig,rlig" } ;
static int8_t  gsub_attr_changed[2] ;


/* --- global functions --- */

void
ml_set_gsub_attr(
	char *  value ,
	ml_gsub_attr_t  attr
	)
{
	if( 0 <= attr && attr < MAX_GSUB_ATTRS)
	{
		if( gsub_attr_changed[attr])
		{
			free( gsub_attrs[attr]) ;
		}
		else
		{
			gsub_attr_changed[attr] = 1 ;
		}

		if( value)
		{
			gsub_attrs[attr] = strdup( value) ;
		}
		else
		{
			gsub_attrs[attr] = (attr == GSUB_SCRIPT) ?
						"latn" :
						"liga,clig,dlig,hlig,rlig" ;
		}
	}
}

void
ml_gsub_set_shape_func(
	u_int  (*func1)( void * , u_int32_t * , u_int , u_int32_t * , u_int32_t * , u_int ,
		const char * , const char *) ,
	void *  (*func2)( void * , ml_font_t)
	)
{
	shape_func = func1 ;
	get_font_func = func2 ;
}

u_int
ml_gsub_shape(
	void *  font ,
	u_int32_t *  gsub ,
	u_int  gsub_len ,
	u_int32_t *  cmap ,
	u_int32_t *  src ,
	u_int  src_len
	)
{
	if( ! shape_func)
	{
		return  0 ;
	}

	return  (*shape_func)( font , gsub , gsub_len , cmap , src , src_len ,
			gsub_attrs[GSUB_SCRIPT] , gsub_attrs[GSUB_FEATURES]) ;
}

void *
ml_gsub_get_font(
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

ml_gsub_state_t
ml_gsub_new(void)
{
	return  calloc( 1 , sizeof( struct ml_gsub_state)) ;
}

int
ml_gsub_delete(
	ml_gsub_state_t  state
	)
{
	free( state->num_of_chars_array) ;
	free( state) ;

	return  1 ;
}

int
ml_gsub(
	ml_gsub_state_t  state ,
	ml_char_t *  src ,
	u_int  src_len
	)
{
	int  dst_pos ;
	int  src_pos ;
	u_int32_t *  ucs_buf ;
	u_int32_t *  gsub_buf ;
	u_int  gsub_buf_len ;
	u_int  prev_gsub_filled ;
	u_int  ucs_filled ;
	ml_font_t  font ;
	ml_font_t  prev_font ;
	void *  xfont ;

	if( ( ucs_buf = alloca( ( src_len * MAX_COMB_SIZE + 1) * sizeof(*ucs_buf))) == NULL)
	{
		return  0 ;
	}

	gsub_buf_len = src_len * MAX_COMB_SIZE + 1 ;
	if( ( gsub_buf = alloca( gsub_buf_len * sizeof(*gsub_buf))) == NULL)
	{
		return  0 ;
	}

	if( ( state->num_of_chars_array = realloc( state->num_of_chars_array ,
						gsub_buf_len * sizeof(u_int8_t))) == NULL)
	{
		return  0 ;
	}

	state->has_gsub = 0 ;
	dst_pos = -1 ;
	prev_font = font = UNKNOWN_CS ;
	xfont = NULL ;
	for( src_pos = 0 ; src_pos < src_len ; src_pos ++)
	{
		font = ml_char_font( src + src_pos) ;
		if( FONT_CS(font) == US_ASCII && ml_char_code( src + src_pos) != ' ')
		{
			font &= ~MAX_CHARSET ;
			font |= ISO10646_UCS4_1 ;
		}

		if( prev_font != font)
		{
			if( xfont &&
			    ( prev_gsub_filled != ucs_filled ||
			      memcmp( gsub_buf , ucs_buf , prev_gsub_filled) != 0))
			{
				state->has_gsub = 1 ;
			}

			prev_gsub_filled = ucs_filled = 0 ;
			prev_font = font ;

			if( FONT_CS(font) == ISO10646_UCS4_1)
			{
				xfont = ml_gsub_get_font( state->term , font) ;
			}
			else
			{
				xfont = NULL ;
			}
		}

		if( xfont)
		{
			u_int  gsub_filled ;
			u_int  count ;
			ml_char_t *  comb ;
			u_int  num ;

			ucs_buf[ucs_filled] = ml_char_code( src + src_pos) ;
			if( ml_is_rtl_char( ucs_buf[ucs_filled++]))
			{
				return  -1 ;
			}

			if( ( comb = ml_get_combining_chars( src + src_pos , &num)))
			{
				for( count = 0 ; count < num ; count++)
				{
					ucs_buf[ucs_filled] = ml_char_code( comb + count) ;
					if( ml_is_rtl_char( ucs_buf[ucs_filled++]))
					{
						return  -1 ;
					}
				}
			}

			/* store glyph index in ucs_buf. */
			ml_gsub_shape( xfont , NULL , 0 , ucs_buf + ucs_filled - num - 1 ,
				ucs_buf + ucs_filled - num - 1 , num + 1) ;
			/* apply gsub to glyph indeces in ucs_buf. */
			gsub_filled = ml_gsub_shape( xfont , gsub_buf , gsub_buf_len ,
						ucs_buf , NULL , ucs_filled) ;

			if( gsub_filled < prev_gsub_filled)
			{
				dst_pos -= (prev_gsub_filled - gsub_filled) ;

				for( count = 1 ; count <= prev_gsub_filled - gsub_filled ; count++)
				{
					state->num_of_chars_array[dst_pos] +=
						state->num_of_chars_array[dst_pos + count] ;
				}

				prev_gsub_filled = gsub_filled ; /* goto to the next if block */
			}

			if( dst_pos >= 0 && gsub_filled == prev_gsub_filled)
			{
				state->num_of_chars_array[dst_pos] ++ ;
			}
			else
			{
				state->num_of_chars_array[++dst_pos] = 1 ;

				for( count = 1 ; count < gsub_filled - prev_gsub_filled ; count++)
				{
					state->num_of_chars_array[++dst_pos] = 0 ;
				}
			}

			prev_gsub_filled = gsub_filled ;
		}
		else if( IS_ISCII(FONT_CS(font)))
		{
			return  -2 ;
		}
		else
		{
			state->num_of_chars_array[++dst_pos] = 1 ;
		}
	}

	if( xfont &&
	    ( prev_gsub_filled != ucs_filled ||
	      memcmp( gsub_buf , ucs_buf , prev_gsub_filled) != 0))
	{
		state->has_gsub = 1 ;
	}

	state->size = dst_pos + 1 ;

	return  1 ;
}

int
ml_gsub_copy(
	ml_gsub_state_t  dst ,
	ml_gsub_state_t  src ,
	int  optimize
	)
{
	u_int8_t *  p ;

	if( optimize && ! src->has_gsub)
	{
		ml_gsub_delete( dst) ;

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
	dst->has_gsub = src->has_gsub ;

	return  1 ;
}

int
ml_gsub_reset(
	ml_gsub_state_t  state
	)
{
	state->size = 0 ;

	return  1 ;
}

#endif
