/*
 *	$Id$
 */

#include  "ml_bidi.h"

#include  <stdio.h>
#include  <kiklib/kik_debug.h>


#if  0
#define  __DEBUG
#endif


#ifdef  USE_FRIBIDI

#include  <fribidi/fribidi.h>


/* --- global functions --- */

int
ml_bidi(
	u_int16_t *  order ,
	ml_char_t *  src ,
	u_int  size
	)
{
	FriBidiChar *  fri_src ;
	FriBidiCharType  type = FRIBIDI_TYPE_LTR ;
	u_char *  bytes ;
	int  counter ;

	if( size == 0)
	{
		return  0 ;
	}

	if( ( fri_src = alloca( sizeof( FriBidiChar) * size)) == NULL)
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
		#ifdef  DEBUG
			kik_warn_printf( KIK_DEBUG_TAG " illegal cs %x.\n" , ml_char_cs(&src[counter])) ;
		#endif
		
			/* white space */
			fri_src[counter] = 0x20 ;
		}
	}

	fribidi_log2vis( fri_src , size , &type , NULL , order , NULL , NULL) ;

#ifdef  __DEBUG
	printf( "visual order => ") ;
	for( counter = 0 ; counter < size ; counter ++)
	{
		printf( "%.2d" , order[counter]) ;
	}
	printf( "\n") ;
#endif

#ifdef  DEBUG
	for( counter = 0 ; counter < size ; counter ++)
	{
		if( order[counter] >= size)
		{
			kik_debug_printf( KIK_DEBUG_TAG
				" visual order(%d) of %d is illegal.\n" ,
				order[counter] , counter) ;
			abort() ;
		}

	}
#endif

	return  1 ;
}


#else


/* --- global functions --- */

int
ml_bidi(
	u_int16_t *  order ,
	ml_char_t *  src ,
	u_int  size
	)
{
	int  counter ;

	if( size == 0)
	{
		return  0 ;
	}

	for( counter = 0 ; counter < size ; counter ++)
	{
		order[counter] = counter ;
	}
	
	return  1 ;
}


#endif
