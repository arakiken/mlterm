/*
 *	$Id$
 */

#include  "ml_bidi.h"

#include  <string.h>		/* strcmp */


#if  0
#define  __DEBUG
#endif


/* --- static variables --- */

/* Order of this table must be same as x_bidi_mode_t. */
static char *   bidi_mode_name_table[] =
{
	"normal" , "cmd_l" , "cmd_r" ,
} ;


#ifdef  USE_FRIBIDI

#include  <ctype.h>		/* isalpha */
#include  <kiklib/kik_debug.h>
#include  <kiklib/kik_mem.h>	/* alloca/realloc/free */
#include  <fribidi/fribidi.h>


#define  DIR_LTR_MARK 0x200e
#define  DIR_RTL_MARK 0x200f


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
	state->has_rtl = 0 ;
	state->bidi_mode = BIDI_NORMAL_MODE ;

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
	ml_bidi_mode_t  bidi_mode
	)
{
	FriBidiChar *  fri_src ;
	FriBidiCharType  fri_type ;
	FriBidiStrIndex *  fri_order ;
	u_char *  bytes ;
	u_int  count ;

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

	state->has_rtl = 0 ;

	for( count = 0 ; count < size ; count ++)
	{
		bytes = ml_char_bytes( &src[count]) ;

		if( ml_char_cs( &src[count]) == US_ASCII)
		{
			if( bidi_mode == BIDI_CMD_MODE_L && bytes[0] == ' ')
			{
				fri_src[count] = DIR_LTR_MARK ;
			}
			else if( bidi_mode == BIDI_CMD_MODE_R && ! isalpha(bytes[0]))
			{
				if( bytes[0] == ' ')
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
				fri_src[count] = bytes[0] ;
			}
		}
		else if( ml_char_cs( &src[count]) == ISO10646_UCS4_1)
		{
			fri_src[count] = (bytes[0] << 24) + (bytes[1] << 16) + (bytes[2] << 8) + bytes[3] ;
		}
		else if( ml_char_cs( &src[count]) == DEC_SPECIAL)
		{
			/* Regarded as LTR character. */
			fri_src[count] = 'a' ;
		}
		else
		{
		#ifdef  __DEBUG
			kik_debug_printf( KIK_DEBUG_TAG " %x is not ucs.\n" , ml_char_cs(&src[count])) ;
		#endif

			/*
			 * Regarded as NEUTRAL character.
			 */
			fri_src[count] = ' ' ;
		}

		if( ! state->has_rtl && (fribidi_get_type( fri_src[count]) & FRIBIDI_MASK_RTL))
		{
			state->has_rtl = 1 ;
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
		state->base_is_rtl = 1 ;
	}
	else
	{
		state->base_is_rtl = 0 ;
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


#endif	/* USE_FRIBIDI */


/* --- global functions --- */

ml_bidi_mode_t
ml_get_bidi_mode(
	char *  name
	)
{
	ml_bidi_mode_t  mode ;

	for( mode = 0 ; mode < BIDI_MODE_MAX ; mode++)
	{
		if( strcmp( bidi_mode_name_table[mode] , name) == 0)
		{
			return  mode ;
		}
	}
	
	/* default value */
	return  BIDI_NORMAL_MODE ;
}

char *
ml_get_bidi_mode_name(
	ml_bidi_mode_t  mode
	)
{
	if( mode < 0 || BIDI_MODE_MAX <= mode)
	{
		/* default value */
		mode = BIDI_NORMAL_MODE ;
	}

	return  bidi_mode_name_table[mode] ;
}
