/*
 *	update: <2001/11/26(02:37:32)>
 *	$Id$
 */

#include  "kik_locale.h"


/* --- global functions --- */

#ifndef  HAVE_LANGINFO_H

char *
__kik_langinfo(
	nl_item  item
	)
{
	return  "" ;
}

#endif
