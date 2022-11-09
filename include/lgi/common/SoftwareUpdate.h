#ifndef _SOFTWARE_UPDATE_H_
#define _SOFTWARE_UPDATE_H_

#include "lgi/common/DateTime.h"

class LSoftwareUpdate
{
	struct LSoftwareUpdatePriv *d;

public:
	struct UpdateInfo
	{
		LString Name;
		LString Version;
		LString Build;
		LString Uri;
		LString Proxy;
		LDateTime Date;
		bool Cancel = false;
	};

	LSoftwareUpdate(const char *SoftwareName, const char *UpdateUri, const char *ProxyUri, const char *OptionalTempPath = 0);
	virtual ~LSoftwareUpdate();

	void CheckForUpdate(UpdateInfo &Info, LViewI *WithUi, bool IncBetas, std::function<void(bool, const char*)> callback);
	bool ApplyUpdate(const UpdateInfo &Info, bool DownloadOnly, LViewI *WithUi);
	const char *GetErrorMessage();
};

#endif