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

#define MC_BG_TRANSPARENT 0
#define MC_BG_WALLPICTURE 1
#define MC_BG_COLOR       2

/* --- static variables --- */

static int  bgtype ;
static int  is_changed ;
static GtkWidget *  bg_color ;
static GtkWidget *  wall_picture ;


/* --- static functions --- */

static int
get_bgtype(void)
{
	char *  val;
	
	if (mc_get_flag_value("use_transbg")) return MC_BG_TRANSPARENT ;
	else if ((val = mc_get_str_value("wall_picture")) && *val != '\0') return MC_BG_WALLPICTURE ;
	else return  MC_BG_COLOR ;
}

static void
set_sensitive(void)
{
    if (bgtype == MC_BG_COLOR) {
	gtk_widget_set_sensitive(bg_color, 1);
	gtk_widget_set_sensitive(wall_picture, 0);
    } else if (bgtype == MC_BG_WALLPICTURE) {
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
	bgtype = MC_BG_COLOR; is_changed = 1;
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
	bgtype = MC_BG_TRANSPARENT; is_changed = 1;
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
	bgtype = MC_BG_WALLPICTURE; is_changed = 1;
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
    if (bgtype == MC_BG_COLOR)
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
    if (bgtype == MC_BG_WALLPICTURE)
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
    if (bgtype == MC_BG_TRANSPARENT)
	gtk_toggle_button_set_state(GTK_TOGGLE_BUTTON(radio), TRUE);
    hbox = gtk_hbox_new(FALSE, 2);
    gtk_widget_show(hbox);
    gtk_box_pack_start(GTK_BOX(vbox), hbox, FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(hbox), radio, FALSE, FALSE, 0);

    set_sensitive();
    return frame;
}

void
mc_update_bgtype(void)
{
	if ( bgtype == MC_BG_COLOR) {
	    if( is_changed) {
		mc_set_flag_value("use_transbg", 0);
		mc_set_str_value("wall_picture", "none");
	    }
	    mc_update_bg_color() ;
	} else if ( bgtype == MC_BG_WALLPICTURE) {
	    if( is_changed) {
	        mc_set_flag_value("use_transbg", 0);
	    }
	    mc_update_wall_pic() ;
	} else if ( bgtype == MC_BG_TRANSPARENT) {
	    if( is_changed) {
	        mc_set_flag_value("use_transbg", 1);
		mc_set_str_value("wall_picture", "none");
	    }
	}
}
