/*
 *	$Id$
 */

#include  "ml_bidi.h"


#if  0
#define  __DEBUG
#endif


#ifdef  USE_FRIBIDI

#include  <kiklib/kik_debug.h>
#include  <kiklib/kik_mem.h>	/* alloca */
#include  <fribidi/fribidi.h>


/* --- global functions --- */

ml_bidi_state_t *
ml_bidi_new(void)
{
	ml_bidi_state_t *  state ;

	if( ( state = malloc( sizeof( ml_bidi_state_t))) == NULL)
	{
		return  NULL ;
	}

	state->visual_order = NULL ;
	state->size = 0 ;
	state->base_is_rtl = 0 ;
	state->cursor_pos = -1 ;

	return  state ;
}

int
ml_bidi_delete(
	ml_bidi_state_t *  state
	)
{
	free( state->visual_order) ;
	free( state) ;

	return  1 ;
}

int
ml_bidi_reset(
	ml_bidi_state_t *  state
	)
{
	state->size = 0 ;

	return  1 ;
}

int
ml_bidi(
	ml_bidi_state_t *  state ,
	ml_char_t *  src ,
	u_int  size ,
	int  cursor_pos
	)
{
	FriBidiChar *  fri_src ;
	FriBidiCharType  fri_type ;
	FriBidiStrIndex *  fri_order ;
	u_char *  bytes ;
	int  counter ;

	if( size == 0)
	{
		state->size = 0 ;
		state->base_is_rtl = 0 ;
		
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

	for( counter = 0 ; counter < size ; counter ++)
	{
		bytes = ml_char_bytes( &src[counter]) ;

		if( ml_char_cs( &src[counter]) == US_ASCII)
		{
			fri_src[counter] = bytes[0] ;
		}
		else if( ml_char_cs( &src[counter]) == ISO10646_UCS2_1)
		{
			fri_src[counter] = (bytes[0] << 8) + bytes[1] ;
		}
		else if( ml_char_cs( &src[counter]) == ISO10646_UCS4_1)
		{
			fri_src[counter] = (bytes[0] << 24) + (bytes[1] << 16) + (bytes[2] << 8) + bytes[3] ;
		}
		else
		{
		#ifdef  __DEBUG
			kik_debug_printf( KIK_DEBUG_TAG " %x is not ucs.\n" , ml_char_cs(&src[counter])) ;
		#endif
		
			/* white space */
			fri_src[counter] = 0x20 ;
		}
	}

#ifdef  __DEBUG
	kik_msg_printf( "utf8 text => \n") ;
	for( counter = 0 ; counter < size ; counter ++)
	{
		kik_msg_printf( "%.4x " , fri_src[counter]) ;
	}
	kik_msg_printf( "\n") ;
#endif

	fri_type = FRIBIDI_TYPE_ON ;

	fribidi_log2vis( fri_src , size , &fri_type , NULL , fri_order , NULL , NULL) ;

	state->cursor_pos = -1 ;
	
	if( cursor_pos >= 0)
	{
		int  pos ;
		FriBidiCharType  prev_type ;

		for( pos = cursor_pos ; pos < size ; pos ++)
		{
			if( fri_src[pos] != 0x20)
			{
				goto  end ;
			}
		}
		
		prev_type = 0 ;
		for( pos = cursor_pos + 1 ; pos >= 0 ; pos --)
		{
			if( ( prev_type = fribidi_get_type( fri_src[pos])) & FRIBIDI_MASK_STRONG)
			{
				break ;
			}
		}

		if( ! ( prev_type & FRIBIDI_MASK_RTL) && fri_type == FRIBIDI_TYPE_RTL)
		{
			fri_src[cursor_pos] = 0x61 ;
		}
		else if( ( prev_type & FRIBIDI_MASK_RTL) && fri_type & FRIBIDI_TYPE_LTR)
		{
			fri_src[cursor_pos] = 0x0621 ;
		}
		else
		{
			goto  end ;
		}

		state->cursor_pos = cursor_pos ;

		fribidi_log2vis( fri_src , size , &fri_type , NULL , fri_order , NULL , NULL) ;
	}

end:
	if( state->size != size)
	{
		if( ( state->visual_order = malloc( sizeof( u_int16_t) * size)) == NULL)
		{
			return  0 ;
		}

		state->size = size ;
	}

	for( counter = 0 ; counter < size ; counter ++)
	{
		state->visual_order[counter] = fri_order[counter] ;
	}

	if( fri_type == FRIBIDI_TYPE_RTL)
	{
		state->base_is_rtl = 1 ;
	}
	else
	{
		state->base_is_rtl = 0 ;
	}

#ifdef  __DEBUG
	kik_msg_printf( "visual order => ") ;
	for( counter = 0 ; counter < size ; counter ++)
	{
		kik_msg_printf( "%.2d " , state->visual_order[counter]) ;
	}
	kik_msg_printf( "\n") ;
#endif

#ifdef  DEBUG
	for( counter = 0 ; counter < size ; counter ++)
	{
		if( state->visual_order[counter] >= size)
		{
			kik_warn_printf( KIK_DEBUG_TAG " visual order(%d) of %d is illegal.\n" ,
				state->visual_order[counter] , counter) ;

			kik_msg_printf( "returned order => ") ;
			for( counter = 0 ; counter < size ; counter ++)
			{
				kik_msg_printf( "%d " , state->visual_order[counter]) ;
			}
			kik_msg_printf( "\n") ;
			
			abort() ;
		}

	}
#endif

	return  1 ;
}


#else


/* --- global functions --- */

ml_bidi_state_t *
ml_bidi_new(void)
{
	return  NULL ;
}

int
ml_bidi_delete(
	ml_bidi_state_t *  state
	)
{
	return  0 ;
}

int
ml_bidi_reset(
	ml_bidi_state_t *  state
	)
{
	return  0 ;
}

int
ml_bidi(
	ml_bidi_state_t *  state ,
	ml_char_t *  src ,
	u_int  size ,
	int  cursor_pos
	)
{
	return  0 ;
}


#endif
