/*
 *	$Id$
 */

#ifndef  __KIK_PRIVILEGE_H__
#define  __KIK_PRIVILEGE_H__


#include  "kik_types.h"		/* uid_t / gid_t */


int  kik_priv_change_euid( uid_t  uid) ;

int  kik_priv_restore_euid(void) ;

int  kik_priv_change_egid( gid_t  gid) ;

int  kik_priv_restore_egid(void) ;


#endif
