/*
 *	$Id$
 */

#ifndef  __X_CONNECT_DIALOG_H__
#define  __X_CONNECT_DIALOG_H__


#include  "x.h"


/* Exported for winrs.rs. */
#define IDD_LIST	10
#define IDD_SSH		11
#define IDD_TELNET	12
#define IDD_RLOGIN	13
#define IDD_SERVER	14
#define IDD_USER	15
#define IDD_PASS	16


int  x_connect_dialog( char **  server , char **  user , char **  pass ,
		Window  parent_window , char **  server_list , char *  default_server) ;


#endif
