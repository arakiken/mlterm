/*
 *	$Id$
 */

#include  "mkf_ucs_property.h"

#include  "mkf_tblfunc_loader.h"


/* --- global functions --- */

#ifdef  NO_DYNAMIC_LOAD_TABLE

#include  "../libtbl/mkf_ucs_property.c"

#else

mkf_prop_func( "mkf_ucsprop", mkf_get_raw_ucs_property) ;

#endif

mkf_property_t
mkf_get_ucs_property(
	u_int32_t  ucs
	)
{
	mkf_ucs_property_t  ucs_prop ;

	if( ( ucs_prop = mkf_get_raw_ucs_property( ucs)))
	{
		mkf_property_t  prop ;

		prop = 0 ;
		
		if( UCSPROP_GENERAL(ucs_prop) == MKF_UCSPROP_MC ||
			UCSPROP_GENERAL(ucs_prop) == MKF_UCSPROP_ME ||
			UCSPROP_GENERAL(ucs_prop) == MKF_UCSPROP_MN)
		{
			prop |= MKF_COMBINING ;
		}

		if( UCSPROP_EAST_ASIAN_WIDTH(ucs_prop) == MKF_UCSPROP_EAW_W ||
			UCSPROP_EAST_ASIAN_WIDTH(ucs_prop) == MKF_UCSPROP_EAW_F)
		{
			prop |= MKF_BIWIDTH ;
		}
		else if( UCSPROP_EAST_ASIAN_WIDTH(ucs_prop) == MKF_UCSPROP_EAW_A)
		{
			prop |= MKF_AWIDTH ;
		}

		return  prop ;
	}

	return  0 ;
}
