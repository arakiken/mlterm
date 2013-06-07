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

int
ml_bidi(
	ml_bidi_state_t  state ,
	ml_char_t *  src ,
	u_int  size ,
	ml_bidi_mode_t  bidi_mode
	)
{
	FriBidiChar *  fri_src ;
	FriBidiCharType  fri_type ;
	FriBidiStrIndex *  fri_order ;
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

	for( count = 0 ; count < size ; count ++)
	{
		code = ml_char_code( &src[count]) ;

		if( ml_char_cs( &src[count]) == US_ASCII)
		{
			if( bidi_mode == BIDI_CMD_MODE_L && code == ' ')
			{
				fri_src[count] = DIR_LTR_MARK ;
			}
			else if( bidi_mode == BIDI_CMD_MODE_R && ! isalpha(code))
			{
				if( code == ' ')
				{
					fri_src[count] = DIR_RTL_MARK ;
				}
				else
				{
					/*
					 * Without this hack, following NG happens.
					 * $ ls test.txt test1.txt =>          test.txt ls $
					 * ./text.txt ./test1.txt  => test1.txt/. text.txt/. <- NG
					 *                                     ^^         ^^
					 */
					fri_src[count] = DIR_LTR_MARK ;
				}
			}
			else
			{
				fri_src[count] = code ;
			}
		}
		else if( ml_char_cs( &src[count]) == ISO10646_UCS4_1)
		{
			fri_src[count] = code ;
		}
		else if( ml_char_cs( &src[count]) == DEC_SPECIAL)
		{
			/* Regarded as LTR character. */
			fri_src[count] = 'a' ;
		}
		else
		{
		#ifdef  __DEBUG
			kik_debug_printf( KIK_DEBUG_TAG " %x is not ucs.\n" ,
				ml_char_cs(&src[count])) ;
		#endif

			/*
			 * Regarded as NEUTRAL character.
			 */
			fri_src[count] = ' ' ;
		}

		if( ! HAS_RTL(state) && (fribidi_get_type( fri_src[count]) & FRIBIDI_MASK_RTL))
		{
			SET_HAS_RTL( state) ;
		}
	}

#ifdef  __DEBUG
	kik_msg_printf( "utf8 text => \n") ;
	for( count = 0 ; count < size ; count ++)
	{
		kik_msg_printf( "%.4x " , fri_src[count]) ;
	}
	kik_msg_printf( "\n") ;
#endif

	if( bidi_mode == BIDI_CMD_MODE_R)
	{
		fri_type = FRIBIDI_TYPE_RTL ;
	}
	else
	{
		fri_type = FRIBIDI_TYPE_ON ;
	}

	fribidi_log2vis( fri_src , size , &fri_type , NULL , fri_order , NULL , NULL) ;

	if( state->size != size)
	{
		void *  p ;
		
		if( ( p = realloc( state->visual_order , sizeof( u_int16_t) * size)) == NULL)
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

	for( count = 0 ; count < size ; count ++)
	{
		state->visual_order[count] = fri_order[count] ;
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

#ifdef  __DEBUG
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
			kik_warn_printf( KIK_DEBUG_TAG " visual order(%d) of %d is illegal.\n" ,
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
