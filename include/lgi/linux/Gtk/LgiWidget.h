#ifndef _LGIWIDGET_H_
#define _LGIWIDGET_H_

G_BEGIN_DECLS

G_DECLARE_FINAL_TYPE(LgiWidget, lgi_widget, LGI, WIDGET, GtkContainer)
GtkWidget *lgi_widget_new(LViewI *target, bool pour_largest);
void lgi_widget_detach(GtkWidget *widget);

// Children management
void lgi_widget_add(GtkContainer *wid, GtkWidget *child);
void lgi_widget_remove(GtkContainer *wid, GtkWidget *child);

// Other widget methods
void lgi_widget_setpos(GtkWidget *wid, LRect rc);
void BuildTabStops(LViewI *v, ::LArray<LViewI*> &a);

G_END_DECLS

#endif