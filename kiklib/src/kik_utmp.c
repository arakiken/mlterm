/*
 *	$Id$
 */

#include  "kik_config.h"


#if  defined(HAVE_UTMPER)

#include  "kik_utmp_utmper.c"

#elif  defined(HAVE_LOGIN)

#include  "kik_utmp_login.c"

#elif  defined(HAVE_SETUTENT)

#include  "kik_utmp_sysv.c"

#else

#include  "kik_utmp_bsd.c"

#endif
