/*
 *	update: <2001/11/14(05:16:49)>
 *	$Id$
 */

#ifndef  __KIK_TYPES_H__
#define  __KIK_TYPES_H__


/*
 * various AC_TYPEs are defined here
 * this must be included before <sys/types.h> because sys/types.h itself
 * may use cc unsupported types. (e.g. HP-UX)
 */
#include  "kik_config.h"

#include  <sys/types.h>

#ifdef  TEST_LP64

#define  size_t  kik_size_t
typedef  long long  kik_size_t ;

#endif


#endif
