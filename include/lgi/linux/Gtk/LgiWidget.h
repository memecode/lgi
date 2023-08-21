#ifndef _LGIWIDGET_H_
#define _LGIWIDGET_H_

enum GDK_MouseButtons
{
	GDK_LEFT_BTN = 1,
	GDK_MIDDLE_BTN = 2,
	GDK_RIGHT_BTN = 3,
	
	GDK_BACK_BTN = 8,
	GDK_FORWARD_BTN = 9,
};

G_BEGIN_DECLS

G_DECLARE_FINAL_TYPE(LgiWidget, lgi_widget, LGI, WIDGET, GtkContainer)
GtkWidget *lgi_widget_new(LViewI *target, bool pour_largest);
void lgi_widget_detach(GtkWidget *widget);

// Children management
void lgi_widget_add(GtkContainer *wid, GtkWidget *child);
void lgi_widget_remove(GtkContainer *wid, GtkWidget *child);

// Other widget methods
void lgi_widget_setpos(GtkWidget *wid, LRect rc);
void BuildTabStops(LViewI *v, LArray<LViewI*> &a);

G_END_DECLS

#endif