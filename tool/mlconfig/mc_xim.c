/*
 *	$Id$
 */

#include  "mc_xim.h"

#include  <string.h>
#include  <stdlib.h>		/* free */
#include  <kiklib/kik_debug.h>
#include  <kiklib/kik_file.h>
#include  <kiklib/kik_conf_io.h>
#include  <kiklib/kik_map.h>
#include  <glib.h>
#include  <c_intl.h>

#include  "mc_combo.h"
#include  "mc_io.h"


#if  0
#define  __DEBUG
#endif

#ifndef  SYSCONFDIR
#define  CONFIG_PATH  "/etc"
#else
#define  CONFIG_PATH  SYSCONFDIR
#endif


KIK_MAP_TYPEDEF(xim_locale,char *,char *) ;


/* --- static variables --- */

static char **  xims ;
static char **  locales ;
static u_int  num_of_xims ;

static char *  new_xim ;
static char *  old_xim ;
static char *  new_locale ;
static char *  old_locale ;
static char *  cur_locale ;
static int is_changed;


/* --- static functions --- */

static gint
locale_changed(
	GtkWidget *  widget ,
	gpointer  data
	)
{
	new_locale = gtk_entry_get_text(GTK_ENTRY(widget)) ;

	return  1 ;
}

static char *
get_xim_locale(
	char *  xim
	)
{
	int  count ;
	
	for( count = 0 ; count < num_of_xims ; count ++)
	{
		if( strcmp( xims[count] , new_xim) == 0)
		{
			return  locales[count] ;
		}
	}

	return  NULL ;
}

static gint
xim_selected(
	GtkWidget *  widget ,
	gpointer  data
	)
{
	GtkWidget *  locale_entry ;

	locale_entry = (GtkWidget*)data ;
	
	new_xim = gtk_entry_get_text(GTK_ENTRY(widget)) ;

	if( ( new_locale = get_xim_locale( new_xim)))
	{
		gtk_entry_set_text(GTK_ENTRY(locale_entry) , new_locale) ;

		return  1 ;
	}
	else
	{
		gtk_entry_set_text(GTK_ENTRY(locale_entry) , cur_locale) ;

		return  1 ;
	}
}

static int
read_conf(
	KIK_MAP(xim_locale)  xim_locale_table ,
	char *  filename
	)
{
	kik_file_t *  from ;
	char *  key ;
	char *  value ;
	KIK_PAIR(xim_locale)  pair ;
	int  result ;

	if( ! ( from = kik_file_open( filename , "r")))
	{
	#ifdef  DEBUG
		kik_warn_printf( KIK_DEBUG_TAG " %s couldn't be opened.\n" , filename) ;
	#endif
	
		return  0 ;
	}

	while( kik_conf_io_read( from , &key , &value))
	{
		kik_map_get( result , xim_locale_table , key , pair) ;
		if( result)
		{
			free( pair->value) ;
			pair->value = strdup( value) ;
		}
		else
		{
			key = strdup( key) ;
			value = strdup( value) ;
			
			kik_map_set( result , xim_locale_table , key , value) ;
		}
	}

	kik_file_close( from) ;
	
	return  1 ;
}


/* --- global functions --- */

GtkWidget *
mc_xim_config_widget_new(void)
{
	char *  rcpath ;
	KIK_MAP(xim_locale)  xim_locale_table ;
	KIK_PAIR(xim_locale) *  array ;
	u_int  size ;
	int  count ;
	GtkWidget *  vbox ;
	GtkWidget *  hbox ;
	GtkWidget *  label ;
	GtkWidget *  combo ;
	GtkWidget *  entry ;

	old_xim = new_xim = mc_get_str_value( "xim") ;
	is_changed = 0;
	cur_locale = mc_get_str_value( "locale") ;
	
	kik_map_new(char *, char *, xim_locale_table, kik_map_hash_str,
		    kik_map_compare_str);

	kik_set_sys_conf_dir(CONFIG_PATH);
	
	if ((rcpath = kik_get_sys_rc_path("mlterm/xim"))) {
		read_conf(xim_locale_table, rcpath) ;
		free(rcpath) ;
	}

	if ((rcpath = kik_get_user_rc_path("mlterm/xim"))) {
		read_conf(xim_locale_table, rcpath) ;
		free(rcpath) ;
	}

	kik_map_get_pairs_array(xim_locale_table, array, size) ;

	if ((xims = malloc(sizeof(char*) * (size + 1))) == NULL) return NULL;

	if ((locales = malloc(sizeof(char*) * (size + 1))) == NULL)
	    return  NULL ;

	for (count = 0; count < size; count ++) {
	    xims[count] = array[count]->key;
	    locales[count] = array[count]->value;
	}
	
	xims[count] = strdup("unused");
	locales[count] = NULL;
	
	num_of_xims = size + 1;

	kik_map_delete(xim_locale_table);

	if ((new_locale = get_xim_locale(new_xim)) == NULL) {
	    new_locale = cur_locale ;
	}
	old_locale = strdup( new_locale) ;

	vbox = gtk_vbox_new(FALSE, 5);

	hbox = gtk_hbox_new(FALSE, 5);

	label = gtk_label_new(_("XIM locale"));
	gtk_widget_show(label);
	gtk_box_pack_start(GTK_BOX(hbox), label, FALSE, FALSE, 10);
	
	entry = gtk_entry_new();
	gtk_widget_show(entry);
	gtk_box_pack_start(GTK_BOX(hbox), entry, TRUE, TRUE, 0);
	gtk_entry_set_text(GTK_ENTRY(entry), new_locale);

	gtk_signal_connect(GTK_OBJECT(entry), "changed",
			   GTK_SIGNAL_FUNC(locale_changed), NULL);

	combo = mc_combo_new(_("X Input Method"), xims, num_of_xims,
		new_xim, 0, xim_selected, entry);
	gtk_widget_show(combo);
	gtk_box_pack_start(GTK_BOX(vbox), combo, TRUE, TRUE, 0);

	gtk_widget_show(hbox);
	gtk_box_pack_start(GTK_BOX(vbox), hbox, TRUE, TRUE, 0);

	return vbox;
}

void
mc_update_xim(void)
{
	char *  p ;

	if( strcmp( new_xim , "") == 0)
	{
		new_xim = "unused" ;
	}
	
	if( ( p = malloc( strlen( new_xim) + 1 + strlen( new_locale) + 1)) == NULL)
	{
		return ;
	}

	sprintf( p , "%s:%s" , new_xim , new_locale) ;

	if (strcmp(new_xim, old_xim) || strcmp(new_locale, old_locale))
		is_changed = 1;

	if (is_changed)
	{
		mc_set_str_value( "xim" , p) ;
			free( old_xim) ;
		old_xim = strdup( new_xim) ;
			free( old_locale) ;			
		old_locale = strdup( new_locale) ;
	}

	free( p) ;
}
