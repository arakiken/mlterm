/*
 *	$Id: ccheader,v 1.2 2001/12/01 23:37:26 ken Exp $
 */

#ifndef __MLVTE_H__
#define __MLVTE_H__


#include <glib.h>
#include <glib-object.h>
#include <gtk/gtkwindow.h>


G_BEGIN_DECLS

#define MLVTE_TYPE            (mlvte_get_type ())
#define MLVTE(obj)            (G_TYPE_CHECK_INSTANCE_CAST ((obj), MLVTE_TYPE, Mlvte))
#define MLVTE_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST ((klass), MLVTE_TYPE, MlvteClass))
#define IS_MLVTE(obj)         (G_TYPE_CHECK_INSTANCE_TYPE ((obj), MLVTE_TYPE))
#define IS_MLVTE_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass), MLVTE_TYPE))


typedef struct _Mlvte       Mlvte;
typedef struct _MlvteClass  MlvteClass;

struct _Mlvte
{
	GtkWidget  widget ;
	unsigned int  child ;
} ;

struct _MlvteClass
{
	GtkWidgetClass  parent_class ;
} ;

GType          mlvte_get_type(void) ;
GtkWidget*     mlvte_new(void) ;

G_END_DECLS


#endif /* __MLVTE_H__ */
