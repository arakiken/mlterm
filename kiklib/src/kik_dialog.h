/*
 *	$Id$
 */

#ifndef  __KIK_DIALOG_H__


int  kik_dialog_set_callback( int  type , int (*callback)( int , char *)) ;

int  kik_dialog( int  type , char *  msg) ;


#endif
