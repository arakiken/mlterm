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

int
ml_bidi_support_level(void)
{
	return  1 ;
}

int
ml_bidi(
	u_int16_t *  order ,
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

	if( cursor_pos >= 0)
	{
		int  pos ;

		for( pos = cursor_pos + 1 ; pos < size ; pos ++)
		{
			if( fri_src[pos] != 0x20)
			{
				goto  end ;
			}
		}
		
		if( fribidi_get_type( fri_src[cursor_pos]) &
			(FRIBIDI_MASK_WEAK | FRIBIDI_MASK_NEUTRAL))
		{
			FriBidiCharType  prev_type ;

			prev_type = 0 ;
			for( pos = cursor_pos ; pos >= 0 ; pos --)
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
			
			fribidi_log2vis( fri_src , size , &fri_type , NULL , fri_order , NULL , NULL) ;
		}
	}

end:
	for( counter = 0 ; counter < size ; counter ++)
	{
		order[counter] = fri_order[counter] ;
	}
	
#ifdef  __DEBUG
	kik_msg_printf( "visual order => ") ;
	for( counter = 0 ; counter < size ; counter ++)
	{
		kik_msg_printf( "%.2d " , order[counter]) ;
	}
	kik_msg_printf( "\n") ;
#endif

#ifdef  DEBUG
	for( counter = 0 ; counter < size ; counter ++)
	{
		if( order[counter] >= size)
		{
			kik_warn_printf( KIK_DEBUG_TAG " visual order(%d) of %d is illegal.\n" ,
				order[counter] , counter) ;

			kik_msg_printf( "returned order => ") ;
			for( counter = 0 ; counter < size ; counter ++)
			{
				kik_msg_printf( "%d " , order[counter]) ;
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

int
ml_bidi_support_level(void)
{
	return  0 ;
}

int
ml_bidi(
	u_int16_t *  order ,
	ml_char_t *  src ,
	u_int  size ,
	int  cursor_pos
	)
{
	return  0 ;
}


#endif
