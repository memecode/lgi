/**
	\file
	\author Matthew Allen <fret@memecode.com>
	\brief Simple single window wrapper around the LHtml2 control
 */
#ifndef _GBROWSER_H_
#define _GBROWSER_H_


class LBrowser : public LWindow
{
	class LBrowserPriv *d;

	bool OnViewKey(LView *v, LKey &k);

public:
	class LBrowserEvents
	{
	public:
		virtual ~LBrowserEvents() {}

		virtual bool OnSearch(LBrowser *br, const char *txt) { return false; }
	};
	
	LBrowser(LViewI *owner, const char *Title, char *Uri = 0);
	~LBrowser();

	void SetEvents(LBrowserEvents *Events);
	bool SetUri(const char *Uri = NULL);
	bool SetHtml(const char *Html);

	/// Adds optional local file system paths to search for resources.
	void AddPath(const char *Path);

	void OnPosChange();
	int OnNotify(LViewI *c, LNotification n);
	LMessage::Result OnEvent(LMessage *m);
};

#endif