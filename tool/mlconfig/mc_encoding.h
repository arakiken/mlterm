/*
 *	update: <2001/11/14(01:15:56)>
 *	$Id$
 */

#ifndef  __MC_ENCODING_H__
#define  __MC_ENCODING_H__


#include  <gtk/gtk.h>
#include  <ml_encoding.h>


GtkWidget *  mc_encoding_config_widget_new( ml_encoding_type_t  encoding) ;

ml_encoding_type_t  mc_get_encoding(void) ;


#endif
