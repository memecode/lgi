#ifndef _LGIWIDGET_H_
#define _LGIWIDGET_H_

G_BEGIN_DECLS

#if GTK_MAJOR_VERSION == 3
	/*
	#define LGI_WIDGET(obj) 		G_TYPE_CHECK_INSTANCE_CAST(obj, lgi_widget_get_type(), LgiWidget)
	#define LGI_IS_WIDGET(obj)		G_TYPE_CHECK_INSTANCE_TYPE(obj, lgi_widget_get_type())
	#define LGI_WIDGET_CLASS(klass) G_TYPE_CHECK_CLASS_CAST(klass, lgi_widget_get_type(), LgiWidgetClass)
	*/
#else
	#define LGI_WIDGET(obj)			GTK_CHECK_CAST(obj, lgi_widget_get_type(), LgiWidget)
	#define LGI_IS_WIDGET(obj)		GTK_CHECK_TYPE(obj, lgi_widget_get_type())
	#define LGI_WIDGET_CLASS(klass) GTK_CHECK_CLASS_CAST(klass, lgi_widget_get_type(), LgiWidgetClass)
#endif

#if GTK_MAJOR_VERSION == 3
// #define LGI_TYPE_WIDGET (lgi_widget_get_type())
G_DECLARE_FINAL_TYPE(LgiWidget, lgi_widget, LGI, WIDGET, GtkContainer)
#else
typedef struct _LgiWidgetClass LgiWidgetClass;
struct _LgiWidgetClass
{
	GtkContainerClass parent_class;
};
GtkType lgi_widget_get_type();
void lgi_widget_init(LgiWidget *w);
#endif
GtkWidget *lgi_widget_new(GViewI *target, bool pour_largest);
void lgi_widget_detach(GtkWidget *widget);

// Children management
void lgi_widget_add(GtkContainer *wid, GtkWidget *child);
void lgi_widget_remove(GtkContainer *wid, GtkWidget *child);

// Positioning of widgets
void lgi_widget_setpos(GtkWidget *wid, GRect rc);

G_END_DECLS

#endif