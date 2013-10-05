/*
 *	$Id$
 */

#undef  VTE_SEAL_ENABLE
#include  <vte/vte.h>
#include  <vte/reaper.h>

#include  <pwd.h>			/* getpwuid */
#include  <X11/keysym.h>
#include  <gdk/gdkx.h>
#include  <kiklib/kik_sig_child.h>
#include  <kiklib/kik_str.h>		/* kik_alloca_dup */
#include  <kiklib/kik_mem.h>
#include  <kiklib/kik_path.h>
#include  <kiklib/kik_debug.h>
#include  <kiklib/kik_util.h>		/* DIGIT_STR_LEN */
#include  <kiklib/kik_privilege.h>
#include  <kiklib/kik_unistd.h>
#include  <kiklib/kik_locale.h>
#include  <kiklib/kik_conf_io.h>
#include  <kiklib/kik_pty.h>		/* kik_pty_helper_set_flag */
#include  <kiklib/kik_conf.h>
#include  <ml_str_parser.h>
#include  <ml_term_manager.h>
#include  <x_screen.h>
#include  <x_xic.h>
#include  <x_main_config.h>
#include  <x_imagelib.h>
#include  <xlib/x_xim.h>

#include  "../main/version.h"

#include  "marshal.h"

#ifdef  SYSCONFDIR
#define CONFIG_PATH SYSCONFDIR
#endif

#if  0
#define  __DEBUG
#endif

#ifndef  I_
#define  I_(a)  a
#endif

#if  ! GTK_CHECK_VERSION(2,14,0)
#define  gtk_adjustment_get_upper(adj)  ((adj)->upper)
#define  gtk_adjustment_get_value(adj)  ((adj)->value)
#define  gtk_adjustment_get_page_size(adj)  ((adj)->page_size)
#define  gtk_widget_get_window(widget)  ((widget)->window)
#endif

#if  ! GTK_CHECK_VERSION(2,18,0)
#define  gtk_widget_set_window(widget,win)  ((widget)->window = (win))
#define  gtk_widget_set_allocation(widget,alloc)  ((widget)->allocation = *(alloc))
#define  gtk_widget_get_allocation(widget,alloc)  (*(alloc) = (widget)->allocation)
#endif

#if  GTK_CHECK_VERSION(2,90,0)
#define  GTK_WIDGET_SET_REALIZED(widget)  gtk_widget_set_realized(widget,TRUE)
#define  GTK_WIDGET_UNSET_REALIZED(widget)  gtk_widget_set_realized(widget,FALSE)
#define  GTK_WIDGET_REALIZED(widget)  gtk_widget_get_realized(widget)
#define  GTK_WIDGET_SET_HAS_FOCUS(widget)  (0)
#define  GTK_WIDGET_UNSET_HAS_FOCUS(widget)  (0)
#define  GTK_WIDGET_UNSET_MAPPED(widget)  gtk_widget_set_mapped(widget,FALSE)
#define  GTK_WIDGET_MAPPED(widget)  gtk_widget_get_mapped(widget)
#define  GTK_WIDGET_SET_CAN_FOCUS(widget)  gtk_widget_set_can_focus(widget,TRUE)
#define  gdk_x11_drawable_get_xid(window)  gdk_x11_window_get_xid(window)
#else	/* GTK_CHECK_VERSION(2,90,0) */
#define  GTK_WIDGET_SET_REALIZED(widget)  GTK_WIDGET_SET_FLAGS(widget,GTK_REALIZED)
#define  GTK_WIDGET_UNSET_REALIZED(widget)  GTK_WIDGET_UNSET_FLAGS(widget,GTK_REALIZED)
#define  GTK_WIDGET_SET_HAS_FOCUS(widget)  GTK_WIDGET_SET_FLAGS(widget,GTK_HAS_FOCUS)
#define  GTK_WIDGET_UNSET_HAS_FOCUS(widget)  GTK_WIDGET_UNSET_FLAGS(widget,GTK_HAS_FOCUS)
#define  GTK_WIDGET_UNSET_MAPPED(widget)  GTK_WIDGET_UNSET_FLAGS(widget,GTK_MAPPED)
#define  GTK_WIDGET_SET_CAN_FOCUS(widget)  GTK_WIDGET_SET_FLAGS(widget,GTK_CAN_FOCUS)
#endif	/* GTK_CHECK_VERSION(2,90,0) */

#define  VTE_WIDGET(screen)  ((VteTerminal*)(screen)->system_listener->self)
/* XXX Hack to distinguish x_screen_t from x_{candidate|status}_screent_t */
#define  IS_MLTERM_SCREEN(win)  (! PARENT_WINDOWID_IS_TOP(win))

#define  WINDOW_MARGIN  1

#ifndef  VTE_CHECK_VERSION
#define  VTE_CHECK_VERSION(a,b,c)  (0)
#endif

#define  STATIC_PARAMS (G_PARAM_STATIC_NAME | G_PARAM_STATIC_NICK | G_PARAM_STATIC_BLURB)


struct _VteTerminalPrivate
{
	/* Not NULL until finalized. screen->term is NULL until widget is realized. */
	x_screen_t *  screen ;

	/*
	 * Not NULL until finalized. term->pty is NULL until pty is forked or
	 * inherited from parent.
	 */
	ml_term_t *  term ;

#if  VTE_CHECK_VERSION(0,26,0)
	VtePty *  pty ;
#endif

	x_system_event_listener_t  system_listener ;

	void (*line_scrolled_out)( void *) ;
	void (*set_window_name)( void * , u_char *) ;
	void (*set_icon_name)( void * , u_char *) ;
	x_screen_scroll_event_listener_t  screen_scroll_listener ;
	int8_t  adj_value_changed_by_myself ;

	/* for roxterm-2.6.5 */
	int8_t  init_char_size ;

	GIOChannel *  io ;
	guint  src_id ;

	GdkPixbuf *  image ;	/* Original image which vte_terminal_set_background_image passed */
	Pixmap  pixmap ;
	u_int  pix_width ;
	u_int  pix_height ;
	x_picture_modifier_t *  pic_mod ; /* caching previous pic_mod in update_wall_picture.*/

/* GRegex was not supported */
#if  GLIB_CHECK_VERSION(2,14,0)
	GRegex *  regex ;
#endif
} ;

enum
{
	COPY_CLIPBOARD,
	PASTE_CLIPBOARD,
	LAST_SIGNAL
} ;

enum
{
	PROP_0,
#if  GTK_CHECK_VERSION(2,90,0)
	PROP_HADJUSTMENT,
	PROP_VADJUSTMENT,
	PROP_HSCROLL_POLICY,
	PROP_VSCROLL_POLICY,
#endif
	PROP_ALLOW_BOLD,
	PROP_AUDIBLE_BELL,
	PROP_BACKGROUND_IMAGE_FILE,
	PROP_BACKGROUND_IMAGE_PIXBUF,
	PROP_BACKGROUND_OPACITY,
	PROP_BACKGROUND_SATURATION,
	PROP_BACKGROUND_TINT_COLOR,
	PROP_BACKGROUND_TRANSPARENT,
	PROP_BACKSPACE_BINDING,
	PROP_CURSOR_BLINK_MODE,
	PROP_CURSOR_SHAPE,
	PROP_DELETE_BINDING,
	PROP_EMULATION,
	PROP_ENCODING,
	PROP_FONT_DESC,
	PROP_ICON_TITLE,
	PROP_MOUSE_POINTER_AUTOHIDE,
	PROP_PTY,
	PROP_SCROLL_BACKGROUND,
	PROP_SCROLLBACK_LINES,
	PROP_SCROLL_ON_KEYSTROKE,
	PROP_SCROLL_ON_OUTPUT,
	PROP_WINDOW_TITLE,
	PROP_WORD_CHARS,
	PROP_VISIBLE_BELL
} ;


#if  GTK_CHECK_VERSION(2,90,0)

struct _VteTerminalClassPrivate
{
	GtkStyleProvider *  style_provider ;
} ;

G_DEFINE_TYPE_WITH_CODE(VteTerminal , vte_terminal , GTK_TYPE_WIDGET ,
	g_type_add_class_private(g_define_type_id , sizeof(VteTerminalClassPrivate)) ;
	G_IMPLEMENT_INTERFACE(GTK_TYPE_SCROLLABLE, NULL))

#else

G_DEFINE_TYPE(VteTerminal , vte_terminal , GTK_TYPE_WIDGET) ;

#endif


/* --- static variables --- */

static x_main_config_t  main_config ;
static x_shortcut_t  shortcut ;
static x_termcap_t  termcap ;
static x_display_t  disp ;

#if  VTE_CHECK_VERSION(0,19,0)
static guint signals[LAST_SIGNAL] ;
#endif


/* --- static functions --- */

#ifdef  __DEBUG
static int
error_handler(
	Display *  display ,
	XErrorEvent *  event
	)
{
	char  buffer[1024] ;

	XGetErrorText( display , event->error_code , buffer , 1024) ;

	kik_msg_printf( "%s\n" , buffer) ;

	abort() ;

	return  1 ;
}
#endif

static int
selection(
	x_selection_t *  sel ,
	int  char_index_1 ,
	int  row_1 ,
	int  char_index_2 ,
	int  row_2
	)
{
	x_sel_clear( sel) ;

	x_start_selection( sel , char_index_1 - 1 , row_1 , char_index_1 , row_1 , SEL_CHAR) ;
	x_selecting( sel , char_index_2 , row_2) ;
	x_stop_selecting( sel) ;

	return  1 ;
}

/* GRegex was not supported */
#if  GLIB_CHECK_VERSION(2,14,0)
static int
match(
	size_t *  beg ,
	size_t *  len ,
	void *  regex ,
	u_char *  str ,
	int  backward
	)
{
	GMatchInfo *  info ;

	if( g_regex_match( regex , str , 0 , &info))
	{
		gchar *  word ;
		u_char *  p ;

		p = str ;

		do
		{
			word = g_match_info_fetch( info , 0) ;

			p = strstr( p , word) ;
			*beg = p - str ;
			*len = strlen( word) ;

			g_free( word) ;

			p += (*len) ;
		}
		while( g_match_info_next( info , NULL)) ;

		g_match_info_free( info) ;

		return  1 ;
	}
	
	return  0 ;
}

static gboolean
search_find(
	VteTerminal *  terminal ,
	int  backward
	)
{
	int  beg_char_index ;
	int  beg_row ;
	int  end_char_index ;
	int  end_row ;

	if( ! GTK_WIDGET_REALIZED(GTK_WIDGET(terminal)))
	{
		return  FALSE ;
	}

	if( ml_term_search_find( terminal->pvt->term , &beg_char_index , &beg_row ,
			&end_char_index , &end_row , terminal->pvt->regex , backward))
	{
		gdouble  value ;

		selection( &terminal->pvt->screen->sel ,
			beg_char_index , beg_row , end_char_index , end_row) ;

		value = ml_term_get_num_of_logged_lines( terminal->pvt->term) +
				(beg_row >= 0 ? 0 : beg_row) ;

	#if  GTK_CHECK_VERSION(2,14,0)
		gtk_adjustment_set_value( terminal->adjustment , value) ;
	#else
		terminal->adjustment->value = value ;
		gtk_adjustment_value_changed( terminal->adjustment) ;
	#endif

		/*
		 * XXX
		 * Dirty hack, but without this, selection() above is not reflected to window
		 * if aother word is hit in the same line. (If row is not changed,
		 * gtk_adjustment_set_value() doesn't call x_screen_scroll_to().)
		 */
		x_window_update( &terminal->pvt->screen->window , 1 /* UPDATE_SCREEN */) ;

		return  TRUE ;
	}
	else
	{
		return  FALSE ;
	}
}
#endif

#if  ! GTK_CHECK_VERSION(2,12,0)
/* gdk_color_to_string() was not supported by gtk+ < 2.12. */
static gchar *
gdk_color_to_string(
	const GdkColor *  color
	)
{
	gchar *  str ;

	if( ( str = g_malloc( 14)) == NULL)
	{
		return  NULL ;
	}

	sprintf( str , "#%04x%04x%04x" , color->red , color->green , color->blue) ;

	return  str ;
}
#endif

static int
is_initial_allocation(
	GtkAllocation *  allocation
	)
{
	/* { -1 , -1 , 1 , 1 } is default value of GtkAllocation. */
	return  (allocation->x == -1 && allocation->y == -1 &&
		 allocation->width == 1 && allocation->height == 1) ;
}


static void
catch_child_exited(
	VteReaper *  reaper ,
	int  pid ,
	int  status ,
	VteTerminal *  terminal
	)
{
	kik_trigger_sig_child( pid) ;
}

/*
 * This handler works even if VteTerminal widget is not realized and ml_term_t is not
 * attached to x_screen_t.
 * That's why the time x_screen_attach is called is delayed(in vte_terminal_fork* or
 * vte_terminal_realized).
 */
static gboolean
vte_terminal_io(
	GIOChannel *  source ,
	GIOCondition  conditon ,
	gpointer  data		/* ml_term_t */
	)
{
	ml_term_parse_vt100_sequence( (ml_term_t*)data) ;

	ml_close_dead_terms() ;
	
	return  TRUE ;
}

static void
create_io(
	VteTerminal *  terminal
	)
{
#ifdef  DEBUG
	kik_debug_printf( KIK_DEBUG_TAG " Create GIO of pty master %d\n" ,
		ml_term_get_master_fd( terminal->pvt->term)) ;
#endif

	terminal->pvt->io = g_io_channel_unix_new( ml_term_get_master_fd( terminal->pvt->term)) ;
	terminal->pvt->src_id = g_io_add_watch( terminal->pvt->io ,
						G_IO_IN , vte_terminal_io , terminal->pvt->term) ;
}

static void
destroy_io(
	VteTerminal *  terminal
	)
{
	if( terminal->pvt->io)
	{
	#ifdef  DEBUG
		kik_debug_printf( KIK_DEBUG_TAG " Destroy GIO of pty master %d\n" ,
			ml_term_get_master_fd( terminal->pvt->term)) ;
	#endif

		g_source_destroy(
			g_main_context_find_source_by_id( NULL , terminal->pvt->src_id)) ;
	#if  0
		g_io_channel_shutdown( terminal->pvt->io , TRUE , NULL) ;
	#endif
		g_io_channel_unref( terminal->pvt->io) ;
		terminal->pvt->src_id = 0 ;
		terminal->pvt->io = NULL ;
	}
}

/*
 * ml_pty_event_listener_t overriding handler.
 */
static void
pty_closed(
	void *  p	/* screen->term->pty is NULL */
	)
{
	x_screen_t *  screen ;
	ml_term_t *  term ;

	screen = p ;

	destroy_io( VTE_WIDGET(screen)) ;

	if( ( term = ml_get_detached_term( NULL)))
	{
		VTE_WIDGET(screen)->pvt->term = term ;
		create_io( VTE_WIDGET(screen)) ;
		
		/*
		 * Not screen->term but screen->term->pty is being deleted in ml_close_dead_terms()
		 * because of ml_term_manager_enable_zombie_pty(1) in vte_terminal_class_init().
		 */
		term = screen->term ;
		x_screen_detach( screen) ;
		ml_term_delete( term) ;
		
		/* It is after widget is realized that x_screen_attach can be called. */
		if( GTK_WIDGET_REALIZED(GTK_WIDGET(VTE_WIDGET(screen))))
		{
			x_screen_attach( screen , VTE_WIDGET(screen)->pvt->term) ;

		#ifdef  DEBUG
			kik_debug_printf( KIK_DEBUG_TAG
				" pty is closed and detached pty is re-attached.\n") ;
		#endif
		}
	}
	else
	{
		g_signal_emit_by_name( VTE_WIDGET(screen) , "child-exited") ;

	#ifdef  DEBUG
		kik_debug_printf( KIK_DEBUG_TAG " pty is closed\n") ;
	#endif
	}
}


/*
 * x_system_event_listener_t handlers
 */

static void
font_config_updated(void)
{
	u_int  count ;
	
	x_font_cache_unload_all() ;

	for( count = 0 ; count < disp.num_of_roots ; count++)
	{
		if( IS_MLTERM_SCREEN(disp.roots[count]))
		{
			x_screen_reset_view( (x_screen_t*)disp.roots[count]) ;
		}
	}
}

static void
color_config_updated(void)
{
	u_int  count ;
	
	x_color_cache_unload_all() ;

	for( count = 0 ; count < disp.num_of_roots ; count++)
	{
		if( IS_MLTERM_SCREEN(disp.roots[count]))
		{
			x_screen_reset_view( (x_screen_t*)disp.roots[count]) ;
		}
	}
}

static void
open_pty(
	void *  p ,
	x_screen_t *  screen ,
	char *  dev
	)
{
	if( dev)
	{
		ml_term_t *  new ;

		if( ( new = ml_get_detached_term( dev)))
		{
			destroy_io( VTE_WIDGET(screen)) ;
			VTE_WIDGET(screen)->pvt->term = new ;
			create_io( VTE_WIDGET(screen)) ;

			x_screen_detach( screen) ;

			/* It is after widget is reailzed that x_screen_attach can be called. */
			if( GTK_WIDGET_REALIZED(GTK_WIDGET(VTE_WIDGET(screen))))
			{
				x_screen_attach( screen , new) ;
			}
		}
	}
}

static char *
pty_list(
	void *  p
	)
{
	return  ml_get_pty_list() ;
}

/*
 * EXIT_PROGRAM shortcut calls this at last.
 * this is for debugging.
 */
