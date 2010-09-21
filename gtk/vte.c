/*
 *	$Id: ccheader,v 1.2 2001/12/01 23:37:26 ken Exp $
 */

#include  <vte/vte.h>

#include  <X11/keysym.h>
#include  <gdk/gdkx.h>
#include  <gtk/gtksignal.h>
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
#include  <ml_str_parser.h>
#include  <ml_term_manager.h>
#include  <x_screen.h>
#include  <x_xim.h>
#include  <x_main_config.h>
#include  <version.h>

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

#if  ! ((GTK_MAJOR_VERSION >= 2) && (GTK_MINOR_VERSION >= 14))
#define  gtk_adjustment_get_upper(adj)  ((adj)->upper)
#define  gtk_adjustment_get_value(adj)  ((adj)->value)
#define  gtk_adjustment_get_page_size(adj)  ((adj)->page_size)
#endif

#define  VTE_WIDGET(screen)  ((VteTerminal*)(screen)->system_listener->self)
/* XXX Hack to distinguish x_screen_t from x_{candidate|status}_screent_t */
#define  IS_MLTERM_SCREEN(win)  ((win)->parent_window != None)
#define  WINDOW_MARGIN  2
#define  TERM_TYPE  "xterm"


struct _VteTerminalPrivate
{
	x_screen_t *  screen ;

	x_system_event_listener_t  system_listener ;

	void (*line_scrolled_out)( void *) ;
	x_screen_scroll_event_listener_t  screen_scroll_listener ;
	int8_t  adj_value_changed_by_myself ;

	GIOChannel *  io ;
	guint  src_id ;
} ;

enum
{
	COPY_CLIPBOARD,
	PASTE_CLIPBOARD,
	LAST_SIGNAL
} ;


G_DEFINE_TYPE(VteTerminal , vte_terminal , GTK_TYPE_WIDGET) ;


/* --- static variables --- */

static x_main_config_t  main_config ;
static x_color_config_t  color_config ;
static x_shortcut_t  shortcut ;
static x_termcap_t  termcap ;
static x_display_t  disp ;

static int  display_opened ;
static int  timeout_registered ;

static guint signals[LAST_SIGNAL];

static char *  true = "true" ;
static char *  false = "false" ;


/* --- static functions --- */

/*
 * x_system_event_listener_t handlers
 */

static void
pty_closed(
	void *  p ,
	x_screen_t *  screen	/* screen->term was already deleted. */
	)
{
	GtkWidget *  widget ;
	ml_term_t *  term ;

	widget = p ;
	
	if( ( term = ml_get_detached_term( NULL)) == NULL)
	{
		g_signal_emit_by_name( VTE_TERMINAL(widget) , "child-exited") ;

	#ifdef  __DEBUG
		kik_debug_printf( "pty_closed\n") ;
	#endif
	}
	else
	{
		x_screen_attach( screen , term) ;
	}
}

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


/*
 * ml_screen_event_listener_t (overriding) handler
 */
 
static void
line_scrolled_out(
	void *  p		/* must be x_screen_t */
	)
{
	x_screen_t *  screen ;
	int  upper ;
	int  value ;
	
	screen = p ;

	VTE_WIDGET(screen)->pvt->line_scrolled_out( p) ;

	if( ( upper = gtk_adjustment_get_upper( VTE_WIDGET(screen)->adjustment))
		== ml_term_get_log_size( VTE_WIDGET(screen)->pvt->screen->term))
	{
		return ;
	}

	/*
	 * line_scrolled_out is called in vt100 mode
	 * (after ml_xterm_event_listener_t::start_vt100 event), so
	 * don't call x_screen_scroll_to() in adjustment_value_changed()
	 * in this context.
	 */
	VTE_WIDGET(screen)->pvt->adj_value_changed_by_myself = 1 ;

	value = gtk_adjustment_get_value( VTE_WIDGET(screen)->adjustment) ;
	
#if  (GTK_MAJOR_VERSION >= 2) && (GTK_MINOR_VERSION >= 14)
	gtk_adjustment_set_upper( VTE_WIDGET(screen)->adjustment , upper + 1) ;
	gtk_adjustment_set_value( VTE_WIDGET(screen)->adjustment , value + 1) ;
#else
	VTE_WIDGET(screen)->adjustment->upper ++ ;
	VTE_WIDGET(screen)->adjustment->value ++ ;
	gtk_adjustment_changed( VTE_WIDGET(screen)->adjustment) ;
	gtk_adjustment_value_changed( VTE_WIDGET(screen)->adjustment) ;
#endif

#ifdef  __DEBUG
	kik_debug_printf( "line_scrolled_out upper %d value %d\n" , upper + 1 , value + 1) ;
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

#if  (GTK_MAJOR_VERSION >= 2) && (GTK_MINOR_VERSION >= 14)
	gtk_adjustment_set_value( terminal->adjustment , upper - page_size) ;
#else
	terminal->adjustment->value = upper - page_size ;
	gtk_adjustment_value_changed( terminal->adjustment) ;
#endif
	
#ifdef  __DEBUG
	kik_debug_printf( "bs_mode_exited upper %d page_size %d\n" , upper , page_size) ;
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

#if  (GTK_MAJOR_VERSION >= 2) && (GTK_MINOR_VERSION >= 14)
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

#if  (GTK_MAJOR_VERSION >= 2) && (GTK_MINOR_VERSION >= 14)
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

#if  (GTK_MAJOR_VERSION >= 2) && (GTK_MINOR_VERSION >= 14)
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
	kik_debug_printf( "scroll to %d\n" , value - (upper - page_size)) ;
#endif

	x_screen_scroll_to( terminal->pvt->screen , value - (upper - page_size)) ;
}


