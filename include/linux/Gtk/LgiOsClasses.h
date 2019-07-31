#ifndef __OS_CLASS_H
#define __OS_CLASS_H

#ifdef _MSC_VER
#define LTHREAD_DATA __declspec( thread )
#else
#define LTHREAD_DATA __thread
#endif

template<typename T>
class GlibWrapper
{
	::GArray<Gtk::gulong> Sigs;

public:
	T *obj;

	GlibWrapper()
	{
		obj = NULL;
	}

	~GlibWrapper()
	{
		Empty();
	}

	void Empty()
	{
		if (obj)
		{
			for (auto s: Sigs)
				Gtk::g_signal_handler_disconnect(obj, s);
			Sigs.Empty();

			Gtk::g_object_unref(obj);
			obj = NULL;
		}
	}

	operator T*()
	{
		return obj;
	}

	GlibWrapper<T> &operator =(T *p)
	{
		obj = p;
		return *this;
	}

	bool Connect(const Gtk::gchar *detailed_signal, Gtk::GCallback c_handler, Gtk::gpointer data)
	{
		if (!obj)
			return false;
		auto r = Gtk::g_signal_connect_data(obj, detailed_signal, c_handler, data, NULL, (Gtk::GConnectFlags) 0);
		if (r <= 0)
			return false;

		Sigs.Add(r);
		return true;
	}
};


#endif
