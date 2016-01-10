/*
 *	$Id$
 */

#include  "ml_bidi.h"

#include  <string.h>		/* strcmp */

#include  "ml_ctl_loader.h"


#if  0
#define  __DEBUG
#endif


/* --- static variables --- */

/* Order of this table must be same as ml_bidi_mode_t. */
static char *   bidi_mode_name_table[] =
{
	"normal" , "left" , "right" ,
} ;


/* --- global functions --- */

ml_bidi_mode_t
ml_get_bidi_mode(
	const char *  name
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
	if( (u_int)mode >= BIDI_MODE_MAX)
	{
		/* default value */
		mode = BIDI_NORMAL_MODE ;
	}

	return  bidi_mode_name_table[mode] ;
}

#ifndef  NO_DYNAMIC_LOAD_CTL

#ifdef  __APPLE__
int  ml_is_rtl_char( u_int32_t  code) __attribute__((weak)) ;
#endif

int
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

#endif
