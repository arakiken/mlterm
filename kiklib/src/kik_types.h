/*
 *	$Id$
 */

#ifndef  __KIK_TYPES_H__
#define  __KIK_TYPES_H__


/* various AC_TYPEs are defined in kik_config.h */
#include  "kik_config.h"

#include  <sys/types.h>

#ifdef  HAVE_STDINT_H
#include  <stdint.h>
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


/* only for tests */
#ifdef  TEST_LP64

#define  size_t  kik_size_t
typedef  long long  kik_size_t ;

#endif


#endif