static void
reset_vte_size_member(
	VteTerminal *  terminal
	)
{
	int  emit ;

	emit = 0 ;
	
	if( terminal->char_width != 0 &&
		terminal->char_width != x_col_width( terminal->pvt->screen))
	{
		emit = 1 ;
	}
	terminal->char_width = x_col_width( terminal->pvt->screen) ;

	if( terminal->char_height != 0 &&
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
	
	terminal->char_ascent = x_line_height_to_baseline( terminal->pvt->screen) ;
	terminal->char_descent = terminal->char_height - terminal->char_ascent ;


	emit = 0 ;

	if( /* If row_count == 0, reset_vte_size_member is called from vte_terminal_init */
	    terminal->row_count != 0 &&
	    terminal->row_count != ml_term_get_rows( terminal->pvt->screen->term))
	{
		emit = 1 ;
	}
	
	terminal->row_count = ml_term_get_rows( terminal->pvt->screen->term) ;
	terminal->column_count = ml_term_get_cols( terminal->pvt->screen->term) ;

	if( emit)
	{
	#if  (GTK_MAJOR_VERSION >= 2) && (GTK_MINOR_VERSION >= 14)
		int  value ;

		value = ml_term_get_num_of_logged_lines( terminal->pvt->screen->term) ;
		gtk_adjustment_configure( terminal->adjustment ,
			value /* value */ , 0 /* lower */ ,
			value + terminal->row_count /* upper */ ,
			1 /* step increment */ , terminal->row_count /* page increment */ ,
			terminal->row_count /* page size */) ;
	#else
		terminal->adjustment->value =
			ml_term_get_num_of_logged_lines( terminal->pvt->screen->term) ;
		terminal->adjustment->upper = terminal->adjustment->value + terminal->row_count ;
		terminal->adjustment->page_increment = terminal->row_count ;
		terminal->adjustment->page_size = terminal->row_count ;

		gtk_adjustment_changed( terminal->adjustment) ;
		gtk_adjustment_value_changed( terminal->adjustment) ;
	#endif
	}

	GTK_WIDGET(terminal)->requisition.width = ACTUAL_WIDTH( &terminal->pvt->screen->window) ;
	GTK_WIDGET(terminal)->requisition.height = ACTUAL_HEIGHT( &terminal->pvt->screen->window) ;
}

static gboolean
vte_terminal_timeout(
	gpointer  data
	)
{
	ml_close_dead_terms() ;

	return  TRUE ;
}

static gboolean
vte_terminal_io(
	GIOChannel *  source ,
	GIOCondition  conditon ,
	gpointer  data
	)
{
	GtkWidget *  widget ;

	widget = data ;

	ml_term_parse_vt100_sequence( VTE_TERMINAL(widget)->pvt->screen->term) ;

	ml_close_dead_terms() ;
	
	return  TRUE ;
}

static void
menu_position(
	GtkMenu *  menu ,
	gint *  x ,
	gint *  y ,
	gboolean *  push_in ,
	gpointer  data
	)
{
	XButtonEvent *  ev ;
	int  global_x ;
	int  global_y ;
	Window  child ;

	ev = data ;

	XTranslateCoordinates( ev->display , ev->window , DefaultRootWindow( ev->display) ,
		ev->x , ev->y , &global_x , &global_y , &child) ;
	
	*x = global_x ;
	*y = global_y ;
	*push_in = FALSE ;
}

static void
activate_copy(
	GtkWidget *  widget ,
	gpointer  data
	)
{
	vte_terminal_copy_clipboard( data) ;
}

static void
activate_paste(
	GtkWidget *  widget ,
	gpointer  data
	)
{
	vte_terminal_paste_clipboard( data) ;
}

/*
 * Don't call ml_close_dead_terms() before returning GDK_FILTER_CONTINUE,
 * because ml_close_dead_terms() will destroy widget in pty_closed and
 * destroyed widget can be touched right after this function.
 */
static GdkFilterReturn
vte_terminal_filter(
	GdkXEvent *  xevent ,
	GdkEvent *  event ,
	gpointer  data
	)
{
	u_int  count ;

	if( XFilterEvent( (XEvent*)xevent , None))
	{
		return  GDK_FILTER_REMOVE ;
	}
	
	for( count = 0 ; count < disp.num_of_roots ; count++)
	{
		if( ((XEvent*)xevent)->type == ButtonPress &&
			((XEvent*)xevent)->xbutton.button == Button3 &&
			(((XEvent*)xevent)->xbutton.state & ControlMask) == 0 &&
			((XEvent*)xevent)->xbutton.window == disp.roots[count]->my_window &&
			IS_MLTERM_SCREEN(disp.roots[count]))
		{
			GtkWidget *  menu ;
			GtkWidget *  item ;

			menu = gtk_menu_new() ;
			
			item = gtk_menu_item_new_with_label( "Copy") ;
			g_signal_connect( G_OBJECT(item) , "activate" ,
				G_CALLBACK(activate_copy) ,
				VTE_WIDGET((x_screen_t*)disp.roots[count])) ;
			gtk_menu_append(GTK_MENU(menu), item) ;
			
			item = gtk_menu_item_new_with_label( "Paste") ;
			g_signal_connect( G_OBJECT(item) , "activate" ,
				G_CALLBACK(activate_paste) ,
				VTE_WIDGET((x_screen_t*)disp.roots[count])) ;
			gtk_menu_append(GTK_MENU(menu), item) ;

			gtk_widget_show_all(menu) ;
			gtk_menu_popup( GTK_MENU(menu) , NULL , NULL ,
				menu_position , xevent , 0 , 0) ;
		}
		else if( x_window_receive_event( disp.roots[count] , (XEvent*)xevent))
		{
			return  GDK_FILTER_REMOVE ;
		}
		/*
		 * xconfigure.window: window whose size, position, border, and/or stacking
		 *                    order was changed.
		 *                    => processed in following.
		 * xconfigure.event(=XAnyEvent.window): reconfigured window or to its parent.
		 *                    => processed in x_window_receive_event()
		 */
		else if( ((XEvent*)xevent)->xconfigure.window == disp.roots[count]->my_window)
		{
			if( IS_MLTERM_SCREEN(disp.roots[count]) &&
				((XEvent*)xevent)->type == ConfigureNotify)
			{
				VteTerminal *  terminal ;

				terminal = VTE_WIDGET((x_screen_t*)disp.roots[count]) ;

				if( ((XEvent*)xevent)->xconfigure.width !=
					GTK_WIDGET(terminal)->allocation.width ||
				    ((XEvent*)xevent)->xconfigure.height !=
					GTK_WIDGET(terminal)->allocation.height)
				{
					reset_vte_size_member( terminal) ;

					gtk_widget_queue_resize_no_redraw( GTK_WIDGET(terminal)) ;

				#ifdef  __DEBUG
					kik_debug_printf( "reset size %d %d\n" ,
					       ACTUAL_WIDTH(&terminal->pvt->screen->window) ,
					       ACTUAL_HEIGHT(&terminal->pvt->screen->window)) ;
				#endif
				}

				return  GDK_FILTER_REMOVE ;
			}
		}
	}

	return  GDK_FILTER_CONTINUE ;
}

static void
vte_terminal_realize(
	GtkWidget *  widget
	)
{
	GdkWindowAttr  attr ;
	Window  xid ;
	char *  env[6] ;	/* MLTERM,TERM,WINDOWID,DISPLAY,COLORFGBG,NULL */
	char **  env_p ;
	char  wid_env[9 + DIGIT_STR_LEN(Window) + 1] ; /* "WINDOWID="(9) + [32bit digit] + NULL(1) */
	char  ver_env[13] ;
	char *  disp_name ;
	char *  disp_env ;
	char  term_env[11] ;
	char  colorfgbg_env[26] ;
	
	if( widget->window)
	{
		return ;
	}

#ifdef  __DEBUG
	kik_debug_printf( "vte_realized %d %d %d %d\n" ,
		widget->allocation.x , widget->allocation.y ,
		widget->allocation.width , widget->allocation.height) ;
#endif

	attr.window_type = GDK_WINDOW_CHILD ;
	attr.x = widget->allocation.x ;
	attr.y = widget->allocation.y ;
	attr.width = widget->allocation.width ;
	attr.height = widget->allocation.height ;
	attr.wclass = GDK_INPUT_OUTPUT ;
	attr.visual = gtk_widget_get_visual( widget) ;
	attr.colormap = gtk_widget_get_colormap( widget) ;
	attr.event_mask = gtk_widget_get_events( widget) |
				GDK_FOCUS_CHANGE_MASK |
				GDK_SUBSTRUCTURE_MASK ;		/* DestroyNotify from child */

	widget->window = gdk_window_new( gtk_widget_get_parent_window( widget) , &attr ,
				GDK_WA_X | GDK_WA_Y | GDK_WA_VISUAL | GDK_WA_COLORMAP) ;
	gdk_window_set_user_data( widget->window , widget) ;
	
	GTK_WIDGET_SET_FLAGS( widget , GTK_REALIZED) ;

	widget->style = gtk_style_attach( widget->style , widget->window) ;

	env_p = env ;

	strcpy( ver_env , "MLTERM=3.0.1") ;
	*(env_p ++) = ver_env ;

	xid = gdk_x11_drawable_get_xid( widget->window) ;
	sprintf( wid_env , "WINDOWID=%ld" , xid) ;
	*(env_p ++) = wid_env ;

	disp_name = gdk_display_get_name( gtk_widget_get_display( widget)) ;
	/* "DISPLAY="(8) + NULL(1) */
	if( disp_name && ( disp_env = alloca( 8 + strlen( disp_name) + 1)))
	{
		sprintf( disp_env , "DISPLAY=%s" , disp_name) ;
		*(env_p ++) = disp_env ;
	}

	strcpy( term_env , "TERM=" TERM_TYPE) ;
	*(env_p ++) = term_env ;

	strcpy( colorfgbg_env , "COLORFGBG=default;default") ;
	*(env_p ++) = colorfgbg_env ;

	/* NULL terminator */
	*env_p = NULL ;

	vte_terminal_forkpty( VTE_TERMINAL(widget) , env , NULL , FALSE, FALSE, FALSE) ;

	VTE_TERMINAL(widget)->pvt->io = g_io_channel_unix_new(
				ml_term_get_pty_fd( VTE_TERMINAL(widget)->pvt->screen->term)) ;
	VTE_TERMINAL(widget)->pvt->src_id = g_io_add_watch( VTE_TERMINAL(widget)->pvt->io ,
						G_IO_IN , vte_terminal_io , widget) ;

	if( ! timeout_registered)
	{
		/* 100 miliseconds */
		g_timeout_add( 100 , vte_terminal_timeout , NULL) ;
		timeout_registered = 1 ;
	}

	VTE_TERMINAL(widget)->pvt->screen->window.create_gc = 1 ;

	x_display_show_root( &disp , &VTE_TERMINAL(widget)->pvt->screen->window ,
		0 , 0 , 0 , "mlterm" , xid) ;

	/*
	 * allocation passed by size_allocate is not necessarily same as requisition
	 * passed by size_request, so x_window_resize must be called here.
	 */
	x_window_resize( &VTE_TERMINAL(widget)->pvt->screen->window ,
		widget->allocation.width , widget->allocation.height , NOTIFY_TO_MYSELF) ;
}

static void
vte_terminal_unrealize(
	GtkWidget *  widget
	)
{
	VteTerminal *  terminal ;

	terminal = VTE_TERMINAL(widget) ;

	g_source_destroy( g_main_context_find_source_by_id( NULL , terminal->pvt->src_id)) ;
#if  0
	kik_debug_printf( "g_io_channel %d\n" , terminal->pvt->io->ref_count) ;
#endif
	g_io_channel_unref( terminal->pvt->io) ;
#if  0
	g_io_channel_shutdown( terminal->pvt->io , FALSE , NULL) ;
#endif
	terminal->pvt->io = NULL ;
	
	x_screen_detach( terminal->pvt->screen) ;
	x_font_manager_delete( terminal->pvt->screen->font_man) ;
	x_color_manager_delete( terminal->pvt->screen->color_man) ;

	x_display_remove_root( &disp , &terminal->pvt->screen->window) ;

	terminal->pvt->screen = NULL ;
	
	free( terminal->pvt) ;

	if( GTK_WIDGET_MAPPED( widget))
	{
		gtk_widget_unmap( widget) ;
	}

	gtk_style_detach( widget->style) ;
	gdk_window_destroy( widget->window) ;
	widget->window = NULL ;

	GTK_WIDGET_UNSET_FLAGS( widget , GTK_REALIZED) ;
	
#ifdef  __DEBUG
	kik_debug_printf( "vte_terminal_unrealize\n") ;
#endif
}

static gboolean
vte_terminal_focus_in_event(
	GtkWidget *  widget ,
	GdkEventFocus *  event
	)
{
	if( ( GTK_WIDGET_FLAGS(widget) & GTK_MAPPED))
	{
	#ifdef  __DEBUG
		kik_debug_printf( "focus in\n") ;
	#endif

		XSetInputFocus( gdk_x11_drawable_get_xdisplay( widget->window) ,
			VTE_TERMINAL( widget)->pvt->screen->window.my_window ,
			RevertToParent , CurrentTime) ;
	}

	return  FALSE ;
}

static void
vte_terminal_size_request(
	GtkWidget *  widget ,
	GtkRequisition *  req
	)
{
	*req = widget->requisition ;

#ifdef  __DEBUG
	kik_debug_printf( "req %d %d cur alloc %d %d\n" , req->width , req->height ,
			widget->allocation.width , widget->allocation.height) ;
#endif
}

static void
vte_terminal_size_allocate(
	GtkWidget *  widget ,
	GtkAllocation *  allocation
	)
{
	widget->allocation = *allocation ;
	widget->requisition.width = allocation->width ;
	widget->requisition.height = allocation->height ;

#ifdef  __DEBUG
	kik_debug_printf( "alloc %d %d %d %d\n" ,
		allocation->x , allocation->y , allocation->width , allocation->height) ;
#endif

	if( GTK_WIDGET_REALIZED(widget))
	{
		gdk_window_move_resize( widget->window,
					allocation->x, allocation->y,
					allocation->width, allocation->height);

		x_window_resize( &VTE_TERMINAL(widget)->pvt->screen->window ,
			allocation->width , allocation->height , NOTIFY_TO_MYSELF) ;
	}
}

static void
vte_terminal_finalize(
	GObject *  obj
	)
{
	VteTerminal *  terminal ;
	GtkSettings *  settings ;

	terminal = VTE_TERMINAL(obj) ;

	if( terminal->adjustment)
	{
		g_signal_handlers_disconnect_by_func( terminal->adjustment ,
				adjustment_value_changed , terminal) ;
		g_object_unref( terminal->adjustment) ;
	}

	settings = gtk_widget_get_settings( GTK_WIDGET(obj)) ;
	g_signal_handlers_disconnect_matched( settings , G_SIGNAL_MATCH_DATA ,
		0 , 0 , NULL , NULL , terminal) ;

	G_OBJECT_CLASS(vte_terminal_parent_class)->finalize(obj) ;
	
#ifdef  __DEBUG
	kik_debug_printf( "vte_terminal_unrealize\n") ;
#endif
}

static void
vte_terminal_class_init(
	VteTerminalClass *  vclass
	)
{
	kik_conf_t *  conf ;
	char *  argv[] = { "mlterm" , NULL } ;
	GObjectClass *  oclass ;
	GtkWidgetClass *  wclass ;

	kik_sig_child_init() ;
	
	kik_priv_change_euid( kik_getuid()) ;
	kik_priv_change_egid( kik_getgid()) ;
	
	if( ! kik_locale_init( ""))
	{
		kik_msg_printf( "locale settings failed.\n") ;
	}

	kik_set_sys_conf_dir( CONFIG_PATH) ;
	
	ml_term_manager_init( 1) ;
	x_color_config_init( &color_config) ;
	x_shortcut_init( &shortcut) ;
	x_termcap_init( &termcap) ;
	x_xim_init( 1) ;

	conf = kik_conf_new( "mlterm" , MAJOR_VERSION , MINOR_VERSION , REVISION ,
			PATCH_LEVEL , CHANGE_DATE) ;
	x_prepare_for_main_config( conf) ;
	x_main_config_init( &main_config , conf , 1 , argv) ;
	kik_conf_delete( conf) ;
	
	oclass = G_OBJECT_CLASS(vclass) ;
	wclass = GTK_WIDGET_CLASS(vclass) ;

	oclass->finalize = vte_terminal_finalize ;
	wclass->realize = vte_terminal_realize ;
	wclass->unrealize = vte_terminal_unrealize ;
	wclass->focus_in_event = vte_terminal_focus_in_event ;
	wclass->size_allocate = vte_terminal_size_allocate ;
	wclass->size_request = vte_terminal_size_request ;

	vclass->eof_signal =
		g_signal_new( I_("eof") ,
			G_OBJECT_CLASS_TYPE(vclass) ,
			G_SIGNAL_RUN_LAST ,
			G_STRUCT_OFFSET( VteTerminalClass , eof) ,
			NULL , NULL , g_cclosure_marshal_VOID__VOID ,
			G_TYPE_NONE , 0) ;

	vclass->child_exited_signal =
		g_signal_new( I_("child-exited") ,
			G_OBJECT_CLASS_TYPE(vclass) ,
			G_SIGNAL_RUN_LAST ,
			G_STRUCT_OFFSET( VteTerminalClass , child_exited) ,
			NULL , NULL , g_cclosure_marshal_VOID__VOID ,
			G_TYPE_NONE , 0) ;

	vclass->window_title_changed_signal =
		g_signal_new( I_("window-title-changed") ,
			G_OBJECT_CLASS_TYPE(vclass) ,
			G_SIGNAL_RUN_LAST ,
			G_STRUCT_OFFSET( VteTerminalClass , window_title_changed) ,
			NULL , NULL , g_cclosure_marshal_VOID__VOID ,
			G_TYPE_NONE , 0) ;

	vclass->icon_title_changed_signal =
		g_signal_new( I_("icon-title-changed") ,
			G_OBJECT_CLASS_TYPE(vclass) ,
			G_SIGNAL_RUN_LAST ,
			G_STRUCT_OFFSET( VteTerminalClass , icon_title_changed) ,
			NULL , NULL , g_cclosure_marshal_VOID__VOID ,
			G_TYPE_NONE , 0) ;

	vclass->encoding_changed_signal =
		g_signal_new( I_("encoding-changed") ,
			G_OBJECT_CLASS_TYPE(vclass) ,
			G_SIGNAL_RUN_LAST ,
			G_STRUCT_OFFSET( VteTerminalClass , encoding_changed) ,
			NULL , NULL , g_cclosure_marshal_VOID__VOID ,
			G_TYPE_NONE , 0) ;

	vclass->commit_signal =
		g_signal_new( I_("commit") ,
			G_OBJECT_CLASS_TYPE(vclass) ,
			G_SIGNAL_RUN_LAST ,
			G_STRUCT_OFFSET(VteTerminalClass , commit) ,
			NULL ,
			NULL ,
			_vte_marshal_VOID__STRING_UINT ,
			G_TYPE_NONE , 2 , G_TYPE_STRING , G_TYPE_UINT) ;

	vclass->emulation_changed_signal =
		g_signal_new( I_("emulation-changed") ,
			G_OBJECT_CLASS_TYPE(vclass) ,
			G_SIGNAL_RUN_LAST ,
			G_STRUCT_OFFSET(VteTerminalClass , emulation_changed) ,
			NULL , NULL , g_cclosure_marshal_VOID__VOID ,
			G_TYPE_NONE , 0) ;

	vclass->char_size_changed_signal =
		g_signal_new( I_("char-size-changed") ,
			G_OBJECT_CLASS_TYPE(vclass) ,
			G_SIGNAL_RUN_LAST ,
			G_STRUCT_OFFSET(VteTerminalClass , char_size_changed) ,
			NULL , NULL , _vte_marshal_VOID__UINT_UINT ,
			G_TYPE_NONE , 2 , G_TYPE_UINT , G_TYPE_UINT) ;

	vclass->selection_changed_signal =
		g_signal_new ( I_("selection-changed") ,
			G_OBJECT_CLASS_TYPE(vclass) ,
			G_SIGNAL_RUN_LAST ,
			G_STRUCT_OFFSET(VteTerminalClass , selection_changed) ,
			NULL , NULL , g_cclosure_marshal_VOID__VOID ,
			G_TYPE_NONE , 0) ;

	vclass->contents_changed_signal =
		g_signal_new( I_("contents-changed") ,
			G_OBJECT_CLASS_TYPE(vclass) ,
			G_SIGNAL_RUN_LAST ,
			G_STRUCT_OFFSET(VteTerminalClass , contents_changed) ,
			NULL , NULL , g_cclosure_marshal_VOID__VOID ,
			G_TYPE_NONE , 0) ;

	vclass->cursor_moved_signal =
		g_signal_new( I_("cursor-moved") ,
			G_OBJECT_CLASS_TYPE(vclass) ,
			G_SIGNAL_RUN_LAST ,
			G_STRUCT_OFFSET(VteTerminalClass , cursor_moved) ,
			NULL , NULL , g_cclosure_marshal_VOID__VOID ,
			G_TYPE_NONE , 0) ;

	vclass->deiconify_window_signal =
		g_signal_new( I_("deiconify-window") ,
			G_OBJECT_CLASS_TYPE(vclass) ,
			G_SIGNAL_RUN_LAST ,
			G_STRUCT_OFFSET(VteTerminalClass , deiconify_window) ,
			NULL , NULL , g_cclosure_marshal_VOID__VOID ,
			G_TYPE_NONE , 0) ;

	vclass->iconify_window_signal =
		g_signal_new( I_("iconify-window") ,
			G_OBJECT_CLASS_TYPE(vclass) ,
			G_SIGNAL_RUN_LAST ,
			G_STRUCT_OFFSET(VteTerminalClass , iconify_window) ,
			NULL , NULL , g_cclosure_marshal_VOID__VOID ,
			G_TYPE_NONE , 0) ;

	vclass->raise_window_signal =
		g_signal_new( I_("raise-window") ,
			G_OBJECT_CLASS_TYPE(vclass) ,
			G_SIGNAL_RUN_LAST ,
			G_STRUCT_OFFSET(VteTerminalClass , raise_window) ,
			NULL , NULL , g_cclosure_marshal_VOID__VOID ,
			G_TYPE_NONE , 0) ;

	vclass->lower_window_signal =
		g_signal_new( I_("lower-window") ,
			G_OBJECT_CLASS_TYPE(vclass) ,
			G_SIGNAL_RUN_LAST ,
			G_STRUCT_OFFSET(VteTerminalClass , lower_window) ,
			NULL , NULL , g_cclosure_marshal_VOID__VOID ,
			G_TYPE_NONE , 0) ;

	vclass->refresh_window_signal =
		g_signal_new( I_("refresh-window") ,
			G_OBJECT_CLASS_TYPE(vclass) ,
			G_SIGNAL_RUN_LAST ,
			G_STRUCT_OFFSET(VteTerminalClass , refresh_window) ,
			NULL , NULL , g_cclosure_marshal_VOID__VOID ,
			G_TYPE_NONE , 0) ;

	vclass->restore_window_signal =
		g_signal_new( I_("restore-window") ,
			G_OBJECT_CLASS_TYPE(vclass) ,
			G_SIGNAL_RUN_LAST ,
			G_STRUCT_OFFSET(VteTerminalClass , restore_window) ,
			NULL , NULL , g_cclosure_marshal_VOID__VOID ,
			G_TYPE_NONE , 0) ;

	vclass->maximize_window_signal =
		g_signal_new( I_("maximize-window") ,
			G_OBJECT_CLASS_TYPE(vclass) ,
			G_SIGNAL_RUN_LAST ,
			G_STRUCT_OFFSET(VteTerminalClass , maximize_window) ,
			NULL , NULL , g_cclosure_marshal_VOID__VOID ,
			G_TYPE_NONE , 0) ;

	vclass->resize_window_signal =
		g_signal_new( I_("resize-window") ,
			G_OBJECT_CLASS_TYPE(vclass) ,
			G_SIGNAL_RUN_LAST ,
			G_STRUCT_OFFSET(VteTerminalClass , resize_window) ,
			NULL , NULL , _vte_marshal_VOID__UINT_UINT ,
			G_TYPE_NONE , 2 , G_TYPE_UINT , G_TYPE_UINT) ;

	vclass->move_window_signal =
		g_signal_new( I_("move-window") ,
			G_OBJECT_CLASS_TYPE(vclass) ,
			G_SIGNAL_RUN_LAST ,
			G_STRUCT_OFFSET(VteTerminalClass , move_window) ,
			NULL , NULL , _vte_marshal_VOID__UINT_UINT ,
			G_TYPE_NONE , 2 , G_TYPE_UINT , G_TYPE_UINT) ;

	vclass->status_line_changed_signal =
		g_signal_new( I_("status-line-changed") ,
			G_OBJECT_CLASS_TYPE(vclass) ,
			G_SIGNAL_RUN_LAST ,
			G_STRUCT_OFFSET(VteTerminalClass , status_line_changed) ,
			NULL , NULL , g_cclosure_marshal_VOID__VOID ,
			G_TYPE_NONE , 0) ;

	vclass->increase_font_size_signal =
		g_signal_new( I_("increase-font-size") ,
			G_OBJECT_CLASS_TYPE(vclass) ,
			G_SIGNAL_RUN_LAST ,
			G_STRUCT_OFFSET(VteTerminalClass , increase_font_size) ,
			NULL , NULL , g_cclosure_marshal_VOID__VOID ,
			G_TYPE_NONE , 0) ;

	vclass->decrease_font_size_signal =
		g_signal_new( I_("decrease-font-size") ,
			G_OBJECT_CLASS_TYPE(vclass) ,
			G_SIGNAL_RUN_LAST ,
			G_STRUCT_OFFSET(VteTerminalClass , decrease_font_size) ,
			NULL , NULL , g_cclosure_marshal_VOID__VOID ,
			G_TYPE_NONE , 0) ;

	vclass->text_modified_signal =
		g_signal_new( I_("text-modified") ,
			G_OBJECT_CLASS_TYPE(vclass) ,
			G_SIGNAL_RUN_LAST ,
			G_STRUCT_OFFSET(VteTerminalClass , text_modified) ,
			NULL , NULL , g_cclosure_marshal_VOID__VOID ,
			G_TYPE_NONE , 0) ;

	vclass->text_inserted_signal =
		g_signal_new( I_("text-inserted") ,
			G_OBJECT_CLASS_TYPE(vclass) ,
			G_SIGNAL_RUN_LAST ,
			G_STRUCT_OFFSET(VteTerminalClass , text_inserted) ,
			NULL , NULL , g_cclosure_marshal_VOID__VOID ,
			G_TYPE_NONE , 0) ;

	vclass->text_deleted_signal =
		g_signal_new( I_("text-deleted") ,
			G_OBJECT_CLASS_TYPE(vclass) ,
			G_SIGNAL_RUN_LAST ,
			G_STRUCT_OFFSET(VteTerminalClass , text_deleted) ,
			NULL , NULL , g_cclosure_marshal_VOID__VOID ,
			G_TYPE_NONE , 0) ;

	vclass->text_scrolled_signal =
		g_signal_new( I_("text-scrolled") ,
			G_OBJECT_CLASS_TYPE(vclass) ,
			G_SIGNAL_RUN_LAST ,
			G_STRUCT_OFFSET(VteTerminalClass , text_scrolled) ,
			NULL , NULL , g_cclosure_marshal_VOID__INT ,
			G_TYPE_NONE , 1 , G_TYPE_INT) ;

#ifdef  VTE_CHECK_VERSION
#if  VTE_CHECK_VERSION(0,24,0)
	gtk_widget_class_install_style_property( wclass ,
		g_param_spec_boxed( "inner-border" , NULL , NULL , GTK_TYPE_BORDER ,
			G_PARAM_READABLE | G_PARAM_STATIC_STRINGS)) ;

	gtk_rc_parse_string( "style \"vte-default-style\" {\n"
			"VteTerminal::inner-border = { 1, 1, 1, 1 }\n"
			"}\n"
			"class \"VteTerminal\" style : gtk \"vte-default-style\"\n") ;
#endif
#endif

#ifdef  VTE_CHECK_VERSION
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
#endif
}

static void
vte_terminal_init(
	VteTerminal *  terminal
	)
{
	ml_term_t *  term ;
	x_color_manager_t *  color_man ;
	x_font_manager_t *  font_man ;
	mkf_charset_t  usascii_font_cs ;
	int  usascii_font_cs_changable ;

	GTK_WIDGET_SET_FLAGS( terminal , GTK_CAN_FOCUS) ;

	terminal->pvt = calloc( 1 , sizeof(VteTerminalPrivate)) ;
	
	terminal->adjustment = GTK_ADJUSTMENT(gtk_adjustment_new( 0 , 0 ,
				main_config.rows , 1 , main_config.rows , main_config.rows)) ;
	g_object_ref_sink( terminal->adjustment) ;
	g_signal_connect_swapped( terminal->adjustment , "value-changed" ,
		G_CALLBACK(adjustment_value_changed) , terminal) ;
	terminal->pvt->adj_value_changed_by_myself = 0 ;

	term = ml_create_term( 80 /* main_config.cols */ , 25 /* main_config.rows */ ,
			main_config.tab_size , main_config.num_of_log_lines ,
			main_config.encoding , main_config.is_auto_encoding , 
			main_config.unicode_font_policy ,
			main_config.col_size_of_width_a , main_config.use_char_combining ,
			main_config.use_multi_col_char , main_config.use_bidi ,
			x_termcap_get_bool_field(
				x_termcap_get_entry( &termcap , main_config.term_type) , ML_BCE) ,
			main_config.use_dynamic_comb , main_config.bs_mode ,
			main_config.vertical_mode , main_config.iscii_lang_type) ;

	if( ! display_opened)
	{
		memset( &disp , 0 , sizeof(x_display_t)) ;
		disp.display = gdk_x11_display_get_xdisplay( gdk_display_get_default()) ;
		disp.screen = DefaultScreen(disp.display) ;
		disp.my_window = DefaultRootWindow(disp.display) ;
		disp.modmap.serial = 0 ;
		disp.modmap.map = XGetModifierMapping( disp.display) ;

		x_xim_display_opened( disp.display) ;
		x_picture_display_opened( disp.display) ;

		gdk_window_add_filter( NULL , vte_terminal_filter , &disp) ;

		display_opened = 1 ;
	}

	if( main_config.unicode_font_policy == NOT_USE_UNICODE_FONT ||
		main_config.iso88591_font_for_usascii)
	{
		usascii_font_cs = x_get_usascii_font_cs( ML_ISO8859_1) ;
		usascii_font_cs_changable = 0 ;
	}
	else if( main_config.unicode_font_policy == ONLY_USE_UNICODE_FONT)
	{
		usascii_font_cs = x_get_usascii_font_cs( ML_UTF8) ;
		usascii_font_cs_changable = 0 ;
	}
	else
	{
		usascii_font_cs = x_get_usascii_font_cs( ml_term_get_encoding( term)) ;
		usascii_font_cs_changable = 1 ;
	}

	font_man = x_font_manager_new( disp.display , /* main_config.type_engine */
		#ifdef  USE_TYPE_XFT
			TYPE_XFT ,
		#else
			TYPE_XCORE ,
		#endif
			main_config.font_present ,
			main_config.font_size , usascii_font_cs ,
			usascii_font_cs_changable , main_config.use_multi_col_char ,
			main_config.step_in_changing_font_size) ;
		
	color_man = x_color_manager_new( disp.display , DefaultScreen(disp.display) ,
			&color_config , main_config.fg_color , main_config.bg_color ,
			main_config.cursor_fg_color , main_config.cursor_bg_color) ;

	terminal->pvt->screen = x_screen_new( term , font_man , color_man ,
			x_termcap_get_entry( &termcap , main_config.term_type) ,
			main_config.brightness , main_config.contrast , main_config.gamma ,
			main_config.alpha , main_config.fade_ratio , &shortcut ,
			main_config.screen_width_ratio , main_config.screen_height_ratio ,
			main_config.mod_meta_key ,
			main_config.mod_meta_mode , main_config.bel_mode ,
			main_config.receive_string_via_ucs , main_config.pic_file_path ,
			main_config.use_transbg , main_config.use_vertical_cursor ,
			main_config.big5_buggy , main_config.conf_menu_path_1 ,
			main_config.conf_menu_path_2 , main_config.conf_menu_path_3 ,
			main_config.use_extended_scroll_shortcut ,
			main_config.borderless , main_config.line_space ,
			main_config.input_method) ;

	memset( &terminal->pvt->system_listener , 0 , sizeof(x_system_event_listener_t)) ;
	terminal->pvt->system_listener.self = terminal ;
	terminal->pvt->system_listener.pty_closed = pty_closed ;
	terminal->pvt->system_listener.font_config_updated = font_config_updated ;
	terminal->pvt->system_listener.color_config_updated = color_config_updated ;
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
	
	terminal->pvt->io = NULL ;

	terminal->window_title = vte_terminal_get_window_title( terminal) ;
	terminal->window_title = vte_terminal_get_window_title( terminal) ;

	reset_vte_size_member( terminal) ;
}


/* --- global functions --- */

GtkWidget*
vte_terminal_new()
{
	return  g_object_new( VTE_TYPE_TERMINAL , NULL) ;
}

pid_t
vte_terminal_fork_command(
	VteTerminal *  terminal ,
	const char *  command ,
	char **  argv ,
	char **  envv ,
	const char *  directory ,
	gboolean  lastlog ,
	gboolean  utmp ,
	gboolean  wtmp
	)
{
	ml_term_open_pty( terminal->pvt->screen->term , command , argv , envv ,
		gdk_display_get_name( gtk_widget_get_display( GTK_WIDGET(terminal)))) ;
	
	return  ml_term_get_child_pid( terminal->pvt->screen->term) ;
}

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
	char *  cmd_path ;
	char *  cmd_file ;
	char *  argv[2] ;

	if( ( cmd_path = getenv( "SHELL")) == NULL)
	{
		cmd_path = "/bin/sh" ;
	}

	cmd_file = kik_basename( cmd_path) ;

#if  0
	/* 2 = `-' and NULL */
	if( ( argv[0] = alloca( strlen( cmd_path) + 2)) == NULL)
	{
		return  -1 ;
	}

	sprintf( argv[0] , "-%s" , cmd_file) ;
#else
	argv[0] = cmd_file ;
#endif

	argv[1] = NULL ;

	ml_term_open_pty( terminal->pvt->screen->term , cmd_path , argv , envv ,
		gdk_display_get_name( gtk_widget_get_display( GTK_WIDGET(terminal)))) ;

	return  ml_term_get_child_pid( terminal->pvt->screen->term) ;
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

	if( ! terminal->pvt->screen->sel.sel_str || terminal->pvt->screen->sel.sel_len == 0 ||
		! (clipboard = gtk_clipboard_get( GDK_SELECTION_CLIPBOARD)))
	{
		return ;
	}

	/* UTF8 max len = 8 */
	len = terminal->pvt->screen->sel.sel_len * 8 ;
	if( ! ( buf = alloca( len)))
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

	write( 0 , buf , len) ;
}

