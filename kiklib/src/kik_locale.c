/*
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