#ifdef  KIK_DEBUG
#include  <kiklib/kik_locale.h>		/* kik_locale_final */
#endif
static void
__exit(
	void *  p ,
	int  status
	)
{
#ifdef  KIK_DEBUG
	u_int  count ;

#if  1
	kik_mem_dump_all() ;
#endif

	ml_free_word_separators() ;

	/*
	 * Don't loop from 0 to dis.num_of_roots owing to processing inside x_display_remove_root.
	 */
	for( count = disp.num_of_roots ; count > 0 ; count--)
	{
		if( IS_MLTERM_SCREEN( disp.roots[count - 1]))
		{
			gtk_widget_destroy(
				GTK_WIDGET( VTE_WIDGET((x_screen_t*)disp.roots[count - 1]))) ;
		}
		else
		{
			x_display_remove_root( &disp , disp.roots[count - 1]) ;
		}
	}
	free( disp.roots) ;
	x_gc_delete( disp.gc) ;
	x_xim_display_closed( disp.display) ;
	x_picture_display_closed( disp.display) ;

	ml_term_manager_final() ;
	kik_locale_final() ;
	x_main_config_final( &main_config) ;
	ml_color_config_final() ;
	x_shortcut_final( &shortcut) ;
	x_termcap_final( &termcap) ;
	x_xim_final() ;
	kik_sig_child_final() ;

	kik_alloca_garbage_collect() ;

	kik_msg_printf( "reporting unfreed memories --->\n") ;
	kik_mem_free_all() ;
#endif

#if  1
	exit(1) ;
#else
	gtk_main_quit() ;
#endif
}


/*
 * ml_xterm_event_listener_t (overriding) handlers
 */

static void
set_window_name(
	void *  p ,
	u_char *  name
	)
{
	x_screen_t *  screen ;

	screen = p ;

	VTE_WIDGET(screen)->window_title = ml_term_window_name( screen->term) ;
	
	gdk_window_set_title( gtk_widget_get_window( GTK_WIDGET(VTE_WIDGET(screen))) ,
		VTE_WIDGET(screen)->window_title) ;
	g_signal_emit_by_name( VTE_WIDGET(screen) , "window-title-changed") ;

#if  VTE_CHECK_VERSION(0,20,0)
	g_object_notify( G_OBJECT(VTE_WIDGET(screen)) , "window-title") ;
#endif
}

static void
set_icon_name(
	void *  p ,
	u_char *  name
	)
{
	x_screen_t *  screen ;

	screen = p ;

	VTE_WIDGET(screen)->icon_title = ml_term_icon_name( screen->term) ;

	gdk_window_set_icon_name( gtk_widget_get_window(GTK_WIDGET(VTE_WIDGET(screen))) ,
		VTE_WIDGET(screen)->icon_title) ;
	g_signal_emit_by_name( VTE_WIDGET(screen) , "icon-title-changed") ;

#if  VTE_CHECK_VERSION(0,20,0)
	g_object_notify( G_OBJECT(VTE_WIDGET(screen)) , "icon-title") ;
#endif
}


/*
 * ml_screen_event_listener_t (overriding) handler
 */
 
static void
line_scrolled_out(
	void *  p		/* must be x_screen_t */
	)
{
	x_screen_t *  screen ;
	gdouble  upper ;
	gdouble  value ;
	
	screen = p ;

	VTE_WIDGET(screen)->pvt->line_scrolled_out( p) ;

	/*
	 * line_scrolled_out is called in vt100 mode
	 * (after ml_xterm_event_listener_t::start_vt100 event), so
	 * don't call x_screen_scroll_to() in adjustment_value_changed()
	 * in this context.
	 */
	VTE_WIDGET(screen)->pvt->adj_value_changed_by_myself = 1 ;

	value = gtk_adjustment_get_value( VTE_WIDGET(screen)->adjustment) ;

	if( ( upper = gtk_adjustment_get_upper( VTE_WIDGET(screen)->adjustment))
	    < ml_term_get_log_size( VTE_WIDGET(screen)->pvt->term) + VTE_WIDGET(screen)->row_count)
	{
	#if  GTK_CHECK_VERSION(2,14,0)
		gtk_adjustment_set_upper( VTE_WIDGET(screen)->adjustment , upper + 1) ;
	#else
		VTE_WIDGET(screen)->adjustment->upper ++ ;
		gtk_adjustment_changed( VTE_WIDGET(screen)->adjustment) ;
	#endif

		if( ml_term_is_backscrolling( VTE_WIDGET(screen)->pvt->term) != BSM_STATIC)
		{
		#if  GTK_CHECK_VERSION(2,14,0)
			gtk_adjustment_set_value( VTE_WIDGET(screen)->adjustment , value + 1) ;
		#else
			VTE_WIDGET(screen)->adjustment->value ++ ;
			gtk_adjustment_value_changed( VTE_WIDGET(screen)->adjustment) ;
		#endif
		}
	}
	else if( ml_term_is_backscrolling( VTE_WIDGET(screen)->pvt->term) == BSM_STATIC &&
			value > 0)
	{
	#if  GTK_CHECK_VERSION(2,14,0)
		gtk_adjustment_set_value( VTE_WIDGET(screen)->adjustment , value - 1) ;
	#else
		VTE_WIDGET(screen)->adjustment->value -- ;
		gtk_adjustment_value_changed( VTE_WIDGET(screen)->adjustment) ;
	#endif
	}

#ifdef  __DEBUG
	kik_debug_printf( KIK_DEBUG_TAG " line_scrolled_out upper %f value %f\n" ,
		gtk_adjustment_get_upper( VTE_WIDGET(screen)->adjustment) ,
		gtk_adjustment_get_value( VTE_WIDGET(screen)->adjustment)) ;
#endif
}


/*
 * x_screen_scroll_event_listener_t handlers
 */
 
static void
bs_mode_exited(
	void *  p
	)
{
	VteTerminal *  terminal ;
	int  upper ;
	int  page_size ;

	terminal = p ;

	terminal->pvt->adj_value_changed_by_myself = 1 ;

	upper = gtk_adjustment_get_upper( terminal->adjustment) ;
	page_size = gtk_adjustment_get_page_size( terminal->adjustment) ;

#if  GTK_CHECK_VERSION(2,14,0)
	gtk_adjustment_set_value( terminal->adjustment , upper - page_size) ;
#else
	terminal->adjustment->value = upper - page_size ;
	gtk_adjustment_value_changed( terminal->adjustment) ;
#endif
	
#ifdef  __DEBUG
	kik_debug_printf( KIK_DEBUG_TAG " bs_mode_exited upper %d page_size %d\n" ,
		upper , page_size) ;
#endif
}

static void
scrolled_upward(
	void *  p ,
	u_int  size
	)
{
	VteTerminal *  terminal ;
	int  value ;
	int  upper ;
	int  page_size ;

	terminal = p ;

	value = gtk_adjustment_get_value( terminal->adjustment) ;
	upper = gtk_adjustment_get_upper( terminal->adjustment) ;
	page_size = gtk_adjustment_get_page_size( terminal->adjustment) ;

	if( value + page_size >= upper)
	{
		return ;
	}

	if( value + page_size + size > upper)
	{
		size = upper - value - page_size ;
	}

	terminal->pvt->adj_value_changed_by_myself = 1 ;

#if  GTK_CHECK_VERSION(2,14,0)
	gtk_adjustment_set_value( terminal->adjustment , value + size) ;
#else
	terminal->adjustment->value += size ;
	gtk_adjustment_value_changed( terminal->adjustment) ;
#endif
}

static void
scrolled_downward(
	void *  p ,
	u_int  size
	)
{
	VteTerminal *  terminal ;
	int  value ;

	terminal = p ;

	if( ( value = gtk_adjustment_get_value( terminal->adjustment)) == 0)
	{
		return ;
	}

	if( value < size)
	{
		value = size ;
	}

	terminal->pvt->adj_value_changed_by_myself = 1 ;

#if  GTK_CHECK_VERSION(2,14,0)
	gtk_adjustment_set_value( terminal->adjustment , value - size) ;
#else
	terminal->adjustment->value -= size ;
	gtk_adjustment_value_changed( terminal->adjustment) ;
#endif
}

static void
log_size_changed(
	void *  p ,
	u_int  log_size
	)
{
	VteTerminal *  terminal ;

	terminal = p ;

#if  GTK_CHECK_VERSION(2,14,0)
	gtk_adjustment_set_upper( terminal->adjustment , log_size) ;
#else
	terminal->adjustment->upper = log_size ;
	gtk_adjustment_changed( terminal->adjustment) ;
#endif
}


static void
adjustment_value_changed(
	VteTerminal *  terminal
	)
{
	int  value ;
	int  upper ;
	int  page_size ;

	if( terminal->pvt->adj_value_changed_by_myself)
	{
		terminal->pvt->adj_value_changed_by_myself = 0 ;
	
		return ;
	}
	
	value = gtk_adjustment_get_value( terminal->adjustment) ;
	upper = gtk_adjustment_get_upper( terminal->adjustment) ;
	page_size = gtk_adjustment_get_page_size( terminal->adjustment) ;

#ifdef  __DEBUG
	kik_debug_printf( KIK_DEBUG_TAG " scroll to %d\n" , value - (upper - page_size)) ;
#endif

	x_screen_scroll_to( terminal->pvt->screen , value - (upper - page_size)) ;
}

static void
set_adjustment(
	VteTerminal *  terminal ,
	GtkAdjustment *  adjustment
	)
{
	if( adjustment == terminal->adjustment || adjustment == NULL)
	{
		return ;
	}

	if( terminal->adjustment)
	{
		g_signal_handlers_disconnect_by_func( terminal->adjustment ,
			G_CALLBACK(adjustment_value_changed) , terminal) ;
		g_object_unref( terminal->adjustment) ;
	}
	
	g_object_ref_sink( adjustment) ;
	terminal->adjustment = adjustment ;
	g_signal_connect_swapped( terminal->adjustment , "value-changed" ,
		G_CALLBACK(adjustment_value_changed) , terminal) ;
	terminal->pvt->adj_value_changed_by_myself = 0 ;
}


static void
reset_vte_size_member(
	VteTerminal *  terminal
	)
{
	int  emit ;

	emit = 0 ;
	
	if( /* If char_width == 0, reset_vte_size_member is called from vte_terminal_init */
	    terminal->char_width != 0 &&
	    terminal->char_width != x_col_width( terminal->pvt->screen))
	{
		emit = 1 ;
	}
	terminal->char_width = x_col_width( terminal->pvt->screen) ;

	if( /* If char_height == 0, reset_vte_size_member is called from vte_terminal_init */
	    terminal->char_height != 0 &&
	    terminal->char_height != x_line_height( terminal->pvt->screen))
	{
		emit = 1 ;
	}
	terminal->char_height = x_line_height( terminal->pvt->screen) ;

	if( emit)
	{
		g_signal_emit_by_name( terminal , "char-size-changed" ,
				terminal->char_width , terminal->char_height) ;
	}
	
	terminal->char_ascent = x_line_ascent( terminal->pvt->screen) ;
	terminal->char_descent = terminal->char_height - terminal->char_ascent ;


	emit = 0 ;

	if( /* If row_count == 0, reset_vte_size_member is called from vte_terminal_init */
	    terminal->row_count != 0 &&
	    terminal->row_count != ml_term_get_rows( terminal->pvt->term))
	{
		emit = 1 ;
	}

	terminal->row_count = ml_term_get_rows( terminal->pvt->term) ;

	if( /* If column_count == 0, reset_vte_size_member is called from vte_terminal_init */
	    terminal->column_count != 0 &&
	    terminal->column_count != ml_term_get_cols( terminal->pvt->term))
	{
		emit = 1 ;
	}

	terminal->column_count = ml_term_get_cols( terminal->pvt->term) ;

	if( emit)
	{
	#if  GTK_CHECK_VERSION(2,14,0)
		int  value ;

		value = ml_term_get_num_of_logged_lines( terminal->pvt->term) ;
		gtk_adjustment_configure( terminal->adjustment ,
			value /* value */ , 0 /* lower */ ,
			value + terminal->row_count /* upper */ ,
			1 /* step increment */ , terminal->row_count /* page increment */ ,
			terminal->row_count /* page size */) ;
	#else
		terminal->adjustment->value =
			ml_term_get_num_of_logged_lines( terminal->pvt->term) ;
		terminal->adjustment->upper = terminal->adjustment->value + terminal->row_count ;
		terminal->adjustment->page_increment = terminal->row_count ;
		terminal->adjustment->page_size = terminal->row_count ;

		gtk_adjustment_changed( terminal->adjustment) ;
		gtk_adjustment_value_changed( terminal->adjustment) ;
	#endif
	}

#if  ! GTK_CHECK_VERSION(2,90,0)
	/*
	 * XXX
	 * Vertical writing mode and screen_(width|height)_ratio option are not supported.
	 *
	 * Processing similar to vte_terminal_get_preferred_{width|height}().
	 */
	GTK_WIDGET(terminal)->requisition.width =
		terminal->column_count * terminal->char_width + WINDOW_MARGIN * 2 ;
	GTK_WIDGET(terminal)->requisition.height =
		terminal->row_count * terminal->char_height + WINDOW_MARGIN * 2 ;

#ifdef  __DEBUG
	kik_debug_printf( KIK_DEBUG_TAG
		" char_width %d char_height %d row_count %d column_count %d width %d height %d\n" ,
		terminal->char_width , terminal->char_height ,
		terminal->row_count , terminal->column_count ,
		GTK_WIDGET(terminal)->requisition.width ,
		GTK_WIDGET(terminal)->requisition.height) ;
#endif
#endif	/* ! GTK_CHECK_VERSION(2,90,0) */

#if  defined(__DEBUG) && VTE_CHECK_VERSION(0,23,2)
	{
		GtkBorder *  border = NULL ;
		
		gtk_widget_style_get( GTK_WIDGET(terminal) , "inner-border" , &border , NULL) ;
		if( border)
		{
			kik_debug_printf( " inner-border is { %d, %d, %d, %d }\n",
				border->left, border->right, border->top, border->bottom) ;
			gtk_border_free(border) ;
		}
		else
		{
			kik_debug_printf( " inner-border is not found.\n") ;
		}
	}
#endif
}

static gboolean
toplevel_configure(
	gpointer  data
	)
{
	VteTerminal *  terminal ;

	terminal = data ;

	if( terminal->pvt->screen->window.is_transparent)
	{
		XEvent  ev ;

		if( ! XCheckTypedWindowEvent( disp.display ,
				gdk_x11_drawable_get_xid( gtk_widget_get_window(
					gtk_widget_get_toplevel( GTK_WIDGET(terminal)))) ,
				ConfigureNotify , &ev))
		{
			x_window_set_transparent( &terminal->pvt->screen->window ,
				x_screen_get_picture_modifier( terminal->pvt->screen)) ;
		}
		else
		{
			XPutBackEvent( disp.display , &ev) ;
		}
	}

	return  FALSE ;
}

static void
vte_terminal_hierarchy_changed(
	GtkWidget *  widget ,
	GtkWidget *  old_toplevel ,
	gpointer  data
	)
{
	if( old_toplevel)
	{
		g_signal_handlers_disconnect_by_func( old_toplevel , toplevel_configure ,
			widget) ;
	}
	
	g_signal_connect_swapped( gtk_widget_get_toplevel( widget) , "configure-event" ,
		G_CALLBACK(toplevel_configure) , VTE_TERMINAL(widget)) ;
}

static gboolean
vte_terminal_timeout(
	gpointer  data
	)
{
	/*
	 * If gdk_threads_add_timeout (2.12 or later) doesn't exist,
	 * call gdk_threads_{enter|leave} manually for MT-safe.
	 */
#if  ! GTK_CHECK_VERSION(2,12,0)
	gdk_threads_enter() ;
#endif

	ml_close_dead_terms() ;

	x_display_idling( &disp) ;

#if  ! GTK_CHECK_VERSION(2,12,0)
	gdk_threads_leave() ;
#endif

	return  TRUE ;
}


static void vte_terminal_size_allocate( GtkWidget *  widget , GtkAllocation *  allocation) ;

/*
 * Don't call ml_close_dead_terms() before returning GDK_FILTER_CONTINUE,
 * because ml_close_dead_terms() will destroy widget in pty_closed and
 * destroyed widget can be touched right after this function.
 */
