/*
 *	$Id$
 */

#include  "kik_langinfo.h"


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
