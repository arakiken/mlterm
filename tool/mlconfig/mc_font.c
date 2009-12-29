/*
 *	$Id$
 */

#include  "mc_font.h"

#include  <stdio.h>
#include  <kiklib/kik_str.h>
#include  <kiklib/kik_mem.h>		/* free */
#include  <kiklib/kik_debug.h>
#include  <glib.h>
#include  <c_intl.h>

#include  "mc_combo.h"
#include  "mc_flags.h"
#include  "mc_vertical.h"


#if  0
#define  __DEBUG
#endif

/* XXX Hack */
#ifdef  NO_G_LOCALE
#define  g_locale_to_utf8(a,b,c,d,e) (a)
#endif


/* --- static variables --- */

static char *  new_fontsize = NULL ;
static char *  old_fontsize = NULL ;
static int is_fontsize_changed ;

static char *  cslist[] =
{
	"DEC_SPECIAL" ,
	"ISO8859_1" ,
	"ISO8859_2" ,
	"ISO8859_3" ,
	"ISO8859_4" ,
	"ISO8859_5" ,
	"ISO8859_6" ,
	"ISO8859_7" ,
	"ISO8859_8" ,
	"ISO8859_9" ,
	"ISO8859_10" ,
	"TIS620" ,
	"ISO8859_13" ,
	"ISO8859_14" ,
	"ISO8859_15" ,
	"ISO8859_16" ,
	"TCVN5712" ,
	"ISCII" ,
	"VISCII" ,
	"KOI8_R" ,
	"KOI8_U" ,
	"JISX0201_KATA" ,
	"JISX0201_ROMAN" ,
	"JISX0208_1978" ,
	"JISC6226_1978" ,
	"JISX0208_1983" ,
	"JISX0208_1990" ,
	"JISX0212_1990" ,
	"JISX0213_2000_1" ,
	"JISX0213_2000_2" ,
	"KSC5601_1987" ,
	"KSX1001_1997" ,
	"UHC" ,
	"JOHAB" ,
	"GB2312_80" ,
	"GBK" ,
	"BIG5" ,
	"HKSCS" ,
	"CNS11643_1992_1" ,
	"CNS11643_1992_2" ,
	"CNS11643_1992_3" ,
	"CNS11643_1992_4" ,
	"CNS11643_1992_5" ,
	"CNS11643_1992_6" ,
	"CNS11643_1992_7" ,
	"ISO10646_UCS2_1" ,
	"ISO10646_UCS4_1" ,
} ;
static char *  new_fontname[sizeof(cslist)/sizeof(cslist[0])] ;
static int selected_cs ;
static GtkWidget *  fontname_entry ;
static GtkWidget *  select_font_button ;
static GtkWidget *  xft_flag ;
static GtkWidget *  aa_flag ;
static GtkWidget *  vcol_flag ;


/* --- static functions  --- */

static char *
get_font_file(void)
{
	if( GTK_TOGGLE_BUTTON(xft_flag)->active)
	{
		if( GTK_TOGGLE_BUTTON(vcol_flag)->active)
		{
			return "vaafont" ;
		}
		else if( mc_is_vertical())
		{
			return "taafont" ;
		}
		else
		{
			return "aafont" ;
		}
	}
	else
	{
		if( GTK_TOGGLE_BUTTON(vcol_flag)->active)
		{
			return "vfont" ;
		}
		else if( mc_is_vertical())
		{
			return "tfont" ;
		}
		else
		{
			return "font" ;
		}
	}
}

static gint
aa_flag_checked(
	GtkWidget *  widget ,
	gpointer  data
	)
{
	if( GTK_TOGGLE_BUTTON(widget)->active)
	{
		if( ! mc_gui_is_win32())
		{
		#if (GTK_MAJOR_VERSION == 1 && GTK_MINOR_VERSION >= 2)
			gtk_widget_set_sensitive( select_font_button , 0) ;
		#elif  (GTK_MAJOR_VERSION >= 2)
			gtk_widget_set_sensitive( select_font_button , 1) ;
		#endif
		}

		if( ! GTK_TOGGLE_BUTTON(xft_flag)->active)
		{
			gtk_toggle_button_set_active( GTK_TOGGLE_BUTTON(xft_flag) , 1) ;
		
			gtk_entry_set_text(GTK_ENTRY(fontname_entry) ,
				g_locale_to_utf8(
					mc_get_font_name( get_font_file() , new_fontsize ,
						cslist[selected_cs]) ,
					-1 , NULL , NULL , NULL) ) ;
		}
	}
	
	return  1 ;
}