static GdkFilterReturn
vte_terminal_filter(
	GdkXEvent *  xevent ,
	GdkEvent *  event ,	/* GDK_NOTHING */
	gpointer  data
	)
{
	u_int  count ;
	int  is_key_event ;

	if( XFilterEvent( (XEvent*)xevent , None))
	{
		return  GDK_FILTER_REMOVE ;
	}

	if( ( ((XEvent*)xevent)->type == KeyPress ||
		((XEvent*)xevent)->type == KeyRelease) )
	{
		is_key_event = 1 ;
	}
	else
	{
		is_key_event = 0 ;
	}

	for( count = 0 ; count < disp.num_of_roots ; count++)
	{
		VteTerminal *  terminal ;

		if( IS_MLTERM_SCREEN(disp.roots[count]))
		{
			terminal = VTE_WIDGET((x_screen_t*)disp.roots[count]) ;

			if( ! terminal->pvt->term)
			{
				/* pty is already closed and new pty is not attached yet. */
				continue ;
			}

			/*
			 * Key events are ignored if window isn't focused.
			 * This processing is added for key binding of popup menu.
			 */
			if( is_key_event &&
			    ((XEvent*)xevent)->xany.window == disp.roots[count]->my_window)
			{
				ml_term_search_reset_position( terminal->pvt->term) ;

				if( ! disp.roots[count]->is_focused)
				{
					((XEvent*)xevent)->xany.window =
						gdk_x11_drawable_get_xid(
							gtk_widget_get_window(
								GTK_WIDGET(terminal))) ;

					return  GDK_FILTER_CONTINUE ;
				}
			}

			if( terminal->pvt->screen->window.is_transparent &&
			    ((XEvent*)xevent)->type == ConfigureNotify &&
			    ((XEvent*)xevent)->xconfigure.event ==
					gdk_x11_drawable_get_xid(
						gtk_widget_get_window( GTK_WIDGET(terminal))))
			{
				/*
				 * If terminal position is changed by adding menu bar or tab,
				 * transparent background is reset.
				 */

				gint  x ;
				gint  y ;

				gdk_window_get_position(
					gtk_widget_get_window( GTK_WIDGET(terminal)) , &x , &y) ;

				/*
				 * XXX
				 * I don't know why but the height of menu bar has been already
				 * added to the position of terminal before first
				 * GdkConfigureEvent whose x and y is 0 is received.
				 * But (x != xconfigure.x || y != xconfigure.y) is true eventually
				 * and x_window_set_transparent() is called expectedly.
				 */
				if( x != ((XEvent*)xevent)->xconfigure.x ||
				    y != ((XEvent*)xevent)->xconfigure.y)
				{
					x_window_set_transparent( &terminal->pvt->screen->window ,
						x_screen_get_picture_modifier(
							terminal->pvt->screen)) ;
				}

				return  GDK_FILTER_CONTINUE ;
			}
		}
		else
		{
			terminal = NULL ;
		}

		if( x_window_receive_event( disp.roots[count] , (XEvent*)xevent))
		{
			static pid_t  config_menu_pid = 0 ;

			if( ! terminal || /* SCIM etc window */
			    /* XFilterEvent in x_window_receive_event. */
			    ((XEvent*)xevent)->xany.window != disp.roots[count]->my_window)
			{
				return  GDK_FILTER_REMOVE ;
			}
			
			/* XXX Hack for waiting for config menu program exiting. */
			if( config_menu_pid != terminal->pvt->term->config_menu.pid)
			{
				if( ( config_menu_pid = terminal->pvt->term->config_menu.pid))
				{
					vte_reaper_add_child( config_menu_pid) ;
				}
			}

			if( is_key_event ||
			    ((XEvent*)xevent)->type == ButtonPress ||
			    ((XEvent*)xevent)->type == ButtonRelease)
			{
				/* Hook key and button events for popup menu. */
				((XEvent*)xevent)->xany.window =
					gdk_x11_drawable_get_xid(
						gtk_widget_get_window( GTK_WIDGET(terminal))) ;

				return  GDK_FILTER_CONTINUE ;
			}
			else
			{
				return  GDK_FILTER_REMOVE ;
			}
		}
		/*
		 * xconfigure.window:  window whose size, position, border, and/or stacking
		 *                     order was changed.
		 *                      => processed in following.
		 * xconfigure.event:   reconfigured window or to its parent.
		 * (=XAnyEvent.window)  => processed in x_window_receive_event()
		 */
		else if( /* terminal && */ ((XEvent*)xevent)->type == ConfigureNotify &&
			((XEvent*)xevent)->xconfigure.window == disp.roots[count]->my_window)
		{
		#if  0
			/*
			 * This check causes resize problem in opening tab in
			 * gnome-terminal(2.29.6).
			 */
			if( ((XEvent*)xevent)->xconfigure.width !=
				GTK_WIDGET(terminal)->allocation.width ||
			    ((XEvent*)xevent)->xconfigure.height !=
				GTK_WIDGET(terminal)->allocation.height)
		#else
			if( terminal->char_width != x_col_width( terminal->pvt->screen) ||
			    terminal->char_height != x_line_height( terminal->pvt->screen))
		#endif
			{
				/* Window was changed due to change of font size inside mlterm. */
				GtkAllocation  alloc ;

				gtk_widget_get_allocation( GTK_WIDGET(terminal) , &alloc) ;
				alloc.width = ((XEvent*)xevent)->xconfigure.width ;
				alloc.height = ((XEvent*)xevent)->xconfigure.height ;

			#ifdef  __DEBUG
				kik_debug_printf( KIK_DEBUG_TAG " child is resized\n") ;
			#endif

				vte_terminal_size_allocate( GTK_WIDGET(terminal) , &alloc) ;
			}

			return  GDK_FILTER_REMOVE ;
		}
	}

	return  GDK_FILTER_CONTINUE ;
}

static void
vte_terminal_finalize(
	GObject *  obj
	)
{
	VteTerminal *  terminal ;
	GtkSettings *  settings ;

#ifdef  DEBUG
	kik_debug_printf( KIK_DEBUG_TAG " vte terminal finalized.\n") ;
#endif

	terminal = VTE_TERMINAL(obj) ;

#if  VTE_CHECK_VERSION(0,26,0)
	if( terminal->pvt->pty)
	{
		g_object_unref( terminal->pvt->pty) ;
	}
#endif

	x_font_manager_delete( terminal->pvt->screen->font_man) ;
	x_color_manager_delete( terminal->pvt->screen->color_man) ;

	if( terminal->pvt->image)
	{
		g_object_unref( terminal->pvt->image) ;
		terminal->pvt->image = NULL ;
	}

	if( terminal->pvt->pixmap)
	{
		XFreePixmap( disp.display , terminal->pvt->pixmap) ;
		terminal->pvt->pixmap = None ;
	}

	free( terminal->pvt->pic_mod) ;

	x_window_final( &terminal->pvt->screen->window) ;
	terminal->pvt->screen = NULL ;


	if( terminal->adjustment)
	{
		g_object_unref( terminal->adjustment) ;
	}

	settings = gtk_widget_get_settings( GTK_WIDGET(obj)) ;
	g_signal_handlers_disconnect_matched( settings , G_SIGNAL_MATCH_DATA ,
		0 , 0 , NULL , NULL , terminal) ;

	G_OBJECT_CLASS(vte_terminal_parent_class)->finalize(obj) ;
	
#ifdef  __DEBUG
	kik_debug_printf( KIK_DEBUG_TAG " vte_terminal_finalize\n") ;
#endif
}

static void
vte_terminal_get_property(
	GObject *  obj ,
	guint  prop_id ,
	GValue *  value ,
	GParamSpec *  pspec
	)
{
	VteTerminal *  terminal ;

	terminal = VTE_TERMINAL(obj) ;

	switch( prop_id)
	{
	#if  GTK_CHECK_VERSION(2,90,0)
		case  PROP_VADJUSTMENT:
			g_value_set_object( value , terminal->adjustment) ;
			break ;
	#endif

		case  PROP_ICON_TITLE:
			g_value_set_string( value , vte_terminal_get_icon_title( terminal)) ;
			break ;
			
		case  PROP_WINDOW_TITLE:
			g_value_set_string( value , vte_terminal_get_window_title( terminal)) ;
			break ;

	#if  0
		default:
			G_OBJECT_WARN_INVALID_PROPERTY_ID( obj , prop_id , pspec) ;
	#endif
	}
}

static void
vte_terminal_set_property(
	GObject *  obj ,
	guint  prop_id ,
	const GValue *  value ,
	GParamSpec *  pspec
	)
{
	VteTerminal *  terminal ;

	terminal = VTE_TERMINAL(obj) ;

	switch( prop_id)
	{
	#if  GTK_CHECK_VERSION(2,90,0)
		case  PROP_VADJUSTMENT:
			set_adjustment( terminal , g_value_get_object(value)) ;
			break ;
	#endif

	#if  0
		case  PROP_ICON_TITLE:
			set_icon_name( terminal->pvt->screen , g_value_get_string(value)) ;
			break ;
			
		case  PROP_WINDOW_TITLE:
			set_window_name( terminal->pvt->screen , g_value_get_string(value)) ;
			break ;
	#endif

	#if  0
		default:
			G_OBJECT_WARN_INVALID_PROPERTY_ID( obj , prop_id , pspec) ;
	#endif
	}
}


static void
init_screen(
	VteTerminal *  terminal ,
	x_font_manager_t *  font_man ,
	x_color_manager_t *  color_man
	)
{
	/*
	 * XXX
	 * terminal->pvt->term is specified to x_screen_new in order to set
	 * x_window_t::width and height property, but screen->term is NULL
	 * until widget is realized.
	 */
	terminal->pvt->screen = x_screen_new( terminal->pvt->term , font_man , color_man ,
			x_termcap_get_entry( &termcap , main_config.term_type) ,
			main_config.brightness , main_config.contrast , main_config.gamma ,
			main_config.alpha , main_config.fade_ratio , &shortcut ,
			main_config.screen_width_ratio , main_config.screen_height_ratio ,
			main_config.mod_meta_key ,
			main_config.mod_meta_mode , main_config.bel_mode ,
			main_config.receive_string_via_ucs , main_config.pic_file_path ,
			main_config.use_transbg , main_config.use_vertical_cursor ,
			main_config.big5_buggy , main_config.use_extended_scroll_shortcut ,
			main_config.borderless , main_config.line_space ,
			main_config.input_method , main_config.allow_osc52 ,
			main_config.blink_cursor , WINDOW_MARGIN , main_config.hide_underline) ;
	if( terminal->pvt->term)
	{
		ml_term_detach( terminal->pvt->term) ;
		terminal->pvt->screen->term = NULL ;
	}
	else
	{
		/*
		 * terminal->pvt->term can be NULL if this function is called from
		 * vte_terminal_unrealize.
		 */
	}

	memset( &terminal->pvt->system_listener , 0 , sizeof(x_system_event_listener_t)) ;
	terminal->pvt->system_listener.self = terminal ;
	terminal->pvt->system_listener.font_config_updated = font_config_updated ;
	terminal->pvt->system_listener.color_config_updated = color_config_updated ;
	terminal->pvt->system_listener.open_pty = open_pty ;
	terminal->pvt->system_listener.pty_list = pty_list ;
	terminal->pvt->system_listener.exit = __exit ;
	x_set_system_listener( terminal->pvt->screen , &terminal->pvt->system_listener) ;

	memset( &terminal->pvt->screen_scroll_listener , 0 ,
		sizeof(x_screen_scroll_event_listener_t)) ;
	terminal->pvt->screen_scroll_listener.self = terminal ;
	terminal->pvt->screen_scroll_listener.bs_mode_exited = bs_mode_exited ;
	terminal->pvt->screen_scroll_listener.scrolled_upward = scrolled_upward ;
	terminal->pvt->screen_scroll_listener.scrolled_downward = scrolled_downward ;
	terminal->pvt->screen_scroll_listener.log_size_changed = log_size_changed ;
	x_set_screen_scroll_listener( terminal->pvt->screen ,
		&terminal->pvt->screen_scroll_listener) ;

	terminal->pvt->line_scrolled_out =
		terminal->pvt->screen->screen_listener.line_scrolled_out ;
	terminal->pvt->screen->screen_listener.line_scrolled_out = line_scrolled_out ;
	
	terminal->pvt->set_window_name =
		terminal->pvt->screen->xterm_listener.set_window_name ;
	terminal->pvt->screen->xterm_listener.set_window_name = set_window_name ;
	terminal->pvt->set_icon_name =
		terminal->pvt->screen->xterm_listener.set_icon_name ;
	terminal->pvt->screen->xterm_listener.set_icon_name = set_icon_name ;

	/* overriding */
	terminal->pvt->screen->pty_listener.closed = pty_closed ;
}

static void
update_wall_picture(
	VteTerminal *  terminal
	)
{
	x_window_t *  win ;
	x_picture_modifier_t *  pic_mod ;
	GdkPixbuf *  image ;
	char  file[7 + DIGIT_STR_LEN(terminal->pvt->pixmap) + 1] ;

	if( ! terminal->pvt->image)
	{
		goto  set_bg_alpha ;
	}
	
	win = &terminal->pvt->screen->window ;
	pic_mod = x_screen_get_picture_modifier( terminal->pvt->screen) ;

	if( terminal->pvt->pix_width == ACTUAL_WIDTH(win) &&
	    terminal->pvt->pix_height == ACTUAL_WIDTH(win) &&
	    x_picture_modifiers_equal( pic_mod , terminal->pvt->pic_mod) &&
	    terminal->pvt->pixmap )
	{
		goto  set_bg_image ;
	}
	else if( gdk_pixbuf_get_width(terminal->pvt->image) != ACTUAL_WIDTH(win) ||
	         gdk_pixbuf_get_height(terminal->pvt->image) != ACTUAL_HEIGHT(win) )
	{
	#ifdef  DEBUG
		kik_debug_printf( KIK_DEBUG_TAG " Scaling bg img %d %d => %d %d\n" ,
				gdk_pixbuf_get_width(terminal->pvt->image) ,
				gdk_pixbuf_get_height(terminal->pvt->image) ,
				ACTUAL_WIDTH(win) , ACTUAL_HEIGHT(win)) ;
	#endif

		image = gdk_pixbuf_scale_simple( terminal->pvt->image ,
					ACTUAL_WIDTH(win) , ACTUAL_HEIGHT(win) ,
					GDK_INTERP_BILINEAR) ;
	}
	else
	{
		image = terminal->pvt->image ;
	}

	if( terminal->pvt->pixmap)
	{
		XFreePixmap( disp.display , terminal->pvt->pixmap) ;
	}

	terminal->pvt->pixmap = x_imagelib_pixbuf_to_pixmap( win , pic_mod , image) ;

	if( image != terminal->pvt->image)
	{
		g_object_unref( image) ;
	}

	if( terminal->pvt->pixmap == None)
	{
		kik_msg_printf( "Failed to convert pixbuf to pixmap. "
			"Rebuild mlterm with gdk-pixbuf.\n") ;

		terminal->pvt->pix_width = 0 ;
		terminal->pvt->pix_height = 0 ;
		terminal->pvt->pic_mod = NULL ;

		return ;
	}

	terminal->pvt->pix_width = ACTUAL_WIDTH(win) ;
	terminal->pvt->pix_height = ACTUAL_HEIGHT(win) ;
	if( pic_mod)
	{
		if( terminal->pvt->pic_mod == NULL)
		{
			terminal->pvt->pic_mod = malloc( sizeof( x_picture_modifier_t)) ;
		}
		
		*terminal->pvt->pic_mod = *pic_mod ;
	}
	else
	{
		free( terminal->pvt->pic_mod) ;
		terminal->pvt->pic_mod = NULL ;
	}

set_bg_image:
	x_color_manager_change_alpha( terminal->pvt->screen->color_man , 0xff) ;

	sprintf( file , "pixmap:%lu" , terminal->pvt->pixmap) ;
	vte_terminal_set_background_image_file( terminal , file) ;

	return ;

set_bg_alpha:
	/* If screen->pic_file_path is already set, true transparency is not enabled. */
	if( ! terminal->pvt->screen->pic_file_path &&
	    x_color_manager_change_alpha( terminal->pvt->screen->color_man ,
				terminal->pvt->screen->pic_mod.alpha))
	{
		/* Same processing as change_bg_color in x_screen.c */

		if( x_window_set_bg_color( &terminal->pvt->screen->window ,
			x_get_xcolor( terminal->pvt->screen->color_man , ML_BG_COLOR)))
		{
			x_xic_bg_color_changed( &terminal->pvt->screen->window) ;
		}
	}
}

static void
vte_terminal_realize(
	GtkWidget *  widget
	)
{
	GdkWindowAttr  attr ;
	GtkAllocation  allocation ;
	XID  xid ;

	if( gtk_widget_get_window(widget))
	{
		return ;
	}

	x_screen_attach( VTE_TERMINAL(widget)->pvt->screen , VTE_TERMINAL(widget)->pvt->term) ;

	gtk_widget_get_allocation( widget , &allocation) ;

#ifdef  DEBUG
	kik_debug_printf( KIK_DEBUG_TAG " vte terminal realized with size x %d y %d w %d h %d\n" ,
		allocation.x , allocation.y , allocation.width , allocation.height) ;
#endif

	GTK_WIDGET_SET_REALIZED( widget) ;

	attr.window_type = GDK_WINDOW_CHILD ;
	attr.x = allocation.x ;
	attr.y = allocation.y ;
	attr.width = allocation.width ;
	attr.height = allocation.height ;
	attr.wclass = GDK_INPUT_OUTPUT ;
	attr.visual = gtk_widget_get_visual( widget) ;
#if  ! GTK_CHECK_VERSION(2,90,0)
	attr.colormap = gtk_widget_get_colormap( widget) ;
#endif
	attr.event_mask = gtk_widget_get_events( widget) |
				GDK_FOCUS_CHANGE_MASK |
				GDK_BUTTON_PRESS_MASK |
				GDK_BUTTON_RELEASE_MASK |
				GDK_KEY_PRESS_MASK |
				GDK_KEY_RELEASE_MASK |
				GDK_SUBSTRUCTURE_MASK ;		/* DestroyNotify from child */

	gtk_widget_set_window( widget ,
		gdk_window_new( gtk_widget_get_parent_window( widget) , &attr ,
				GDK_WA_X | GDK_WA_Y |
				(attr.visual ? GDK_WA_VISUAL : 0)
			#if  ! GTK_CHECK_VERSION(2,90,0)
				| (attr.colormap ? GDK_WA_COLORMAP : 0)
			#endif
				)) ;

	/*
	 * Note that hook key and button events in vte_terminal_filter doesn't work without this.
	 */
	gdk_window_set_user_data( gtk_widget_get_window(widget) , widget) ;
	
#if  ! GTK_CHECK_VERSION(2,90,0)
	if( widget->style->font_desc)
	{
		pango_font_description_free( widget->style->font_desc) ;
		widget->style->font_desc = NULL ;
	}

	/* private_font(_desc) should be NULL if widget->style->font_desc is set NULL above. */
	if( widget->style->private_font)
	{
		gdk_font_unref( widget->style->private_font) ;
		widget->style->private_font = NULL ;
	}

	if( widget->style->private_font_desc)
	{
		pango_font_description_free( widget->style->private_font_desc) ;
		widget->style->private_font_desc = NULL ;
	}
#endif

	g_signal_connect_swapped( gtk_widget_get_toplevel( widget) , "configure-event" ,
		G_CALLBACK(toplevel_configure) , VTE_TERMINAL(widget)) ;

	xid = gdk_x11_drawable_get_xid( gtk_widget_get_window(widget)) ;

	if( disp.gc->gc == DefaultGC( disp.display , disp.screen))
	{
		/*
		 * Replace visual, colormap, depth and gc with those inherited from parent xid.
		 * In some cases that those of parent xid is not DefaultVisual, DefaultColormap
		 * and so on (e.g. compiz), BadMatch error can happen.
		 */

		XWindowAttributes  attr ;
		XGCValues  gc_value ;
		int  depth_is_changed ;

		XGetWindowAttributes( disp.display , xid , &attr) ;
		disp.visual = attr.visual ;
		disp.colormap = attr.colormap ;
		depth_is_changed = (disp.depth != attr.depth) ;
		disp.depth = attr.depth ;

		/* x_gc_t using DefaultGC is already created in vte_terminal_class_init */
		gc_value.foreground = disp.gc->fg_color ;
		gc_value.background = disp.gc->bg_color ;
		gc_value.graphics_exposures = True ;
		disp.gc->gc = XCreateGC( disp.display , xid ,
				GCForeground | GCBackground | GCGraphicsExposures , &gc_value) ;

	#ifdef  __DEBUG
		kik_debug_printf( KIK_DEBUG_TAG " Visual %x Colormap %x Depth %d\n" ,
			disp.visual , disp.colormap , disp.depth) ;
	#endif

		if( depth_is_changed)
		{
			x_color_manager_reload( VTE_TERMINAL(widget)->pvt->screen->color_man) ;

			/* No colors are cached for now. */
		#if  0
			x_color_cache_unload_all() ;
		#endif
		}
	}

	x_display_show_root( &disp , &VTE_TERMINAL(widget)->pvt->screen->window ,
		0 , 0 , 0 , "mlterm" , xid) ;

	/*
	 * allocation passed by size_allocate is not necessarily to be reflected
	 * to x_window_t or ml_term_t, so x_window_resize must be called here.
	 */
	if( VTE_TERMINAL(widget)->pvt->term->pty &&
	    ! is_initial_allocation( &allocation))
	{
		if( x_window_resize_with_margin( &VTE_TERMINAL(widget)->pvt->screen->window ,
			allocation.width , allocation.height , NOTIFY_TO_MYSELF))
		{
			reset_vte_size_member( VTE_TERMINAL(widget)) ;
		}
	}

	update_wall_picture( VTE_TERMINAL(widget)) ;
}

