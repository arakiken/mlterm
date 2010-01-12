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


typedef struct  cs_info
{
	char *   cs ;

	/*
	 * Default font encoding name.
	 * This used only if xcore font is used.
	 */
	char *   encoding ;

} cs_info_t ;


/* --- static variables --- */

/*
 * Combination of x_font_config.c:cs_table and x_font.c:cs_info_table
 */
static cs_info_t  cs_info_table[] =
{
	/*
	 * ISO10646_1 => ISO10646_UCS4_1 in get_correct_cs().
	 */
	{ "ISO10646_1" , "iso10646-1" , } ,
	{ "ISO10646_1_BIWIDTH" , "iso10646-1" , } ,
#if  0
	/*
	 * Font of these charsets is not manipulated by mlconfig at present.
	 */
	{ "ISO10646_UCS2_1" , "iso10646-1" , } ,
	{ "ISO10646_UCS2_1_BIWIDTH" , "iso10646-1" , } ,
#endif

	{ "DEC_SPECIAL" , "iso8859-1" , } ,
	{ "ISO8859_1" , "iso8859-1" , } ,
	{ "ISO8859_2" , "iso8859-2" , } ,
	{ "ISO8859_3" , "iso8859-3" , } ,
	{ "ISO8859_4" , "iso8859-4" , } ,
	{ "ISO8859_5" , "iso8859-5" , } ,
	{ "ISO8859_6" , "iso8859-6" , } ,
	{ "ISO8859_7" , "iso8859-7" , } ,
	{ "ISO8859_8" , "iso8859-8" , } ,
	{ "ISO8859_9" , "iso8859-9" , } ,
	{ "ISO8859_10" , "iso8859-10" , } ,
	{ "TIS620_2533" , "tis620.2533-1" , } ,
	{ "ISO8859_13" , "iso8859-13" , } ,
	{ "ISO8859_14" , "iso8859-14" , } ,
	{ "ISO8859_15" , "iso8859-15" , } ,
	{ "ISO8859_16" , "iso8859-16" , } ,

	/*
	 * XXX
	 * The encoding of TCVN font is iso8859-1 , and its font family is
	 * .VnTime or .VnTimeH ...
	 * How to deal with it ?
	 */
	{ "TCVN5712" , NULL , } ,
	{ "ISCII" , NULL , } ,

	{ "VISCII" , "viscii-1" , } ,
	{ "KOI8_R" , "koi8-r" , } ,
	{ "KOI8_U" , "koi8-u" , } ,
#if  0
	/*
	 * XXX
	 * KOI8_T, GEORGIAN_PS and CP125X charset can be shown by unicode font only.
	 */
	{ "KOI8_T" , NULL , } ,
	{ "GEORGIAN_PS" , NULL , } ,
#endif
#ifdef  USE_WIN32GUI
	{ "CP1250" , NULL , } ,
	{ "CP1251" , NULL , } ,
	{ "CP1252" , NULL , } ,
	{ "CP1253" , NULL , } ,
	{ "CP1254" , NULL , } ,
	{ "CP1255" , NULL , } ,
	{ "CP1256" , NULL , } ,
	{ "CP1257" , NULL , } ,
	{ "CP1258" , NULL , } ,
#endif

	{ "JISX0201_KATA" , "jisx0201.1976-0" , } ,
	{ "JISX0201_ROMAN" , "jisx0201.1976-0" , } ,
	{ "JISC6226_1978" , "jisx0208.1978-0" , } ,
	{ "JISX0208_1983" , "jisx0208.1983-0" , } ,
	{ "JISX0208_1990" , "jisx0208.1990-0" , } ,
	{ "JISX0212_1990" , "jisx0212.1990-0" , } ,
	{ "JISX0213_2000_1" , "jisx0213.2000-1" , } ,
	{ "JISX0213_2000_2" , "jisx0213.2000-2" , } ,
	{ "KSC5601_1987" , "ksc5601.1987-0" , } ,

#if  0
	/*
	 * XXX
	 * UHC and JOHAB fonts are not used at the present time.
	 * see ml_vt100_parser.c:ml_parse_vt100_sequence().
	 */
	{ "UHC" , NULL , } ,
	{ "JOHAB" , "johabsh-1" , } ,
#endif

	{ "GB2312_80" , "gb2312.1980-0" , } ,
	{ "GBK" , "gbk-0" , } ,
	{ "BIG5" , "big5.eten-0" , } ,
	{ "HKSCS" , "big5hkscs-0" , } ,
	{ "CNS11643_1992_1" , "cns11643.1992-1" , } ,
	{ "CNS11643_1992_2" , "cns11643.1992-2" , } ,
	{ "CNS11643_1992_3" , "cns11643.1992-3" , } ,
	{ "CNS11643_1992_4" , "cns11643.1992-4" , } ,
	{ "CNS11643_1992_5" , "cns11643.1992-5" , } ,
	{ "CNS11643_1992_6" , "cns11643.1992-6" , } ,
	{ "CNS11643_1992_7" , "cns11643.1992-7" , } ,

} ;