static gint
xft_flag_checked(
	GtkWidget *  widget ,
	gpointer  data
	)
{
	if( GTK_TOGGLE_BUTTON(widget)->active)
	{
		if( ! mc_gui_is_win32())
		{
		#if (GTK_MAJOR_VERSION == 1 && GTK_MINOR_VERSION >= 2)
			gtk_widget_set_sensitive( select_font_button , 0) ;
		#elif  (GTK_MAJOR_VERSION >= 2)
			gtk_widget_set_sensitive( select_font_button , 1) ;
		#endif
		}
	}
	else
	{
		gtk_toggle_button_set_active( GTK_TOGGLE_BUTTON(aa_flag) , 0) ;

		if( ! mc_gui_is_win32())
		{
		#if (GTK_MAJOR_VERSION == 1 && GTK_MINOR_VERSION >= 2)
			gtk_widget_set_sensitive( select_font_button , 1) ;
		#elif  (GTK_MAJOR_VERSION >= 2)
			gtk_widget_set_sensitive( select_font_button , 0) ;
		#endif
		}
	}

	gtk_entry_set_text(GTK_ENTRY(fontname_entry) ,
		g_locale_to_utf8(
			mc_get_font_name( get_font_file() , new_fontsize , cslist[selected_cs]) ,
			-1 , NULL , NULL , NULL) ) ;

	return  1 ;
}

static gint
fontsize_selected(
	GtkWidget *  widget ,
	gpointer  data
	)
{
	g_free( new_fontsize);
	new_fontsize = gtk_editable_get_chars(GTK_EDITABLE(widget), 0, -1) ;
	
#ifdef  __DEBUG
	kik_debug_printf( KIK_DEBUG_TAG " %s font size is selected.\n" , new_fontsize) ;
#endif

	return  1 ;
}

static gint
fontcs_selected(
	GtkWidget *  widget ,
	gpointer  data
	)
{
	char *  cs ;
	int  count ;

	cs = gtk_entry_get_text(GTK_ENTRY(widget)) ;
	
	for( count = 0 ; count < sizeof(cslist)/sizeof(cslist[0]) ; count++)
	{
		if( strcmp( cs , cslist[count]) == 0)
		{
			selected_cs = count ;

			break ;
		}
	}
	
	gtk_entry_set_text(GTK_ENTRY(fontname_entry) ,
		g_locale_to_utf8(
			mc_get_font_name( get_font_file() , new_fontsize , cslist[selected_cs]) ,
			-1 , NULL , NULL , NULL) ) ;
	
	return  1 ;
}

#if (GTK_MAJOR_VERSION == 1 && GTK_MINOR_VERSION >= 2)

void ok_pressed(GtkWidget *widget, gpointer  dialog)
{
	g_free( new_fontname[selected_cs]) ;
	new_fontname[selected_cs] = gtk_font_selection_dialog_get_font_name(
						GTK_FONT_SELECTION_DIALOG(dialog)) ;
	gtk_entry_set_text(GTK_ENTRY(fontname_entry), new_fontname[selected_cs]) ;

#if  0
	printf( "%s=%s\n" , cslist[selected_cs] , new_fontname[selected_cs]) ;
	fflush(NULL) ;
#endif

	gtk_widget_destroy( GTK_WIDGET(dialog)) ;
}

void apply_pressed(GtkWidget *widget, gpointer  dialog)
{
	g_free( new_fontname[selected_cs]) ;
	new_fontname[selected_cs] = gtk_font_selection_dialog_get_font_name(
						GTK_FONT_SELECTION_DIALOG(dialog)) ;
	gtk_entry_set_text(GTK_ENTRY(fontname_entry), new_fontname[selected_cs]) ;

#if  0
	printf( "%s=%s\n" , cslist[selected_cs] , new_fontname[selected_cs]) ;
	fflush(NULL) ;
#endif
}

void cancel_pressed(GtkWidget *widget, gpointer  dialog)
{
	gtk_widget_destroy( GTK_WIDGET(dialog)) ;
}

void select_font(GtkWidget *widget, gpointer label)
{
	GtkWidget *  dialog ;
	char *  font_name ;

	dialog = gtk_font_selection_dialog_new( "Select Font") ;

	font_name = gtk_entry_get_text(GTK_ENTRY(fontname_entry)) ;
	if( font_name && *font_name)
	{
		gtk_font_selection_dialog_set_font_name(
			GTK_FONT_SELECTION_DIALOG(dialog) , font_name) ;
	}
	
	gtk_signal_connect(GTK_OBJECT(GTK_FONT_SELECTION_DIALOG(dialog)->ok_button) ,
		"clicked" , GTK_SIGNAL_FUNC(ok_pressed) , dialog) ;
	gtk_signal_connect(GTK_OBJECT(GTK_FONT_SELECTION_DIALOG(dialog)->apply_button) ,
		"clicked" , GTK_SIGNAL_FUNC(apply_pressed) , dialog) ;
	gtk_signal_connect(GTK_OBJECT(GTK_FONT_SELECTION_DIALOG(dialog)->cancel_button) ,
		"clicked" , GTK_SIGNAL_FUNC(cancel_pressed) , dialog) ;

	gtk_widget_show_all( dialog) ;
}

