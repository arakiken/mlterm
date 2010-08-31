/*
 *	$Id: ccheader,v 1.2 2001/12/01 23:37:26 ken Exp $
 */

#include "mlvte.h"

#include <unistd.h>	/* setsid etc */
#include <stdlib.h>	/* malloc */
#include <sys/wait.h>
#include <X11/keysym.h>
#include <gdk/gdkx.h>
#include <gtk/gtksignal.h>


#if  1
#define  USE_XEMBED
#endif


/* XEMBED messages */
#define XEMBED_EMBEDDED_NOTIFY          0
#define XEMBED_WINDOW_ACTIVATE          1
#define XEMBED_WINDOW_DEACTIVATE        2
#define XEMBED_REQUEST_FOCUS            3
#define XEMBED_FOCUS_IN                 4
#define XEMBED_FOCUS_OUT                5
#define XEMBED_FOCUS_NEXT               6
#define XEMBED_FOCUS_PREV               7
/* 8-9 were used for XEMBED_GRAB_KEY/XEMBED_UNGRAB_KEY */
#define XEMBED_MODALITY_ON              10
#define XEMBED_MODALITY_OFF             11
#define XEMBED_REGISTER_ACCELERATOR     12
#define XEMBED_UNREGISTER_ACCELERATOR   13
#define XEMBED_ACTIVATE_ACCELERATOR     14

/* Details for  XEMBED_FOCUS_IN: */
#define XEMBED_FOCUS_CURRENT            0
#define XEMBED_FOCUS_FIRST              1
#define XEMBED_FOCUS_LAST               2


#ifdef  USE_XEMBED
static Atom xembedatom ;
#endif


typedef struct mlterm
{
	Window  win ;

} mlterm_t ;


/* --- static functions --- */

#ifdef  USE_XEMBED
static void
send_xembed(
	Display *  disp ,
	Window  win ,
	long  msg ,
	long  d1 ,
	long  d2 ,
	long  d3
	)
{
	XEvent e ;

	e.xclient.type = ClientMessage ;
	e.xclient.serial = 0 ;
	e.xclient.send_event = 0 ;
	e.xclient.display = 0 ;
	e.xclient.window = win ;
	e.xclient.message_type = xembedatom ;
	e.xclient.format = 32 ;
	e.xclient.data.l[0] = CurrentTime ;
	e.xclient.data.l[1] = msg ;
	e.xclient.data.l[2] = d1 ;
	e.xclient.data.l[3] = d2 ;
	e.xclient.data.l[4] = d3 ;

	XSendEvent( disp , win , False , NoEventMask , &e) ;
}
#endif

static void
sig_int(
	int  sig
	)
{
	char *  argv[2] ;

	argv[0] = "mlclient" ;
	argv[1] = "--kill" ;
	argv[2] = NULL ;
	
	execvp( argv[0] , argv) ;
}

static void
focus_in(
	Display *  disp ,
	Window  win
	)
{
	XSetInputFocus( disp , win , RevertToParent , CurrentTime) ;

#ifdef  USE_XEMBED
	send_xembed( disp , win , XEMBED_FOCUS_IN, XEMBED_FOCUS_CURRENT, 0, 0) ;
	send_xembed( disp , win , XEMBED_WINDOW_ACTIVATE, 0, 0, 0) ;
#endif
}

static Bool
check_createnotify(
	Display *  disp ,
	XEvent *  ev ,
	XPointer  p	/* Mlvte */
	)
{
	GtkWidget *  widget ;

	widget = (GtkWidget*)p ;
	
	if( ev->type == CreateNotify)
	{
	#ifdef  USE_XEMBED		
		MLVTE(widget)->mlterm->win = ev->xcreatewindow.window ;

		send_xembed( disp , DefaultRootWindow( disp) , XEMBED_EMBEDDED_NOTIFY , 0 ,
			MLVTE(widget)->mlterm->win , 0) ;
			
		XSync( disp , False) ;
	#endif

	#if  1
		printf( "Child window %d(parent %d) is created.\n" ,
			MLVTE(widget)->mlterm->win , ev->xcreatewindow.parent) ;
		fflush( NULL) ;
	#endif
	}
	else if( ev->type == MapNotify && ev->xmap.window == MLVTE(widget)->mlterm->win)
	{
		return  True ;
	}
	
	return  False ;
}

static void
reset_size(
	GtkWidget *  widget	/* Mlvte */
	)
{
	Display *  disp ;
	
	XSizeHints  hints ;
	u_long  supplied ;

	Window  root ;
	int  x ;
	int  y ;
	u_int  width ;
	u_int  height ;
	u_int  border_width ;
	u_int  depth ;

	disp = gdk_x11_drawable_get_xdisplay(widget->window) ;
	
	if( XGetWMNormalHints( disp , MLVTE(widget)->mlterm->win , &hints , &supplied))
	{
		GtkWidget *  window ;
		GdkGeometry   geom ;

	#if  1	
		printf( "Width inc %d  Height inc %d\n" , hints.width_inc , hints.height_inc) ;
		fflush( NULL) ;
	#endif
	
		geom.width_inc = hints.width_inc ;
		geom.height_inc = hints.height_inc ;
		window = gtk_widget_get_toplevel(widget) ;
		gtk_window_set_geometry_hints( GTK_WINDOW(window) , window ,  &geom ,
			GDK_HINT_RESIZE_INC) ;
	}
	
	if( XGetGeometry( disp , MLVTE(widget)->mlterm->win , &root , &x , &y ,
		&width , &height , &border_width , &depth))
	{
	#if  1
		printf( "Width %d  Height %d\n" , width , height) ;
		fflush( NULL) ;
	#endif
	
		gtk_widget_set_size_request( widget , width , height) ;
	}
}