static char *  new_fontsize = NULL ;
static char *  old_fontsize = NULL ;
static int is_fontsize_changed ;

static char *  new_fontname_list[sizeof(cs_info_table)/sizeof(cs_info_table[0])] ;
static int selected_cs ;	/* 0 = ISO10646_UCS4_1 */
static GtkWidget *  fontname_entry ;
static GtkWidget *  select_font_button ;
static GtkWidget *  xft_flag ;
static GtkWidget *  aa_flag ;
static GtkWidget *  vcol_flag ;


/* --- static functions  --- */

static void
reset_fontname_list(void)
{
	int  count ;

	for( count = 0 ; count < sizeof(cs_info_table)/sizeof(cs_info_table[0]) ; count++)
	{
		free( new_fontname_list[count]) ;
		new_fontname_list[count] = NULL ;
	}
}

void
save_current_fontname_entry(void)
{
	char *  name ;
		
	if( ! ( name = gtk_entry_get_text(GTK_ENTRY(fontname_entry))))
	{
		if( new_fontname_list[selected_cs])
		{
			/*
			 * Font name for selected_cs was specfied, but now
			 * font name for selected_cs is cleared.
			 */
			name = "" ;
		}
	}

	if( name)
	{
		free( new_fontname_list[selected_cs]) ;
		new_fontname_list[selected_cs] = strdup(name) ;
	}
}

/*
 * If you use functions in mc_io.c, use this function instead of direct
 * access to cs_info_t::cs.
 */
