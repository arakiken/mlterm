/*
 *	$Id$
 */

#ifndef  __KIK_LANGINFO_H__
#define  __KIK_LANGINFO_H__


#include  "kik_config.h"


#ifdef  HAVE_LANGINFO_H
#include  <langinfo.h>
#endif


#ifdef CODESET

#define  kik_langinfo( item)  nl_langinfo( item)

#else	/* CODESET */

/*
 * same as NetBSD 1.5 langinfo.h (revision 1.5)
 */
 
typedef  long  nl_item ;

#define D_T_FMT		0
#define D_FMT		1 
#define T_FMT		2
#define T_FMT_AMPM	3
#define AM_STR		4
#define PM_STR		5
 
#define DAY_1		6
#define DAY_2		7 
#define DAY_3		8 
#define DAY_4		9 
#define DAY_5		10 
#define DAY_6		11 
#define DAY_7		12 
 
#define ABDAY_1		13
#define ABDAY_2		14 
#define ABDAY_3		15 
#define ABDAY_4		16 
#define ABDAY_1		13
#define ABDAY_2		14 
#define ABDAY_3		15 
#define ABDAY_4		16 
#define ABDAY_5		17 
#define ABDAY_6		18 
#define ABDAY_7		19 
 
#define MON_1		20
#define MON_2		21 
#define MON_3		22 
#define MON_4		23 
#define MON_5		24 
#define MON_6		25 
#define MON_7		26 
#define MON_8		27 
#define MON_9		28 
#define MON_10		29 
#define MON_11		30 
#define MON_12		31 
 
#define ABMON_1		32
#define ABMON_2		33 
#define ABMON_3		34 
#define ABMON_4		35 
#define ABMON_5		36 
#define ABMON_6		37 
#define ABMON_7		38 
#define ABMON_8		39 
#define ABMON_9		40 
#define ABMON_10	41 
#define ABMON_11	42 
#define ABMON_12	43 
 
#define RADIXCHAR	44
#define THOUSEP		45
#define YESSTR		46
#define YESEXPR		47
#define NOSTR		48
#define NOEXPR		49
#define CRNCYSTR	50
 
#define CODESET		51

#define USE_BUILTIN_LANGINFO

#define  kik_langinfo( item)  __kik_langinfo( item)

char *  __kik_langinfo( nl_item  item) ;

#endif	/* CODESET */


#endif