static void
vte_terminal_unrealize(
	GtkWidget *  widget
	)
{
	VteTerminal *  terminal ;
	x_screen_t *  screen ;

#ifdef  DEBUG
	kik_debug_printf( KIK_DEBUG_TAG " vte terminal unrealized.\n") ;
#endif

	terminal = VTE_TERMINAL(widget) ;

	x_screen_detach( terminal->pvt->screen) ;

	if( ! terminal->pvt->term->pty)
	{
		/* terminal->pvt->term is not deleted in pty_closed() */
		ml_term_delete( terminal->pvt->term) ;
		terminal->pvt->term = NULL ;
	}

	screen = terminal->pvt->screen ;
	/* Create dummy screen in case terminal will be realized again. */
	init_screen( terminal , screen->font_man , screen->color_man) ;
	x_display_remove_root( &disp , &screen->window) ;

	g_signal_handlers_disconnect_by_func( gtk_widget_get_toplevel( GTK_WIDGET(terminal)) ,
		G_CALLBACK(toplevel_configure) , terminal) ;

	GTK_WIDGET_CLASS(vte_terminal_parent_class)->unrealize( widget) ;
}

static gboolean
vte_terminal_focus_in(
	GtkWidget *  widget ,
	GdkEventFocus *  event
	)
{
	GTK_WIDGET_SET_HAS_FOCUS( widget) ;

	if( GTK_WIDGET_MAPPED( widget))
	{
	#ifdef  __DEBUG
		kik_debug_printf( KIK_DEBUG_TAG " focus in\n") ;
	#endif

		XSetInputFocus( disp.display ,
			VTE_TERMINAL( widget)->pvt->screen->window.my_window ,
			RevertToParent , CurrentTime) ;
	}

	return  FALSE ;
}

static gboolean
vte_terminal_focus_out(
	GtkWidget *  widget ,
	GdkEventFocus *  event
	)
{
	GTK_WIDGET_UNSET_HAS_FOCUS( widget) ;

	return  FALSE ;
}


#if  GTK_CHECK_VERSION(2,90,0)

static void
vte_terminal_get_preferred_width(
	GtkWidget *  widget ,
	gint *  minimum_width ,
	gint *  natural_width
	)
{
	/* Processing similar to setting GtkWidget::requisition in reset_vte_size_member(). */

	if( minimum_width)
	{
		*minimum_width = VTE_TERMINAL(widget)->char_width + WINDOW_MARGIN * 2 ;

	#ifdef  DEBUG
		kik_debug_printf( KIK_DEBUG_TAG " preferred minimum width %d\n" , *minimum_width) ;
	#endif
	}

	if( natural_width)
	{
		*natural_width =
			VTE_TERMINAL(widget)->column_count * VTE_TERMINAL(widget)->char_width +
			WINDOW_MARGIN * 2 ;

	#ifdef  DEBUG
		kik_debug_printf( KIK_DEBUG_TAG " preferred natural width %d\n" , *natural_width) ;
	#endif
	}
}

static void
vte_terminal_get_preferred_width_for_height(
	GtkWidget *  widget ,
	gint  height ,
	gint *  minimum_width ,
	gint *  natural_width
	)
{
#ifdef  __DEBUG
	kik_debug_printf( KIK_DEBUG_TAG " preferred width for height %d\n" , height) ;
#endif

	vte_terminal_get_preferred_width( widget , minimum_width , natural_width) ;
}

static void
vte_terminal_get_preferred_height(
	GtkWidget *  widget ,
	gint *  minimum_height ,
	gint *  natural_height
	)
{
	/* Processing similar to setting GtkWidget::requisition in reset_vte_size_member(). */

	/* XXX */
	if( strstr( g_get_prgname() , "roxterm"))
	{
		/*
		 * XXX
		 * I don't know why, but the size of roxterm 2.6.5 (GTK+3) is
		 * minimized unless "char-size-changed" signal is emit once in
		 * vte_terminal_get_preferred_height() or
		 * vte_terminal_get_preferred_height() in startup.
		 *
		 * Note that this hack might not work for roxterm started by
		 * "x-terminal-emulator" or "exo-open --launch TerminalEmulator"
		 * (which calls "x-terminal-emulator" internally).
		 */
		if( ! VTE_TERMINAL(widget)->pvt->init_char_size)
		{
			g_signal_emit_by_name( widget , "char-size-changed" ,
					VTE_TERMINAL(widget)->char_width ,
					VTE_TERMINAL(widget)->char_height) ;
			VTE_TERMINAL(widget)->pvt->init_char_size = 1 ;
		}
	}

	if( minimum_height)
	{
		*minimum_height = VTE_TERMINAL(widget)->char_height + WINDOW_MARGIN * 2 ;

	#ifdef  DEBUG
		kik_debug_printf( KIK_DEBUG_TAG " preferred minimum height %d\n" ,
			*minimum_height) ;
	#endif
	}

	if( natural_height)
	{
		*natural_height =
			VTE_TERMINAL(widget)->row_count * VTE_TERMINAL(widget)->char_height +
			WINDOW_MARGIN * 2 ;

	#ifdef  DEBUG
		kik_debug_printf( KIK_DEBUG_TAG " preferred natural height %d\n" ,
			*natural_height) ;
	#endif
	}
}

static void
vte_terminal_get_preferred_height_for_width(
	GtkWidget *  widget ,
	gint  width ,
	gint *  minimum_height ,
	gint *  natural_height
	)
{
#ifdef  __DEBUG
	kik_debug_printf( KIK_DEBUG_TAG " preferred height for width %d\n" , width) ;
#endif

	vte_terminal_get_preferred_height( widget , minimum_height , natural_height) ;
}

#else /* GTK_CHECK_VERSION(2,90,0) */

static void
vte_terminal_size_request(
	GtkWidget *  widget ,
	GtkRequisition *  req
	)
{
	*req = widget->requisition ;

#ifdef  DEBUG
	kik_debug_printf( KIK_DEBUG_TAG " size_request %d %d cur alloc %d %d\n" ,
			req->width , req->height ,
			widget->allocation.width , widget->allocation.height) ;
#endif
}

#endif /* GTK_CHECK_VERSION(2,90,0) */


static void
vte_terminal_size_allocate(
	GtkWidget *  widget ,
	GtkAllocation *  allocation
	)
{
	int  is_resized ;
	GtkAllocation  cur_allocation ;
	
	gtk_widget_get_allocation( widget , &cur_allocation) ;

#ifdef  DEBUG
	kik_debug_printf( KIK_DEBUG_TAG " size_allocate %d %d %d %d => %d %d %d %d\n" ,
		cur_allocation.x , cur_allocation.y ,
		cur_allocation.width , cur_allocation.height ,
		allocation->x , allocation->y , allocation->width , allocation->height) ;
#endif

	if( ! (is_resized = (cur_allocation.width != allocation->width ||
	                       cur_allocation.height != allocation->height)) &&
	    cur_allocation.x == allocation->x &&
	    cur_allocation.y == allocation->y)
	{
		return ;
	}

	gtk_widget_set_allocation( widget , allocation) ;

	if( GTK_WIDGET_REALIZED(widget))
	{
		if( is_resized && VTE_TERMINAL(widget)->pvt->term->pty)
		{
			/*
			 * Even if x_window_resize_with_margin returns 0,
			 * reset_vte_size_member etc functions must be called,
			 * because VTE_TERMNAL(widget)->pvt->screen can be already
			 * resized and vte_terminal_size_allocate can be called
			 * from vte_terminal_filter.
			 */
			x_window_resize_with_margin(
				&VTE_TERMINAL(widget)->pvt->screen->window ,
				allocation->width , allocation->height , NOTIFY_TO_MYSELF) ;
			reset_vte_size_member( VTE_TERMINAL(widget)) ;
			update_wall_picture( VTE_TERMINAL(widget)) ;
			/*
			 * gnome-terminal(2.29.6 or later ?) is not resized correctly
			 * without this.
			 */
			gtk_widget_queue_resize_no_redraw( widget) ;
		}
		
		gdk_window_move_resize( gtk_widget_get_window(widget),
				allocation->x, allocation->y,
				allocation->width, allocation->height) ;
	}
	else
	{
		/*
		 * x_window_resize_with_margin( widget->allocation.width, height)
		 * will be called in vte_terminal_realize() or vte_terminal_fork*().
		 */
	}
}

static gboolean
vte_terminal_key_press(
	GtkWidget *  widget ,
	GdkEventKey *  event
	)
{
	/* Check if GtkWidget's behavior already does something with this key. */
	GTK_WIDGET_CLASS(vte_terminal_parent_class)->key_press_event( widget , event) ;

	/* If FALSE is returned, tab operation is unexpectedly started in gnome-terminal. */
	return  TRUE ;
}

