/*
 *	$Id: ccheader,v 1.2 2001/12/01 23:37:26 ken Exp $
 */

#include "mlvte.h"

#include <X11/keysym.h>
#include <gdk/gdkx.h>
#include <gtk/gtksignal.h>
#include <x_screen.h>
#include <ml_term_manager.h>
#include <kiklib/kik_sig_child.h>


typedef struct mlterm
{
	x_window_t *  win ;
	
	x_display_t  xdisp ;
	x_system_event_listener_t  system_listener ;

	GIOChannel *  io ;

} mlterm_t ;


/* --- static variables --- */

static x_color_config_t  color_config ;
static x_shortcut_t  shortcut ;
static x_termcap_t  termcap ;
static int  timeout_registered ;


/* --- static functions --- */

static GdkFilterReturn mlvte_filter( GdkXEvent *  xevent , GdkEvent *  event , gpointer  data) ;

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
		x_screen_detach( screen) ;
		x_font_manager_delete( screen->font_man) ;
		x_color_manager_delete( screen->color_man) ;
		
		x_window_unmap( MLVTE(widget)->mlterm->win) ;
		x_window_final( MLVTE(widget)->mlterm->win) ;

		g_io_channel_close( MLVTE(widget)->mlterm->io) ;

		gdk_window_remove_filter( NULL , mlvte_filter , widget) ;
		gdk_window_destroy( widget->window) ;
		gtk_widget_destroy( widget) ;
		
		printf( "pty_closed\n") ;
		fflush( NULL) ;
	}
	else
	{
		x_screen_attach( screen , term) ;
	}
}

static void
reset_size(
	GtkWidget *  widget	/* Mlvte */
	)
{
	GdkGeometry  geom ;
	GtkWidget *  window ;

	geom.width_inc = MLVTE(widget)->mlterm->win->width_inc ;
	geom.height_inc = MLVTE(widget)->mlterm->win->height_inc ;
	geom.min_width = MLVTE(widget)->mlterm->win->min_width ;
	geom.min_height = MLVTE(widget)->mlterm->win->min_height ;

	window = gtk_widget_get_toplevel(widget) ;
	gtk_window_set_geometry_hints( GTK_WINDOW(window) , window ,  &geom ,
		GDK_HINT_RESIZE_INC /* | GDK_HINT_MIN_SIZE */) ;

#if  0
	printf( "%d %d\n" , ACTUAL_WIDTH(MLVTE(widget)->mlterm->win) ,
		ACTUAL_HEIGHT(MLVTE(widget)->mlterm->win)) ;
	fflush( NULL) ;
#endif
	gtk_widget_set_size_request( widget ,
		ACTUAL_WIDTH(MLVTE(widget)->mlterm->win) ,
		ACTUAL_HEIGHT(MLVTE(widget)->mlterm->win)) ;
}

static gboolean
root_delete_event(
	GtkWidget *  widget ,
	GdkEvent *  event ,
	gpointer  data
	)
{
	widget = data ;

	(MLVTE(widget)->mlterm->win->window_deleted)( MLVTE(widget)->mlterm->win) ;

	return  FALSE ;
}

static gboolean
mlvte_timeout(
	gpointer  data
	)
{
	ml_close_dead_terms() ;

	return  TRUE ;
}

static gboolean
mlvte_io(
	GIOChannel *  source ,
	GIOCondition  conditon ,
	gpointer  data
	)
{
	GtkWidget *  widget ;

	widget = data ;

	ml_term_parse_vt100_sequence( ((x_screen_t*)MLVTE(widget)->mlterm->win)->term) ;

	ml_close_dead_terms() ;
	
	return  TRUE ;
}

/*
 * Don't call ml_close_dead_terms() before returning GDK_FILTER_CONTINUE,
 * because ml_close_dead_terms() will destroy widget in pty_closed and
 * destroyed widget can be touched right after this function.
 */
