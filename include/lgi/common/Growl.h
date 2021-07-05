#ifndef _GGROWL_H_
#define _GGROWL_H_

class LgiClass GGrowl
{
	class GGrowlPriv *d;
	
public:
	struct GNotifyType
	{
		LString Name;
		LString DisplayName; // Optional
		bool Enabled;
		LString IconUrl;
		
		GNotifyType()
		{
			Enabled = true;
		}
	};

	struct GRegister
	{
		LString App;
		LString IconUrl; // Optional
		LArray<GNotifyType> Types;
	};

	struct GNotify
	{
		LString Name;
		LString Title;
		LString Id; // Optional
		LString Text; // Optional
		bool Sticky; // Optional
		int Priority; // Optional
		LString IconUrl; // Optional
		LString CoalescingID; // Optional
		LString CallbackData; // Optional
		LString CallbackDataType; // Optional
		LString CallbackTarget; // Optional
		
		GNotify()
		{
			Sticky = false;
			Priority = 0;
		}
	};


	GGrowl(bool async = true);
	~GGrowl();

	bool Register(LAutoPtr<GRegister> r);
	bool Notify(LAutoPtr<GNotify> n);
};

#endif