#elif  (GTK_MAJOR_VERSION >= 2)

static char *
my_gtk_font_selection_dialog_get_font_name(
	GtkFontSelectionDialog *  dialog
	)
{
	char *  str ;
	int  count ;
	char *  p ;
	char *  dup_str ;

	str = gtk_font_selection_dialog_get_font_name( dialog) ;

	if( ( dup_str = g_malloc( strlen( str) * 2 + 1)) == NULL)
	{
		g_free( str) ;

		return  NULL ;
	}

	/* Escape '-' */
	count = 0 ;
	p = str ;
	while( *p)
	{
		if( *p == '-')
		{
			dup_str[count++] = '\\' ;
		}

		dup_str[count++] = *(p++) ;
	}

	dup_str[count] = '\0' ;

	g_free( str) ;

	return  dup_str ;
}

static char *
get_gtk_font_name(
	char *  font_name
	)
{
	int  count ;
	char *  str ;

	if( ( str = g_malloc( strlen( font_name) + 1)) == NULL)
	{
		return  NULL ;
	}

	count = 0 ;
	while( *font_name && *font_name != '-')
	{
		if( *font_name == '\\')
		{
			font_name ++ ;
		}

		str[count++] = *(font_name++) ;
	}

	str[count] = '\0' ;

	return  str ;
}

void select_font(GtkWidget *  widget, gpointer  p)
{
	GtkWidget *  dialog ;
	char *  font_name ;
	GtkResponseType result ;

	dialog = gtk_font_selection_dialog_new("Select Font") ;

	font_name = get_gtk_font_name( gtk_entry_get_text(GTK_ENTRY(fontname_entry))) ;
	if( font_name && *font_name)
	{
		gtk_font_selection_dialog_set_font_name(
			GTK_FONT_SELECTION_DIALOG(dialog) , font_name) ;
		g_free( font_name) ;
	}

	result = gtk_dialog_run(GTK_DIALOG(dialog)) ;
	if (result == GTK_RESPONSE_OK)
	{
		g_free( new_fontname[selected_cs]) ;
		new_fontname[selected_cs] = my_gtk_font_selection_dialog_get_font_name(
						GTK_FONT_SELECTION_DIALOG(dialog)) ;
		gtk_entry_set_text(GTK_ENTRY(fontname_entry), new_fontname[selected_cs]) ;

#if  0
		printf( "%s=%s\n" , cslist[selected_cs] , new_fontname[selected_cs]) ;
		fflush(NULL) ;
#endif
	}

	gtk_widget_destroy( dialog) ;
}

#else

void select_font(GtkWidget *  widget , gpointer  p)
{
}

#endif


/* --- global functions --- */

