/*
 *	$Id$
 */

#include  "mc_bgtype.h"

#include  <glib.h>
#include  <c_intl.h>

#include  "mc_color.h"
#include  "mc_wall_pic.h"
#include  "mc_io.h"


#if  0
#define  __DEBUG
#endif


/* --- static variables --- */

static char *  bgtype ;
static int  is_changed ;
static GtkWidget *  bg_color ;
static GtkWidget *  wall_picture ;


/* --- static functions --- */

static char *
get_bgtype(void)
{
	char *  val;
	
	if (mc_get_flag_value("use_transbg")) return "transparent" ;
	else if ((val = mc_get_str_value("wall_picture")) && *val != '\0') return "picture" ;
	else return  "color" ;
}

static void
set_sensitive(void)
{
    if (!strcmp(bgtype, "color")) {
	gtk_widget_set_sensitive(bg_color, 1);
	gtk_widget_set_sensitive(wall_picture, 0);
    } else if (!strcmp(bgtype, "picture")) {
	gtk_widget_set_sensitive(bg_color, 0);
	gtk_widget_set_sensitive(wall_picture, 1);
    } else {
	gtk_widget_set_sensitive(bg_color, 0);
	gtk_widget_set_sensitive(wall_picture, 0);
    }
}

static gint
button_color_checked(
	GtkWidget *  widget ,
	gpointer  data
	)
{
    if (GTK_TOGGLE_BUTTON(widget)->active) {
	bgtype = "color"; is_changed = 1;
	set_sensitive();
    }
    return  1 ;
}

static gint
button_transparent_checked(
	GtkWidget *  widget ,
	gpointer  data
	)
{
    if (GTK_TOGGLE_BUTTON(widget)->active) {
	bgtype = "transparent"; is_changed = 1;
	set_sensitive();
    }
    return  1 ;
}

static gint
button_picture_checked(
	GtkWidget *  widget ,
	gpointer  data
	)
{
    if (GTK_TOGGLE_BUTTON(widget)->active) {
	bgtype = "picture"; is_changed = 1;
	set_sensitive();
    }
    return  1 ;
}


/* --- global functions --- */

GtkWidget *
mc_bgtype_config_widget_new(void)
{
    GtkWidget *frame;
    GtkWidget *vbox;
    GtkWidget *hbox;
    GtkWidget *radio;
    GSList *group ;

	bgtype = get_bgtype() ;

	if( ( bg_color = mc_bg_color_config_widget_new()) == NULL) return NULL ;
	if( ( wall_picture = mc_wall_pic_config_widget_new()) == NULL) return NULL ;
	
    group = NULL;

    frame = gtk_frame_new(_("Background type"));
    vbox = gtk_vbox_new(TRUE, 2);
    gtk_container_set_border_width(GTK_CONTAINER(vbox), 5);
    gtk_container_add(GTK_CONTAINER(frame), vbox);
    gtk_widget_show(vbox);

    /* color button */
    radio = gtk_radio_button_new_with_label(group, _("Color"));
    group = gtk_radio_button_group(GTK_RADIO_BUTTON(radio));
    gtk_signal_connect(GTK_OBJECT(radio), "toggled",
		       GTK_SIGNAL_FUNC(button_color_checked), NULL);
    gtk_widget_show(GTK_WIDGET(radio));
    if (strcmp(bgtype, "color") == 0)
	gtk_toggle_button_set_state(GTK_TOGGLE_BUTTON(radio), TRUE);
    hbox = gtk_hbox_new(FALSE, 2);
    gtk_widget_show(hbox);
    gtk_box_pack_start(GTK_BOX(vbox), hbox, FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(hbox), radio, FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(hbox), bg_color, TRUE, TRUE, 0);

    /* picture button */
    radio = gtk_radio_button_new_with_label(group, _("Picture"));
    group = gtk_radio_button_group(GTK_RADIO_BUTTON(radio));
    gtk_signal_connect(GTK_OBJECT(radio), "toggled", 
		       GTK_SIGNAL_FUNC(button_picture_checked), NULL);
    gtk_widget_show(GTK_WIDGET(radio));
    if (strcmp(bgtype, "picture") == 0)
	gtk_toggle_button_set_state(GTK_TOGGLE_BUTTON(radio), TRUE);
    hbox = gtk_hbox_new(FALSE, 2);
    gtk_widget_show(hbox);
    gtk_box_pack_start(GTK_BOX(vbox), hbox, FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(hbox), radio, FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(hbox), wall_picture, TRUE, TRUE, 0);
#if !defined(USE_IMLIB) && !defined(USE_GDK_PIXBUF)
    gtk_widget_set_sensitive(radio, 0);
#endif

    /* transparent button */
    radio = gtk_radio_button_new_with_label(group, _("Transparent"));
    group = gtk_radio_button_group(GTK_RADIO_BUTTON(radio));
    gtk_signal_connect(GTK_OBJECT(radio), "toggled",
		       GTK_SIGNAL_FUNC(button_transparent_checked), NULL);
    gtk_widget_show(GTK_WIDGET(radio));
    if (strcmp(bgtype, "transparent") == 0)
	gtk_toggle_button_set_state(GTK_TOGGLE_BUTTON(radio), TRUE);
    hbox = gtk_hbox_new(FALSE, 2);
    gtk_widget_show(hbox);
    gtk_box_pack_start(GTK_BOX(vbox), hbox, FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(hbox), radio, FALSE, FALSE, 0);

    set_sensitive();
    return frame;
}

void
mc_update_bgtype(
	int  save
	)
{
	if( is_changed) {
		if ( ! strcmp( bgtype, "color")) {
		    mc_set_flag_value("use_transbg", 0, save);
		    mc_set_str_value("wall_picture", "none", save);
		    mc_update_bg_color( save) ;
		} else if (!strcmp( bgtype, "picture")) {
		    mc_set_flag_value("use_transbg", 0, save);
		    mc_update_wall_pic( save) ;
		} else if (!strcmp( bgtype, "transparent")) {
		    mc_set_flag_value("use_transbg", 1, save);
		}
	}
}