static GdkFilterReturn
mlvte_filter(
	GdkXEvent *  xevent ,
	GdkEvent *  event ,
	gpointer  data		/* Mlvte */
	)
{
	GtkWidget *  widget ;

	widget = data ;

	if( ! MLVTE(widget)->mlterm->win)
	{
		return  GDK_FILTER_CONTINUE ;
	}
	
	if( x_window_receive_event( MLVTE(widget)->mlterm->win , (XEvent*)xevent))
	{
		return  GDK_FILTER_REMOVE ;
	}
	else if( ((XEvent*)xevent)->xconfigure.window == MLVTE(widget)->mlterm->win->my_window)
	{
		if( ((XEvent*)xevent)->type == ConfigureNotify)
		{
		#if  0
			gtk_widget_queue_resize( widget) ;
		#else
			reset_size( widget) ;
		#endif
		
			return  GDK_FILTER_REMOVE ;
		}
	}

	return  GDK_FILTER_CONTINUE ;
}

static void
mlvte_realize(
	GtkWidget *  widget
	)
{
	GdkWindowAttr  attr ;
	Display *  disp ;
	Window  win ;
	
	if( MLVTE(widget)->mlterm->win)
	{
		return ;
	}

	GTK_WIDGET_SET_FLAGS( widget , GTK_REALIZED) ;

	attr.window_type = GDK_WINDOW_CHILD ;
	attr.x = widget->allocation.x ;
	attr.y = widget->allocation.y ;
	attr.width = widget->allocation.width ;
	attr.width = 1000 ;
	attr.height = widget->allocation.height ;
	attr.wclass = GDK_INPUT_OUTPUT ;
	attr.visual = gtk_widget_get_visual( widget) ;
	attr.colormap = gtk_widget_get_colormap( widget) ;
	attr.event_mask = gtk_widget_get_events( widget) |
				GDK_FOCUS_CHANGE_MASK |
				GDK_SUBSTRUCTURE_MASK ;		/* DestroyNotify from child */

	widget->window = gdk_window_new( gtk_widget_get_parent_window( widget) , &attr ,
				GDK_WA_X | GDK_WA_Y | GDK_WA_VISUAL | GDK_WA_COLORMAP) ;
	gdk_window_add_filter( NULL , mlvte_filter , widget) ;
	gdk_window_set_user_data( widget->window , widget) ;
	widget->style = gtk_style_attach( widget->style , widget->window) ;
	
	disp = gdk_x11_drawable_get_xdisplay( widget->window) ;
	win = gdk_x11_drawable_get_xid( widget->window) ;

	{
		ml_term_t *  term ;
		x_screen_t *  screen ;
		x_color_manager_t *  color_man ;
		x_font_manager_t *  font_man ;

		term = ml_create_term( 80 , 40 , 8 , 100 , ML_EUCJP , 0 , 0 ,
				2 , 1 , 1 , 1 , 1 , 0 , 0 , 0 , 0) ;
		
		{
			char *  cmd_path ;
			char *  argv[2] ;
			char *  env[2] = { NULL , } ;

			if( ( cmd_path = getenv( "SHELL")) == NULL)
			{
				cmd_path = "/bin/sh" ;
			}

			argv[0] = cmd_path ;
			argv[1] = NULL ;
			
			ml_term_open_pty( term , cmd_path , argv , env , "localhost") ;
		}
		
		font_man = x_font_manager_new( disp , TYPE_XCORE , 0 , 12 , ISO8859_1_R ,
				0 , 1 , 1) ;
		color_man = x_color_manager_new( disp , DefaultScreen(disp) ,
				&color_config , "black" , "white" , NULL , NULL) ;
		screen = x_screen_new( term , font_man , color_man ,
				x_termcap_get_entry( &termcap , "xterm") ,
				100 , 100 , 100 , 255 , 100 , &shortcut , 100 , 100 , NULL ,
				MOD_META_NONE , BEL_VISUAL , 0 , NULL , 0 , 0 , 0 , NULL , NULL ,
				NULL , 1 , 0 , 0 , "none") ;

		memset( &(MLVTE(widget)->mlterm->system_listener) , 0 ,
			sizeof(x_system_event_listener_t)) ;
		MLVTE(widget)->mlterm->system_listener.self = widget ;
		MLVTE(widget)->mlterm->system_listener.pty_closed = pty_closed ;
		x_set_system_listener( screen , &(MLVTE(widget)->mlterm->system_listener)) ;
		
		memset( &(MLVTE(widget)->mlterm->xdisp) , 0 , sizeof(x_display_t)) ;
		MLVTE(widget)->mlterm->xdisp.display = disp ;
		MLVTE(widget)->mlterm->xdisp.screen = DefaultScreen(disp) ;
		MLVTE(widget)->mlterm->xdisp.my_window = DefaultRootWindow(disp) ;

		screen->window.create_gc = 1 ;
		screen->window.disp = &(MLVTE(widget)->mlterm->xdisp) ;
		screen->window.parent_window = win ;
		x_window_show( &screen->window , 0) ;

		MLVTE(widget)->mlterm->win = &screen->window ;
		
		MLVTE(widget)->mlterm->io = g_io_channel_unix_new( ml_term_get_pty_fd( term)) ;
		g_io_add_watch( MLVTE(widget)->mlterm->io , G_IO_IN , mlvte_io , widget) ;

		if( ! timeout_registered)
		{
			/* 100 miliseconds */
			g_timeout_add( 100 , mlvte_timeout , widget) ;
			timeout_registered = 1 ;
		}
	}

	reset_size( widget) ;

	g_signal_connect( gtk_widget_get_toplevel( widget) , "delete-event" ,
		G_CALLBACK(root_delete_event) , widget) ;
}

