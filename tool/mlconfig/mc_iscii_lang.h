/*
 *	$Id$
 */

#ifndef  __MC_ISCII_LANG_H__
#define  __MC_ISCII_LANG_H__


#include  <gtk/gtk.h>
#include  <ml_iscii.h>


GtkWidget *  mc_iscii_lang_config_widget_new( ml_iscii_lang_t  iscii_lang) ;

ml_iscii_lang_t  mc_get_iscii_lang(void) ;


#endif
