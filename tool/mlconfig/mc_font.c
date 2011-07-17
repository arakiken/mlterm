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

#include  "gtkxlfdsel.h"

#include  "mc_combo.h"
#include  "mc_flags.h"
#include  "mc_vertical.h"


#if  0
#define  __DEBUG
#endif


typedef struct  cs_info
{
	char *   cs ;

	/*
	 * Default font encoding name.
	 * This used only if xcore font is used.
	 *
	 * !! Notice !!
	 * The last element must be NULL.
	 */
	char *   encoding_names[3] ;

} cs_info_t ;


/* --- static variables --- */

/*
 * Combination of x_font_config.c:cs_table and x_font.c:cs_info_table
 */
static cs_info_t  cs_info_table[] =
{
	{ "DEFAULT" , { NULL , NULL , NULL , } , } ,
	
	/*
	 * ISO10646_1 => ISO10646_UCS4_1 in get_correct_cs().
	 */
	{ "ISO10646_1" , { "iso10646-1" , NULL , NULL , } , } ,
	{ "ISO10646_1_BIWIDTH" , { "iso10646-1" , NULL , NULL , } , } ,

#if  0
	/*
	 * Font of these charsets is not manipulated by mlconfig at present.
	 */
	{ "ISO10646_UCS2_1" , { "iso10646-1" , NULL , NULL , } , } ,
	{ "ISO10646_UCS2_1_BIWIDTH" , { "iso10646-1"  , NULL , NULL , } , } ,
#endif

	{ "DEC_SPECIAL" , { "iso8859-1" , NULL , NULL , } , } ,
	{ "ISO8859_1" , { "iso8859-1" , NULL , NULL , } , } ,
	{ "ISO8859_2" , { "iso8859-2" , NULL , NULL , } , } ,
	{ "ISO8859_3" , { "iso8859-3" , NULL , NULL , } , } ,
	{ "ISO8859_4" , { "iso8859-4" , NULL , NULL , } , } ,
	{ "ISO8859_5" , { "iso8859-5" , NULL , NULL , } , } ,
	{ "ISO8859_6" , { "iso8859-6" , NULL , NULL , } , } ,
	{ "ISO8859_7" , { "iso8859-7" , NULL , NULL , } , } ,
	{ "ISO8859_8" , { "iso8859-8" , NULL , NULL , } , } ,
	{ "ISO8859_9" , { "iso8859-9" , NULL , NULL , } , } ,
	{ "ISO8859_10" , { "iso8859-10" , NULL , NULL , } , } ,
	{ "TIS620" , { "tis620.2533-1" , "tis620.2529-1" , NULL , } , } ,
	{ "ISO8859_13" , { "iso8859-13" , NULL , NULL , } , } ,
	{ "ISO8859_14" , { "iso8859-14" , NULL , NULL , } , } ,
	{ "ISO8859_15" , { "iso8859-15" , NULL , NULL , } , } ,
	{ "ISO8859_16" , { "iso8859-16" , NULL , NULL , } , } ,

	/*
	 * XXX
	 * The encoding of TCVN font is iso8859-1 , and its font family is
	 * .VnTime or .VnTimeH ...
	 * How to deal with it ?
	 */
	{ "TCVN5712" , { NULL , NULL , NULL , } , } ,

	{ "ISCII_ASSAMESE" , { NULL , NULL , NULL , } , } ,
	{ "ISCII_BENGALI" , { NULL , NULL , NULL , } , } ,
	{ "ISCII_GUJARATI" , { NULL , NULL , NULL , } , } ,
	{ "ISCII_HINDI" , { NULL , NULL , NULL , } , } ,
	{ "ISCII_KANNADA" , { NULL , NULL , NULL , } , } ,
	{ "ISCII_MALAYALAM" , { NULL , NULL , NULL , } , } ,
	{ "ISCII_ORIYA" , { NULL , NULL , NULL , } , } ,
	{ "ISCII_PUNJABI" , { NULL , NULL , NULL , } , } ,
	{ "ISCII_ROMAN" , { NULL , NULL , NULL , } , } ,
	{ "ISCII_TAMIL" , { NULL , NULL , NULL , } , } ,
	{ "ISCII_TELUGU" , { NULL , NULL , NULL , } , } ,
	{ "VISCII" , { "viscii-1" , NULL , NULL , } , } ,
	{ "KOI8_R" , { "koi8-r" , NULL , NULL , } , } ,
	{ "KOI8_U" , { "koi8-u" , NULL , NULL , } , } ,

#if  0
	/*
	 * XXX
	 * KOI8_T, GEORGIAN_PS and CP125X charset can be shown by unicode font only.
	 */
	{ "KOI8_T" , { NULL , NULL , NULL , } , } ,
	{ "GEORGIAN_PS" , { NULL , NULL , NULL , } , } ,
#endif
#ifdef  USE_WIN32GUI
	{ "CP1250" , { NULL , NULL , NULL , } , } ,
	{ "CP1251" , { NULL , NULL , NULL , } , } ,
	{ "CP1252" , { NULL , NULL , NULL , } , } ,
	{ "CP1253" , { NULL , NULL , NULL , } , } ,
	{ "CP1254" , { NULL , NULL , NULL , } , } ,
	{ "CP1255" , { NULL , NULL , NULL , } , } ,
	{ "CP1256" , { NULL , NULL , NULL , } , } ,
	{ "CP1257" , { NULL , NULL , NULL , } , } ,
	{ "CP1258" , { NULL , NULL , NULL , } , } ,
	{ "CP874" , { NULL , NULL , NULL , } , } ,
#endif

	{ "JISX0201_KATA" , { "jisx0201.1976-0" , NULL , NULL , } , } ,
	{ "JISX0201_ROMAN" , { "jisx0201.1976-0" , NULL , NULL , } , } ,
	{ "JISX0208_1978" , { "jisx0208.1978-0" , "jisx0208.1983-0" , NULL , } , } ,
	{ "JISX0208_1983" , { "jisx0208.1983-0" , "jisx0208.1990-0" , NULL , } , } ,
	{ "JISX0208_1990" , { "jisx0208.1990-0" , "jisx0208.1983-0" , NULL , } , } ,
	{ "JISX0212_1990" , { "jisx0212.1990-0" , NULL , NULL , } , } ,
	{ "JISX0213_2000_1" , { "jisx0213.2000-1" , NULL , NULL , } , } ,
	{ "JISX0213_2000_2" , { "jisx0213.2000-2" , NULL , NULL , } , } ,
	{ "KSX1001_1997" , { "ksc5601.1987-0" , "ksx1001.1997-0" , NULL , } , } ,

#if  0
	/*
	 * XXX
	 * UHC and JOHAB fonts are not used at the present time.
	 * see ml_vt100_parser.c:ml_parse_vt100_sequence().
	 */
	{ "UHC" , { NULL , NULL , NULL , } , } ,
	{ "JOHAB" , { "johabsh-1" , /* "johabs-1" , */ "johab-1" , NULL , } , } ,
#endif

	{ "GB2312_80" , { "gb2312.1980-0" , NULL , NULL , } , } ,
	{ "GBK" , { "gbk-0" , NULL , NULL , } , } ,
	{ "BIG5" , { "big5.eten-0" , "big5.hku-0" , NULL , } , } ,
	{ "HKSCS" , { "big5hkscs-0" , "big5-0" , NULL , } , } ,
	{ "CNS11643_1992_1" , { "cns11643.1992-1" , "cns11643.1992.1-0" , NULL , } , } ,
	{ "CNS11643_1992_2" , { "cns11643.1992-2" , "cns11643.1992.2-0" , NULL , } , } ,
	{ "CNS11643_1992_3" , { "cns11643.1992-3" , "cns11643.1992.3-0" , NULL , } , } ,
	{ "CNS11643_1992_4" , { "cns11643.1992-4" , "cns11643.1992.4-0" , NULL , } , } ,
	{ "CNS11643_1992_5" , { "cns11643.1992-5" , "cns11643.1992.5-0" , NULL , } , } ,
	{ "CNS11643_1992_6" , { "cns11643.1992-6" , "cns11643.1992.6-0" , NULL , } , } ,
	{ "CNS11643_1992_7" , { "cns11643.1992-7" , "cns11643.1992.7-0" , NULL , } , } ,

} ;

