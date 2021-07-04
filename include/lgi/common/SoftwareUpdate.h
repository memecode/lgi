#ifndef _SOFTWARE_UPDATE_H_
#define _SOFTWARE_UPDATE_H_

#include "lgi/common/DateTime.h"

class GSoftwareUpdate
{
	struct GSoftwareUpdatePriv *d;

public:
	struct UpdateInfo
	{
		GAutoString Name;
		GAutoString Version;
		GAutoString Build;
		GAutoString Uri;
		GAutoString Proxy;
		LDateTime Date;
		bool Cancel;
		
		UpdateInfo()
		{
		    Cancel = false;
		}
	};

	GSoftwareUpdate(const char *SoftwareName, const char *UpdateUri, const char *ProxyUri, const char *OptionalTempPath = 0);
	virtual ~GSoftwareUpdate();

	bool CheckForUpdate(UpdateInfo &Info, LViewI *WithUi, bool IncBetas);
	bool ApplyUpdate(UpdateInfo &Info, bool DownloadOnly, LViewI *WithUi);
	char *GetErrorMessage();
};

#endif