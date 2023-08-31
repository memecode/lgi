#ifndef _SOFTWARE_UPDATE_H_
#define _SOFTWARE_UPDATE_H_

#include "lgi/common/DateTime.h"

class LSoftwareUpdate
{
	struct LSoftwareUpdatePriv *d;
	friend struct LSoftwareUpdatePriv;

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
		bool HasUpdate = false;
	};

	typedef std::function<void(UpdateInfo*,const char*)> UpdateCb;

	LSoftwareUpdate(const char *SoftwareName, const char *UpdateUri, const char *ProxyUri, const char *OptionalTempPath = NULL);
	virtual ~LSoftwareUpdate();

	/// This function should not be called using a stack object. Dynamically allocation the LSoftwareUpdate and it'll
	/// be deleted after the callback is called.
	void CheckForUpdate(
		/// The callback to receive the update info.
		/// If 'WithUi' is provided, the callback will be in the GUI thread.
		/// Otherwise the callback will be called by a worker thread, not the GUI thread.
		UpdateCb callback,
		/// [optional] ptr to parent view, if present a spinner window is shown.
		LViewI *WithUi = NULL,
		/// [optional] also look for beta releases.
		bool IncBetas = false
	);

	void ApplyUpdate(const UpdateInfo *Info, bool DownloadOnly, LViewI *WithUi, std::function<void(bool)> Callback);

	const char *GetErrorMessage();
};

#endif