/*
 *	$Id$
 */

#include  "mc_font.h"

#include  <stdio.h>
#include  <ctype.h>
#include  <kiklib/kik_str.h>
#include  <kiklib/kik_mem.h>		/* free */
#include  <kiklib/kik_debug.h>
#include  <glib.h>
#include  <c_intl.h>

#if  ! defined(USE_WIN32GUI) && ! defined(G_PLATFORM_WIN32) && ! GTK_CHECK_VERSION(2,90,0) && ! defined(USE_QUARTZ)
#include  "gtkxlfdsel.h"
#endif

#include  "mc_combo.h"
#include  "mc_flags.h"
#include  "mc_radio.h"
#include  "mc_char_encoding.h"
#include  "mc_color.h"
#include  "mc_unicode_areas.h"


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
	 * (Conforming to the specification of 'charsets' argument of
	 * gtk_xlfd_selection_dialog_set_filter()).
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
	 * UNICODE => ISO10646_UCS4_1 or U+XXXX-XXXX in get_correct_cs().
	 */
	{ "UNICODE" , { "iso10646-1" , NULL , NULL , } , } ,
	{ "UNICODE (Full Width)" , { "iso10646-1" , NULL , NULL , } , } ,
	{ "UNICODE (Emoji)" , { "iso10646-1" , NULL , NULL , } , } ,
	{ "UNICODE (Hankaku Kana)" , { "iso10646-1" , NULL , NULL , } , } ,
	{ "UNICODE (Hebrew)" , { "iso10646-1" , NULL , NULL , } , } ,
	{ "UNICODE (Arabic)" , { "iso10646-1" , NULL , NULL , } , } ,
	{ "UNICODE (Hindi)" , { "iso10646-1" , NULL , NULL , } , } ,
	{ "UNICODE (Bengali)" , { "iso10646-1" , NULL , NULL , } , } ,
	{ "UNICODE (Punjabi)" , { "iso10646-1" , NULL , NULL , } , } ,
	{ "UNICODE (Gujarati)" , { "iso10646-1" , NULL , NULL , } , } ,
	{ "UNICODE (Oriya)" , { "iso10646-1" , NULL , NULL , } , } ,
	{ "UNICODE (Tamil)" , { "iso10646-1" , NULL , NULL , } , } ,
	{ "UNICODE (Telugu)" , { "iso10646-1" , NULL , NULL , } , } ,
	{ "UNICODE (Kannada)" , { "iso10646-1" , NULL , NULL , } , } ,
	{ "UNICODE (Malayalam)" , { "iso10646-1" , NULL , NULL , } , } ,

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
	{ "JISX0213_2000_1" , { "jisx0213.2000-1" , "jisx0208.1983-0" , NULL , } , } ,
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
static int  dont_change_new_fontname_list = 0 ;
static int  selected_cs = 0 ;	/* 0 = DEFAULT */
static GtkWidget *  fontcs_entry ;
static GtkWidget *  fontname_entry ;
static GtkWidget *  select_font_button ;
static GtkWidget *  xft_flag ;
static GtkWidget *  cairo_flag ;
static GtkWidget *  aa_flag ;
static GtkWidget *  vcol_flag ;
static int  dont_change_type_engine ;
static GtkWidget *  noconv_areas_button ;
static char *  noconv_areas ;


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

/*
 * If you use functions in mc_io.c, use this function instead of direct
 * access to cs_info_t::cs.
 */