static void
vte_terminal_class_init(
	VteTerminalClass *  vclass
	)
{
	char *  value ;
	kik_conf_t *  conf ;
	char *  argv[] = { "mlterm" , NULL } ;
	GObjectClass *  oclass ;
	GtkWidgetClass *  wclass ;

#ifdef  __DEBUG
	XSetErrorHandler( error_handler) ;
	XSynchronize( gdk_x11_display_get_xdisplay( gdk_display_get_default()) , True) ;
#endif

	/* kik_sig_child_init() calls signal(3) internally. */
#if  0
	kik_sig_child_init() ;
#endif

	kik_priv_change_euid( kik_getuid()) ;
	kik_priv_change_egid( kik_getgid()) ;

#if  0
	bindtextdomain( "vte" , LOCALEDIR) ;
	bind_textdomain_codeset( "vte" , "UTF-8") ;
#endif

	if( ! kik_locale_init( ""))
	{
		kik_msg_printf( "locale settings failed.\n") ;
	}

	kik_set_sys_conf_dir( CONFIG_PATH) ;

	ml_term_manager_init( 1) ;
	ml_term_manager_enable_zombie_pty() ;
#if  GTK_CHECK_VERSION(2,12,0)
	gdk_threads_add_timeout( 100 , vte_terminal_timeout , NULL) ;	/* 100 miliseconds */
#else
	g_timeout_add( 100 , vte_terminal_timeout , NULL) ;	/* 100 miliseconds */
#endif

	ml_color_config_init() ;
	x_shortcut_init( &shortcut) ;
	x_shortcut_parse( &shortcut , "Button3" , "\"none\"") ;
	x_termcap_init( &termcap) ;
	x_xim_init( 1) ;
	x_font_use_point_size_for_fc( 1) ;

	kik_init_prog( g_get_prgname() , VERSION) ;

	if( ( conf = kik_conf_new()) == NULL)
	{
		return ;
	}
	
	x_prepare_for_main_config( conf) ;

	/*
	 * Same processing as main_loop_init().
	 * Following options are not possible to specify as arguments of mlclient.
	 * 1) Options which are used only when mlterm starts up and which aren't
	 *    changed dynamically. (e.g. "startup_screens")
	 * 2) Options which change status of all ptys or windows. (Including ones
	 *    which are possible to change dynamically.)
	 *    (e.g. "font_size_range")
	 */

#if  0
	kik_conf_add_opt( conf , 'R' , "fsrange" , 0 , "font_size_range" , NULL) ;
#endif
	kik_conf_add_opt( conf , 'W' , "sep" , 0 , "word_separators" , NULL) ;
	kik_conf_add_opt( conf , 'Y' , "decsp" , 1 , "compose_dec_special_font" , NULL) ;
	kik_conf_add_opt( conf , 'c' , "cp932" , 1 , "use_cp932_ucs_for_xft" , NULL) ;
	kik_conf_add_opt( conf , '\0' , "restart" , 1 , "auto_restart" , NULL) ;

#if  0
	if( ( value = kik_conf_get_value( conf , "font_size_range")))
	{
		u_int  min_font_size ;
		u_int  max_font_size ;

		if( get_font_size_range( &min_font_size , &max_font_size , value))
		{
			x_set_font_size_range( min_font_size , max_font_size) ;
		}
	}
#endif

	x_main_config_init( &main_config , conf , 1 , argv) ;

	/* BACKWARD COMPAT (3.1.7 or before) */
#if  1
	{
		size_t  count ;
		/*
		 * Compat with button3_behavior (shortcut_str[3]) is not applied
		 * because button3 is disabled by
		 * x_shortcut_parse( &shortcut , "Button3" , "\"none\"") above.
		 */
		char *  keys[] = { "Control+Button1" , "Control+Button2" ,
					"Control+Button3" } ;

		for( count = 0 ; count < sizeof(keys) / sizeof(keys[0]) ; count ++)
		{
			if( main_config.shortcut_strs[count])
			{
				x_shortcut_parse( &shortcut ,
					keys[count] , main_config.shortcut_strs[count]) ;
			}
		}
	}
#endif

	if( main_config.type_engine == TYPE_XCORE)
	{
		/*
		 * XXX Hack
		 * Default value of type_engine is TYPE_XCORE in normal mlterm,
		 * but default value in libvte compatible library of mlterm is TYPE_XFT.
		 */
		char *  value ;
		
		if( ( value = kik_conf_get_value( conf , "type_engine")) == NULL ||
			strcmp( value , "xcore") != 0)
		{
			/*
			 * cairo is prefered if mlterm works as libvte because gtk+
			 * usually depends on cairo.
			 */
		#if  ! defined(USE_TYPE_CAIRO) && defined(USE_TYPE_XFT)
			main_config.type_engine = TYPE_XFT ;
		#else
			main_config.type_engine = TYPE_CAIRO ;
		#endif
		}
	}

	/* Default value of vte "audible-bell" is TRUE, while "visible-bell" is FALSE. */
	main_config.bel_mode = BEL_SOUND ;

	if( ( value = kik_conf_get_value( conf , "compose_dec_special_font")))
	{
		if( strcmp( value , "true") == 0)
		{
			x_compose_dec_special_font() ;
		}
	}

	if( ( value = kik_conf_get_value( conf , "use_cp932_ucs_for_xft")) == NULL ||
		strcmp( value , "true") == 0)
	{
		x_use_cp932_ucs_for_xft() ;
	}

	if( ( value = kik_conf_get_value( conf , "word_separators")))
	{
		ml_set_word_separators( value) ;
	}

	if( ! ( value = kik_conf_get_value( conf , "auto_restart")) ||
	    strcmp( value , "true") == 0)
	{
		ml_set_auto_restart_cmd( kik_get_prog_path()) ;
	}

	kik_conf_delete( conf) ;


	g_signal_connect( vte_reaper_get() , "child-exited" ,
		G_CALLBACK(catch_child_exited) , NULL) ;

	g_type_class_add_private( vclass , sizeof(VteTerminalPrivate)) ;

	memset( &disp , 0 , sizeof(x_display_t)) ;
	disp.display = gdk_x11_display_get_xdisplay( gdk_display_get_default()) ;
	disp.screen = DefaultScreen(disp.display) ;
	disp.my_window = DefaultRootWindow(disp.display) ;
	disp.visual = DefaultVisual( disp.display , disp.screen) ;
	disp.colormap = DefaultColormap( disp.display , disp.screen) ;
	disp.depth = DefaultDepth( disp.display , disp.screen) ;
	disp.gc = x_gc_new( disp.display , None) ;
	disp.width = DisplayWidth( disp.display , disp.screen) ;
	disp.height = DisplayHeight( disp.display , disp.screen) ;
	disp.modmap.serial = 0 ;
	disp.modmap.map = XGetModifierMapping( disp.display) ;

	x_xim_display_opened( disp.display) ;
	x_picture_display_opened( disp.display) ;

	gdk_window_add_filter( NULL , vte_terminal_filter , NULL) ;

	oclass = G_OBJECT_CLASS(vclass) ;
	wclass = GTK_WIDGET_CLASS(vclass) ;

	oclass->finalize = vte_terminal_finalize ;
	oclass->get_property = vte_terminal_get_property ;
	oclass->set_property = vte_terminal_set_property ;
	wclass->realize = vte_terminal_realize ;
	wclass->unrealize = vte_terminal_unrealize ;
	wclass->focus_in_event = vte_terminal_focus_in ;
	wclass->focus_out_event = vte_terminal_focus_out ;
	wclass->size_allocate = vte_terminal_size_allocate ;
#if  GTK_CHECK_VERSION(2,90,0)
	wclass->get_preferred_width = vte_terminal_get_preferred_width ;
	wclass->get_preferred_height = vte_terminal_get_preferred_height ;
	wclass->get_preferred_width_for_height = vte_terminal_get_preferred_width_for_height ;
	wclass->get_preferred_height_for_width = vte_terminal_get_preferred_height_for_width ;
#else
	wclass->size_request = vte_terminal_size_request ;
#endif
	wclass->key_press_event = vte_terminal_key_press ;

#if  GTK_CHECK_VERSION(2,90,0)
	g_object_class_override_property( oclass , PROP_HADJUSTMENT , "hadjustment") ;
	g_object_class_override_property( oclass , PROP_VADJUSTMENT , "vadjustment") ;
	g_object_class_override_property( oclass , PROP_HSCROLL_POLICY , "hscroll-policy") ;
	g_object_class_override_property( oclass , PROP_VSCROLL_POLICY , "vscroll-policy") ;
#endif

#if  ! GTK_CHECK_VERSION(2,90,0)
	vclass->eof_signal =
#endif
		g_signal_new( I_("eof") ,
			G_OBJECT_CLASS_TYPE(vclass) ,
			G_SIGNAL_RUN_LAST ,
			G_STRUCT_OFFSET( VteTerminalClass , eof) ,
			NULL , NULL , g_cclosure_marshal_VOID__VOID ,
			G_TYPE_NONE , 0) ;

#if  ! GTK_CHECK_VERSION(2,90,0)
	vclass->child_exited_signal =
#endif
		g_signal_new( I_("child-exited") ,
			G_OBJECT_CLASS_TYPE(vclass) ,
			G_SIGNAL_RUN_LAST ,
			G_STRUCT_OFFSET( VteTerminalClass , child_exited) ,
			NULL , NULL , g_cclosure_marshal_VOID__VOID ,
			G_TYPE_NONE , 0) ;

#if  ! GTK_CHECK_VERSION(2,90,0)
	vclass->window_title_changed_signal =
#endif
		g_signal_new( I_("window-title-changed") ,
			G_OBJECT_CLASS_TYPE(vclass) ,
			G_SIGNAL_RUN_LAST ,
			G_STRUCT_OFFSET( VteTerminalClass , window_title_changed) ,
			NULL , NULL , g_cclosure_marshal_VOID__VOID ,
			G_TYPE_NONE , 0) ;

#if  ! GTK_CHECK_VERSION(2,90,0)
	vclass->icon_title_changed_signal =
#endif
		g_signal_new( I_("icon-title-changed") ,
			G_OBJECT_CLASS_TYPE(vclass) ,
			G_SIGNAL_RUN_LAST ,
			G_STRUCT_OFFSET( VteTerminalClass , icon_title_changed) ,
			NULL , NULL , g_cclosure_marshal_VOID__VOID ,
			G_TYPE_NONE , 0) ;

#if  ! GTK_CHECK_VERSION(2,90,0)
	vclass->encoding_changed_signal =
#endif
		g_signal_new( I_("encoding-changed") ,
			G_OBJECT_CLASS_TYPE(vclass) ,
			G_SIGNAL_RUN_LAST ,
			G_STRUCT_OFFSET( VteTerminalClass , encoding_changed) ,
			NULL , NULL , g_cclosure_marshal_VOID__VOID ,
			G_TYPE_NONE , 0) ;

#if  ! GTK_CHECK_VERSION(2,90,0)
	vclass->commit_signal =
#endif
		g_signal_new( I_("commit") ,
			G_OBJECT_CLASS_TYPE(vclass) ,
			G_SIGNAL_RUN_LAST ,
			G_STRUCT_OFFSET(VteTerminalClass , commit) ,
			NULL ,
			NULL ,
			_vte_marshal_VOID__STRING_UINT ,
			G_TYPE_NONE , 2 , G_TYPE_STRING , G_TYPE_UINT) ;

#if  ! GTK_CHECK_VERSION(2,90,0)
	vclass->emulation_changed_signal =
#endif
		g_signal_new( I_("emulation-changed") ,
			G_OBJECT_CLASS_TYPE(vclass) ,
			G_SIGNAL_RUN_LAST ,
			G_STRUCT_OFFSET(VteTerminalClass , emulation_changed) ,
			NULL , NULL , g_cclosure_marshal_VOID__VOID ,
			G_TYPE_NONE , 0) ;

#if  ! GTK_CHECK_VERSION(2,90,0)
	vclass->char_size_changed_signal =
#endif
		g_signal_new( I_("char-size-changed") ,
			G_OBJECT_CLASS_TYPE(vclass) ,
			G_SIGNAL_RUN_LAST ,
			G_STRUCT_OFFSET(VteTerminalClass , char_size_changed) ,
			NULL , NULL , _vte_marshal_VOID__UINT_UINT ,
			G_TYPE_NONE , 2 , G_TYPE_UINT , G_TYPE_UINT) ;

#if  ! GTK_CHECK_VERSION(2,90,0)
	vclass->selection_changed_signal =
#endif
		g_signal_new ( I_("selection-changed") ,
			G_OBJECT_CLASS_TYPE(vclass) ,
			G_SIGNAL_RUN_LAST ,
			G_STRUCT_OFFSET(VteTerminalClass , selection_changed) ,
			NULL , NULL , g_cclosure_marshal_VOID__VOID ,
			G_TYPE_NONE , 0) ;

#if  ! GTK_CHECK_VERSION(2,90,0)
	vclass->contents_changed_signal =
#endif
		g_signal_new( I_("contents-changed") ,
			G_OBJECT_CLASS_TYPE(vclass) ,
			G_SIGNAL_RUN_LAST ,
			G_STRUCT_OFFSET(VteTerminalClass , contents_changed) ,
			NULL , NULL , g_cclosure_marshal_VOID__VOID ,
			G_TYPE_NONE , 0) ;

#if  ! GTK_CHECK_VERSION(2,90,0)
	vclass->cursor_moved_signal =
#endif
		g_signal_new( I_("cursor-moved") ,
			G_OBJECT_CLASS_TYPE(vclass) ,
			G_SIGNAL_RUN_LAST ,
			G_STRUCT_OFFSET(VteTerminalClass , cursor_moved) ,
			NULL , NULL , g_cclosure_marshal_VOID__VOID ,
			G_TYPE_NONE , 0) ;

#if  ! GTK_CHECK_VERSION(2,90,0)
	vclass->deiconify_window_signal =
#endif
		g_signal_new( I_("deiconify-window") ,
			G_OBJECT_CLASS_TYPE(vclass) ,
			G_SIGNAL_RUN_LAST ,
			G_STRUCT_OFFSET(VteTerminalClass , deiconify_window) ,
			NULL , NULL , g_cclosure_marshal_VOID__VOID ,
			G_TYPE_NONE , 0) ;

#if  ! GTK_CHECK_VERSION(2,90,0)
	vclass->iconify_window_signal =
#endif
		g_signal_new( I_("iconify-window") ,
			G_OBJECT_CLASS_TYPE(vclass) ,
			G_SIGNAL_RUN_LAST ,
			G_STRUCT_OFFSET(VteTerminalClass , iconify_window) ,
			NULL , NULL , g_cclosure_marshal_VOID__VOID ,
			G_TYPE_NONE , 0) ;

#if  ! GTK_CHECK_VERSION(2,90,0)
	vclass->raise_window_signal =
#endif
		g_signal_new( I_("raise-window") ,
			G_OBJECT_CLASS_TYPE(vclass) ,
			G_SIGNAL_RUN_LAST ,
			G_STRUCT_OFFSET(VteTerminalClass , raise_window) ,
			NULL , NULL , g_cclosure_marshal_VOID__VOID ,
			G_TYPE_NONE , 0) ;

#if  ! GTK_CHECK_VERSION(2,90,0)
	vclass->lower_window_signal =
#endif
		g_signal_new( I_("lower-window") ,
			G_OBJECT_CLASS_TYPE(vclass) ,
			G_SIGNAL_RUN_LAST ,
			G_STRUCT_OFFSET(VteTerminalClass , lower_window) ,
			NULL , NULL , g_cclosure_marshal_VOID__VOID ,
			G_TYPE_NONE , 0) ;

#if  ! GTK_CHECK_VERSION(2,90,0)
	vclass->refresh_window_signal =
#endif
		g_signal_new( I_("refresh-window") ,
			G_OBJECT_CLASS_TYPE(vclass) ,
			G_SIGNAL_RUN_LAST ,
			G_STRUCT_OFFSET(VteTerminalClass , refresh_window) ,
			NULL , NULL , g_cclosure_marshal_VOID__VOID ,
			G_TYPE_NONE , 0) ;

#if  ! GTK_CHECK_VERSION(2,90,0)
	vclass->restore_window_signal =
#endif
		g_signal_new( I_("restore-window") ,
			G_OBJECT_CLASS_TYPE(vclass) ,
			G_SIGNAL_RUN_LAST ,
			G_STRUCT_OFFSET(VteTerminalClass , restore_window) ,
			NULL , NULL , g_cclosure_marshal_VOID__VOID ,
			G_TYPE_NONE , 0) ;

#if  ! GTK_CHECK_VERSION(2,90,0)
	vclass->maximize_window_signal =
#endif
		g_signal_new( I_("maximize-window") ,
			G_OBJECT_CLASS_TYPE(vclass) ,
			G_SIGNAL_RUN_LAST ,
			G_STRUCT_OFFSET(VteTerminalClass , maximize_window) ,
			NULL , NULL , g_cclosure_marshal_VOID__VOID ,
			G_TYPE_NONE , 0) ;

#if  ! GTK_CHECK_VERSION(2,90,0)
	vclass->resize_window_signal =
#endif
		g_signal_new( I_("resize-window") ,
			G_OBJECT_CLASS_TYPE(vclass) ,
			G_SIGNAL_RUN_LAST ,
			G_STRUCT_OFFSET(VteTerminalClass , resize_window) ,
			NULL , NULL , _vte_marshal_VOID__UINT_UINT ,
			G_TYPE_NONE , 2 , G_TYPE_UINT , G_TYPE_UINT) ;

#if  ! GTK_CHECK_VERSION(2,90,0)
	vclass->move_window_signal =
#endif
		g_signal_new( I_("move-window") ,
			G_OBJECT_CLASS_TYPE(vclass) ,
			G_SIGNAL_RUN_LAST ,
			G_STRUCT_OFFSET(VteTerminalClass , move_window) ,
			NULL , NULL , _vte_marshal_VOID__UINT_UINT ,
			G_TYPE_NONE , 2 , G_TYPE_UINT , G_TYPE_UINT) ;

#if  ! GTK_CHECK_VERSION(2,90,0)
	vclass->status_line_changed_signal =
#endif
		g_signal_new( I_("status-line-changed") ,
			G_OBJECT_CLASS_TYPE(vclass) ,
			G_SIGNAL_RUN_LAST ,
			G_STRUCT_OFFSET(VteTerminalClass , status_line_changed) ,
			NULL , NULL , g_cclosure_marshal_VOID__VOID ,
			G_TYPE_NONE , 0) ;

#if  ! GTK_CHECK_VERSION(2,90,0)
	vclass->increase_font_size_signal =
#endif
		g_signal_new( I_("increase-font-size") ,
			G_OBJECT_CLASS_TYPE(vclass) ,
			G_SIGNAL_RUN_LAST ,
			G_STRUCT_OFFSET(VteTerminalClass , increase_font_size) ,
			NULL , NULL , g_cclosure_marshal_VOID__VOID ,
			G_TYPE_NONE , 0) ;

#if  ! GTK_CHECK_VERSION(2,90,0)
	vclass->decrease_font_size_signal =
#endif
		g_signal_new( I_("decrease-font-size") ,
			G_OBJECT_CLASS_TYPE(vclass) ,
			G_SIGNAL_RUN_LAST ,
			G_STRUCT_OFFSET(VteTerminalClass , decrease_font_size) ,
			NULL , NULL , g_cclosure_marshal_VOID__VOID ,
			G_TYPE_NONE , 0) ;

#if  ! GTK_CHECK_VERSION(2,90,0)
	vclass->text_modified_signal =
#endif
		g_signal_new( I_("text-modified") ,
			G_OBJECT_CLASS_TYPE(vclass) ,
			G_SIGNAL_RUN_LAST ,
			G_STRUCT_OFFSET(VteTerminalClass , text_modified) ,
			NULL , NULL , g_cclosure_marshal_VOID__VOID ,
			G_TYPE_NONE , 0) ;

#if  ! GTK_CHECK_VERSION(2,90,0)
	vclass->text_inserted_signal =
#endif
		g_signal_new( I_("text-inserted") ,
			G_OBJECT_CLASS_TYPE(vclass) ,
			G_SIGNAL_RUN_LAST ,
			G_STRUCT_OFFSET(VteTerminalClass , text_inserted) ,
			NULL , NULL , g_cclosure_marshal_VOID__VOID ,
			G_TYPE_NONE , 0) ;

#if  ! GTK_CHECK_VERSION(2,90,0)
	vclass->text_deleted_signal =
#endif
		g_signal_new( I_("text-deleted") ,
			G_OBJECT_CLASS_TYPE(vclass) ,
			G_SIGNAL_RUN_LAST ,
			G_STRUCT_OFFSET(VteTerminalClass , text_deleted) ,
			NULL , NULL , g_cclosure_marshal_VOID__VOID ,
			G_TYPE_NONE , 0) ;

#if  ! GTK_CHECK_VERSION(2,90,0)
	vclass->text_scrolled_signal =
#endif
		g_signal_new( I_("text-scrolled") ,
			G_OBJECT_CLASS_TYPE(vclass) ,
			G_SIGNAL_RUN_LAST ,
			G_STRUCT_OFFSET(VteTerminalClass , text_scrolled) ,
			NULL , NULL , g_cclosure_marshal_VOID__INT ,
			G_TYPE_NONE , 1 , G_TYPE_INT) ;


#if  VTE_CHECK_VERSION(0,20,0)
	g_object_class_install_property( oclass , PROP_WINDOW_TITLE ,
		g_param_spec_string( "window-title" , NULL , NULL , NULL ,
			G_PARAM_READABLE | STATIC_PARAMS)) ;

	g_object_class_install_property( oclass , PROP_ICON_TITLE ,
		g_param_spec_string( "icon-title" , NULL , NULL , NULL ,
			G_PARAM_READABLE | STATIC_PARAMS)) ;
#endif

#if  VTE_CHECK_VERSION(0,23,2)
	/*
	 * doc/references/html/VteTerminal.html describes that inner-border property
	 * is since 0.24.0, but actually it is added at Nov 30 2009 (between 0.23.1 and 0.23.2)
	 * in ChangeLog.
	 */
	gtk_widget_class_install_style_property( wclass ,
		g_param_spec_boxed( "inner-border" , NULL , NULL , GTK_TYPE_BORDER ,
			G_PARAM_READABLE | G_PARAM_STATIC_STRINGS)) ;

#if  GTK_CHECK_VERSION(2,90,0)
	vclass->priv = G_TYPE_CLASS_GET_PRIVATE(vclass , VTE_TYPE_TERMINAL ,
				VteTerminalClassPrivate) ;
	vclass->priv->style_provider = GTK_STYLE_PROVIDER(gtk_css_provider_new()) ;
	gtk_css_provider_load_from_data( GTK_CSS_PROVIDER(vclass->priv->style_provider) ,
		"VteTerminal {\n"
		"-VteTerminal-inner-border: " KIK_INT_TO_STR(WINDOW_MARGIN) ";\n"
		"}\n" ,
		-1 , NULL) ;
#else
	gtk_rc_parse_string( "style \"vte-default-style\" {\n"
			"VteTerminal::inner-border = { "
			KIK_INT_TO_STR(WINDOW_MARGIN) " , "
			KIK_INT_TO_STR(WINDOW_MARGIN) " , "
			KIK_INT_TO_STR(WINDOW_MARGIN) " , "
			KIK_INT_TO_STR(WINDOW_MARGIN) " }\n"
			"}\n"
			"class \"VteTerminal\" style : gtk \"vte-default-style\"\n") ;
#endif
#endif	/* VTE_CHECK_VERSION(0,23,2) */

#if  VTE_CHECK_VERSION(0,19,0)
	signals[COPY_CLIPBOARD] =
		g_signal_new( I_("copy-clipboard") ,
			G_OBJECT_CLASS_TYPE(vclass) ,
			G_SIGNAL_RUN_LAST | G_SIGNAL_ACTION ,
			G_STRUCT_OFFSET(VteTerminalClass , copy_clipboard) ,
			NULL , NULL , g_cclosure_marshal_VOID__VOID ,
			G_TYPE_NONE , 0) ;

	signals[PASTE_CLIPBOARD] =
		g_signal_new( I_("paste-clipboard") ,
			G_OBJECT_CLASS_TYPE(vclass) ,
			G_SIGNAL_RUN_LAST | G_SIGNAL_ACTION ,
			G_STRUCT_OFFSET(VteTerminalClass , paste_clipboard) ,
			NULL , NULL , g_cclosure_marshal_VOID__VOID ,
			G_TYPE_NONE , 0) ;
#endif
}