static char *  new_fontsize = NULL ;
static char *  old_fontsize = NULL ;
static int is_fontsize_changed ;

static char *  new_fontname_list[sizeof(cs_info_table)/sizeof(cs_info_table[0])] ;
static int  selected_cs = 0 ;	/* 0 = DEFAULT */
static GtkWidget *  fontname_entry ;
static GtkWidget *  select_font_button ;
static GtkWidget *  xft_flag ;
static GtkWidget *  cairo_flag ;
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
	else if( idx == 1)
	{
		return  "ISO10646_UCS4_1" ;
	}
	else if( idx == 2)
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
	if( GTK_TOGGLE_BUTTON(xft_flag)->active || GTK_TOGGLE_BUTTON(cairo_flag)->active)
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
		if( ! GTK_TOGGLE_BUTTON(xft_flag)->active &&
		    ! GTK_TOGGLE_BUTTON(cairo_flag)->active)
		{
		#if  defined(USE_TYPE_XFT) || defined(USE_TYPE_CAIRO)
		#ifdef  USE_TYPE_XFT
			gtk_toggle_button_set_active( GTK_TOGGLE_BUTTON(xft_flag) , 1) ;
		#else
			gtk_toggle_button_set_active( GTK_TOGGLE_BUTTON(cairo_flag) , 1) ;
		#endif
		
			reset_fontname_list() ;
			gtk_entry_set_text(GTK_ENTRY(fontname_entry) ,
				g_locale_to_utf8(
					mc_get_font_name( get_font_file() , new_fontsize ,
						get_correct_cs( selected_cs)) ,
					-1 , NULL , NULL , NULL) ) ;
		#endif
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
	if( ! GTK_TOGGLE_BUTTON(widget)->active)
	{
		gtk_toggle_button_set_active( GTK_TOGGLE_BUTTON(aa_flag) , 0) ;
	}
	else
	{
		gtk_toggle_button_set_active( GTK_TOGGLE_BUTTON(cairo_flag) , 0) ;
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
cairo_flag_checked(
	GtkWidget *  widget ,
	gpointer  data
	)
{
	if( ! GTK_TOGGLE_BUTTON(widget)->active)
	{
		gtk_toggle_button_set_active( GTK_TOGGLE_BUTTON(aa_flag) , 0) ;
	}
	else
	{
		gtk_toggle_button_set_active( GTK_TOGGLE_BUTTON(xft_flag) , 0) ;
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
vcol_flag_checked(
	GtkWidget *  widget ,
	gpointer  data
	)
{
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


#if  ! defined(USE_WIN32GUI) && ! defined(G_PLATFORM_WIN32)

static gchar *
get_xlfd_font_name(
	gpointer  dialog
	)
{
	char *  name ;
	
	name = gtk_xlfd_selection_dialog_get_font_name( GTK_XLFD_SELECTION_DIALOG(dialog)) ;
	if( selected_cs == 0 && name && *name)	/* DEFAULT */
	{
		/*
		 * Removing font encoding such as "iso8859-1".
		 */
		 
		char *  p ;
		
		if( ( p = strrchr( name , '-')))
		{
			*p = '\0' ;
			if( ( p = strrchr( name , '-')))
			{
				*(p + 1) = '\0' ;
			}
		}
	}

	return  name ;
}

static void
ok_pressed(
	GtkWidget *  widget,
	gpointer  dialog
	)
{
	gchar *  name ;

	name = get_xlfd_font_name(dialog) ;
	gtk_entry_set_text( GTK_ENTRY(fontname_entry), name) ;
	g_free( name) ;
	
	gtk_widget_destroy( GTK_WIDGET(dialog)) ;
}

static void
apply_pressed(
	GtkWidget *  widget,
	gpointer  dialog
	)
{
	gchar *  name ;

	name = get_xlfd_font_name(dialog) ;
	gtk_entry_set_text( GTK_ENTRY(fontname_entry), name) ;
	g_free( name) ;
}

static void
cancel_pressed(
	GtkWidget *  widget,
	gpointer  dialog
	)
{
	gtk_widget_destroy( GTK_WIDGET(dialog)) ;
}

static int
set_current_font_name(
	GtkWidget *  dialog
	)
{
	gchar *  font_name ;

	font_name = gtk_entry_get_text(GTK_ENTRY(fontname_entry)) ;
	if( font_name && *font_name)
	{
		char *  p ;

		/*
		 * Modify DEFAULT font name. "-*-...-*-" => "-*-...-*-*-*"
		 */
		if( selected_cs == 0 && ( p = alloca( strlen( font_name) + 4)))
		{
			sprintf( p , "%s*-*" , font_name) ;
			font_name = p ;
		}

		return  gtk_xlfd_selection_dialog_set_font_name(
				GTK_XLFD_SELECTION_DIALOG(dialog) , font_name) ;
	}
	else
	{
		return  0 ;
	}
}

static void
select_xlfd_font(
	GtkWidget *  widget ,
	gpointer label
	)
{
	GtkWidget *  dialog ;

	dialog = gtk_xlfd_selection_dialog_new( "Select Font") ;
	if( ! set_current_font_name( dialog))
	{
		char *  encoding ;
		char *  font_name ;
		char  format[] = "-misc-fixed-medium-*-normal--%s-*-*-*-*-*-%s" ;
		
		if( ( encoding = cs_info_table[selected_cs].encoding_names[0]) == NULL)
		{
			encoding = "*-*" ;
		}

		if( ( font_name = alloca( sizeof(format) + strlen(new_fontsize)
					+ strlen(encoding))))
		{
			sprintf( font_name , format , new_fontsize , encoding) ;

			gtk_xlfd_selection_dialog_set_font_name(
				GTK_XLFD_SELECTION_DIALOG(dialog) , font_name) ;
		}
	}

	if( cs_info_table[selected_cs].encoding_names[0])
	{
		gtk_xlfd_selection_dialog_set_filter( GTK_XLFD_SELECTION_DIALOG(dialog) ,
			GTK_XLFD_FILTER_USER , GTK_XLFD_ALL , NULL , NULL , NULL , NULL , NULL ,
			cs_info_table[selected_cs].encoding_names) ;
	}

	gtk_signal_connect(GTK_OBJECT(GTK_XLFD_SELECTION_DIALOG(dialog)->ok_button) ,
		"clicked" , GTK_SIGNAL_FUNC(ok_pressed) , dialog) ;
	gtk_signal_connect(GTK_OBJECT(GTK_XLFD_SELECTION_DIALOG(dialog)->apply_button) ,
		"clicked" , GTK_SIGNAL_FUNC(apply_pressed) , dialog) ;
	gtk_signal_connect(GTK_OBJECT(GTK_XLFD_SELECTION_DIALOG(dialog)->cancel_button) ,
		"clicked" , GTK_SIGNAL_FUNC(cancel_pressed) , dialog) ;

	gtk_widget_show_all( dialog) ;
}

#endif


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

	/* Escape '-' in Xft. */
	count = 0 ;
	p = str ;
	while( *p)
	{
	#ifndef  USE_WIN32GUI
		if( *p == '-')
		{
			dup_str[count++] = '\\' ;
		}
	#endif

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

static void
select_fc_font(
	GtkWidget *  widget,
	gpointer  p
	)
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

static void
select_font(
	GtkWidget *  widget ,
	gpointer  p
	)
{
#if  ! defined(USE_WIN32GUI) && ! defined(G_PLATFORM_WIN32)
	if( ! GTK_TOGGLE_BUTTON(xft_flag)->active &&
	    ! GTK_TOGGLE_BUTTON(cairo_flag)->active)
	{
		select_xlfd_font( widget , p) ;
	}
	else
#endif
	{
		select_fc_font( widget , p) ;
	}
}


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
#if  GTK_MAJOR_VERSION > 2 || (GTK_MAJOR_VERSION == 2 && GTK_MINOR_VERSION > 12)
	gtk_widget_set_tooltip_text(combo ,
		"If you change fonts from \"Select\" button, "
		"it is not recommended to change font size here.") ;
#endif


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
	
	cairo_flag = mc_flag_config_widget_new( MC_FLAG_CAIRO) ;
	gtk_widget_show( cairo_flag) ;
	gtk_box_pack_start(GTK_BOX(hbox) , cairo_flag , TRUE , TRUE , 0) ;
	gtk_signal_connect(GTK_OBJECT(cairo_flag) , "toggled" ,
		GTK_SIGNAL_FUNC(cairo_flag_checked) , NULL) ;
	if( mc_gui_is_win32())
	{
		gtk_widget_set_sensitive(cairo_flag, 0);
	}

	aa_flag = mc_flag_config_widget_new( MC_FLAG_AA) ;
#if  ! defined(USE_TYPE_XFT) && ! defined(USE_TYPE_CAIRO) && ! defined(USE_WIN32GUI)
	gtk_widget_set_sensitive(aa_flag, 0) ;
#endif
	gtk_widget_show( aa_flag) ;
	gtk_box_pack_start(GTK_BOX(hbox) , aa_flag , TRUE , TRUE , 0) ;
	gtk_signal_connect(GTK_OBJECT(aa_flag) , "toggled" ,
		GTK_SIGNAL_FUNC(aa_flag_checked) , NULL) ;
	
	if( GTK_TOGGLE_BUTTON(aa_flag)->active)
	{
	#if  defined(USE_TYPE_XFT)
		gtk_toggle_button_set_active( GTK_TOGGLE_BUTTON(xft_flag) , 1) ;
	#elif  defined(USE_TYPE_CAIRO)
		gtk_toggle_button_set_active( GTK_TOGGLE_BUTTON(cairo_flag) , 1) ;
	#endif
	}
	
	vcol_flag = mc_flag_config_widget_new( MC_FLAG_VCOL) ;
	gtk_widget_show( vcol_flag) ;
	gtk_box_pack_start(GTK_BOX(hbox) , vcol_flag , TRUE , TRUE , 0) ;
	gtk_signal_connect(GTK_OBJECT(vcol_flag) , "toggled" ,
		GTK_SIGNAL_FUNC(vcol_flag_checked) , NULL) ;

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
	 * MC_FLAG_{XFT|CAIRO} should be updated last because MC_FLAG_{XFT|CAIRO} are
	 * invalid in some environments.
	 */
	mc_update_flag_mode( MC_FLAG_AA) ;
	mc_update_flag_mode( MC_FLAG_VCOL) ;
	mc_update_flag_mode( MC_FLAG_XFT) ;
	mc_update_flag_mode( MC_FLAG_CAIRO) ;
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
