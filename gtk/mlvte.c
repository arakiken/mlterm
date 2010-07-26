/*
 *	$Id: ccheader,v 1.2 2001/12/01 23:37:26 ken Exp $
 */

#include "mlvte.h"

#include <unistd.h>	/* setsid etc */
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


/* --- static functions --- */

#ifdef  USE_XEMBED
static void
sendxembed(
	Display *  disp ,
	Window  win ,
	long msg ,
	long detail,
	long d1 ,
	long d2
	)
{
	XEvent e = { 0 };

	e.xclient.window = win ;
	e.xclient.type = ClientMessage ;
	e.xclient.message_type = xembedatom ;
	e.xclient.format = 32 ;
	e.xclient.data.l[0] = CurrentTime ;
	e.xclient.data.l[1] = msg ;
	e.xclient.data.l[2] = detail ;
	e.xclient.data.l[3] = d1 ;
	e.xclient.data.l[4] = d2 ;

	XSendEvent( disp , win , False , NoEventMask , &e) ;
}
#endif

static void
focus_in(
	Display *  disp ,
	Window  win
	)
{
	XSetInputFocus( disp , win , RevertToParent , CurrentTime) ;

#ifdef  USE_XEMBED
	sendxembed( disp , win , XEMBED_FOCUS_IN, XEMBED_FOCUS_CURRENT, 0, 0) ;
	sendxembed( disp , win , XEMBED_WINDOW_ACTIVATE, 0, 0, 0) ;
#endif
}

static Bool
check_createnotify(
	Display *  disp ,
	XEvent *  ev ,
	XPointer  p
	)
{
	Mlvte *  mlvte ;

	mlvte = (Mlvte*)p ;
	
	if( ev->type == CreateNotify)
	{
	#ifdef  USE_XEMBED
		XEvent e ;
		
		mlvte->child = ev->xcreatewindow.window ;

		e.xclient.window = mlvte->child ;
		e.xclient.type = ClientMessage ;
		e.xclient.message_type = xembedatom ;
		e.xclient.format = 32 ;
		e.xclient.data.l[0] = CurrentTime ;
		e.xclient.data.l[1] = XEMBED_EMBEDDED_NOTIFY ;
		e.xclient.data.l[2] = 0 ;
		e.xclient.data.l[3] = mlvte->child ;
		e.xclient.data.l[4] = 0 ;
		XSendEvent( disp , DefaultRootWindow( disp) , False , NoEventMask , &e) ;

		XSync( disp , False) ;
	#endif

	#if  1
		printf( "Child window %d(parent %d) is created.\n" ,
			mlvte->child , ev->xcreatewindow.parent) ;
		fflush( NULL) ;
	#endif
	}
	else if( ev->type == MapNotify && ev->xmap.window == mlvte->child)
	{
		return  True ;
	}
	
	return  False ;
}

/*
 * GtkWidgetClass::destroy_event is not invoked when child window is destroyed.
 * (why?)
 */
static GdkFilterReturn
mlvte_filter(
	GdkXEvent *  xevent ,
	GdkEvent *  event ,
	gpointer  data
	)
{
	Mlvte *  mlvte ;

	mlvte = (Mlvte*)data ;

	if( ((XEvent*)xevent)->xdestroywindow.window == mlvte->child)
	{
		if( ((XEvent*)xevent)->type == DestroyNotify)
		{
		#if  1
			printf( "Child window %d is destroyed\n" , mlvte->child) ;
			fflush( NULL) ;
		#endif

			mlvte->child = 0 ;

			gtk_widget_destroy( GTK_WIDGET(mlvte)) ;

			return  GDK_FILTER_REMOVE ;
		}
		else if( ((XEvent*)xevent)->type == ConfigureNotify)
		{
			gtk_widget_queue_resize( GTK_WIDGET(mlvte)) ;

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
	xev.xclient.window = MLVTE(widget)->child ;
	xev.xclient.message_type = 0 ;
	xev.xclient.format = 32 ;
	xev.xclient.data.l[0] = XInternAtom( xev.xclient.display , "WM_DELETE_WINDOW" , False) ;

	/* Detach mlterm window. Otherwise mlterm server will die. */
	XUnmapWindow( xev.xclient.display , MLVTE(widget)->child) ;
	XReparentWindow( xev.xclient.display , MLVTE(widget)->child ,
		DefaultRootWindow( xev.xclient.display) , 0 , 0) ;
		
	XSendEvent( xev.xclient.display , MLVTE(widget)->child , False , None , &xev) ;

#if  1
	printf( "DELETE_EVENT => child %d\n" , MLVTE(widget)->child) ;
	fflush( NULL) ;
#endif

	return  FALSE ;
}

static void
mlvte_realize(
	GtkWidget *  widget
	)
{
#if  0
	int  status ;
#endif
	GdkWindowAttr  attr ;
	Display *  disp ;
	Window  win ;
	XEvent  ev ;
	
	if( MLVTE(widget)->child)
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
	gdk_window_add_filter( widget->window , mlvte_filter , MLVTE(widget)) ;
	
	disp = gdk_x11_drawable_get_xdisplay( widget->window) ;
	win = gdk_x11_drawable_get_xid( widget->window) ;

#ifdef  USE_XEMBED
	xembedatom = XInternAtom(disp, "_XEMBED", False) ;
#endif

	if( fork() == 0)
	{
		/* Child */
		char  wid[1024] ;
		char *  argv[3] ;

		sprintf( wid , "%d" , win) ;
		
		argv[0] = "mlclient" ;
		argv[1] = "-parent" ;
		argv[2] = wid ;
		argv[3] = NULL ;
		
		setsid() ;
		execvp( argv[0] , argv) ;
	}

#if  0
	/* Parent */
	wait( &status) ;

	if( status)
	{
		printf( "Start mlterm with --parent %d option.\n" , win) ;
		fflush( NULL) ;
	}
	else
#endif
	{
		XIfEvent( disp , &ev , check_createnotify , widget) ;
	}

#if  0
	{
		XSizeHints  hints ;
		u_long  supplied ;
		if( XGetWMNormalHints( disp , MLVTE(widget)->child , &hints , &supplied))
		{
			printf( "%d %d\n" , hints.width_inc , hints.height_inc) ;
		}

		XSetWMNormalHints( disp ,
			gdk_x11_drawable_get_xid( gtk_widget_get_toplevel(widget)->window) ,
			&hints) ;
	}
#endif

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
	if( MLVTE(widget)->child)
	{
	#if  1
		printf( "focus in\n") ;
		fflush( NULL) ;
	#endif
	
		focus_in( gdk_x11_drawable_get_xdisplay( widget->window) , MLVTE( widget)->child) ;
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
		MLVTE(widget)->child , allocation->width , allocation->height) ;
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
}

static void
mlvte_init(
	Mlvte *  mlvte
	)
{
	mlvte->child = 0 ;
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