void
vte_terminal_paste_clipboard(
	VteTerminal *  terminal
	)
{
	x_screen_set_config( terminal->pvt->screen , NULL , "paste" , NULL) ;
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
}

void
vte_terminal_select_all(
	VteTerminal *  terminal
	)
{
}

void
vte_terminal_select_none(
	VteTerminal *  terminal
	)
{
}

void
vte_terminal_set_size(
	VteTerminal *  terminal,
	glong  columns ,
	glong  rows
	)
{
#ifdef  __DEBUG
	kik_debug_printf( "set_size %d %d\n" , columns , rows) ;
#endif

	GTK_WIDGET(terminal)->requisition.width =
		x_col_width( terminal->pvt->screen) * columns + WINDOW_MARGIN * 2 ;
	GTK_WIDGET(terminal)->requisition.height =
		x_line_height( terminal->pvt->screen) * rows + WINDOW_MARGIN * 2 ;

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
	if( is_audible)
	{
		main_config.bel_mode = BEL_SOUND ;
	}
	else
	{
		main_config.bel_mode = BEL_VISUAL ;
	}
	
	x_screen_set_config( terminal->pvt->screen , NULL , "bel_mode" , "sound") ;
}

gboolean
vte_terminal_get_audible_bell(
	VteTerminal *  terminal
	)
{
	if( terminal->pvt->screen->bel_mode == BEL_SOUND)
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
	if( is_visible)
	{
		main_config.bel_mode = BEL_VISUAL ;
	}
	else
	{
		main_config.bel_mode = BEL_SOUND ;
	}

	x_screen_set_config( terminal->pvt->screen , NULL , "bel_mode" , "visual") ;
}

