/*
 *	$Id$
 */

#include  "mc_bgtype.h"

#include  <glib.h>
#include  <c_intl.h>


#if  0
#define  __DEBUG
#endif


/* --- static variables --- */

static char *  new_bgtype ;
static int  is_changed ;
static GtkWidget *color, *picture;


/* --- static functions --- */

static void
set_sensitive(void)
{
    if (!strcmp(new_bgtype, "color")) {
	gtk_widget_set_sensitive(color, 1);
	gtk_widget_set_sensitive(picture, 0);
    } else if (!strcmp(new_bgtype, "picture")) {
	gtk_widget_set_sensitive(color, 0);
	gtk_widget_set_sensitive(picture, 1);
    } else {
	gtk_widget_set_sensitive(color, 0);
	gtk_widget_set_sensitive(picture, 0);
    }
}

static gint
button_color_checked(
	GtkWidget *  widget ,
	gpointer  data
	)
{
    if (GTK_TOGGLE_BUTTON(widget)->active) {
	new_bgtype = "color"; is_changed = 1;
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
	new_bgtype = "transparent"; is_changed = 1;
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
	new_bgtype = "picture"; is_changed = 1;
	set_sensitive();
    }
    return  1 ;
}


/* --- global functions --- */

GtkWidget *
mc_bgtype_config_widget_new(
	char *  bgtype,
	GtkWidget * bgcolor,
	GtkWidget * bgpicture
	)
{
    GtkWidget *frame;
    GtkWidget *vbox;
    GtkWidget *hbox;
    GtkWidget *radio;
    GSList *group ;

    color = bgcolor;
    picture = bgpicture;
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
    gtk_box_pack_start(GTK_BOX(hbox), color, TRUE, TRUE, 0);

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
    gtk_box_pack_start(GTK_BOX(hbox), picture, TRUE, TRUE, 0);
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

    new_bgtype = bgtype;
    set_sensitive();
    return frame;
}

int
mc_bgtype_ischanged(void)
{
    return is_changed;
}

char *
mc_get_bgtype_mode(void)
{
    is_changed = 0;
    return new_bgtype;
}
