/*
 *	$Id$
 */

#include  "mc_sb_view.h"

#include  <string.h>
#include  <stdlib.h>		/* free */
#include  <kiklib/kik_debug.h>
#include  <kiklib/kik_conf_io.h>
#include  <glib.h>
#include  <c_intl.h>
#include  <sys/types.h>
#include  <dirent.h>

#include  "mc_combo.h"
#include  "mc_io.h"

#ifndef  DATADIR
#define  SB_DIR  "/usr/local/share/mlterm/scrollbars"
#else
#define  SB_DIR  DATADIR "/mlterm/scrollbars"
#endif

#define MAX_SCROLLBARS 100


#if  0
#define  __DEBUG
#endif


/* --- static variables --- */

static char *  new_sb_view_name = NULL;
static char *  old_sb_view_name = NULL;
static int is_changed;


/* --- static functions --- */

static gint
sb_view_name_selected(
	GtkWidget *  widget ,
	gpointer  data
	)
{
	free( new_sb_view_name);
	new_sb_view_name = gtk_editable_get_chars(GTK_EDITABLE(widget), 0, -1) ;
	
#ifdef  __DEBUG
	kik_debug_printf( KIK_DEBUG_TAG " %s sb_view_name is selected.\n" , new_sb_view_name) ;
#endif

	return  1 ;
}

static int
has_rc_file(char *dirname, char *sbdirname)
{
	DIR *d;
	struct dirent *e;
	char path[PATH_MAX];
	int result = 0;

	snprintf(path, PATH_MAX, "%s/%s", dirname, sbdirname);
	if(!(d = opendir(path))) return 0;

	while ((e = readdir(d))) {
		if (strcmp("rc", e->d_name) == 0) {
			result = 1;
			break;
		}
	}

	closedir(d);

	return result;
}

static int
read_sb_names(char *dirname, char **sbnames, int n)
{
    int n0, j;
    DIR *d;
    struct dirent *e;

    d = opendir(dirname);
    if (!d) return n;

    n0 = n;

    while (n<MAX_SCROLLBARS) {
    ignore:
	e = readdir(d);
	if (!e) break;
	if (e->d_name[0] == '.' || !has_rc_file(dirname, e->d_name)) continue;
	sbnames[n] = strdup(e->d_name);
	if (!sbnames[n]) break;
	for(j=0; j<n0; j++){
	    if (!strcmp(sbnames[n], sbnames[j])) goto ignore;
	}
	n++;
    }
    closedir(d);
    return n;
}


/* --- global functions --- */

GtkWidget *
mc_sb_view_config_widget_new(void)
{
	char *  sb_view0[] =
	{
		"simple" ,
		"sample" ,
		"sample2" ,
		"sample3" ,
		"athena" ,
		"motif" ,
		"mozmodern" ,
		"next"
	} ;
	char *sb_view_names[MAX_SCROLLBARS];
	char *userdir;
	int n;

	for (n=0; n<sizeof(sb_view0)/sizeof(sb_view0[0]); n++) {
		sb_view_names[n] = sb_view0[n];
	}

	userdir = kik_get_user_rc_path("mlterm/scrollbars");
	if (userdir) n = read_sb_names(userdir, sb_view_names, n);
	n = read_sb_names(SB_DIR, sb_view_names, n);

	new_sb_view_name = old_sb_view_name =
	  mc_get_str_value( "scrollbar_view_name") ;
	is_changed = 0;

	return  mc_combo_new( _("View") , sb_view_names , n,
		new_sb_view_name , 0 , sb_view_name_selected , NULL) ;
}

void
mc_update_sb_view_name(void)
{
	if (strcmp(new_sb_view_name, old_sb_view_name)) is_changed = 1;

	if (is_changed)
	{
		mc_set_str_value( "scrollbar_view_name" , new_sb_view_name) ;
		free( old_sb_view_name) ;
		old_sb_view_name = strdup( new_sb_view_name) ;
	}
}
