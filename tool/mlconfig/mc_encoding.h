/*
 *	$Id$
 */

#ifndef  __MC_ENCODING_H__
#define  __MC_ENCODING_H__


#include  <gtk/gtk.h>
#include  <ml_char_encoding.h>


GtkWidget *  mc_encoding_config_widget_new( ml_char_encoding_t  encoding) ;

ml_char_encoding_t  mc_get_encoding(void) ;


#endif