static char *
get_correct_cs(
	int  idx
	)
{
	if( idx < 0)
	{
		return  NULL ;
	}
	else if( idx == 0)
	{
		return  "ISO10646_UCS4_1" ;
	}
	else if( idx == 1)
	{
		return  "ISO10646_UCS4_1_BIWIDTH" ;
	}
	else if( idx < sizeof(cs_info_table) / sizeof(cs_info_table[0]))
	{
		return  cs_info_table[idx].cs ;
	}
	else
	{
		return  NULL ;
	}
}

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
	#if (GTK_MAJOR_VERSION == 1 && GTK_MINOR_VERSION >= 2)
		gtk_widget_set_sensitive( select_font_button , 0) ;
	#elif  (GTK_MAJOR_VERSION >= 2)
		gtk_widget_set_sensitive( select_font_button , 1) ;
	#endif

	#ifdef  USE_TYPE_XFT
		if( ! GTK_TOGGLE_BUTTON(xft_flag)->active)
		{
			gtk_toggle_button_set_active( GTK_TOGGLE_BUTTON(xft_flag) , 1) ;

			reset_fontname_list() ;
			gtk_entry_set_text(GTK_ENTRY(fontname_entry) ,
				g_locale_to_utf8(
					mc_get_font_name( get_font_file() , new_fontsize ,
						get_correct_cs( selected_cs)) ,
					-1 , NULL , NULL , NULL) ) ;
		}
	#endif
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
	#if (GTK_MAJOR_VERSION == 1 && GTK_MINOR_VERSION >= 2)
		gtk_widget_set_sensitive( select_font_button , 0) ;
	#elif  (GTK_MAJOR_VERSION >= 2)
		gtk_widget_set_sensitive( select_font_button , 1) ;
	#endif
	}
	else
	{
		gtk_toggle_button_set_active( GTK_TOGGLE_BUTTON(aa_flag) , 0) ;

	#if (GTK_MAJOR_VERSION == 1 && GTK_MINOR_VERSION >= 2)
		gtk_widget_set_sensitive( select_font_button , 1) ;
	#elif  (GTK_MAJOR_VERSION >= 2)
		gtk_widget_set_sensitive( select_font_button , 0) ;
	#endif
	}

	reset_fontname_list() ;
	gtk_entry_set_text(GTK_ENTRY(fontname_entry) ,
		g_locale_to_utf8(
			mc_get_font_name( get_font_file() , new_fontsize ,
						get_correct_cs( selected_cs)) ,
			-1 , NULL , NULL , NULL) ) ;

	return  1 ;
}

static gint
fontsize_selected(
	GtkWidget *  widget ,
	gpointer  data
	)
{
	free( new_fontsize);
	new_fontsize = gtk_editable_get_chars(GTK_EDITABLE(widget), 0, -1) ;

	reset_fontname_list() ;
	gtk_entry_set_text(GTK_ENTRY(fontname_entry) ,
		g_locale_to_utf8(
			mc_get_font_name( get_font_file() , new_fontsize ,
						get_correct_cs( selected_cs)) ,
			-1 , NULL , NULL , NULL) ) ;
	
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

	for( count = 0 ; count < sizeof(cs_info_table)/sizeof(cs_info_table[0]) ; count++)
	{
		if( strcmp( cs , cs_info_table[count].cs) == 0)
		{
			if( selected_cs != count)
			{
				/* In case fontname is editted in text entry widget. */
				save_current_fontname_entry() ;

			#if  0
				kik_debug_printf( "Before: cs %s fontname %s => " ,
					cs_info_table[selected_cs].cs ,
					new_fontname_list[selected_cs]) ;
			#endif

				selected_cs = count ;
				
				if( new_fontname_list[selected_cs])
				{
					gtk_entry_set_text(GTK_ENTRY(fontname_entry) ,
						new_fontname_list[selected_cs]) ;
				}
				else
				{
					gtk_entry_set_text(GTK_ENTRY(fontname_entry) ,
						g_locale_to_utf8(
							mc_get_font_name( get_font_file() ,
								new_fontsize ,
								get_correct_cs(selected_cs)) ,
							-1 , NULL , NULL , NULL) ) ;
				}
				
			#if  0
				kik_debug_printf( "After: cs %s fontname %s\n" ,
					cs , new_fontname_list[selected_cs]) ;
			#endif

				return  1 ;
			}
		}
	}

	return  0 ;
}

#if (GTK_MAJOR_VERSION == 1 && GTK_MINOR_VERSION >= 2)

void ok_pressed(GtkWidget *widget, gpointer  dialog)
{
	char *  name ;

	name = gtk_font_selection_dialog_get_font_name( GTK_FONT_SELECTION_DIALOG(dialog)) ;
	gtk_entry_set_text( GTK_ENTRY(fontname_entry), name) ;
	g_free( name) ;
	
	gtk_widget_destroy( GTK_WIDGET(dialog)) ;
}