static void
vte_terminal_init(
	VteTerminal *  terminal
	)
{
	static int  init_inherit_ptys ;
	mkf_charset_t  usascii_font_cs ;
	gdouble  dpi ;

	GTK_WIDGET_SET_CAN_FOCUS( GTK_WIDGET(terminal)) ;

	terminal->pvt = G_TYPE_INSTANCE_GET_PRIVATE( terminal , VTE_TYPE_TERMINAL ,
				VteTerminalPrivate) ;

#if  GTK_CHECK_VERSION(2,18,0)
	gtk_widget_set_has_window( GTK_WIDGET(terminal) , TRUE) ;
#endif

	/* We do our own redrawing. */
	gtk_widget_set_redraw_on_allocate( GTK_WIDGET(terminal) , FALSE) ;

	terminal->adjustment = NULL ;
	set_adjustment( terminal , GTK_ADJUSTMENT(gtk_adjustment_new( 0 , 0 , main_config.rows ,
					1 , main_config.rows , main_config.rows))) ;

	g_signal_connect( terminal , "hierarchy-changed" ,
		G_CALLBACK(vte_terminal_hierarchy_changed) , NULL) ;

#if  GTK_CHECK_VERSION(2,90,0)
	{
		GtkStyleContext *  context ;

		context = gtk_widget_get_style_context( GTK_WIDGET(terminal)) ;
		gtk_style_context_add_provider( context ,
			VTE_TERMINAL_GET_CLASS(terminal)->priv->style_provider ,
			GTK_STYLE_PROVIDER_PRIORITY_APPLICATION) ;
	}
#endif

	terminal->pvt->term =
		ml_create_term( main_config.cols , main_config.rows ,
			main_config.tab_size , main_config.num_of_log_lines ,
			main_config.encoding , main_config.is_auto_encoding , 
			main_config.unicode_policy , main_config.col_size_of_width_a ,
			main_config.use_char_combining , main_config.use_multi_col_char ,
			main_config.use_bidi , main_config.bidi_mode , main_config.use_ind ,
			x_termcap_get_bool_field(
				x_termcap_get_entry( &termcap , main_config.term_type) , ML_BCE) ,
			main_config.use_dynamic_comb , main_config.bs_mode ,
			main_config.vertical_mode , main_config.use_local_echo ,
			main_config.title , main_config.icon_name) ;
	if( ! init_inherit_ptys)
	{
		u_int  num ;
		ml_term_t **  terms ;
		u_int  count ;

		num = ml_get_all_terms( &terms) ;
		for( count = 0 ; count < num ; count++)
		{
			if( terms[count] != terminal->pvt->term)
			{
				vte_reaper_add_child( ml_term_get_child_pid( terms[count])) ;
			}
		}

		init_inherit_ptys = 1 ;
	}

	if( main_config.logging_vt_seq)
	{
		ml_term_set_logging_vt_seq( terminal->pvt->term , main_config.logging_vt_seq) ;
	}

#if  VTE_CHECK_VERSION(0,26,0)
	terminal->pvt->pty = NULL ;
#endif

	if( main_config.unicode_policy & NOT_USE_UNICODE_FONT ||
		main_config.iso88591_font_for_usascii)
	{
		usascii_font_cs = x_get_usascii_font_cs( ML_ISO8859_1) ;
	}
	else if( main_config.unicode_policy & ONLY_USE_UNICODE_FONT)
	{
		usascii_font_cs = x_get_usascii_font_cs( ML_UTF8) ;
	}
	else
	{
		usascii_font_cs = x_get_usascii_font_cs(
					ml_term_get_encoding(terminal->pvt->term)) ;
	}

	/* related to x_font_use_point_size_for_fc(1) in vte_terminal_class_init. */
	if( ( dpi = gdk_screen_get_resolution(
			gtk_widget_get_screen( GTK_WIDGET(terminal)))) != -1)
	{
	#ifdef  DEBUG
		kik_debug_printf( KIK_DEBUG_TAG " Setting dpi %f\n" , dpi) ;
	#endif

		x_font_set_dpi_for_fc( dpi) ;
	}

	init_screen( terminal ,
		x_font_manager_new( disp.display ,
			main_config.type_engine ,
			main_config.font_present ,
			main_config.font_size , usascii_font_cs ,
			main_config.use_multi_col_char ,
			main_config.step_in_changing_font_size ,
			main_config.letter_space , main_config.use_bold_font) ,
		x_color_manager_new( &disp ,
			main_config.fg_color , main_config.bg_color ,
			main_config.cursor_fg_color , main_config.cursor_bg_color ,
			main_config.bd_color , main_config.ul_color)) ;

	terminal->pvt->io = NULL ;
	terminal->pvt->src_id = 0 ;

	terminal->pvt->image = NULL ;
	terminal->pvt->pixmap = None ;
	terminal->pvt->pix_width = 0 ;
	terminal->pvt->pix_height = 0 ;
	terminal->pvt->pic_mod = NULL ;

/* GRegex was not supported */
#if  GLIB_CHECK_VERSION(2,14,0)
	terminal->pvt->regex = NULL ;
#endif

	terminal->window_title = ml_term_window_name( terminal->pvt->term) ;
	terminal->icon_title = ml_term_icon_name( terminal->pvt->term) ;

#if  ! GTK_CHECK_VERSION(2,90,0)
	/* XXX */
	if( strstr( g_get_prgname() , "roxterm"))
	{
		/*
		 * XXX
		 * I don't know why, but gtk_widget_ensure_style() doesn't apply "inner-border"
		 * and min width/height of roxterm are not correctly set.
		 */
		gtk_widget_set_rc_style( &terminal->widget) ;
	}
	else
#endif
	{
		/*
		 * gnome-terminal(2.32.1) fails to set "inner-border" and
		 * min width/height without this.
		 */
		gtk_widget_ensure_style( &terminal->widget) ;
	}

	reset_vte_size_member( terminal) ;
}


static int
ml_term_open_pty_wrap(
	VteTerminal *  terminal ,
	const char *  cmd_path ,
	char **  argv ,
	char **  envv ,
	const char *  pass ,
	const char *  pubkey ,
	const char *  privkey
	)
{
	const char *  host ;
	char **  env_p ;
	u_int  num_of_env ;

	host = gdk_display_get_name( gtk_widget_get_display( GTK_WIDGET(terminal))) ;

	num_of_env = 0 ;
	if( envv)
	{
		env_p = envv ;
		while( *(env_p ++))
		{
			num_of_env ++ ;
		}
	}

	if( ( env_p = alloca( sizeof( char*) * (num_of_env + 5))))
	{
		if( num_of_env > 0)
		{
			envv = memcpy( env_p , envv , sizeof(char*) * num_of_env) ;
			env_p += num_of_env ;
		}
		else
		{
			envv = env_p ;
		}

		/* "WINDOWID="(9) + [32bit digit] + NULL(1) */
		if( GTK_WIDGET_REALIZED(GTK_WIDGET(terminal)) &&
		    ( *env_p = alloca( 9 + DIGIT_STR_LEN(Window) + 1)))
		{
			sprintf( *(env_p ++) , "WINDOWID=%ld" ,
			#if  1
				gdk_x11_drawable_get_xid(
					gtk_widget_get_window( GTK_WIDGET(terminal)))
			#else
					terminal->pvt->screen->window.my_window
			#endif
					) ;
		}

		/* "DISPLAY="(8) + NULL(1) */
		if( ( *env_p = alloca( 8 + strlen( host) + 1)))
		{
			sprintf( *(env_p ++) , "DISPLAY=%s" , host) ;
		}

		if( ( *env_p = alloca( 5 + strlen( main_config.term_type) + 1)))
		{
			sprintf( *(env_p ++) , "TERM=%s" , main_config.term_type) ;
		}

		*(env_p ++) = "COLORFGBG=default;default" ;

		/* NULL terminator */
		*env_p = NULL ;
	}

#if  0
	env_p = envv ;
	while( *env_p)
	{
		kik_debug_printf( "%s\n" , *(env_p ++)) ;
	}
#endif

	return  ml_term_open_pty( terminal->pvt->term , cmd_path , argv , envv ,
				host , pass , pubkey , privkey) ;
}


/* --- global functions --- */

GtkWidget*
vte_terminal_new()
{
	return  g_object_new( VTE_TYPE_TERMINAL , NULL) ;
}

/*
 * vte_terminal_fork_command or vte_terminal_forkpty functions are possible
 * to call before VteTerminal widget is realized.
 */

pid_t
vte_terminal_fork_command(
	VteTerminal *  terminal ,
	const char *  command ,		/* If NULL, open default shell. */
	char **  argv ,			/* If NULL, open default shell. */
	char **  envv ,
	const char *  directory ,
	gboolean  lastlog ,
	gboolean  utmp ,
	gboolean  wtmp
	)
{
	/*
	 * If pty is inherited from dead parent, terminal->pvt->term->pty is non-NULL
	 * but create_io() and vte_reaper_add_child() aren't executed.
	 * So terminal->pvt->io is used to check if pty is completely set up.
	 */
	if( ! terminal->pvt->io)
	{
	#ifdef  DEBUG
		kik_debug_printf( KIK_DEBUG_TAG " forking with %s\n" , command) ;
	#endif

		if( ! command)
		{
			if( ! ( command = getenv( "SHELL")) || *command == '\0')
			{
				struct passwd *  pw ;

				if( ( pw = getpwuid( getuid())) == NULL ||
				    *( command = pw->pw_shell) == '\0')
				{
					command = "/bin/sh" ;
				}
			}
		}

		if( ! argv || ! argv[0])
		{
			argv = alloca( sizeof(char*) * 2) ;
			argv[0] = command ;
			argv[1] = NULL ;
		}

		kik_pty_helper_set_flag( lastlog , utmp , wtmp) ;

		if( ! ml_term_open_pty_wrap( terminal , command , argv , envv ,
				NULL , NULL , NULL) )
		{
		#ifdef  DEBUG
			kik_debug_printf( KIK_DEBUG_TAG " fork failed\n") ;
		#endif
		
			return  -1 ;
		}

		create_io( terminal) ;

		vte_reaper_add_child( ml_term_get_child_pid( terminal->pvt->term)) ;

		if( GTK_WIDGET_REALIZED(GTK_WIDGET(terminal)))
		{
			GtkAllocation  allocation ;

			gtk_widget_get_allocation( GTK_WIDGET(terminal) , &allocation) ;

			if( ! is_initial_allocation( &allocation) &&
			    x_window_resize_with_margin( &terminal->pvt->screen->window ,
				allocation.width , allocation.height , NOTIFY_TO_MYSELF))
			{
				reset_vte_size_member( terminal) ;
				update_wall_picture( terminal) ;
			}
		}

		/*
		 * In order to receive pty_closed() event even if vte_terminal_realize()
		 * isn't called.
		 */
		ml_pty_set_listener( terminal->pvt->term->pty ,
			&terminal->pvt->screen->pty_listener) ;
	}
	
	return  ml_term_get_child_pid( terminal->pvt->term) ;
}

#if  VTE_CHECK_VERSION(0,26,0)
gboolean
vte_terminal_fork_command_full(
	VteTerminal *  terminal ,
	VtePtyFlags  pty_flags ,
	const char *  working_directory ,
	char **  argv ,
	char **  envv ,
	GSpawnFlags  spawn_flags ,
	GSpawnChildSetupFunc  child_setup ,
	gpointer  child_setup_data ,
	GPid *  child_pid /* out */ ,
	GError **  error
	)
{
	GPid  pid ;

	pid = vte_terminal_fork_command( terminal , argv[0] , argv + 1 , envv ,
				working_directory ,
				(pty_flags & VTE_PTY_NO_LASTLOG) ? FALSE : TRUE ,
				(pty_flags & VTE_PTY_NO_UTMP) ? FALSE : TRUE ,
				(pty_flags & VTE_PTY_NO_WTMP) ? FALSE : TRUE) ;
	if( child_pid)
	{
		*child_pid = pid ;
	}

	if( pid > 0)
	{
		return  TRUE ;
	}
	else
	{
		return  FALSE ;
	}
}
#endif

pid_t
vte_terminal_forkpty(
	VteTerminal *  terminal ,
	char **  envv ,
	const char *  directory ,
	gboolean  lastlog ,
	gboolean  utmp ,
	gboolean  wtmp
	)
{
	/*
	 * If pty is inherited from dead parent, terminal->pvt->term->pty is non-NULL
	 * but create_io() and vte_reaper_add_child() aren't executed.
	 * So terminal->pvt->io is used to check if pty is completely set up.
	 */
	if( ! terminal->pvt->io)
	{
	#ifdef  DEBUG
		kik_debug_printf( KIK_DEBUG_TAG " forking pty\n") ;
	#endif
	
		kik_pty_helper_set_flag( lastlog , utmp , wtmp) ;
		
		if( ! ml_term_open_pty_wrap( terminal , NULL , NULL , envv , NULL , NULL , NULL))
		{
		#ifdef  DEBUG
			kik_debug_printf( KIK_DEBUG_TAG " fork failed\n") ;
		#endif
		
			return  -1 ;
		}

		if( ml_term_get_child_pid( terminal->pvt->term) == 0)
		{
			/* Child process */
			
			return  0 ;
		}

		create_io( terminal) ;

		vte_reaper_add_child( ml_term_get_child_pid( terminal->pvt->term)) ;
		
		if( GTK_WIDGET_REALIZED(GTK_WIDGET(terminal)))
		{
			GtkAllocation  allocation ;

			gtk_widget_get_allocation( GTK_WIDGET(terminal) , &allocation) ;

			if( ! is_initial_allocation( &allocation) &&
			    x_window_resize_with_margin( &terminal->pvt->screen->window ,
				allocation.width , allocation.height , NOTIFY_TO_MYSELF))
			{
				reset_vte_size_member( terminal) ;
				update_wall_picture( terminal) ;
			}
		}

		/*
		 * In order to receive pty_closed() event even if vte_terminal_realize()
		 * isn't called.
		 */
		ml_pty_set_listener( terminal->pvt->term->pty ,
			&terminal->pvt->screen->pty_listener) ;
	}

	return  ml_term_get_child_pid( terminal->pvt->term) ;
}

void
vte_terminal_feed(
	VteTerminal *  terminal ,
	const char *  data ,
	glong  length
	)
{
}

void
vte_terminal_feed_child(
	VteTerminal *  terminal ,
	const char *  text ,
	glong  length
	)
{
}

void
vte_terminal_feed_child_binary(
	VteTerminal *  terminal ,
	const char *  data ,
	glong  length
	)
{
}

void
vte_terminal_copy_clipboard(
	VteTerminal *  terminal
	)
{
	GtkClipboard *  clipboard ;
	u_char *  buf ;
	size_t  len ;

	if( ! vte_terminal_get_has_selection( terminal) ||
		! (clipboard = gtk_clipboard_get( GDK_SELECTION_CLIPBOARD)))
	{
		return ;
	}

	len = terminal->pvt->screen->sel.sel_len * MLCHAR_UTF_MAX_SIZE ;

	/*
	 * Don't use alloca() here because len can be too big value.
	 * (MLCHAR_UTF_MAX_SIZE defined in ml_char.h is 48 byte.)
	 */
	if( ! ( buf = malloc( len)))
	{
		return ;
	}
	
	(*terminal->pvt->screen->ml_str_parser->init)( terminal->pvt->screen->ml_str_parser) ;
	ml_str_parser_set_str( terminal->pvt->screen->ml_str_parser ,
		terminal->pvt->screen->sel.sel_str , terminal->pvt->screen->sel.sel_len) ;
	(*terminal->pvt->screen->utf_conv->init)( terminal->pvt->screen->utf_conv) ;

	len = (*terminal->pvt->screen->utf_conv->convert)( terminal->pvt->screen->utf_conv ,
						buf , len , terminal->pvt->screen->ml_str_parser) ;
	
	gtk_clipboard_set_text( clipboard , buf , len) ;
	gtk_clipboard_store( clipboard) ;

	free( buf) ;
}

void
vte_terminal_paste_clipboard(
	VteTerminal *  terminal
	)
{
	x_screen_exec_cmd( terminal->pvt->screen , "paste") ;
}

void
vte_terminal_copy_primary(
	VteTerminal *  terminal
	)
{
}

void
vte_terminal_paste_primary(
	VteTerminal *  terminal
	)
{
	vte_terminal_paste_clipboard( terminal) ;
}

void
vte_terminal_select_all(
	VteTerminal *  terminal
	)
{
	int  beg_row ;
	int  end_row ;
	ml_line_t *  line ;
	
	if( ! GTK_WIDGET_REALIZED(GTK_WIDGET(terminal)))
	{
		return ;
	}
	
	beg_row = - ml_term_get_num_of_logged_lines( terminal->pvt->term) ;

	for( end_row = ml_term_get_rows( terminal->pvt->term) - 1 ; end_row >= 0 ; end_row --)
	{
		if( (line = ml_term_get_line( terminal->pvt->term , end_row)) &&
		    ! ml_line_is_empty( line))
		{
			break ;
		}
	}

	selection( &terminal->pvt->screen->sel , 0 , beg_row ,
			line->num_of_filled_chars - 1 , end_row) ;

	x_window_update( &terminal->pvt->screen->window , 1 /* UPDATE_SCREEN */) ;
}

void
vte_terminal_select_none(
	VteTerminal *  terminal
	)
{
	if( ! GTK_WIDGET_REALIZED(GTK_WIDGET(terminal)))
	{
		return ;
	}
	
	x_sel_clear( &terminal->pvt->screen->sel) ;

	x_window_update( &terminal->pvt->screen->window ,
			3 /* UPDATE_SCREEN|UPDATE_CURSOR */) ;
}

void
vte_terminal_set_size(
	VteTerminal *  terminal,
	glong  columns ,
	glong  rows
	)
{
#ifdef  DEBUG
	kik_debug_printf( KIK_DEBUG_TAG " set cols %d rows %d\n" , columns , rows) ;
#endif

	ml_term_resize( terminal->pvt->term , columns , rows) ;
	reset_vte_size_member( terminal) ;

	/* gnome-terminal(2.29.6 or later ?) is not resized correctly without this. */
	if( GTK_WIDGET_REALIZED(GTK_WIDGET(terminal)))
	{
		gtk_widget_queue_resize_no_redraw( GTK_WIDGET(terminal)) ;
	}
}

void
vte_terminal_set_audible_bell(
	VteTerminal *  terminal ,
	gboolean is_audible
	)
{
	x_screen_set_config( terminal->pvt->screen , NULL , "bel_mode" ,
		x_get_bel_mode_name( is_audible ?
			(BEL_SOUND | terminal->pvt->screen->bel_mode) :
			(~BEL_SOUND & terminal->pvt->screen->bel_mode))) ;
}

gboolean
vte_terminal_get_audible_bell(
	VteTerminal *  terminal
	)
{
	if( terminal->pvt->screen->bel_mode & BEL_SOUND)
	{
		return  TRUE ;
	}
	else
	{
		return  FALSE ;
	}
}

void
vte_terminal_set_visible_bell(
	VteTerminal *  terminal ,
	gboolean  is_visible
	)
{
	x_screen_set_config( terminal->pvt->screen , NULL , "bel_mode" ,
		x_get_bel_mode_name( is_visible ?
			(BEL_VISUAL | terminal->pvt->screen->bel_mode) :
			(~BEL_VISUAL & terminal->pvt->screen->bel_mode))) ;
}