static const char *
get_correct_cs(
	int  idx
	)
{
	const char *  unicode_names[] =
	{
		"ISO10646_UCS4_1" ,
		"ISO10646_UCS4_1_FULLWIDTH" ,
		"U+1F000-1F77F" ,	/* Emoji */
		"U+FF61-FF9F" ,		/* Hankaku Kana */
		"U+0590-05FF" ,		/* Hebrew */
		"U+0600-06FF" ,		/* Arabic */
		"U+0900-097F" ,		/* Hindi */
		"U+0980-09FF" ,		/* Bengali */
		"U+0A00-0A7F" ,		/* Punjabi */
		"U+0A80-0AFF" ,		/* Gujarati */
		"U+0B00-0B7F" ,		/* Oriya */
		"U+0B80-0BFF" ,		/* Tamil */
		"U+0C00-0C7F" ,		/* Telugu */
		"U+0C80-0CFF" ,		/* Kannada */
		"U+0D00-0D7F" ,		/* Malayalam */
	} ;

	if( idx < 0)
	{
		return  NULL ;
	}
	else if( 1 <= idx && idx <= 15)
	{
		return  unicode_names[idx - 1] ;
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
	if( gtk_toggle_button_get_active( GTK_TOGGLE_BUTTON(xft_flag)) ||
	    gtk_toggle_button_get_active( GTK_TOGGLE_BUTTON(cairo_flag)))
	{
		if( gtk_toggle_button_get_active( GTK_TOGGLE_BUTTON(vcol_flag)))
		{
			return "vaafont" ;
		}
		else if( mc_radio_get_value( MC_RADIO_VERTICAL_MODE))
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
		if( gtk_toggle_button_get_active( GTK_TOGGLE_BUTTON(vcol_flag)))
		{
			return "vfont" ;
		}
		else if( mc_radio_get_value( MC_RADIO_VERTICAL_MODE))
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
	if( gtk_toggle_button_get_active( GTK_TOGGLE_BUTTON(widget)))
	{
		if( ! gtk_toggle_button_get_active( GTK_TOGGLE_BUTTON(xft_flag)) &&
		    ! gtk_toggle_button_get_active( GTK_TOGGLE_BUTTON(cairo_flag)))
		{
			gtk_toggle_button_set_active( GTK_TOGGLE_BUTTON(xft_flag) , 1) ;
		
			reset_fontname_list() ;
			gtk_entry_set_text(GTK_ENTRY(fontname_entry) ,
				g_locale_to_utf8(
					mc_get_font_name( get_font_file() ,
						get_correct_cs( selected_cs)) ,
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
	if( ! gtk_toggle_button_get_active( GTK_TOGGLE_BUTTON(widget)) &&
	    ! gtk_toggle_button_get_active( GTK_TOGGLE_BUTTON(cairo_flag)))
	{
		gtk_toggle_button_set_active( GTK_TOGGLE_BUTTON(aa_flag) , 0) ;
	}
	else if( ! dont_change_type_engine)
	{
		dont_change_type_engine = 1 ;
		gtk_toggle_button_set_active( GTK_TOGGLE_BUTTON(cairo_flag) , 0) ;
		dont_change_type_engine = 0 ;
	}

	reset_fontname_list() ;
	gtk_entry_set_text(GTK_ENTRY(fontname_entry) ,
		g_locale_to_utf8(
			mc_get_font_name( get_font_file() ,
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
	if( ! gtk_toggle_button_get_active( GTK_TOGGLE_BUTTON(widget)) &&
	    ! gtk_toggle_button_get_active( GTK_TOGGLE_BUTTON(xft_flag)))
	{
		gtk_toggle_button_set_active( GTK_TOGGLE_BUTTON(aa_flag) , 0) ;
	}
	else if( ! dont_change_type_engine)
	{
		dont_change_type_engine = 1 ;
		gtk_toggle_button_set_active( GTK_TOGGLE_BUTTON(xft_flag) , 0) ;
		dont_change_type_engine = 0 ;
	}

	reset_fontname_list() ;
	gtk_entry_set_text(GTK_ENTRY(fontname_entry) ,
		g_locale_to_utf8(
			mc_get_font_name( get_font_file() ,
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
			mc_get_font_name( get_font_file() ,
						get_correct_cs( selected_cs)) ,
			-1 , NULL , NULL , NULL) ) ;

	return  1 ;
}

static void
vertical_mode_changed(void)
{
	reset_fontname_list() ;
	gtk_entry_set_text(GTK_ENTRY(fontname_entry) ,
		g_locale_to_utf8(
			mc_get_font_name( get_font_file() ,
						get_correct_cs( selected_cs)) ,
			-1 , NULL , NULL , NULL) ) ;

	if( mc_radio_get_value( MC_RADIO_VERTICAL_MODE))
	{
		gtk_toggle_button_set_active( GTK_TOGGLE_BUTTON(vcol_flag) , 0) ;
		gtk_widget_set_sensitive( vcol_flag , 0) ;
	}
	else
	{
		gtk_widget_set_sensitive( vcol_flag , 1) ;
	}
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

static void
specify_width(
	GtkWidget *  widget ,
	int  flag
	)
{
	gchar *  fontname ;

	if( ( ( fontname = new_fontname_list[selected_cs]) ||
	      ( fontname = gtk_entry_get_text(GTK_ENTRY(fontname_entry)))) &&
	    *fontname)
	{
		gchar *  p ;
		int  percent ;
		size_t  len ;

		if( ! ( p = strrchr( fontname , ':')) ||
		    ( percent = atoi( p + 1)) == 0)
		{
			if( flag == 0)
			{
				return ;
			}

			len = strlen( fontname) ;
			percent = (flag < 0 ? 100 : 110) ;
		}
		else
		{
			len = p - fontname ;

			if( flag == 0)
			{
				percent = 0 ;
			}
			else
			{
				percent += (flag < 0 ? -10 : 10) ;

				if( percent <= 0 || 200 < percent)
				{
					return ;
				}
			}
		}

		if( ( p = malloc( len + 5)))
		{
			strncpy( p , fontname , len) ;
			if( percent == 0)
			{
				p[len] = '\0' ;
			}
			else
			{
				sprintf( p + len , ":%d" , percent) ;
			}
			free( new_fontname_list[selected_cs]) ;
			new_fontname_list[selected_cs] = p ;

			dont_change_new_fontname_list = 1 ;
			gtk_entry_set_text( GTK_ENTRY(fontname_entry) , p) ;
			dont_change_new_fontname_list = 0 ;
		}
	}
}

static void
widen_width(
	GtkWidget *  widget ,
	gpointer  data
	)
{
	specify_width( widget , 1) ;
}

static void
narrow_width(
	GtkWidget *  widget ,
	gpointer  data
	)
{
	specify_width( widget , -1) ;
}

static void
default_width(
	GtkWidget *  widget ,
	gpointer  data
	)
{
	specify_width( widget , 0) ;
}

static void
fontcs_changed(void)
{
	dont_change_new_fontname_list = 1 ;

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
					get_correct_cs(selected_cs)) ,
				-1 , NULL , NULL , NULL) ) ;
	}

	dont_change_new_fontname_list = 0 ;
}

/* compare two encoding names, returns non-zero if equal
 */
static int
compare(
	const char *  e1 ,
	const char *  e2
	)
{
	while(1)
	{
		/* ')' is for "auto (currently EUC-JP)" */
		if( *e1 == '-' || *e1 == '_' || *e1 == ')')
		{
			e1++ ;
			continue ;
		}
		if( *e2 == '-' || *e2 == '_' || *e2 == ')')
		{
			e2++ ;
			continue ;
		}
		if( (*e1 == 0 || *e1 == ' ') && (*e2 == 0 || *e2 == ' '))
		{
			return  1 ;
		}
		if( toupper(*e1) != toupper(*e2))
		{
			return  0 ;
		}
		e1++ ;
		e2++ ;
	}
}

static void
fontcs_map(
	GtkWidget *  widget ,
	gpointer  data
	)
{
	char *  encoding ;
	int  count ;

	encoding = mc_get_char_encoding() ;

	if( ( strstr( encoding , "UTF") && mc_radio_get_value(MC_RADIO_FONT_POLICY) != 2) ||
	    mc_radio_get_value(MC_RADIO_FONT_POLICY) == 1)
	{
		if( selected_cs == 1)
		{
			return ;
		}

		selected_cs = 1 ;	/* UNICODE */
	}
	else
	{
		for( count = 0 ; count < sizeof(cs_info_table)/sizeof(cs_info_table[0]) ; count++)
		{
			if( strcmp( cs_info_table[count].cs , "JISX0201_KATA") == 0)
			{
				break ;
			}
			else if( compare( encoding , cs_info_table[count].cs))
			{
				if( selected_cs == count)
				{
					return ;
				}

				selected_cs = count ;

				goto  update_cs ;
			}
		}

		if( selected_cs == 0)
		{
			return ;
		}

		selected_cs = 0 ;	/* DEFAULT */
	}

update_cs:
	gtk_entry_set_text( GTK_ENTRY(widget) , cs_info_table[selected_cs].cs) ;
	fontcs_changed() ;
}

static void
font_policy_changed(void)
{
	fontcs_map( fontcs_entry , NULL) ;

	if( noconv_areas_button)
	{
		gtk_widget_set_sensitive( noconv_areas_button ,
			( mc_radio_get_value( MC_RADIO_FONT_POLICY) == 2)) ;
	}
}

static gint
fontcs_selected(
	GtkWidget *  widget ,
	gpointer  data
	)
{
	const char *  cs ;
	int  count ;

	cs = gtk_entry_get_text(GTK_ENTRY(widget)) ;

	for( count = 0 ; count < sizeof(cs_info_table)/sizeof(cs_info_table[0]) ; count++)
	{
		if( strcmp( cs , cs_info_table[count].cs) == 0)
		{
			if( selected_cs != count)
			{
			#if  0
				kik_debug_printf( "Before: cs %s fontname %s => " ,
					cs_info_table[selected_cs].cs ,
					new_fontname_list[selected_cs]) ;
			#endif

				selected_cs = count ;

				fontcs_changed() ;

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

#if  ! defined(USE_WIN32GUI) && ! defined(G_PLATFORM_WIN32) && ! GTK_CHECK_VERSION(2,90,0) && ! defined(USE_QUARTZ)

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
	const gchar *  font_name ;

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

	g_signal_connect(GTK_XLFD_SELECTION_DIALOG(dialog)->ok_button ,
		"clicked" , G_CALLBACK(ok_pressed) , dialog) ;
	g_signal_connect(GTK_XLFD_SELECTION_DIALOG(dialog)->apply_button ,
		"clicked" , G_CALLBACK(apply_pressed) , dialog) ;
	g_signal_connect(GTK_XLFD_SELECTION_DIALOG(dialog)->cancel_button ,
		"clicked" , G_CALLBACK(cancel_pressed) , dialog) ;

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
	const char *  font_name
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
fontname_entry_edit(
	GtkWidget *  widget,
	gpointer  p
	)
{
	/* In case fontname is editted in text entry widget. */

	if( ! dont_change_new_fontname_list)
	{
		const char *  name ;

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
#if  ! defined(USE_WIN32GUI) && ! defined(G_PLATFORM_WIN32) && ! GTK_CHECK_VERSION(2,90,0) && ! defined(USE_QUARTZ)
	if( ! gtk_toggle_button_get_active( GTK_TOGGLE_BUTTON(xft_flag)) &&
	    ! gtk_toggle_button_get_active( GTK_TOGGLE_BUTTON(cairo_flag)))
	{
		select_xlfd_font( widget , p) ;
	}
	else
#endif
	{
		select_fc_font( widget , p) ;
	}
}

static void
edit_noconv_areas(
	GtkWidget *  widget ,
	gpointer  data
	)
{
	char *  cur_areas ;
	char *  new_areas ;

	if( noconv_areas)
	{
		cur_areas = strdup( noconv_areas) ;
	}
	else
	{
		cur_areas = mc_get_str_value( "unicode_noconv_areas") ;
	}

	if( ( new_areas = mc_get_unicode_areas( cur_areas)) &&
	    kik_compare_str( noconv_areas , new_areas) != 0)
	{
		free( noconv_areas) ;
		noconv_areas = new_areas ;
	}

	free( cur_areas) ;
}


/* --- global functions --- */

GtkWidget *
mc_font_config_widget_new(void)
{
	GtkWidget *  vbox ;
	GtkWidget *  hbox ;
	GtkWidget *  fgcolor ;
	GtkWidget *  combo ;
	GtkWidget *  entry ;
	GtkWidget *  radio ;
	GtkWidget *  label ;
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

	vbox = gtk_vbox_new( FALSE , 0) ;

	hbox = gtk_hbox_new( FALSE , 0) ;
	gtk_widget_show( hbox) ;
	gtk_box_pack_start( GTK_BOX(vbox) , hbox , FALSE , FALSE , 0) ;

	combo = mc_combo_new_with_width(_("Font size (pixels)") , fontlist ,
		sizeof(fontlist) / sizeof(fontlist[0]) , new_fontsize , 1 ,
		30 , &entry) ;
	g_signal_connect( entry , "changed" , G_CALLBACK(fontsize_selected) , NULL) ;
	gtk_box_pack_start( GTK_BOX(hbox) , combo , FALSE , FALSE , 0) ;
#if  GTK_CHECK_VERSION(2,12,0)
	gtk_widget_set_tooltip_text( combo ,
		"If you change fonts from \"Select\" button, "
		"it is not recommended to change font size here.") ;
#endif

	fgcolor = mc_color_config_widget_new( MC_COLOR_FG) ;
	gtk_widget_show( fgcolor) ;
	gtk_box_pack_start( GTK_BOX(hbox) , fgcolor , FALSE , FALSE , 5) ;

	hbox = gtk_hbox_new( FALSE , 0) ;
	gtk_widget_show( hbox) ;

	xft_flag = mc_flag_config_widget_new( MC_FLAG_XFT) ;
	gtk_widget_show( xft_flag) ;
	gtk_box_pack_start( GTK_BOX(hbox) , xft_flag , FALSE , FALSE , 0) ;
	g_signal_connect( xft_flag , "toggled" ,
		G_CALLBACK(xft_flag_checked) , NULL) ;
	if( mc_gui_is_win32())
	{
		gtk_widget_set_sensitive( xft_flag , 0) ;
	}
	
	cairo_flag = mc_flag_config_widget_new( MC_FLAG_CAIRO) ;
	gtk_widget_show( cairo_flag) ;
	gtk_box_pack_start( GTK_BOX(hbox) , cairo_flag , FALSE , FALSE , 0) ;
	g_signal_connect( cairo_flag , "toggled" ,
		G_CALLBACK(cairo_flag_checked) , NULL) ;
	if( mc_gui_is_win32())
	{
		gtk_widget_set_sensitive( cairo_flag , 0) ;
	}

	aa_flag = mc_flag_config_widget_new( MC_FLAG_AA) ;
	gtk_widget_show( aa_flag) ;
	gtk_box_pack_start( GTK_BOX(hbox) , aa_flag , FALSE , FALSE , 0) ;
	g_signal_connect( aa_flag , "toggled" ,
		G_CALLBACK(aa_flag_checked) , NULL) ;
	if( mc_gui_is_win32())
	{
		gtk_widget_set_sensitive( aa_flag , 0) ;
	}
	
	vcol_flag = mc_flag_config_widget_new( MC_FLAG_VCOL) ;
	gtk_widget_show( vcol_flag) ;
	gtk_box_pack_start( GTK_BOX(hbox) , vcol_flag , FALSE , FALSE , 0) ;
	g_signal_connect( vcol_flag , "toggled" ,
		G_CALLBACK(vcol_flag_checked) , NULL) ;
	gtk_box_pack_start( GTK_BOX(vbox) , hbox , FALSE , FALSE , 0) ;


	radio = mc_radio_config_widget_new( MC_RADIO_VERTICAL_MODE) ;
	gtk_widget_show( radio) ;
	gtk_box_pack_start( GTK_BOX(vbox) , radio , FALSE , FALSE , 0) ;
	mc_radio_set_callback( MC_RADIO_VERTICAL_MODE , vertical_mode_changed) ;
	if( mc_radio_get_value( MC_RADIO_VERTICAL_MODE))
	{
		gtk_widget_set_sensitive( vcol_flag , 0) ;
	}


	hbox = gtk_hbox_new( FALSE , 0) ;
	gtk_widget_show( hbox) ;

	if( ( cslist = alloca( sizeof(char*) * sizeof(cs_info_table) / sizeof(cs_info_table[0]))))
	{
		int  count ;

		for( count = 0 ; count < sizeof(cs_info_table) / sizeof(cs_info_table[0]) ;
			count++)
		{
			cslist[count] = cs_info_table[count].cs ;
		}

		combo = mc_combo_new_with_width( _("Font name") , cslist , count ,
				cslist[selected_cs] , 1 , 90 , &fontcs_entry) ;
		g_signal_connect( fontcs_entry , "changed" , G_CALLBACK(fontcs_selected) , NULL) ;
		g_signal_connect( fontcs_entry , "map" , G_CALLBACK(fontcs_map) , NULL) ;
		gtk_box_pack_start( GTK_BOX(hbox) , combo , FALSE , FALSE , 0) ;
	}

	fontname_entry = gtk_entry_new() ;
	gtk_entry_set_text( GTK_ENTRY(fontname_entry) ,
		g_locale_to_utf8(
			mc_get_font_name( get_font_file() ,
					get_correct_cs( selected_cs)) ,
			-1 , NULL , NULL , NULL) ) ;
	gtk_widget_show( fontname_entry) ;
	gtk_box_pack_start( GTK_BOX(hbox) , fontname_entry , TRUE , TRUE , 1) ;
	g_signal_connect( fontname_entry , "changed" ,
		G_CALLBACK(fontname_entry_edit) , NULL) ;

	select_font_button = gtk_button_new_with_label( _("Select")) ;
	gtk_widget_show( select_font_button) ;
	gtk_box_pack_start( GTK_BOX(hbox) , select_font_button , FALSE , FALSE , 1) ;
	g_signal_connect( select_font_button , "clicked" ,
		G_CALLBACK(select_font) , NULL) ;

	gtk_box_pack_start( GTK_BOX(vbox) , hbox , TRUE , TRUE , 0) ;


	hbox = gtk_hbox_new( FALSE , 0) ;
	gtk_widget_show( hbox) ;

	label = gtk_label_new( _("Font width")) ;
	gtk_widget_show( label) ;
	gtk_box_pack_start( GTK_BOX(hbox) , label , FALSE , FALSE , 5) ;

	button = gtk_button_new_with_label( _("Narrow")) ;
	gtk_widget_show( button) ;
	gtk_box_pack_start( GTK_BOX(hbox) , button , FALSE , FALSE , 5) ;
	g_signal_connect( button , "clicked" , G_CALLBACK(narrow_width) , NULL) ;

	button = gtk_button_new_with_label( _("Widen")) ;
	gtk_widget_show( button) ;
	gtk_box_pack_start( GTK_BOX(hbox) , button , FALSE , FALSE , 5) ;
	g_signal_connect( button , "clicked" , G_CALLBACK(widen_width) , NULL) ;

	button = gtk_button_new_with_label( _("Default")) ;
	gtk_widget_show( button) ;
	gtk_box_pack_start( GTK_BOX(hbox) , button , FALSE , FALSE , 5) ;
	g_signal_connect( button , "clicked" , G_CALLBACK(default_width) , NULL) ;

	gtk_box_pack_start( GTK_BOX(vbox) , hbox , TRUE , TRUE , 0) ;


	radio = mc_radio_config_widget_new( MC_RADIO_FONT_POLICY) ;
	gtk_widget_show( radio) ;
	gtk_box_pack_start( GTK_BOX(vbox) , radio , FALSE , FALSE , 0) ;
	mc_radio_set_callback( MC_RADIO_FONT_POLICY , font_policy_changed) ;

	hbox = gtk_hbox_new( FALSE , 0) ;
	gtk_widget_show( hbox) ;

	label = gtk_label_new( _("Unicode areas you won't convert to other charsets")) ;
	gtk_widget_show( label) ;
	gtk_box_pack_start( GTK_BOX(hbox) , label , FALSE , FALSE , 5) ;
	noconv_areas_button = gtk_button_new_with_label( _(" Edit ")) ;
	gtk_widget_show( noconv_areas_button) ;
	gtk_box_pack_start( GTK_BOX(hbox) , noconv_areas_button , FALSE , FALSE , 0) ;
	g_signal_connect( noconv_areas_button , "clicked" , G_CALLBACK(edit_noconv_areas) , NULL) ;
	if( mc_radio_get_value( MC_RADIO_FONT_POLICY) != 2)
	{
		gtk_widget_set_sensitive( noconv_areas_button , 0) ;
	}

	gtk_box_pack_start( GTK_BOX(vbox) , hbox , FALSE , FALSE , 0) ;

	radio = mc_radio_config_widget_new( MC_RADIO_BOX_DRAWING) ;
	gtk_widget_show( radio) ;
	gtk_box_pack_start( GTK_BOX(vbox) , radio , FALSE , FALSE , 0) ;


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
	mc_update_radio( MC_RADIO_VERTICAL_MODE) ;
	/* vcol is forcibly disabled in vertical mode, so update after vertical mode. */
	mc_update_flag_mode( MC_FLAG_VCOL) ;
	mc_update_flag_mode( MC_FLAG_XFT) ;
	mc_update_flag_mode( MC_FLAG_CAIRO) ;
	mc_update_radio( MC_RADIO_BOX_DRAWING) ;
	mc_update_radio( MC_RADIO_FONT_POLICY) ;

	if( noconv_areas)
	{
		mc_set_str_value( "unicode_noconv_areas" , noconv_areas) ;
	}
}

void
mc_update_font_name(mc_io_t  io)
{
	size_t  count ;

	for( count = 0 ; count < sizeof(cs_info_table) / sizeof(cs_info_table[0]) ; count++)
	{
		if( new_fontname_list[count])
		{
			mc_set_font_name( io , get_font_file() ,
				get_correct_cs( count) , new_fontname_list[count]) ;

			if( count == 6)
			{
				/* Arabic Supplement */
				mc_set_font_name( io , get_font_file() ,
					"U+0750-77F" , new_fontname_list[count]) ;
				/* Arabic Extended-A */
				mc_set_font_name( io , get_font_file() ,
					"U+08A0-8FF" , new_fontname_list[count]) ;
				/* Arabic Presentation Forms-A */
				mc_set_font_name( io , get_font_file() ,
					"U+FB50-FDFF" , new_fontname_list[count]) ;
				/* Arabic Presentation Forms-B */
				mc_set_font_name( io , get_font_file() ,
					"U+FE70-FEFF" , new_fontname_list[count]) ;
				/* Arabic Mathematical Alphabetic Symbols */
				mc_set_font_name( io , get_font_file() ,
					"U+1EE00-1EEFF" , new_fontname_list[count]) ;
			}
		}
	}
}
