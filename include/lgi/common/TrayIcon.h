#ifndef _GTRAYICON_H_
#define _GTRAYICON_H_

/////////////////////////////////////////////////////////////////////////
/// Put an icon in the system tray
class LgiClass LTrayIcon :
	public LBase
{
	class LTrayIconPrivate *d;

public:
	/// Constructor
	LTrayIcon
	(
		/// The owner LWindow
		LWindow *p
	);
	
	~LTrayIcon();

	/// Add an icon to the list
	bool Load(const TCHAR *Str);
	
	/// Is it visible?
	bool Visible();

	/// Show / Hide the tray icon
	void Visible(bool v);

	/// The index of the icon visible
	int64 Value();

	/// Set the index of the icon you want visible
	void Value(int64 v);

	/// Call this in your window's OnEvent handler
	virtual LMessage::Result OnEvent(LMessage *Msg);
};

#endif