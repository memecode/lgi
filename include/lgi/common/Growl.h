#ifndef _GGROWL_H_
#define _GGROWL_H_

class LgiClass LGrowl
{
	class LGrowlPriv *d;
	
public:
	struct LNotifyType
	{
		LString Name;
		LString DisplayName; // Optional
		bool Enabled;
		LString IconUrl;
		
		LNotifyType()
		{
			Enabled = true;
		}
	};

	struct LRegister
	{
		LString App;
		LString IconUrl; // Optional
		LArray<LNotifyType> Types;
	};

	struct LNotify
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
		
		LNotify()
		{
			Sticky = false;
			Priority = 0;
		}
	};


	LGrowl(bool async = true);
	~LGrowl();

	bool Register(LAutoPtr<LRegister> r);
	bool Notify(LAutoPtr<LNotify> n);
};

#endif