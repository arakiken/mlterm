/*
 *	$Id$
 *
 *	Note that protocols except ssh aren't supported if USE_LIBSSH2 is defined.
 */

#ifdef  USE_LIBSSH2


#include  "../x_connect_dialog.h"


/* --- global functions --- */

int
x_connect_dialog(
	char **  uri ,		/* Should be free'ed by those who call this. */
	char **  pass ,		/* Same as uri. If pass is not input, "" is set. */
	char **  exec_cmd ,	/* Same as uri. If exec_cmd is not input, NULL is set. */
	char *  display_name ,
	Window  parent_window ,
	char **  sv_list ,
	char *  def_server	/* (<user>@)(<proto>:)<server address>(:<encoding>). */
	)
{
	return  0 ;
}


#endif