gboolean
vte_terminal_get_visible_bell(
	VteTerminal *  terminal
	)
{
	if( terminal->pvt->screen->bel_mode == BEL_VISUAL)
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
	/* gdk_color_to_string() was not supported. */
#if  (GTK_MAJOR_VERSION >= 2) && (GTK_MINOR_VERSION >= 12)
	gchar *  str ;

	if( ! foreground)
	{
		return ;
	}

	/* #rrrrggggbbbb */
	str = gdk_color_to_string( foreground) ;
	
#ifdef  __DEBUG
	kik_debug_printf( "set_color_foreground %s\n" , str) ;
#endif

	free( main_config.fg_color) ;
	main_config.fg_color = strdup( str) ;

	if( GTK_WIDGET_REALIZED(GTK_WIDGET(terminal)))
	{
		x_screen_set_config( terminal->pvt->screen , NULL , "fg_color" , str) ;
	}
	else
	{
		x_color_manager_set_fg_color( terminal->pvt->screen->color_man , str) ;
	}

	g_free( str) ;
#endif
}

void
vte_terminal_set_color_background(
	VteTerminal *  terminal ,
	const GdkColor *  background
	)
{
	/* gdk_color_to_string() was not supported. */
#if  (GTK_MAJOR_VERSION >= 2) && (GTK_MINOR_VERSION >= 12)
	gchar *  str ;

	if( ! background)
	{
		return ;
	}
	
	/* #rrrrggggbbbb */
	str = gdk_color_to_string( background) ;

#ifdef  __DEBUG
	kik_debug_printf( "set_color_background %s\n" , str) ;
#endif

	free( main_config.bg_color) ;
	main_config.bg_color = strdup( str) ;
	
	if( GTK_WIDGET_REALIZED(GTK_WIDGET(terminal)))
	{
		x_screen_set_config( terminal->pvt->screen , NULL , "bg_color" , str) ;
	}
	else
	{
		x_color_manager_set_bg_color( terminal->pvt->screen->color_man , str) ;
	}

	g_free( str) ;
#endif
}