void apply_pressed(GtkWidget *widget, gpointer  dialog)
{
	char *  name ;

	name = gtk_font_selection_dialog_get_font_name( GTK_FONT_SELECTION_DIALOG(dialog)) ;
	gtk_entry_set_text( GTK_ENTRY(fontname_entry), name) ;
	g_free( name) ;
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
	if( ! font_name || ! *font_name ||
		! gtk_font_selection_dialog_set_font_name(
			GTK_FONT_SELECTION_DIALOG(dialog) , font_name))
	{
		char *  encoding ;
		char  format[] = "-misc-fixed-medium-*-normal--%s-*-*-*-*-*-%s" ;
		
		if( ( encoding = cs_info_table[selected_cs].encoding) == NULL)
		{
			encoding = "*-*" ;
		}

		if( ( font_name = malloc( sizeof(format) + strlen(new_fontsize)
					+ strlen(encoding))) == NULL)
		{
			return ;
		}

		sprintf( font_name , format , new_fontsize , encoding) ;
	
		gtk_font_selection_dialog_set_font_name(
			GTK_FONT_SELECTION_DIALOG(dialog) , font_name) ;

		free( font_name) ;
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

	if( ( dup_str = malloc( strlen( str) * 2 + 1)) == NULL)
	{
		free( str) ;

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

	if( ( str = malloc( strlen( font_name) + 1)) == NULL)
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
	if( ! font_name || ! *font_name ||
		! gtk_font_selection_dialog_set_font_name(
			GTK_FONT_SELECTION_DIALOG(dialog) , font_name))
	{
		free( font_name) ;

		if( ( font_name = malloc( 6 + strlen(new_fontsize))) == NULL)
		{
			return ;
		}

		sprintf( font_name , "Sans %s" , new_fontsize) ;
		
		gtk_font_selection_dialog_set_font_name(
			GTK_FONT_SELECTION_DIALOG(dialog) , font_name) ;
	}
	
	free( font_name) ;

	result = gtk_dialog_run(GTK_DIALOG(dialog)) ;
	if (result == GTK_RESPONSE_OK)
	{
		gtk_entry_set_text( GTK_ENTRY(fontname_entry) ,
			my_gtk_font_selection_dialog_get_font_name(
				GTK_FONT_SELECTION_DIALOG(dialog))) ;
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
	char **  cslist ;

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
#if  ! defined(USE_TYPE_XFT) && ! defined(USE_WIN32GUI)
	gtk_widget_set_sensitive(aa_flag, 0) ;
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

	if( ( cslist = alloca( sizeof(char*) * sizeof(cs_info_table) / sizeof(cs_info_table[0]))))
	{
		int  count ;

		for( count = 0 ; count < sizeof(cs_info_table) / sizeof(cs_info_table[0]) ;
			count++)
		{
			cslist[count] = cs_info_table[count].cs ;
		}
		
		combo = mc_combo_new_with_width(_("Font name") , cslist , count ,
				cslist[selected_cs] , 1 ,
				fontcs_selected, NULL, 110) ;
		gtk_box_pack_start(GTK_BOX(hbox) , combo , TRUE , TRUE , 0) ;
	}

	fontname_entry = gtk_entry_new() ;
	gtk_entry_set_text(GTK_ENTRY(fontname_entry),
		g_locale_to_utf8(
			mc_get_font_name( get_font_file() , new_fontsize ,
					get_correct_cs(selected_cs)) ,
			-1 , NULL , NULL , NULL) ) ;
	gtk_widget_show(fontname_entry) ;
	gtk_widget_set_usize(fontname_entry , 100 , 0) ;
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
		free( old_fontsize) ;
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

	/* In case fontname is editted in text entry widget. */
	save_current_fontname_entry() ;
	
	for( count = 0 ; count < sizeof(cs_info_table) / sizeof(cs_info_table[0]) ; count++)
	{
		if( new_fontname_list[count])
		{
			mc_set_font_name( io , get_font_file() , new_fontsize ,
				get_correct_cs( count) , new_fontname_list[count]) ;
		}
	}
}
