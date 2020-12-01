#ifndef _GGROWL_H_
#define _GGROWL_H_

class LgiClass GGrowl
{
	class GGrowlPriv *d;
	
public:
	struct GNotifyType
	{
		GString Name;
		GString DisplayName; // Optional
		bool Enabled;
		GString IconUrl;
		
		GNotifyType()
		{
			Enabled = true;
		}
	};

	struct GRegister
	{
		GString App;
		GString IconUrl; // Optional
		GArray<GNotifyType> Types;
	};

	struct GNotify
	{
		GString Name;
		GString Title;
		GString Id; // Optional
		GString Text; // Optional
		bool Sticky; // Optional
		int Priority; // Optional
		GString IconUrl; // Optional
		GString CoalescingID; // Optional
		GString CallbackData; // Optional
		GString CallbackDataType; // Optional
		GString CallbackTarget; // Optional
		
		GNotify()
		{
			Sticky = false;
			Priority = 0;
		}
	};


	GGrowl(bool async = true);
	~GGrowl();

	bool Register(GAutoPtr<GRegister> r);
	bool Notify(GAutoPtr<GNotify> n);
};

#endif