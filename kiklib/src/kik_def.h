/*
 *	$Id$
 */

#ifndef  __KIK_DEF_H__
#define  __KIK_DEF_H__


#include  "kik_config.h"


#ifndef  HAVE_FUNCTION
#define  __FUNCTION__ "__FUNCTION__"
#endif

#ifndef PATH_MAX
#ifndef _POSIX_PATH_MAX
#define _POSIX_PATH_MAX 255
#endif
#define PATH_MAX _POSIX_PATH_MAX
#endif


#endif
