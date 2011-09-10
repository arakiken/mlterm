
/*
 *	$Id$
 */

#include  "ml_ctl_loader.h"


#ifndef  NO_DYNAMIC_LOAD_CTL

#include  <stdio.h>		/* NULL */
#include  <kiklib/kik_dlfcn.h>
#include  <kiklib/kik_debug.h>


#ifndef  LIBDIR
#define  CTLLIB_DIR  "/usr/local/lib/mlterm/"
#else
#define  CTLLIB_DIR  LIBDIR "/mlterm/"
#endif


/* --- global functions --- */

void *
ml_load_ctl_bidi_func(
	ml_ctl_bidi_id_t  id
	)
{
	static void **  func_table ;
	static int  is_tried ;

	if( ! is_tried)
	{
		kik_dl_handle_t  handle ;

		is_tried = 1 ;

		if( ( ! ( handle = kik_dl_open( CTLLIB_DIR , "ctl_bidi")) &&
		      ! ( handle = kik_dl_open( "" , "ctl_bidi"))))
		{
			return  NULL ;
		}

	#ifdef  DEBUG
		kik_debug_printf( KIK_DEBUG_TAG " Loading libctl_bidi.so\n") ;
	#endif

		func_table = kik_dl_func_symbol( handle , "ml_ctl_bidi_func_table") ;
	}

	if( func_table)
	{
		return  func_table[id] ;
	}
	else
	{
		return  NULL ;
	}
}

void *
ml_load_ctl_iscii_func(
	ml_ctl_iscii_id_t  id
	)
{
	static void **  func_table ;
	static int  is_tried ;

	if( ! is_tried)
	{
		kik_dl_handle_t  handle ;

		is_tried = 1 ;

		if( ( ! ( handle = kik_dl_open( CTLLIB_DIR , "ctl_iscii")) &&
		      ! ( handle = kik_dl_open( "" , "ctl_iscii"))))
		{
			return  NULL ;
		}

	#ifdef  DEBUG
		kik_debug_printf( KIK_DEBUG_TAG " Loading libctl_iscii.so\n") ;
	#endif

		func_table = kik_dl_func_symbol( handle , "ml_ctl_iscii_func_table") ;
	}

	if( func_table)
	{
		return  func_table[id] ;
	}
	else
	{
		return  NULL ;
	}
}

#endif	/* NO_DYNAMIC_LOAD_CTL */
