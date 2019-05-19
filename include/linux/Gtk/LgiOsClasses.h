#ifndef __OS_CLASS_H
#define __OS_CLASS_H

#ifdef _MSC_VER
#define LTHREAD_DATA __declspec( thread )
#else
#define LTHREAD_DATA __thread
#endif

extern LTHREAD_DATA int GtkLockCount;

#if GTK_MAJOR_VERSION == 3
#else
class LgiClass GtkLock
{
public:
	GtkLock()
	{
		if (GtkLockCount == 0)
			Gtk::gdk_threads_enter();
		GtkLockCount++;
	}
	
	~GtkLock()
	{
		GtkLockCount--;
		if (GtkLockCount == 0)
			Gtk::gdk_threads_leave();
	}
};
#endif


#endif
