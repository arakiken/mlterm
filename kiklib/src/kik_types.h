/*
 *	$Id$
 */

#ifndef  __KIK_TYPES_H__
#define  __KIK_TYPES_H__


/* various AC_TYPEs are defined in kik_config.h */
#include  "kik_config.h"

#include  <sys/types.h>


/* only for tests */
#ifdef  TEST_LP64

#define  size_t  kik_size_t
typedef  long long  kik_size_t ;

#endif


#endif
