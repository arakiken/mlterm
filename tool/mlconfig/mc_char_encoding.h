/*
 *	$Id$
 */

#ifndef  __MC_CHAR_ENCODING_H__
#define  __MC_CHAR_ENCODING_H__


#include  <gtk/gtk.h>


GtkWidget *  mc_char_encoding_config_widget_new( char *  encoding) ;

char *  mc_get_char_encoding(void) ;


#endif
