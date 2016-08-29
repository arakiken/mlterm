/*
 *	$Id$
 */

#ifndef __BL_TYPES_H__
#define __BL_TYPES_H__

/* various AC_TYPEs are defined in bl_config.h */
#include "bl_config.h"

#include <sys/types.h>

#ifdef HAVE_STDINT_H
#include <stdint.h>
#endif

/* only for tests */
#ifdef TEST_LP64

#define size_t bl_size_t
typedef long long bl_size_t;

#endif

#endif
