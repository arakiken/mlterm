/*
 *	$Id$
 */

#include  "kik_dialog.h"

#include  "kik_debug.h"


/* --- static variables --- */

static int (*callback)( kik_dialog_style_t , char *) ;


/* --- global functions --- */

int
kik_dialog_set_callback(
	int (*cb)( kik_dialog_style_t , char *)
	)
{
	callback = cb ;

	return  1 ;
}

#if  0
int
kik_dialog_set_exec_file(
	kik_dialog_style_t  style
	const char *  path
	)
{
	return  1 ;
}
#endif

int
kik_dialog(
	kik_dialog_style_t  style ,
	char *  msg
	)
{
	int  ret ;

	if( callback && (ret = (*callback)( style , msg)) != -1)
	{
		return  ret ;
	}
	else
	{
		kik_msg_printf( "%s\n" , msg) ;

		return  -1 ;
	}
}
