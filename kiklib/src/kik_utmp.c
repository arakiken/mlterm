/*
 *	$Id$
 */

#include  "kik_config.h"

#if  defined(UTMP_NONE)

#include  "kik_utmp_none.c"

#elif  defined(have_utmper)

#include  "kik_utmp_utmper.c"

#elif  defined(HAVE_SETUTENT)

#if  defined(HAVE_LOGIN)

#include  "kik_utmp_login.c"

#else

#include  "kik_utmp_sysv.c"

#endif	/* HAVE_LOGIN */

#else

#include  "kik_utmp_bsd.c"

#endif
