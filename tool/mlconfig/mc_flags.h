/*
 *	$Id$
 */

#ifndef  __MC_FLAGS_H__
#define  __MC_FLAGS_H__


#include  <gtk/gtk.h>

#define  MC_FLAG_MODES        14

#define  MC_FLAG_XFT           0
#define  MC_FLAG_CAIRO         1
#define  MC_FLAG_AA            2
#define  MC_FLAG_VCOL          3
#define  MC_FLAG_COMB          4
#define  MC_FLAG_DYNCOMB       5
#define  MC_FLAG_RECVUCS       6
#define  MC_FLAG_MCOL          7
#define  MC_FLAG_BIDI          8
#define  MC_FLAG_IND           9
#define  MC_FLAG_AWIDTH       10
#define  MC_FLAG_CLIPBOARD    11
#define  MC_FLAG_LOCALECHO    12
#define  MC_FLAG_BLINKCURSOR  13


GtkWidget *  mc_flag_config_widget_new( int id) ;

void  mc_update_flag_mode( int id) ;


#endif
