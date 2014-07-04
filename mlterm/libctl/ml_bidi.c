/*
 *	$Id$
 */

#include  "ml_bidi.h"

#include  <string.h>		/* memset */
#include  <ctype.h>		/* isalpha */
#include  <kiklib/kik_debug.h>
#include  <kiklib/kik_mem.h>	/* alloca/realloc/free */
#include  <fribidi.h>


#if  0
#define  __DEBUG
#endif

#define  DIR_LTR_MARK 0x200e
#define  DIR_RTL_MARK 0x200f


/* --- global functions --- */

ml_bidi_state_t
ml_bidi_new(void)
{
	ml_bidi_state_t  state ;

	if( ( state = malloc( sizeof( *state))) == NULL)
	{
		return  NULL ;
	}

	state->visual_order = NULL ;
	state->size = 0 ;
	state->rtl_state = 0 ;
	state->bidi_mode = BIDI_NORMAL_MODE ;

	return  state ;
}

int
ml_bidi_delete(
	ml_bidi_state_t  state
	)
{
	free( state->visual_order) ;
	free( state) ;

	return  1 ;
}

/*
 * Don't call this functions with type_p == FRIBIDI_TYPE_ON and size == cur_pos.
 */
static void
log2vis(
	FriBidiChar *  str ,
	u_int  size ,
	FriBidiCharType *  type_p ,
	ml_bidi_mode_t  bidi_mode ,
	FriBidiStrIndex *  order ,
	u_int  cur_pos ,
	int  append
	)
{
	FriBidiCharType  type ;
	u_int  pos ;

	if( size > cur_pos)
	{
		if( bidi_mode == BIDI_ALWAYS_RIGHT)
		{
			type = FRIBIDI_TYPE_RTL ;
		}
		else if( bidi_mode == BIDI_ALWAYS_LEFT)
		{
			type = FRIBIDI_TYPE_LTR ;
		}
		else
		{
			type = FRIBIDI_TYPE_ON ;
		}

		fribidi_log2vis( str + cur_pos , size - cur_pos , &type , NULL ,
			order + cur_pos , NULL , NULL) ;

		if( *type_p == FRIBIDI_TYPE_ON)
		{
			*type_p = type ;
		}
	}
	else
	{
		/*
		 * This functions is never called if type_p == FRIBIDI_TYPE_ON and
		 * size == cur_pos.
		 */
		type = *type_p ;
	}

	if( *type_p == FRIBIDI_TYPE_LTR)
	{
		if( type == FRIBIDI_TYPE_RTL)
		{
			/*
			 * (Logical) "LLL/RRRNNN " ('/' is a separator)
			 *                      ^-> endsp
			 * => (Visual) "LLL/ NNNRRR" => "LLL/NNNRRR "
			 */

			u_int  endsp_num ;

			for( pos = size ; pos > cur_pos ; pos--)
			{
				if( str[pos - 1] != ' ')
				{
					break ;
				}

				order[pos - 1] = pos - 1 ;
			}

			endsp_num = size - pos ;

			for( pos = cur_pos ; pos < size - endsp_num ; pos++)
			{
				order[pos] = order[pos] + cur_pos - endsp_num ;
			}
		}
		else if( cur_pos > 0)
		{
			for( pos = cur_pos ; pos < size ; pos++)
			{
				order[pos] += cur_pos ;
			}
		}

		if( append)
		{
			order[size] = size ;
		}
	}
	else /* if( *type_p == FRIBIDI_TYPE_RTL) */
	{
		if( cur_pos > 0)
		{
			for( pos = 0 ; pos < cur_pos ; pos++)
			{
				order[pos] += (size - cur_pos) ;
			}
		}

		if( type == FRIBIDI_TYPE_LTR)
		{
			/*
			 * (Logical) "RRRNNN/LLL " ('/' is a separator)
			 *                      ^-> endsp
			 * => (Visual) "LLL /NNNRRR" => " LLL/NNNRRR"
			 */

			u_int  endsp_num ;

			for( pos = size ; pos > cur_pos ; pos--)
			{
				if( str[pos - 1] != ' ')
				{
					break ;
				}

				order[pos - 1] = size - pos ;
			}

			endsp_num = size - pos ;
			for( pos = cur_pos ; pos < size - endsp_num ; pos++)
			{
				order[pos] += endsp_num ;
			}
		}

		if( append)
		{
			for( pos = 0 ; pos < size ; pos++)
			{
				order[pos] ++ ;
			}

			order[size] = 0 ;
		}
	}
}

static void
log2log(
	FriBidiStrIndex *  order ,
	u_int  cur_pos ,
	u_int  size
	)
{
	u_int  pos ;

	for( pos = cur_pos ; pos < size ; pos++)
	{
		order[pos] = pos ;
	}
}

