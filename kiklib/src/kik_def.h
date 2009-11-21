/*
 *	$Id$
 */

#ifndef  __KIK_DEF_H__
#define  __KIK_DEF_H__


#include  <limits.h>	/* PATH_MAX */

#include  "kik_config.h"


#ifndef PATH_MAX
#ifndef _POSIX_PATH_MAX
#define _POSIX_PATH_MAX 255
#endif
#define PATH_MAX _POSIX_PATH_MAX
#endif


#endif
