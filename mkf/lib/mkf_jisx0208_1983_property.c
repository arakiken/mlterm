/*
 *	update: <2001/11/26(02:43:30)>
 *	$Id$
 */

#include  "mkf_jisx0208_1983_property.h"


/*
 * COMBINING_ENCLOSING_CIRCLE is regarded as LARGE_CIRCLE in JISX0208_1997.
 */
#if  0
#define  JISX0208_COMBINING_ENCLOSING_CIRCLE
#endif


/* --- global functions --- */

mkf_property_t
mkf_get_jisx0208_1983_property(
	u_char *  ch ,
	size_t  size
	)
{
#ifdef  JISX0208_COMBINING_ENCLOSING_CIRCLE
	if( size == 2 && memcmp( ch , "\x22\x7e" , 2) == 0)
	{
		return  MKF_COMBINING ;
	}
#endif

	return  0 ;
}
