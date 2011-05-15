/*
 *	$Id$
 */

#ifndef  __X_CONNECT_DIALOG_H__
#define  __X_CONNECT_DIALOG_H__


#include  "x.h"


#ifdef  USE_WIN32GUI
/* Exported for winrs.rs. */
#define IDD_LIST	10
#define IDD_SSH		11
#define IDD_TELNET	12
#define IDD_RLOGIN	13
#define IDD_SERVER	14
#define IDD_PORT	15
#define IDD_USER	16
#define IDD_PASS	17
#define IDD_ENCODING	18
#endif


int  x_connect_dialog( char **  info , char **  pass , char *  display_name ,
		Window  parent_window , char **  server_list , char *  default_server) ;


#endif