gboolean
vte_terminal_get_visible_bell(
	VteTerminal *  terminal
	)
{
	if( terminal->pvt->screen->bel_mode & BEL_VISUAL)
	{
		return  TRUE ;
	}
	else
	{
		return  FALSE ;
	}
}

void
vte_terminal_set_scroll_background(
	VteTerminal *  terminal ,
	gboolean  scroll
	)
{
}

void
vte_terminal_set_scroll_on_output(
	VteTerminal *  terminal ,
	gboolean  scroll
	)
{
}

void
vte_terminal_set_scroll_on_keystroke(
	VteTerminal *  terminal ,
	gboolean  scroll
	)
{
}

void
vte_terminal_set_color_dim(
	VteTerminal *  terminal ,
	const GdkColor *  dim
	)
{
}

void
vte_terminal_set_color_bold(
	VteTerminal *  terminal ,
	const GdkColor *  bold
	)
{
}

void
vte_terminal_set_color_foreground(
	VteTerminal *  terminal ,
	const GdkColor *  foreground
	)
{
	gchar *  str ;

	if( ! foreground)
	{
		return ;
	}

	/* #rrrrggggbbbb */
	str = gdk_color_to_string( foreground) ;
	
#ifdef  DEBUG
	kik_debug_printf( KIK_DEBUG_TAG " set_color_foreground %s\n" , str) ;
#endif

	if( GTK_WIDGET_REALIZED(GTK_WIDGET(terminal)))
	{
		x_screen_set_config( terminal->pvt->screen , NULL , "fg_color" , str) ;
		x_window_update( &terminal->pvt->screen->window ,
			3 /* UPDATE_SCREEN|UPDATE_CURSOR */) ;
	}
	else
	{
		x_color_manager_set_fg_color( terminal->pvt->screen->color_man , str) ;
	}

	g_free( str) ;
}

void
vte_terminal_set_color_background(
	VteTerminal *  terminal ,
	const GdkColor *  background
	)
{
	gchar *  str ;

	if( ! background)
	{
		return ;
	}
	
	/* #rrrrggggbbbb */
	str = gdk_color_to_string( background) ;

#ifdef  DEBUG
	kik_debug_printf( KIK_DEBUG_TAG " set_color_background %s\n" , str) ;
#endif

	if( GTK_WIDGET_REALIZED(GTK_WIDGET(terminal)))
	{
		x_screen_set_config( terminal->pvt->screen , NULL , "bg_color" , str) ;
		x_window_update( &terminal->pvt->screen->window ,
			3 /* UPDATE_SCREEN|UPDATE_CURSOR */) ;

		if( terminal->pvt->image && terminal->pvt->screen->pic_mod.alpha < 255)
		{
			update_wall_picture( terminal) ;
		}
	}
	else
	{
		x_color_manager_set_bg_color( terminal->pvt->screen->color_man , str) ;
	}
	
	g_free( str) ;
}

void
vte_terminal_set_color_cursor(
	VteTerminal *  terminal ,
	const GdkColor *  cursor_background
	)
{
	gchar *  str ;

	if( ! cursor_background)
	{
		return ;
	}

	/* #rrrrggggbbbb */
	str = gdk_color_to_string( cursor_background) ;

#ifdef  DEBUG
	kik_debug_printf( KIK_DEBUG_TAG " set_color_cursor %s\n" , str) ;
#endif

	if( GTK_WIDGET_REALIZED(GTK_WIDGET(terminal)))
	{
		x_screen_set_config( terminal->pvt->screen , NULL , "cursor_bg_color" , str) ;
		x_window_update( &terminal->pvt->screen->window ,
			3 /* UPDATE_SCREEN|UPDATE_CURSOR */) ;
	}
	else
	{
		x_color_manager_set_cursor_bg_color( terminal->pvt->screen->color_man , str) ;
	}

	g_free( str) ;
}

void
vte_terminal_set_color_highlight(
	VteTerminal *  terminal ,
	const GdkColor *  highlight_background
	)
{
}

void
vte_terminal_set_colors(
	VteTerminal *  terminal ,
	const GdkColor *  foreground ,
	const GdkColor *  background ,
	const GdkColor *  palette ,
	glong palette_size
	)
{
	if( palette_size != 0 && palette_size != 8 && palette_size != 16 &&
	    ( palette_size < 24 || 256 < palette_size))
	{
	#ifdef  DEBUG
		kik_debug_printf( KIK_DEBUG_TAG " palette_size %d is illegal\n" , palette_size) ;
	#endif

		return ;
	}

	if( palette_size >= 8)
	{
		ml_color_t  color ;
		int  need_redraw = 0 ;

		if( foreground == NULL)
		{
			foreground = &palette[7] ;
		}
		if( background == NULL)
		{
			background = &palette[0] ;
		}

		for( color = 0 ; color < palette_size ; color++)
		{
			gchar *  rgb ;
			char *  name ;

			rgb = gdk_color_to_string( palette + color) ;
			name = ml_get_color_name( color) ;

		#ifdef  DEBUG
			kik_debug_printf( KIK_DEBUG_TAG " Setting rgb %s=%s\n" , name , rgb) ;
		#endif

			need_redraw |= ml_customize_color_file( name , rgb , 0) ;

			g_free( rgb) ;
		}

		if( need_redraw && GTK_WIDGET_REALIZED(GTK_WIDGET(terminal)))
		{
			x_color_cache_unload_all() ;
			x_screen_reset_view( terminal->pvt->screen) ;
		}
	}

	vte_terminal_set_color_foreground( terminal , foreground) ;
	vte_terminal_set_color_background( terminal , background) ;
}

void
vte_terminal_set_default_colors(
	VteTerminal *  terminal
	)
{
}

void
vte_terminal_set_background_image(
	VteTerminal *  terminal ,
	GdkPixbuf *  image		/* can be NULL and same as current terminal->pvt->image */
	)
{
	if( terminal->pvt->image == image)
	{
		return ;
	}
	
	if( terminal->pvt->image)
	{
		g_object_unref( terminal->pvt->image) ;
	}
	
	if( ( terminal->pvt->image = image) == NULL)
	{
		vte_terminal_set_background_image_file( terminal , "") ;

		return ;
	}

	g_object_ref( image) ;

	if( GTK_WIDGET_REALIZED(GTK_WIDGET(terminal)))
	{
		update_wall_picture( terminal) ;
	}
}

void
vte_terminal_set_background_image_file(
	VteTerminal *  terminal ,
	const char *  path
	)
{
#ifdef  DEBUG
	kik_debug_printf( KIK_DEBUG_TAG " Setting image file %s\n" , path) ;
#endif

	/*
	 * Don't unref terminal->pvt->image if path is
	 * "pixmap:<pixmap id>" (Ex. the case of vte_terminal_set_background_image_file()
	 * being called from update_wall_picture().)
	 */
	if( terminal->pvt->image && strncmp( path , "pixmap:" , 7) != 0)
	{
		g_object_unref( terminal->pvt->image) ;
		terminal->pvt->image = NULL ;
	}

	if( GTK_WIDGET_REALIZED(GTK_WIDGET(terminal)))
	{
		x_screen_set_config( terminal->pvt->screen , NULL , "wall_picture" , path) ;
	}
	else
	{
		free( terminal->pvt->screen->pic_file_path) ;
		terminal->pvt->screen->pic_file_path = strdup( path) ;
	}
}

void
vte_terminal_set_background_tint_color(
	VteTerminal *  terminal ,
	const GdkColor *  color
	)
{
}

void
vte_terminal_set_background_saturation(
	VteTerminal *  terminal ,
	double  saturation
	)
{
#ifdef  DEBUG
	kik_debug_printf( KIK_DEBUG_TAG " SATURATION => %f\n" , saturation) ;
#endif

#if  1
	/* XXX old roxterm (1.18.5) always sets opacity 0xffff after it changes saturation. */
	if( strstr( g_get_prgname() , "roxterm"))
	{
		/* "Use solid color" => saturation == 1.0 (roxterm 1.22.2) */
		if( saturation == 1.0)
		{
			vte_terminal_set_opacity( terminal , 0xfffe) ;

			return ;
		}
	}
#endif

	vte_terminal_set_opacity( terminal , 0xffff * (1.0 - saturation)) ;
}

void
vte_terminal_set_background_transparent(
	VteTerminal *  terminal ,
	gboolean  transparent
	)
{
#ifdef  DEBUG
	kik_debug_printf( KIK_DEBUG_TAG " Pseudo transparent %s.\n" ,
		transparent ? "on" : "off") ;
#endif

	if( GTK_WIDGET_REALIZED(GTK_WIDGET(terminal)))
	{
		char *  value ;

		if( transparent)
		{
			value = "true" ;
		}
		else
		{
			value = "false" ;
		}

		x_screen_set_config( terminal->pvt->screen , NULL , "use_transbg" , value) ;
	}
	else if( transparent)
	{
		x_window_set_transparent( &terminal->pvt->screen->window ,
			x_screen_get_picture_modifier( terminal->pvt->screen)) ;
	}
}

void
vte_terminal_set_opacity(
	VteTerminal *  terminal ,
	guint16  opacity
	)
{
	u_int8_t  alpha ;

#ifdef  DEBUG
	kik_debug_printf( KIK_DEBUG_TAG " OPACITY => %x\n" , opacity) ;
#endif

#if  1
	/* XXX old roxterm (1.18.5) always sets opacity 0xffff after it changes saturation. */
	if( strstr( g_get_prgname() , "roxterm"))
	{
		if( opacity == 0xffff)
		{
			return ;
		}
		else if( opacity == 0xfffe)
		{
			/* 0xfffe seemed to be set in vte_terminal_set_background_saturation(). */
			opacity = 0xffff ;
		}
	}
#endif

	alpha = ((opacity >> 8) & 0xff) ;

	if( GTK_WIDGET_REALIZED(GTK_WIDGET(terminal)))
	{
		char  value[DIGIT_STR_LEN(u_int8_t) + 1] ;

		sprintf( value , "%d" , (int)alpha) ;

		x_screen_set_config( terminal->pvt->screen , NULL , "alpha" , value) ;
		x_window_update( &terminal->pvt->screen->window ,
			3 /* UPDATE_SCREEN|UPDATE_CURSOR */) ;
		update_wall_picture( terminal) ;
	}
	else
	{
		terminal->pvt->screen->pic_mod.alpha = alpha ;
	}
}

#if  VTE_CHECK_VERSION(0,17,1)
void
vte_terminal_set_cursor_blink_mode(
	VteTerminal *  terminal ,
	VteTerminalCursorBlinkMode  mode
	)
{
	char *  value ;

	if( mode == VTE_CURSOR_BLINK_OFF)
	{
		value = "false" ;
	}
	else
	{
		value = "true" ;
	}

	x_screen_set_config( terminal->pvt->screen , NULL , "blink_cursor" , value) ;
}

VteTerminalCursorBlinkMode
vte_terminal_get_cursor_blink_mode(
	VteTerminal *  terminal
	)
{
	/*
	 * XXX
	 * Not work until x_display_idling is implemented.
	 */

	if( terminal->pvt->screen->window.idling)
	{
		return  VTE_CURSOR_BLINK_ON ;
	}
	else
	{
		return  VTE_CURSOR_BLINK_OFF ;
	}
}

void
vte_terminal_set_cursor_shape(
	VteTerminal *  terminal ,
	VteTerminalCursorShape  shape
	)
{
}

VteTerminalCursorShape
vte_terminal_get_cursor_shape(
	VteTerminal *  terminal
	)
{
	return  VTE_CURSOR_SHAPE_IBEAM ;
}
#endif

void
vte_terminal_set_scrollback_lines(
	VteTerminal *  terminal ,
	glong  lines
	)
{
}

void
vte_terminal_im_append_menuitems(
	VteTerminal *  terminal ,
	GtkMenuShell *  menushell
	)
{
}

void
vte_terminal_set_font(
	VteTerminal *  terminal ,
	const PangoFontDescription *  font_desc
	)
{
	char *  name ;

	name = pango_font_description_to_string( font_desc) ;

#ifdef  DEBUG
	kik_debug_printf( KIK_DEBUG_TAG " set_font %s\n" , name) ;
#endif

	vte_terminal_set_font_from_string( terminal , name) ;
	
	g_free( name) ;
}

void
vte_terminal_set_font_from_string(
	VteTerminal *  terminal ,
	const char *  name
	)
{
#ifdef  DEBUG
	kik_debug_printf( KIK_DEBUG_TAG " set_font_from_string %s\n" , name) ;
#endif

	if( ! name)
	{
		name = "monospace" ;
	}

	if( x_customize_font_file( "aafont" , "DEFAULT" , name , 0))
	{
		x_font_cache_unload_all() ;
	
		if( GTK_WIDGET_REALIZED(GTK_WIDGET(terminal)))
		{
			x_screen_reset_view( terminal->pvt->screen) ;
		}
		else
		{
			/*
			 * XXX
			 * Forcibly fix width and height members of x_window_t,
			 * or widget->requisition is not set correctly in
			 * reset_vte_size_member.
			 */
			terminal->pvt->screen->window.width =
				x_col_width( terminal->pvt->screen) *
				ml_term_get_cols( terminal->pvt->term) ;
			terminal->pvt->screen->window.height =
				x_line_height( terminal->pvt->screen) *
				ml_term_get_rows( terminal->pvt->term) ;
		}
		
		reset_vte_size_member( terminal) ;

		if( GTK_WIDGET_REALIZED(GTK_WIDGET(terminal)))
		{
			/*
			 * gnome-terminal(2.29.6 or later?) is not resized correctly
			 * without this.
			 */
			gtk_widget_queue_resize_no_redraw( GTK_WIDGET(terminal)) ;
		}
	}
}

const PangoFontDescription *
vte_terminal_get_font(
	VteTerminal *  terminal
	)
{
	return  NULL ;
}

void
vte_terminal_set_allow_bold(
	VteTerminal *  terminal ,
	gboolean  allow_bold
	)
{
}

gboolean
vte_terminal_get_allow_bold(
	VteTerminal *  terminal
	)
{
	return  FALSE ;
}

gboolean
vte_terminal_get_has_selection(
	VteTerminal *  terminal
	)
{
	if( terminal->pvt->screen->sel.sel_str && terminal->pvt->screen->sel.sel_len > 0)
	{
		return  TRUE ;
	}
	else
	{
		return  FALSE ;
	}
}

void
vte_terminal_set_word_chars(
	VteTerminal *  terminal ,
	const char *  spec
	)
{
	char *  sep ;

	if( ! spec || ! *spec)
	{
		sep = ",. " ;
	}
	else if( ( sep = alloca( 0x5f)))
	{
		char *  sep_p ;
		char  c ;

		sep_p = sep ;
		c = 0x20 ;

		do
		{
			const char *  spec_p ;

			spec_p = spec ;

			while( *spec_p)
			{
				if( *spec_p == '-' &&
				    spec_p > spec && *(spec_p + 1) != '\0')
				{
					if( *(spec_p - 1) < c && c < *(spec_p + 1))
					{
						goto  next ;
					}
				}
				else if( *spec_p == c)
				{
					goto  next ;
				}

				spec_p ++ ;
			}

			*(sep_p++) = c ;

		next:
			c ++ ;
		}
		while( c < 0x7f) ;

		*(sep_p) = '\0' ;
	}
	else
	{
		return ;
	}

	ml_set_word_separators( sep) ;
}

gboolean
vte_terminal_is_word_char(
	VteTerminal *  terminal ,
	gunichar  c
	)
{
	return  TRUE ;
}

void
vte_terminal_set_backspace_binding(
	VteTerminal *  terminal ,
	VteTerminalEraseBinding  binding
	)
{
	x_termcap_entry_t *  entry ;
	char *  seq ;

#ifdef  DEBUG
	kik_debug_printf( KIK_DEBUG_TAG " set backtrace binding => %d\n") ;
#endif

	if( binding == VTE_ERASE_ASCII_BACKSPACE)
	{
		seq = "\x08" ;
	}
	else if( binding == VTE_ERASE_ASCII_DELETE)
	{
		seq = "\x7f" ;
	}
	else if( binding == VTE_ERASE_DELETE_SEQUENCE)
	{
		seq = "\x1b[3~" ;
	}
#if  VTE_CHECK_VERSION(0,20,4)
	else if( binding == VTE_ERASE_TTY)
	{
		return ;
	}
#endif
	else
	{
		return ;
	}
	
	entry = x_termcap_get_entry( &termcap , main_config.term_type) ;
	free( entry->str_fields[ML_BACKSPACE]) ;
	/* ^H (compatible with libvte) */
	entry->str_fields[ML_BACKSPACE] = strdup(seq) ;
}

void
vte_terminal_set_delete_binding(
	VteTerminal *  terminal ,
	VteTerminalEraseBinding  binding
	)
{
	x_termcap_entry_t *  entry ;
	char *  seq ;

	if( binding == VTE_ERASE_ASCII_BACKSPACE)
	{
		seq = "\x08" ;
	}
	else if( binding == VTE_ERASE_ASCII_DELETE)
	{
		seq = "\x7f" ;
	}
	else if( binding == VTE_ERASE_DELETE_SEQUENCE)
	{
		seq = "\x1b[3~" ;
	}
#if  VTE_CHECK_VERSION(0,20,4)
	else if( binding == VTE_ERASE_TTY)
	{
		return ;
	}
#endif
	else
	{
		return ;
	}
	
	entry = x_termcap_get_entry( &termcap , main_config.term_type) ;
	free( entry->str_fields[ML_DELETE]) ;
	/* ^H (compatible with libvte) */
	entry->str_fields[ML_DELETE] = strdup(seq) ;
}

void
vte_terminal_set_mouse_autohide(
	VteTerminal *  terminal ,
	gboolean  setting
	)
{
}

gboolean
vte_terminal_get_mouse_autohide(
	VteTerminal *  terminal
	)
{
	return  FALSE ;
}

void
vte_terminal_reset(
	VteTerminal *  terminal ,
	gboolean  full ,
	gboolean  clear_history
	)
{
	if( GTK_WIDGET_REALIZED(GTK_WIDGET(terminal)))
	{
		x_screen_exec_cmd( terminal->pvt->screen , "full_reset") ;
	}
}

