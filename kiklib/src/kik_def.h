/*
 *	$Id$
 */

#ifndef  __KIK_DEF_H__
#define  __KIK_DEF_H__


#include  <limits.h>	/* PATH_MAX,SIZE_MAX */

/* various AC_DEFINEs are defined in kik_config.h */
#include  "kik_config.h"


#ifndef PATH_MAX
#ifndef _POSIX_PATH_MAX
#define _POSIX_PATH_MAX 255
#endif
#define PATH_MAX _POSIX_PATH_MAX
#endif

#ifndef SIZE_MAX
#ifdef  SIZE_T_MAX
#define SIZE_MAX SIZE_T_MAX
#else
#define SIZE_MAX ((size_t)-1)
#endif
#endif

#if  0
/* Check integer overflow. Use this with malloc or alloca. */
#define  _X(a,b) ((a) > SIZE_MAX / (b) ? SIZE_MAX : (a) * (b))
#endif


#endif
