/*
 *	$Id$
 */

#ifndef  __KIK_DIALOG_H__


typedef enum
{
	KIK_DIALOG_OKCANCEL = 0 ,
	KIK_DIALOG_ALERT = 1 ,

} kik_dialog_style_t ;


int  kik_dialog_set_callback( int (*callback)( kik_dialog_style_t , char *)) ;

int  kik_dialog( kik_dialog_style_t  style , char *  msg) ;


#endif