int
ml_bidi(
	ml_bidi_state_t  state ,
	ml_char_t *  src ,
	u_int  size ,
	ml_bidi_mode_t  bidi_mode ,
	const char *  separators
	)
{
	FriBidiChar *  fri_src ;
	FriBidiCharType  fri_type ;
	FriBidiStrIndex *  fri_order ;
	u_int  cur_pos ;
	mkf_charset_t  cs ;
	u_int32_t  code ;
	u_int  count ;

	state->rtl_state = 0 ;

	if( size == 0)
	{
		state->size = 0 ;

		return  1 ;
	}

	if( ( fri_src = alloca( sizeof( FriBidiChar) * size)) == NULL)
	{
	#ifdef  DEBUG
		kik_warn_printf( KIK_DEBUG_TAG " alloca() failed.\n") ;
	#endif
	
		return  0 ;
	}

	if( ( fri_order = alloca( sizeof( FriBidiStrIndex) * size)) == NULL)
	{
	#ifdef  DEBUG
		kik_warn_printf( KIK_DEBUG_TAG " alloca() failed.\n") ;
	#endif
	
		return  0 ;
	}

	if( state->size != size)
	{
		void *  p ;

		if( ! ( p = realloc( state->visual_order , sizeof( u_int16_t) * size)))
		{
		#ifdef  DEBUG
			kik_warn_printf( KIK_DEBUG_TAG " realloc() failed.\n") ;
		#endif

			state->size = 0 ;

			return  0 ;
		}

		state->visual_order = p ;
		state->size = size ;
	}

	fri_type = FRIBIDI_TYPE_ON ;

	if( bidi_mode == BIDI_ALWAYS_RIGHT)
	{
		SET_HAS_RTL(state) ;
	}

	for( count = 0 , cur_pos = 0 ; count < size ; count ++)
	{
		cs = ml_char_cs( &src[count]) ;
		code = ml_char_code( &src[count]) ;

		if( cs == US_ASCII)
		{
			if( ! isalpha(code))
			{
				ml_char_t *  comb ;
				u_int  comb_size ;

				if( ( comb = ml_get_combining_chars( &src[count] , &comb_size)) &&
				    ml_char_cs( comb) == PICTURE_CHARSET)
				{
					fri_src[count] = 'a' ;
				}
				else if( separators && strchr( separators , code))
				{
					if( HAS_RTL(state))
					{
						log2vis( fri_src , count , &fri_type , bidi_mode ,
								fri_order , cur_pos , 1) ;
					}
					else
					{
						fri_type = FRIBIDI_TYPE_LTR ;
						log2log( fri_order , cur_pos , count + 1) ;
					}

					cur_pos = count + 1 ;
				}
				else
				{
					fri_src[count] = code ;
				}
			}
			else
			{
				fri_src[count] = code ;
			}
		}
		else if( cs == ISO10646_UCS4_1)
		{
			if( 0x2500 <= code && code <= 0x259f)
			{
				goto  decsp ;
			}
			else
			{
				fri_src[count] = code ;

				if( ! HAS_RTL(state) &&
				    ( fribidi_get_type( fri_src[count]) & FRIBIDI_MASK_RTL))
				{
					SET_HAS_RTL( state) ;
				}
			}
		}
		else if( cs == DEC_SPECIAL)
		{
		decsp:
			fri_type = FRIBIDI_TYPE_LTR ;

			if( HAS_RTL(state))
			{
				log2vis( fri_src , count , &fri_type , bidi_mode ,
						fri_order , cur_pos , 1) ;
			}
			else
			{
				log2log( fri_order , cur_pos , count + 1) ;
			}

			cur_pos = count + 1 ;
		}
		else
		{
		#ifdef  __DEBUG
			kik_debug_printf( KIK_DEBUG_TAG " %x is not ucs.\n" , cs) ;
		#endif

			/*
			 * Regarded as NEUTRAL character.
			 */
			fri_src[count] = ' ' ;
		}
	}

	if( HAS_RTL(state))
	{
		log2vis( fri_src , size , &fri_type , bidi_mode , fri_order , cur_pos , 0) ;

		for( count = 0 ; count < size ; count ++)
		{
			state->visual_order[count] = fri_order[count] ;
		}

	#ifdef  __DEBUG
		kik_msg_printf( "utf8 text => \n") ;
		for( count = 0 ; count < size ; count ++)
		{
			kik_msg_printf( "%.4x " , fri_src[count]) ;
		}
		kik_msg_printf( "\n") ;

		kik_msg_printf( "visual order => ") ;
		for( count = 0 ; count < size ; count ++)
		{
			kik_msg_printf( "%.2d " , state->visual_order[count]) ;
		}
		kik_msg_printf( "\n") ;
	#endif

	#ifdef  DEBUG
		for( count = 0 ; count < size ; count ++)
		{
			if( state->visual_order[count] >= size)
			{
				kik_warn_printf( KIK_DEBUG_TAG
					" visual order(%d) of %d is illegal.\n" ,
					state->visual_order[count] , count) ;

				kik_msg_printf( "returned order => ") ;
				for( count = 0 ; count < size ; count ++)
				{
					kik_msg_printf( "%d " , state->visual_order[count]) ;
				}
				kik_msg_printf( "\n") ;

				abort() ;
			}

		}
	#endif
	}
	else
	{
		state->size = 0 ;
	}

	if( fri_type == FRIBIDI_TYPE_RTL)
	{
		SET_BASE_RTL(state) ;
	}
	else
	{
		UNSET_BASE_RTL(state) ;
	}

	state->bidi_mode = bidi_mode ;

	return  1 ;
}

int
ml_bidi_copy(
	ml_bidi_state_t  dst ,
	ml_bidi_state_t  src
	)
{
	u_int16_t *  p ;

	if( src->size == 0)
	{
		free( dst->visual_order) ;
		p = NULL ;
	}
	else if( ( p = realloc( dst->visual_order , sizeof( u_int16_t) * src->size)))
	{
		memcpy( p , src->visual_order , sizeof( u_int16_t) * src->size) ;
	}
	else
	{
		return  0 ;
	}

	dst->visual_order = p ;
	dst->size = src->size ;
	dst->rtl_state = src->rtl_state ;
	dst->bidi_mode = src->bidi_mode ;

	return  1 ;
}

int
ml_bidi_reset(
	ml_bidi_state_t  state
	)
{
	state->size = 0 ;

	return  1 ;
}

u_int32_t
ml_bidi_get_mirror_char(
	u_int32_t  ch
	)
{
	FriBidiChar  mirror ;

	if( fribidi_get_mirror_char( ch , &mirror))
	{
		return  mirror ;
	}
	else
	{
		return  0 ;
	}
}