/*
 * GtkWidgetClass::destroy_event is not invoked when child window is destroyed.
 * (why?)
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

	if( ((XEvent*)xevent)->xdestroywindow.window == MLVTE(widget)->mlterm->win)
	{
		if( ((XEvent*)xevent)->type == DestroyNotify)
		{
		#if  1
			printf( "Child window %d is destroyed\n" , MLVTE(widget)->mlterm->win) ;
			fflush( NULL) ;
		#endif

			MLVTE(widget)->mlterm->win = 0 ;

			gdk_window_destroy( widget->window) ;
			gtk_widget_destroy( widget) ;

			return  GDK_FILTER_REMOVE ;
		}
		else if( ((XEvent*)xevent)->type == ConfigureNotify)
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

static gboolean
mlvte_delete_event(
	GtkWidget *  widget ,
	GdkEvent *  event ,
	gpointer  data
	)
{
	XEvent  xev ;

	widget = data ;

	xev.xclient.type = ClientMessage ;
	xev.xclient.serial = 0 ;
	xev.xclient.send_event = False ;
	xev.xclient.display = gdk_x11_drawable_get_xdisplay(widget->window) ;
	xev.xclient.window = MLVTE(widget)->mlterm->win ;
	xev.xclient.message_type = 0 ;
	xev.xclient.format = 32 ;
	xev.xclient.data.l[0] = XInternAtom( xev.xclient.display , "WM_DELETE_WINDOW" , False) ;

	/* Detach mlterm window. Otherwise mlterm server will die. */
	XUnmapWindow( xev.xclient.display , MLVTE(widget)->mlterm->win) ;
	XReparentWindow( xev.xclient.display , MLVTE(widget)->mlterm->win ,
		DefaultRootWindow( xev.xclient.display) , 0 , 0) ;
		
	XSendEvent( xev.xclient.display , MLVTE(widget)->mlterm->win , False , None , &xev) ;

#if  1
	printf( "DELETE_EVENT => child %d\n" , MLVTE(widget)->mlterm->win) ;
	fflush( NULL) ;
#endif

	return  FALSE ;
}

static void
mlvte_realize(
	GtkWidget *  widget
	)
{
	pid_t  pid ;
#if  1
	int  status ;
#endif
	GdkWindowAttr  attr ;
	Display *  disp ;
	Window  win ;
	XEvent  ev ;
	
	if( MLVTE(widget)->mlterm->win)
	{
		return ;
	}

	GTK_WIDGET_SET_FLAGS( widget , GTK_REALIZED) ;

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
	widget->style = gtk_style_attach( widget->style , widget->window) ;
	gdk_window_add_filter( widget->window , mlvte_filter , widget) ;
	
	disp = gdk_x11_drawable_get_xdisplay( widget->window) ;
	win = gdk_x11_drawable_get_xid( widget->window) ;

	/* Ensure widget->window exits, otherwise child process can fail. */
	XFlush( disp) ;

#ifdef  USE_XEMBED
	xembedatom = XInternAtom(disp, "_XEMBED", False) ;
#endif

	if( ( pid = fork()) == 0)
	{
		/* Child */
		char  wid[1024] ;
		char *  argv[3] ;

		sprintf( wid , "%d" , win) ;
		
		argv[0] = "mlclient" ;
		argv[1] = "--parent" ;
		argv[2] = wid ;
		argv[3] = NULL ;
		
		setsid() ;
		execvp( argv[0] , argv) ;
	}

#if  1
	/* Parent */
	waitpid( pid , &status , 0) ;

	if( WIFEXITED(status) && WEXITSTATUS(status) != 0)
	{
		printf( "Start mlterm with --parent %d option.\n" , win) ;
		fflush( NULL) ;
	}
#endif

	XIfEvent( disp , &ev , check_createnotify , widget) ;
	
	reset_size( widget) ;

	g_signal_connect( gtk_widget_get_toplevel( widget) , "delete-event" ,
		G_CALLBACK(mlvte_delete_event) , widget) ;
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
	
		focus_in( gdk_x11_drawable_get_xdisplay( widget->window) , MLVTE( widget)->mlterm->win) ;
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
		MLVTE(widget)->mlterm->win , allocation->width , allocation->height) ;
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

	signal( SIGINT , sig_int) ;
}

static void
mlvte_init(
	Mlvte *  mlvte
	)
{
	mlvte->mlterm = malloc( sizeof(mlterm_t)) ;
	mlvte->mlterm->win = None ;
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

	return mlvte_type ;
}