char *
vte_terminal_get_text(
	VteTerminal *  terminal ,
	gboolean (*is_selected)( VteTerminal *  terminal ,
		glong column , glong row , gpointer data) ,
	gpointer data ,
	GArray *  attributes
	)
{
	return  NULL ;
}

char *
vte_terminal_get_text_include_trailing_spaces(
	VteTerminal *  terminal ,
	gboolean (*is_selected)( VteTerminal *  terminal ,
		glong column , glong row , gpointer data) ,
	gpointer  data ,
	GArray *  attributes
	)
{
	return  NULL ;
}

char *
vte_terminal_get_text_range(
	VteTerminal *  terminal ,
	glong  start_row ,
	glong  start_col ,
	glong  end_row ,
	glong  end_col ,
	gboolean (*is_selected)( VteTerminal *  terminal ,
		glong column , glong row , gpointer data) ,
	gpointer  data ,
	GArray *  attributes
	)
{
	return  NULL ;
}

void
vte_terminal_get_cursor_position(
	VteTerminal *  terminal ,
	glong *  column ,
	glong *  row
	)
{
	*column = ml_term_cursor_col( terminal->pvt->term) ;
	*row = ml_term_cursor_row( terminal->pvt->term) ;
}

void
vte_terminal_match_clear_all(
	VteTerminal *  terminal
	)
{
}

/* GRegex was not supported */
#if  GLIB_CHECK_VERSION(2,14,0)
int
vte_terminal_match_add_gregex(
	VteTerminal *  terminal ,
	GRegex *  regex ,
	GRegexMatchFlags  flags
	)
{
	/* XXX */

	if( strstr( g_regex_get_pattern( regex) , "http"))
	{
		/* tag == 1 */
		return  1 ;
	}
	else
	{
		/* tag == 0 */
		return  0 ;
	}
}
#endif

void
vte_terminal_match_set_cursor(
	VteTerminal *  terminal ,
	int tag ,
	GdkCursor *  cursor
	)
{
}

void
vte_terminal_match_set_cursor_type(
	VteTerminal *  terminal ,
	int  tag ,
	GdkCursorType  cursor_type
	)
{
}

void
vte_terminal_match_set_cursor_name(
	VteTerminal *  terminal , 
	int  tag ,
	const char *  cursor_name
	)
{
}

void
vte_terminal_match_remove(
	VteTerminal *  terminal ,
	int tag
	)
{
}

char *
vte_terminal_match_check(
	VteTerminal *  terminal ,
	glong  column ,
	glong  row ,
	int *  tag
	)
{
	u_char *  buf ;
	size_t  len ;

	if( ! vte_terminal_get_has_selection( terminal))
	{
		return  NULL ;
	}

	len = terminal->pvt->screen->sel.sel_len * MLCHAR_UTF_MAX_SIZE + 1 ;
	if( ! ( buf = g_malloc( len)))
	{
		return  NULL ;
	}

	(*terminal->pvt->screen->ml_str_parser->init)( terminal->pvt->screen->ml_str_parser) ;
	ml_str_parser_set_str( terminal->pvt->screen->ml_str_parser ,
		terminal->pvt->screen->sel.sel_str , terminal->pvt->screen->sel.sel_len) ;

	(*terminal->pvt->screen->utf_conv->init)( terminal->pvt->screen->utf_conv) ;
	*(buf + (*terminal->pvt->screen->utf_conv->convert)( terminal->pvt->screen->utf_conv ,
			buf , len , terminal->pvt->screen->ml_str_parser)) = '\0' ;

	/* XXX */
	*tag = 1 ;	/* For pattern including "http" (see vte_terminal_match_add_gregex) */

	return  buf ;
}

/* GRegex was not supported */
#if  GLIB_CHECK_VERSION(2,14,0)
void
vte_terminal_search_set_gregex(
	VteTerminal *  terminal ,
	GRegex *  regex
	)
{
	if( regex)
	{
		if( ! terminal->pvt->regex)
		{
			ml_term_search_init( terminal->pvt->term , match) ;
		}
	}
	else
	{
		ml_term_search_final( terminal->pvt->term) ;
	}
	
	terminal->pvt->regex = regex ;
}

GRegex *
vte_terminal_search_get_gregex(
	VteTerminal *  terminal
	)
{
	return  terminal->pvt->regex ;
}

gboolean
vte_terminal_search_find_previous(
	VteTerminal *  terminal
	)
{
	return  search_find( terminal , 1) ;
}

gboolean
vte_terminal_search_find_next(
	VteTerminal *  terminal
	)
{
	return  search_find( terminal , 0) ;
}
#endif

void
vte_terminal_search_set_wrap_around(
	VteTerminal *  terminal ,
        gboolean  wrap_around
	)
{
}

gboolean
vte_terminal_search_get_wrap_around(
	VteTerminal *  terminal
	)
{
	return  FALSE ;
}

void
vte_terminal_set_emulation(
	VteTerminal *  terminal ,
	const char *  emulation
	)
{
}

const char *
vte_terminal_get_emulation(
	VteTerminal *terminal
	)
{
	return  main_config.term_type ;
}

const char *
vte_terminal_get_default_emulation(
	VteTerminal *  terminal
	)
{
	return  main_config.term_type ;
}

void
vte_terminal_set_encoding(
	VteTerminal *  terminal ,
	const char *  codeset
	)
{
	if( codeset == NULL)
	{
		codeset = "AUTO" ;
	}
	
	if( GTK_WIDGET_REALIZED(GTK_WIDGET(terminal)))
	{
		x_screen_set_config( terminal->pvt->screen , NULL , "encoding" , codeset) ;
	}
	else
	{
		ml_term_change_encoding( terminal->pvt->term ,
			ml_get_char_encoding( codeset)) ;
	}

	g_signal_emit_by_name( terminal , "encoding-changed") ;
}

const char *
vte_terminal_get_encoding(
	VteTerminal *  terminal
	)
{
	return  ml_get_char_encoding_name( ml_term_get_encoding( terminal->pvt->term)) ;
}

const char *
vte_terminal_get_status_line(
	VteTerminal *  terminal
	)
{
	return  "" ;
}

void
vte_terminal_get_padding(
	VteTerminal *  terminal ,
	int *  xpad ,
	int *  ypad
	)
{
	*xpad = WINDOW_MARGIN * 2 /* left + right */ ;
	*ypad = WINDOW_MARGIN * 2 /* top + bottom */ ;
}

void
vte_terminal_set_pty(
	VteTerminal *  terminal ,
	int  pty_master
	)
{
}

int
vte_terminal_get_pty(
	VteTerminal *  terminal
	)
{
	return  ml_term_get_master_fd( terminal->pvt->term) ;
}

GtkAdjustment *
vte_terminal_get_adjustment(
	VteTerminal *terminal
	)
{
	return  terminal->adjustment ;
}

glong
vte_terminal_get_char_width(
	VteTerminal *  terminal
	)
{
	return  terminal->char_width ;
}

glong
vte_terminal_get_char_height(
	VteTerminal *  terminal
	)
{
	return  terminal->char_height ;
}

glong
vte_terminal_get_row_count(
	VteTerminal *  terminal
	)
{
	return  terminal->row_count ;
}

glong
vte_terminal_get_column_count(
	VteTerminal *  terminal
	)
{
	return  terminal->column_count ;
}

const char *
vte_terminal_get_window_title(
	VteTerminal *  terminal
	)
{
	return  terminal->window_title ;
}

const char *
vte_terminal_get_icon_title(
	VteTerminal *  terminal
	)
{
	return  terminal->icon_title ;
}

int
vte_terminal_get_child_exit_status(
	VteTerminal *  terminal
	)
{
	return  0 ;
}

#ifndef  VTE_DISABLE_DEPRECATED

void
vte_terminal_set_cursor_blinks(
	VteTerminal *  terminal ,
	gboolean blink
	)
{
	x_screen_set_config( terminal->pvt->screen , NULL , "blink_cursor" ,
			blink ? "true" : "false") ;
}

gboolean
vte_terminal_get_using_xft(
	VteTerminal *  terminal
	)
{
	if( x_get_type_engine( terminal->pvt->screen->font_man) == TYPE_XFT)
	{
		return  TRUE ;
	}
	else
	{
		return  FALSE ;
	}
}

int
vte_terminal_match_add(
	VteTerminal *  terminal ,
	const char *  match
	)
{
	return  1 ;
}

glong
vte_terminal_get_char_descent(
	VteTerminal *  terminal
	)
{
	return  terminal->char_descent ;
}

glong
vte_terminal_get_char_ascent(
	VteTerminal *  terminal
	)
{
	return  terminal->char_ascent ;
}

static void
set_anti_alias(
	VteTerminal *  terminal ,
	VteTerminalAntiAlias  antialias
	)
{
	char *  value ;
	int  term_is_null ;

	if( antialias == VTE_ANTI_ALIAS_FORCE_ENABLE)
	{
		value = "true" ;
	}
	else if( antialias == VTE_ANTI_ALIAS_FORCE_ENABLE)
	{
		value = "false" ;
	}
	else
	{
		return ;
	}

	/*
	 * XXX
	 * Hack for the case of calling this function before fork pty because
	 * change_font_present() in x_screen.c calls ml_term_get_vertical_mode().
	 */
	if( terminal->pvt->screen->term == NULL)
	{
		terminal->pvt->screen->term = terminal->pvt->term ;
		term_is_null = 1 ;
	}
	else
	{
		term_is_null = 0 ;
	}

	x_screen_set_config( terminal->pvt->screen , NULL , "use_anti_alias" , value) ;

	if( term_is_null)
	{
		terminal->pvt->screen->term = NULL ;
	}
}

void
vte_terminal_set_font_full(
	VteTerminal *  terminal ,
	const PangoFontDescription *  font_desc ,
	VteTerminalAntiAlias  antialias
	)
{
	set_anti_alias( terminal , antialias) ;
	vte_terminal_set_font( terminal , font_desc) ;
}

void
vte_terminal_set_font_from_string_full(
	VteTerminal *  terminal ,
	const char *  name ,
	VteTerminalAntiAlias  antialias
	)
{
	set_anti_alias( terminal , antialias) ;
	vte_terminal_set_font_from_string( terminal , name) ;
}

#endif	/* VTE_DISABLE_DEPRECATED */


#if  VTE_CHECK_VERSION(0,26,0)

#include  <sys/ioctl.h>
#include  <ml_pty_intern.h>	/* XXX in order to operate ml_pty_t::child_pid directly. */
#include  <kiklib/kik_config.h>	/* HAVE_SETSID */


struct _VtePty
{
	GObject  parent_instance ;

	VteTerminal *  terminal ;
	VtePtyFlags  flags ;
} ;

struct _VtePtyClass
{
	GObjectClass  parent_class ;
} ;

G_DEFINE_TYPE(VtePty , vte_pty , G_TYPE_OBJECT) ;

static void
vte_pty_init(
	VtePty *  pty
	)
{
}

static void
vte_pty_class_init(
	VtePtyClass *  kclass
	)
{
}

VtePty *
vte_terminal_pty_new(
	VteTerminal *  terminal ,
	VtePtyFlags  flags ,
	GError **  error
	)
{
	VtePty *  pty ;

	if( terminal->pvt->pty)
	{
		return  terminal->pvt->pty ;
	}

	if( ! ( pty = vte_pty_new( flags , error)))
	{
		return  NULL ;
	}

	vte_terminal_set_pty_object( terminal , pty) ;

	return  pty ;
}

VtePty *
vte_terminal_get_pty_object(
	VteTerminal *  terminal
	)
{
	return  terminal->pvt->pty ;
}

void
vte_terminal_set_pty_object(
	VteTerminal *  terminal ,
	VtePty *  pty
	)
{
	if( terminal->pvt->pty || ! pty)
	{
		return ;
	}

	pty->terminal = terminal ;
	terminal->pvt->pty = g_object_ref( terminal->pvt->pty) ;

	vte_pty_set_term( pty , vte_terminal_get_emulation( terminal)) ;

	if( vte_terminal_forkpty( terminal , NULL , NULL ,
				(pty->flags & VTE_PTY_NO_LASTLOG) ? FALSE : TRUE ,
				(pty->flags & VTE_PTY_NO_UTMP) ? FALSE : TRUE ,
				(pty->flags & VTE_PTY_NO_WTMP) ? FALSE : TRUE) == 0)
	{
		/* child */
		exit(0) ;
	}

	/* Don't catch exit(0) above. */
	terminal->pvt->term->pty->child_pid = -1 ;
}

VtePty *
vte_pty_new(
	VtePtyFlags  flags ,
	GError **  error
	)
{
	VtePty *  pty ;

	if( ( pty = g_object_new( VTE_TYPE_PTY , NULL)))
	{
		pty->flags = flags ;
		pty->terminal = NULL ;
	}

	return  pty ;
}

VtePty *
vte_pty_new_foreign(
	int  fd ,
	GError **  error
	)
{
	return  NULL ;
}

void
vte_pty_close(
	VtePty *  pty
	)
{
}

void
vte_pty_child_setup(
	VtePty *  pty
	)
{
	int  slave ;
	int  master ;
#if  (! defined(HAVE_SETSID) && defined(TIOCNOTTY)) || ! defined(TIOCSCTTY)
	int  fd ;
#endif

	if( ! pty->terminal)
	{
		return ;
	}

#ifdef  HAVE_SETSID
	setsid() ;
#else
#ifdef  TIOCNOTTY
	if( ( fd = open( "/dev/tty" , O_RDWR|O_NOCTTY)) >= 0)
	{
		ioctl( fd , TIOCNOTTY , NULL) ;
		close( fd) ;
	}
#endif
#endif

	master = ml_term_get_master_fd( pty->terminal->pvt->term) ;
	slave = ml_term_get_slave_fd( pty->terminal->pvt->term) ;

#ifdef  TIOCSCTTY
	ioctl( slave, TIOCSCTTY, NULL) ;
#else
	if( ( fd = open( "/dev/tty" , O_RDWR|O_NOCTTY)) >= 0)
	{
		close( fd) ;
	}
	if( ( fd = open( ptsname( master) , O_RDWR)) >= 0)
	{
		close( fd) ;
	}
	if( ( fd = open( "/dev/tty" , O_WRONLY)) >= 0)
	{
		close( fd) ;
	}
#endif

	dup2( slave , 0) ;
	dup2( slave , 1) ;
	dup2( slave , 2) ;

	if( slave > STDERR_FILENO)
	{
		close(slave) ;
	}

	/* Already set in kik_pty_fork() from vte_terminal_forkpty(). */
#if  0
	cfsetispeed( &tio , B9600) ;
	cfsetospeed( &tio , B9600) ;
	tcsetattr( STDIN_FILENO, TCSANOW , &tio) ;
#endif

	close( master) ;
}

int
vte_pty_get_fd(
	VtePty *  pty
	)
{
	if( ! pty->terminal)
	{
		return  -1 ;
	}

	return  ml_term_get_master_fd( pty->terminal->pvt->term) ;
}

gboolean
vte_pty_set_size(
	VtePty *  pty ,
	int  rows ,
	int  columns ,
	GError **  error
	)
{
	if( ! pty->terminal)
	{
		return  FALSE ;
	}

	vte_terminal_set_size( pty->terminal , columns , rows) ;

	return  TRUE ;
}

gboolean
vte_pty_get_size(
	VtePty *  pty ,
	int *  rows ,
	int *  columns ,
	GError **  error
	)
{
	if( ! pty->terminal)
	{
		return  FALSE ;
	}

	*columns = pty->terminal->column_count ;
	*rows = pty->terminal->row_count ;

	return  TRUE ;
}

void
vte_pty_set_term(
	VtePty *  pty ,
	const char *  emulation
	)
{
	if( pty->terminal)
	{
		vte_terminal_set_emulation( pty->terminal , emulation) ;
	}
}

gboolean
vte_pty_set_utf8(
	VtePty *  pty ,
	gboolean utf8 ,
	GError **  error
	)
{
	if( ! pty->terminal)
	{
		return  FALSE ;
	}

	return  ml_term_change_encoding( pty->terminal->pvt->term ,
			utf8 ? ML_UTF8 : ml_get_char_encoding( "auto")) ;
}

void
vte_terminal_watch_child(
	VteTerminal *  terminal ,
	GPid  child_pid
	)
{
	vte_reaper_add_child( child_pid) ;
	terminal->pvt->term->pty->child_pid = child_pid ;
}

#endif


/* Ubuntu original function ? */
void
vte_terminal_set_alternate_screen_scroll(
	VteTerminal *  terminal ,
	gboolean  scroll
	)
{
}


/* Hack for input method module */

static GIOChannel *  gio ;
static guint  src_id ;

int
x_event_source_add_fd(
	int  fd ,
	void (*handler)(void)
	)
{
	if( gio)
	{
	#ifdef  DEBUG
		kik_debug_printf( KIK_DEBUG_TAG " x_event_source_add_fd failed\n") ;
	#endif
	
		return  0 ;
	}

	gio = g_io_channel_unix_new( fd) ;
	src_id = g_io_add_watch( gio , G_IO_IN , (GIOFunc)handler , NULL) ;

	return  1 ;
}

int
x_event_source_remove_fd(
	int fd
	)
{
	if( gio && g_io_channel_unix_get_fd( gio) == fd)
	{
		g_source_destroy( g_main_context_find_source_by_id( NULL , src_id)) ;
		g_io_channel_unref( gio) ;
	
		gio = NULL ;
	}

	return  1 ;
}

/*
 * GTK+-3.0 supports XInput2 which enables multi device.
 * But XInput2 disables popup menu and shortcut key which applications
 * using libvte show, because mlterm doesn't support it.
 * So multi device is disabled for now.
 *
 * __attribute__((constructor)) hack is necessary because gdk_disable_multidevice()
 * must be called before gtk_init().
 */
#if  GTK_CHECK_VERSION(2,90,0) && __GNUC__
static void __attribute__((constructor))
init_vte(void)
{
	gdk_disable_multidevice() ;
}
#endif
