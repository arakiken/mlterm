/*
 *	$Id$
 */

#include  "mc_color.h"

#include  <kiklib/kik_str.h>
#include  <stdlib.h>		/* free */
#include  <kiklib/kik_debug.h>
#include  <glib.h>
#include  <c_intl.h>

#include  "mc_combo.h"
#include  "mc_io.h"


#if  0
#define  __DEBUG
#endif


/* --- static variables --- */

#define MC_COLOR_LEN 256

/* colors are stored in untranslated forms */
static char new_color[MC_COLOR_MODES][MC_COLOR_LEN];
static char old_color[MC_COLOR_MODES][MC_COLOR_LEN];
static int is_changed[MC_COLOR_MODES];

static char *configname[MC_COLOR_MODES] = {
	"fg_color",
	"bg_color",
	"sb_fg_color",
	"sb_bg_color"
};

static char *label[MC_COLOR_MODES] = {
	N_("Foreground color"),
	N_("Background color"),
	N_("Foreground color"),
	N_("Background color")
};

static char *colors[] = {
	N_("black"),
	N_("red"),
	N_("green"),
	N_("yellow"),
	N_("blue"),
	N_("magenta"),
	N_("cyan"),
	N_("white"),
	N_("gray"),
	N_("lightgray"),
	N_("pink"),
	N_("brown")
};

/* initialized by init_i18ncolors() */
static char *i18ncolors[sizeof(colors)/sizeof(colors[0])] = {NULL};

/* --- static functions --- */

static void init_i18ncolors(void)
{
	int j;

	if (i18ncolors[0] != NULL) return;
	for (j=0; j<sizeof(colors)/sizeof(colors[0]); j++)
		i18ncolors[j] = _(colors[j]);
	return;
}

static const char *untranslate(const char *color)
{
	int j;

	for(j=0; j<sizeof(colors)/sizeof(colors[0]); j++) {
		if (strcmp(i18ncolors[j], color) == 0) return colors[j];
	}
	return color;
}

static char *translate(char *color)
{
	int j;

	for(j=0; j<sizeof(colors)/sizeof(colors[0]); j++) {
		if (strcmp(colors[j], color) == 0) return i18ncolors[j];
	}
	return color;
}

static char *color_strncpy(char *dst, const char *src)
{
	strncpy(dst, src, MC_COLOR_LEN);
	dst[MC_COLOR_LEN-1] = 0;
	return dst;
}

static int color_selected(GtkWidget *widget, gpointer data)
{
	/* data: pointer for new_color[n] */

	color_strncpy((char *)data,
		      untranslate(gtk_entry_get_text(GTK_ENTRY(widget))));

#ifdef  __DEBUG
	kik_debug_printf(KIK_DEBUG_TAG " %s color is selected.\n", (char *)data);
#endif

	return 1;
}

/* --- global functions --- */

GtkWidget *
mc_color_config_widget_new(int id)
{
	init_i18ncolors();
	color_strncpy(new_color[id], mc_get_str_value(configname[id]));
	color_strncpy(old_color[id], mc_get_str_value(configname[id]));
	is_changed[id] = 0;

	return mc_combo_new(_(label[id]), i18ncolors,
			    sizeof(colors)/sizeof(colors[0]),
			    translate(new_color[id]), 0, color_selected,
			    (gpointer)(new_color+id));
}

void
mc_update_color(int id)
{
	if (strcmp(new_color[id], old_color[id]) != 0) is_changed[id] = 1;

	if (is_changed[id]) {
		mc_set_str_value(configname[id], new_color[id]);
		strcpy(old_color[id], new_color[id]);
	}
}