static void
mlvte_map(
	GtkWidget *  widget
	)
{
	GTK_WIDGET_SET_FLAGS( widget , GTK_MAPPED) ;
	gdk_window_show( widget->window) ;
#if  0
	GTK_WIDGET_SET_FLAGS( widget , GTK_CAN_FOCUS) ;
	gtk_widget_grab_focus( widget) ;
#endif
}

static gboolean
mlvte_focus_in_event(
	GtkWidget *  widget ,
	GdkEventFocus *  event
	)
{
	if( ( GTK_WIDGET_FLAGS(widget) & GTK_MAPPED) && MLVTE(widget)->mlterm->win)
	{
	#if  1
		printf( "focus in\n") ;
		fflush( NULL) ;
	#endif

		XSetInputFocus( gdk_x11_drawable_get_xdisplay( widget->window) ,
			MLVTE( widget)->mlterm->win->my_window ,
			RevertToParent , CurrentTime) ;
	}

	return  FALSE ;
}

static void
mlvte_size_allocate(
	GtkWidget *  widget ,
	GtkAllocation *  allocation
	)
{
	widget->allocation = *allocation;

	if( GTK_WIDGET_REALIZED(widget) && ! GTK_WIDGET_NO_WINDOW(widget))
	{
		gdk_window_move_resize( widget->window,
					allocation->x, allocation->y,
					allocation->width, allocation->height);
	}

	XResizeWindow( gdk_x11_drawable_get_xdisplay( widget->window) ,
		MLVTE(widget)->mlterm->win->my_window , allocation->width , allocation->height) ;
}

static void
mlvte_class_init(
	MlvteClass *  mclass
	)
{
	GtkWidgetClass *  kclass ;

	kclass = (GtkWidgetClass*) mclass ;

	kclass->realize = mlvte_realize ;
	kclass->map = mlvte_map ;
	kclass->focus_in_event = mlvte_focus_in_event ;
	kclass->size_allocate = mlvte_size_allocate ;

	kik_sig_child_init() ;
	ml_term_manager_init( 1) ;
	x_color_config_init( &color_config) ;
	x_shortcut_init( &shortcut) ;
	x_termcap_init( &termcap) ;
}

static void
mlvte_init(
	Mlvte *  mlvte
	)
{
	mlvte->mlterm = malloc( sizeof(mlterm_t)) ;
	mlvte->mlterm->win = NULL ;
}

GtkWidget*
mlvte_new()
{
	return GTK_WIDGET(g_object_new(mlvte_get_type(), NULL));
}

GType
mlvte_get_type(void)
{
	static GType mlvte_type = 0;

	if( ! mlvte_type)
	{
		static const GTypeInfo mlvte_info =
		{
			sizeof (MlvteClass),
			NULL, /* base_init */
			NULL, /* base_finalize */
			(GClassInitFunc) mlvte_class_init , /* class_init */
			NULL, /* class_finalize */
			NULL, /* class_data */
			sizeof (Mlvte),
			0,
			(GInstanceInitFunc) mlvte_init,
		} ;

		mlvte_type = g_type_register_static(GTK_TYPE_WIDGET, "Mlvte", &mlvte_info, 0);
	}

	return  mlvte_type ;
}