GtkWidget *
mc_font_config_widget_new(void)
{
	GtkWidget *  vbox ;
	GtkWidget *  hbox ;
	GtkWidget *  combo ;
	GtkWidget *  button ;
	char *  fontlist[] =
	{
		"6" , "7" , "8" , "9" , "10" ,
		"11" , "12" , "13" , "14" , "15" , "16" , "17" , "18" , "19" , "20" ,
		"21" , "22" , "23" , "24" , "25" , "26" , "27" , "28" , "29" , "30" ,
	} ;

	new_fontsize = strdup( old_fontsize = mc_get_str_value( "fontsize")) ;
	is_fontsize_changed = 0;

	vbox = gtk_vbox_new(FALSE , 0) ;

	combo = mc_combo_new_with_width(_("Font size (pixels)"), fontlist ,
		sizeof(fontlist) / sizeof(fontlist[0]), new_fontsize , 1 ,
		fontsize_selected, NULL, 80) ;
	gtk_box_pack_start(GTK_BOX(vbox) , combo , TRUE , TRUE , 0) ;


	hbox = gtk_hbox_new(FALSE , 0) ;
	gtk_widget_show(hbox) ;

	xft_flag = mc_flag_config_widget_new( MC_FLAG_XFT) ;
	gtk_widget_show( xft_flag) ;
	gtk_box_pack_start(GTK_BOX(hbox) , xft_flag , TRUE , TRUE , 0) ;
	gtk_signal_connect(GTK_OBJECT(xft_flag) , "toggled" ,
		GTK_SIGNAL_FUNC(xft_flag_checked) , NULL) ;
	if( mc_gui_is_win32())
	{
		gtk_widget_set_sensitive(xft_flag, 0);
	}
	
	aa_flag = mc_flag_config_widget_new( MC_FLAG_AA) ;
#ifndef USE_TYPE_XFT
	gtk_widget_set_sensitive(aa_flag, 0);
#endif
	gtk_widget_show( aa_flag) ;
	gtk_box_pack_start(GTK_BOX(hbox) , aa_flag , TRUE , TRUE , 0) ;
	gtk_signal_connect(GTK_OBJECT(aa_flag) , "toggled" ,
		GTK_SIGNAL_FUNC(aa_flag_checked) , NULL) ;
	
	if( GTK_TOGGLE_BUTTON(aa_flag)->active)
	{
		gtk_toggle_button_set_active( GTK_TOGGLE_BUTTON(xft_flag) , 1) ;
	}
	
	vcol_flag = mc_flag_config_widget_new( MC_FLAG_VCOL) ;
	gtk_widget_show( vcol_flag) ;
	gtk_box_pack_start(GTK_BOX(hbox) , vcol_flag , TRUE , TRUE , 0) ;

	gtk_box_pack_start(GTK_BOX(vbox) , hbox , TRUE , TRUE , 0) ;


	button = mc_vertical_config_widget_new() ;
	gtk_widget_show(button) ;
	gtk_box_pack_start(GTK_BOX(vbox), button, TRUE, TRUE, 0);


	hbox = gtk_hbox_new(FALSE , 0) ;
	gtk_widget_show(hbox) ;

	selected_cs = 1 ;	/* ISO8859_1 */
	
	combo = mc_combo_new_with_width(_("Font name"), cslist ,
			sizeof(cslist) / sizeof(cslist[0]), cslist[selected_cs] , 1 ,
			fontcs_selected, NULL, 80) ;
	gtk_box_pack_start(GTK_BOX(hbox) , combo , TRUE , TRUE , 0) ;

	fontname_entry = gtk_entry_new() ;
	gtk_entry_set_text(GTK_ENTRY(fontname_entry),
		g_locale_to_utf8(
			mc_get_font_name( get_font_file() , new_fontsize , cslist[selected_cs]) ,
			-1 , NULL , NULL , NULL) ) ;
	gtk_widget_show(fontname_entry) ;
	gtk_box_pack_start(GTK_BOX(hbox) , fontname_entry , TRUE , TRUE , 1) ;

	select_font_button = gtk_button_new_with_label( _("Select")) ;
	gtk_widget_show( select_font_button) ;
	gtk_box_pack_start(GTK_BOX(hbox) , select_font_button , TRUE , TRUE , 1) ;
	gtk_signal_connect(GTK_OBJECT(select_font_button) , "clicked" ,
		GTK_SIGNAL_FUNC(select_font) , NULL) ;

	if( ! mc_gui_is_win32())
	{
		if( GTK_TOGGLE_BUTTON(xft_flag)->active)
		{
		#if (GTK_MAJOR_VERSION == 1 && GTK_MINOR_VERSION >= 2)
			gtk_widget_set_sensitive( select_font_button , 0) ;
		#endif
		}
		else
		{
		#if (GTK_MAJOR_VERSION >= 2)
			gtk_widget_set_sensitive( select_font_button , 0) ;
		#endif
		}
	}
	
	gtk_box_pack_start(GTK_BOX(vbox) , hbox , TRUE , TRUE , 0) ;

	return  vbox ;
}

void
mc_update_font_misc(void)
{
	if (strcmp(new_fontsize, old_fontsize)) is_fontsize_changed = 1;

	if (is_fontsize_changed)
	{
		mc_set_str_value( "fontsize" , new_fontsize) ;
		g_free( old_fontsize) ;
		old_fontsize = strdup( new_fontsize) ;
	}

	/*
	 * MC_FLAG_XFT should be updated last because MC_FLAG_XFT is
	 * invalid in some environments.
	 */
	mc_update_flag_mode( MC_FLAG_AA) ;
	mc_update_flag_mode( MC_FLAG_VCOL) ;
	mc_update_flag_mode( MC_FLAG_XFT) ;
	mc_update_vertical_mode() ;
}

void
mc_update_font_name(mc_io_t  io)
{
	int  count ;
	
	for( count = 0 ; count < sizeof(cslist) / sizeof(cslist[0]) ; count++)
	{
		if( new_fontname[count] && *new_fontname[count])
		{
			mc_set_font_name( io , get_font_file() , new_fontsize , cslist[count] ,
				new_fontname[count]) ;
		}
	}
}
