/*
 *	$Id$
 */

#ifndef  __MC_CHAR_ENCODING_H__
#define  __MC_CHAR_ENCODING_H__


#include  <gtk/gtk.h>
#include  <ml_char_encoding.h>


GtkWidget *  mc_char_encoding_config_widget_new( ml_char_encoding_t  encoding) ;

ml_char_encoding_t  mc_get_char_encoding(void) ;


#endif
