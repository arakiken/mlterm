/*
 *	$Id$
 */

#include  "kik_dialog.h"

#include  "kik_debug.h"


/* --- static variables --- */

static int (*callback)( int , char *) ;


/* --- global functions --- */

int
kik_dialog_set_callback(
	int  type ,		/* Not used for now. */
	int (*cb)( int , char *)
	)
{
	callback = cb ;

	return  1 ;
}

#if  0
int
kik_dialog_set_exec_file(
	int  type ,
	const char *  path
	)
{
	return  1 ;
}
#endif

int
kik_dialog(
	int  type ,		/* Not used for now. */
	char *  msg
	)
{
	if( callback)
	{
		return  (*callback)( type , msg) ;
	}
	else
	{
		kik_msg_printf( "%s\n" , msg) ;

		return  -1 ;
	}
}
