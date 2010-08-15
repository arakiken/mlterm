/*
 *	$Id: ccheader,v 1.2 2001/12/01 23:37:26 ken Exp $
 */

#include <stdlib.h>	/* malloc/realloc */
#include <string.h>	/* memset */
#include <gdk/gdkx.h>
#include <gtk/gtk.h>
#include "mlvte.h"


/* --- static variables --- */

static u_int num_of_windows = 0 ;


/* --- static functions --- */

static gboolean
main_destroy(
	GtkWidget *  widget ,
	gpointer  data
	)
{
	if( -- num_of_windows == 0)
	{
		gtk_main_quit() ;
	}

	return  FALSE ;
}

static gboolean
main_focus_in_event(
	GtkWidget *  widget ,
	GdkEventFocus *  event ,
	gpointer  data
	)
{
	GtkWidget *  notebook ;
	gint  nth ;

	notebook = data ;

	if( ( nth = gtk_notebook_get_current_page( GTK_NOTEBOOK(notebook))) != -1)
	{
		nth = 0 ;
	}

	g_signal_emit_by_name(
		gtk_notebook_get_nth_page( GTK_NOTEBOOK(notebook) , nth) ,
		"focus-in-event" , 0 , event) ;

	return  FALSE ;
}


static gboolean
mlvte_destroy(
	GtkWidget *  widget ,
	gpointer  data
	)
{
	GtkWidget *  notebook ;

	notebook = data ;

	if( gtk_notebook_get_n_pages( GTK_NOTEBOOK(notebook)) == 0)
	{
		gtk_widget_destroy( gtk_widget_get_toplevel( notebook)) ;

	#if  1	
		printf( "destroy toplevel window\n") ;
		fflush( NULL) ;
	#endif
	}

	return  FALSE ;
}


static GtkWidget *  get_menubar_menu( GtkWidget *  notebook) ;

static void
new_window(
	GtkWidget *  widget ,
	gpointer  data
	)
{
	GtkWidget *  window ;
	GtkWidget *  vbox ;
	GtkWidget *  menu ;
	GtkWidget *  mlvte ;
#if  1
	GtkWidget *  notebook ;
#else
	GtkWidget *  hbox ;
#endif

	window = gtk_window_new( GTK_WINDOW_TOPLEVEL) ;
	g_signal_connect( G_OBJECT(window) , "destroy" , G_CALLBACK(main_destroy) , NULL) ;
	gtk_widget_show(window) ;

	vbox = gtk_vbox_new( FALSE , 1) ;
	gtk_container_border_width( GTK_CONTAINER(vbox) , 1) ;
	gtk_container_add( GTK_CONTAINER(window) , vbox) ;
	gtk_widget_show( vbox) ;

	notebook = gtk_notebook_new() ;

	g_signal_connect( G_OBJECT(window) , "focus-in-event" , G_CALLBACK(main_focus_in_event) ,
		notebook) ;
	
	menu = get_menubar_menu( notebook) ;
	gtk_box_pack_start( GTK_BOX(vbox) , menu , FALSE , TRUE , 0) ;
	gtk_widget_show( menu) ;

	gtk_box_pack_start( GTK_BOX(vbox) , notebook , TRUE , TRUE , 0) ;
	gtk_widget_show( notebook) ;

	mlvte = mlvte_new() ;
#if  1
	gtk_notebook_append_page( GTK_NOTEBOOK(notebook) , mlvte , NULL) ;
#else
	gtk_box_pack_start(GTK_BOX(hbox) , mlvte , TRUE , TRUE , 0) ;
#endif
	g_signal_connect( G_OBJECT(mlvte) , "destroy" , G_CALLBACK(mlvte_destroy) , notebook) ;
	gtk_widget_show( mlvte) ;

	num_of_windows ++ ;
}

static void
add_page(
	GtkWidget *  widget ,
	gpointer   data
	)
{
	GtkWidget *  notebook ;
	GtkWidget *  mlvte ;

#if  1
	printf( "Add page\n") ;
	fflush( NULL) ;
#endif

	notebook = data ;
	
	mlvte = mlvte_new() ;
	gtk_widget_show( mlvte) ;
	
	gtk_notebook_set_current_page( GTK_NOTEBOOK(notebook) ,
		gtk_notebook_append_page( GTK_NOTEBOOK(notebook) , mlvte , NULL)) ;
	gtk_signal_connect( GTK_OBJECT(mlvte) , "destroy" ,
		GTK_SIGNAL_FUNC(mlvte_destroy) , notebook) ;
}

static GtkWidget *
get_menubar_menu(
	GtkWidget *  notebook
	)
{
	GtkWidget *  menu_bar ;
	GtkWidget *  root_menu ;
	GtkWidget *  menu ;
	GtkWidget *  menu_item ;

	menu = gtk_menu_new() ;

	menu_item = gtk_menu_item_new_with_label( "New tab") ;
	gtk_menu_shell_append( GTK_MENU_SHELL(menu) , menu_item) ;
	g_signal_connect( menu_item , "activate" , G_CALLBACK(add_page) , notebook) ;
	gtk_widget_show(menu_item) ;

	menu_item = gtk_menu_item_new_with_label( "New window") ;
	gtk_menu_shell_append( GTK_MENU_SHELL(menu) , menu_item) ;
	g_signal_connect( menu_item , "activate" , G_CALLBACK(new_window) , notebook) ;
	gtk_widget_show(menu_item) ;
	
	root_menu = gtk_menu_item_new_with_label( "File") ;
	gtk_widget_show( root_menu) ;

	gtk_menu_item_set_submenu( GTK_MENU_ITEM(root_menu) , menu) ;

	menu_bar = gtk_menu_bar_new() ;
	gtk_widget_show( menu_bar) ;
	gtk_menu_shell_append( GTK_MENU_SHELL(menu_bar) , root_menu) ;

	return  menu_bar ;
}


/* --- global functions --- */

int
main(
	int   argc,
	char *argv[]
	)
{
	gtk_init(&argc, &argv) ;

	new_window( NULL , NULL) ;

	gtk_main() ;

	return  0 ;
}