void
vte_terminal_set_color_cursor(
	VteTerminal *  terminal ,
	const GdkColor *  cursor_background
	)
{
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
	GdkPixbuf *  image
	)
{
}

void
vte_terminal_set_background_image_file(
	VteTerminal *  terminal ,
	const char *  path
	)
{
	free( main_config.pic_file_path) ;
	main_config.pic_file_path = strdup( path) ;
	
	if( GTK_WIDGET_REALIZED(GTK_WIDGET(terminal)))
	{
		x_screen_set_config( terminal->pvt->screen , NULL , "wall_picture" , path) ;
	}
	else
	{
		terminal->pvt->screen->pic_file_path = main_config.pic_file_path ;
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
}

void
vte_terminal_set_background_transparent(
	VteTerminal *  terminal ,
	gboolean  transparent
	)
{
	char *  value ;
	
	if( transparent)
	{
		value = true ;
	}
	else
	{
		value = false ;
	}
	
	x_screen_set_config( terminal->pvt->screen , NULL , "use_transbg" , value) ;
}

void
vte_terminal_set_opacity(
	VteTerminal *  terminal ,
	guint16  opacity
	)
{
}

#ifdef  VTE_CHECK_VERSION
#if  VTE_CHECK_VERSION(0,17,1)
void
vte_terminal_set_cursor_blink_mode(
	VteTerminal *  terminal ,
	VteTerminalCursorBlinkMode  mode
	)
{
}

VteTerminalCursorBlinkMode
vte_terminal_get_cursor_blink_mode(
	VteTerminal *  terminal
	)
{
	return  VTE_CURSOR_BLINK_OFF ;
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

#ifdef  __DEBUG
	kik_debug_printf( "set_font %s\n" , name) ;
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
#ifdef  __DEBUG
	kik_debug_printf( "set_font_from_string %s\n" , name) ;
#endif

	if( x_customize_font_file( "aafont" , "DEFAULT" , name , 0))
	{
		x_font_cache_unload_all() ;
		reset_vte_size_member( terminal) ;
		
		if( GTK_WIDGET_REALIZED(GTK_WIDGET(terminal)))
		{
			x_screen_reset_view( terminal->pvt->screen) ;
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
	return  FALSE ;
}

void
vte_terminal_set_word_chars(
	VteTerminal *  terminal ,
	const char *  spec
	)
{
}

gboolean
vte_terminal_is_word_char(
	VteTerminal *  terminal ,
	gunichar  c
	)
{
	return  FALSE ;
}

void
vte_terminal_set_backspace_binding(
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
#ifdef  VTE_CHECK_VERSION
#if  VTE_CHECK_VERSION(0,20,4)
	else if( binding == VTE_ERASE_TTY)
	{
		return ;
	}
#endif
#endif
	else
	{
		return ;
	}
	
	entry = x_termcap_get_entry( &termcap , TERM_TYPE) ;
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
#ifdef  VTE_CHECK_VERSION
#if  VTE_CHECK_VERSION(0,20,4)
	else if( binding == VTE_ERASE_TTY)
	{
		return ;
	}
#endif
#endif
	else
	{
		return ;
	}
	
	entry = x_termcap_get_entry( &termcap , TERM_TYPE) ;
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
	return  "" ;
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
	return  "" ;
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
	return  "" ;
}

void
vte_terminal_get_cursor_position(
	VteTerminal *  terminal ,
	glong *  column ,
	glong *  row
	)
{
	*column = ml_term_cursor_col( terminal->pvt->screen->term) ;
	*row = ml_term_cursor_row( terminal->pvt->screen->term) ;
}

void
vte_terminal_match_clear_all(
	VteTerminal *  terminal
	)
{
}

/* GRegex was not supported */
#if  (GLIB_MAJOR_VERSION >= 2) && (GLIB_MINOR_VERSION >= 14)
int
vte_terminal_match_add_gregex(
	VteTerminal *  terminal ,
	GRegex *  regex ,
	GRegexMatchFlags  flags
	)
{
	return  1 ;
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
	return  "" ;
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
	return  "" ;
}

const char *
vte_terminal_get_default_emulation(
	VteTerminal *  terminal
	)
{
	return  "" ;
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
		ml_term_change_encoding( terminal->pvt->screen->term ,
			ml_get_char_encoding( codeset)) ;
	}
}

const char *
vte_terminal_get_encoding(
	VteTerminal *  terminal
	)
{
	return  ml_get_char_encoding_name( ml_term_get_encoding( terminal->pvt->screen->term)) ;
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
	return  ml_term_get_pty_fd( terminal->pvt->screen->term) ;
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
	return  "mlterm" ;
}

const char *
vte_terminal_get_icon_title(
	VteTerminal *  terminal
	)
{
	return  "mlterm" ;
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

void
vte_terminal_set_font_full(
	VteTerminal *  terminal ,
	const PangoFontDescription *  font_desc ,
	VteTerminalAntiAlias  antialias
	)
{
	char *  value ;
	
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
		goto  set_font ;
	}
	
	x_screen_set_config( terminal->pvt->screen , NULL , "use_anti_alias" , value) ;

set_font:
	vte_terminal_set_font( terminal , font_desc) ;
}

void
vte_terminal_set_font_from_string_full(
	VteTerminal *  terminal ,
	const char *  name ,
	VteTerminalAntiAlias  antialias
	)
{
	char *  value ;
	
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
		goto  set_font ;
	}
	
	x_screen_set_config( terminal->pvt->screen , NULL , "use_anti_alias" , value) ;

set_font:
	vte_terminal_set_font_from_string( terminal , name) ;
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
x_term_manager_add_fd(
	int  fd ,
	void (*handler)(void)
	)
{
	if( gio)
	{
		kik_debug_printf( " x_term_manager_add_fd failed\n") ;
		
		return  0 ;
	}

	gio = g_io_channel_unix_new( fd) ;
	src_id = g_io_add_watch( gio , G_IO_IN , (GIOFunc)handler , NULL) ;

	return  1 ;
}

int
x_term_manager_remove_fd(
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
