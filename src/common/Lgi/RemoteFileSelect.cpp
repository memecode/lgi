#include "lgi/common/Lgi.h"
#include "lgi/common/RemoteFileSelect.h"

#ifdef WINDOWS
	// Use the cross platform file select dialog
	// #include <algorithm>
	#include <lgi/common/Menu.h>
	#include <lgi/common/Layout.h>
	#include <lgi/common/Popup.h>
	#include <lgi/common/Tree.h>
	#include <lgi/common/List.h>
	#include <lgi/common/TextLabel.h>
	#include <lgi/common/Button.h>
	#include <lgi/common/CheckBox.h>
	#include <lgi/common/Combo.h>
	#include <lgi/common/TableLayout.h>
	#include <lgi/common/Box.h>
	
	// But in a new namespace:
	#define FS_NAMESPACE RemoteFs
	namespace FS_NAMESPACE {
		#define FILE_SELECT_CLS
		#define FILE_SELECT_FN
		#include "lgi/common/FileSelect.h"
		#include "../../src/common/Lgi/FileSelect.cpp"
	}
#else
	#define FS_NAMESPACE
	#include "lgi/common/FileSelect.h"
#endif

class RemoteFileSelectSystem : public FS_NAMESPACE::IFileSelectSystem
{
	char dirChar = '/';
	SystemIntf *systemIntf = NULL;

	LString ConvertPath(LString p)
	{
		char pathStr[2] = { GetDirChar(), 0 };
		return p.Replace("\\", pathStr);
	}

public:
	RemoteFileSelectSystem(SystemIntf *intf) :
		systemIntf(intf)
	{
		systemIntf->GetSysType([this](auto sys)
		{
			dirChar = sys == PlatformWin ? '\\' : '/';
		});
	}

	char GetDirChar() override
	{
		return '/';
	}

	LString PathJoin(LString base, LString leaf) override
	{
		LString p = ConvertPath(base);
		if (leaf)
		{
			if (p(-1) != GetDirChar())
			{
				char dirStr[2] = { GetDirChar(), 0 };
				p += dirStr;
			}
			p += leaf;
		}
		return p;
	}

	void GetInitialPath(std::function<void(LString)> cb) override
	{
		if (!cb || !systemIntf)
			return;
		if (auto p = systemIntf->GetBasePath())
			cb(p);
	}

	void GetRootVolume(std::function<void(LVolume*)> cb) override
	{
		// Impl me
	}

	void Stat(LString path, std::function<void(struct stat*, LString, LError)> cb) override
	{
		if (!path || !cb)
			return;
		systemIntf->Stat(ConvertPath(path), [this, cb](auto s, auto path, auto err)
			{
				cb(s, path, err);
			});
	}

	void ReadDir(LString path, std::function<void(LDirectory&)> cb) override
	{
		if (!cb)
			return;
			
		printf("RemoteFileSel: readdir start...\n");
		systemIntf->ReadFolder(SystemIntf::TDebugLogging, ConvertPath(path), [this, cb](auto dir)
		{
			printf("RemoteFileSel: readdir result.\n");
			cb(*dir);
		});
	}

	void CreateFolder(LString path, bool createParents, std::function<void(bool)> cb) override
	{
		systemIntf->CreateFolder(ConvertPath(path), createParents, [this, cb](auto status)
			{
				if (cb)
					cb(status);
			});
	}

	void DeleteFolder(LString path, std::function<void(bool)> cb) override
	{
		systemIntf->Delete(ConvertPath(path), true, [this, cb](auto status)
			{
				if (cb)
					cb(status);
			});
	}

	void DeleteFile(LString path, std::function<void(bool)> cb) override
	{
		systemIntf->Delete(ConvertPath(path), false, [this, cb](auto status)
			{
				if (cb)
					cb(status);
			});
	}

	void Rename(LString oldPath, LString newPath, std::function<void(bool)> cb) override
	{
		systemIntf->Rename(ConvertPath(oldPath), ConvertPath(newPath), [this, cb](auto status)
			{
				if (cb)
					cb(status);
			});
	}
};

void RemoteFileSelect(LViewI *parent, SystemIntf *systemIntf, FileSelectType type, LString initialPath, std::function<void(LString)> callback)
{
	auto dlg = new FS_NAMESPACE::LFileSelect(parent, new RemoteFileSelectSystem(systemIntf));

	if (initialPath)
		dlg->InitialDir(initialPath);

	switch (type)
	{
		default:
		case SelectOpen:
			dlg->Open([callback](auto selectDlg, auto ok)
				{
					if (ok && callback)
						callback(selectDlg->Name());
				});
			break;
		case SelectOpenFolder:
			dlg->OpenFolder([callback](auto selectDlg, auto ok)
				{
					if (ok && callback)
						callback(selectDlg->Name());
				});
			break;
		case SelectSave:
			dlg->Save([callback](auto selectDlg, auto ok)
				{
					if (ok && callback)
						callback(selectDlg->Name());
				});
			break;
	}
}
