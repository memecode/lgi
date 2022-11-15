/*hdr
**      FILE:           LFileSelect.cpp
**      AUTHOR:         Matthew Allen
**      DATE:           26/9/2001
**      DESCRIPTION:    Common file/directory selection dialog
**
**      Copyright (C) 1998-2001, Matthew Allen
**              fret@memecode.com
*/

#include <stdio.h>
#include <CdErr.h>

#include "lgi/common/Lgi.h"
#include "lgi/common/FileSelect.h"

class LFileSelectPrivate
{
	friend class LFileSelect;

	bool			CanMultiSelect;
	bool			MultiSelected;

	LString			InitDir;
	LString			DefExt;
	LString			TitleStr;
	bool			WaitForMessage;
	int				SelectedType;
	LViewI			*ParentWnd;
	List<LFileType> TypeList;
	LString::Array	Files;
	bool			ShowReadOnly;
	bool			ReadOnly;

	char16 *TypeStrW()
	{
		LMemQueue p;

		for (auto Type: TypeList)
		{
			char16 *d = Utf8ToWide(Type->Description());
			char16 *e = Utf8ToWide(Type->Extension());
			
			p.Write((uchar*)d, sizeof(char16)*StrlenW(d));
			p.Write((uchar*)L"", sizeof(char16));
			p.Write((uchar*)e, sizeof(char16)*StrlenW(e));
			p.Write((uchar*)L"", sizeof(char16));

			DeleteArray(d);
			DeleteArray(e);
		}

		return (char16*)p.New(sizeof(char16));
	}

	bool DoFallback(OPENFILENAMEA &Info)
	{
		DWORD err = CommDlgExtendedError();
		if (err == FNERR_INVALIDFILENAME)
		{
			// Something about the filename is making the dialog unhappy...
			// So nuke it and go without.
			Info.lpstrFile[0] = 0;
			return true;
		}

		return false;
	}

	bool DoFallback(OPENFILENAMEW &Info)
	{
		DWORD err = CommDlgExtendedError();
		if (err == FNERR_INVALIDFILENAME)
		{
			// Something about the filename is making the dialog unhappy...
			// So nuke it and go without.
			Info.lpstrFile[0] = 0;
			return true;
		}

		return false;
	}

	void BeforeDlg(OPENFILENAMEW &Info)
	{
		ZeroObj(Info);
		Info.lStructSize = sizeof(Info);
		Info.lpstrFilter = TypeStrW();
		Info.nMaxFile = 4096;
		Info.lpstrFile = new char16[Info.nMaxFile];
		Info.hwndOwner = ParentWnd ? ParentWnd->Handle() : 0;
		
		if (Info.lpstrFile)
		{
			memset(Info.lpstrFile, 0, sizeof(*Info.lpstrFile) * Info.nMaxFile);

			if (Files.Length())
			{
				LAutoWString s(Utf8ToWide(Files[0]));
				if (s)
					StrcpyW(Info.lpstrFile, s);
			}
		}

		Info.lpstrInitialDir = Utf8ToWide(InitDir);

		if (CanMultiSelect)
			Info.Flags |= OFN_ALLOWMULTISELECT;
		else
			Info.Flags &= ~OFN_ALLOWMULTISELECT;

		if (ShowReadOnly)
			Info.Flags &= ~OFN_HIDEREADONLY;
		else
			Info.Flags |= OFN_HIDEREADONLY;

		Info.Flags |= OFN_EXPLORER | OFN_ENABLESIZING | OFN_NOCHANGEDIR;
	}

	void AfterDlg(OPENFILENAMEW &Info, bool Status)
	{
		Files.Empty();
		if (Status)
		{
			MultiSelected = StrlenW(Info.lpstrFile) < Info.nFileOffset;
			if (MultiSelected)
			{
				char16 s[256];
				StrcpyW(s, Info.lpstrFile);
				char16 *e = s + StrlenW(s);
				if (*e != DIR_CHAR) *e++ = DIR_CHAR;
				char16 *f = Info.lpstrFile + Info.nFileOffset;
				while (*f)
				{
					StrcpyW(e, f);
					Files.New() = s;
					f += StrlenW(f) + 1;
				}
			}
			else
			{
				Files.New() = Info.lpstrFile;
			}

			ReadOnly = TestFlag(Info.Flags, OFN_READONLY);
		}
		else
		{
			DWORD err = CommDlgExtendedError();
			LgiTrace("%s:%i - FileNameDlg error 0x%x\n", _FL, err);
		}

		DeleteArray(Info.lpstrFile);
		DeleteArray((char16*&)Info.lpstrInitialDir);
		DeleteArray((char16*&)Info.lpstrFilter);
		SelectedType = Info.nFilterIndex - 1;
	}

	LFileSelectPrivate()
	{
		CanMultiSelect = false;
		MultiSelected = false;
		InitDir = 0;
		DefExt = 0;
		TitleStr = 0;
		SelectedType = 0;
		ParentWnd = 0;
		ReadOnly = false;
		ShowReadOnly = false;
	}

	~LFileSelectPrivate()
	{
	}
};

///////////////////////////////////////////////////////////////////////////////////////////////////
LFileSelect::LFileSelect()
{
	d = new LFileSelectPrivate;
}

LFileSelect::~LFileSelect()
{
	ClearTypes();
	DeleteObj(d);
}

const char *LFileSelect::Name()
{
	return d->Files.Length() ? d->Files[0] : NULL;
}

bool LFileSelect::Name(const char *n)
{
	d->Files.Empty();
	return d->Files.New().Set(n);
}

const char *LFileSelect::operator [](size_t i)
{
	return d->Files.IdxCheck(i) ? d->Files[i] : NULL;
}

size_t LFileSelect::Length()
{
	return d->Files.Length();
}

size_t LFileSelect::Types()
{
	return d->TypeList.Length();
}

ssize_t LFileSelect::SelectedType()
{
	return d->SelectedType;
}

LFileType *LFileSelect::TypeAt(ssize_t n)
{
	return d->TypeList.ItemAt(n);
}

void LFileSelect::ClearTypes()
{
	d->TypeList.DeleteObjects();
}

bool LFileSelect::Type(const char *Description, const char *Extension, int Data)
{
	LFileType *Type = new LFileType;
	if (Type)
	{
		Type->Description(Description);
		Type->Extension(Extension);
		Type->Data(Data);
		d->TypeList.Insert(Type);
	}

	return Type != 0;
}

LViewI *LFileSelect::Parent()
{
	return d->ParentWnd;
}

void LFileSelect::Parent(LViewI *Window)
{
	d->ParentWnd = Window;
}

bool LFileSelect::ReadOnly()
{
	return d->ReadOnly;
}

void LFileSelect::ShowReadOnly(bool ro)
{
	d->ShowReadOnly = ro;
}

bool LFileSelect::MultiSelect()
{
	return d->CanMultiSelect;
}

void LFileSelect::MultiSelect(bool Multi)
{
	d->CanMultiSelect = Multi;
}

const char *LFileSelect::InitialDir()
{
	return d->InitDir;
}

void LFileSelect::InitialDir(const char *InitDir)
{
	d->InitDir = InitDir;
}

const char *LFileSelect::Title()
{
	return d->TitleStr;
}

void LFileSelect::Title(const char *Title)
{
	d->TitleStr = Title;
}

const char *LFileSelect::DefaultExtension()
{
	return d->DefExt;
}

void LFileSelect::DefaultExtension(const char *DefExt)
{
	d->DefExt = DefExt;
}

bool LFileSelect::Open()
{
	bool Status = FALSE;

	OPENFILENAMEW	Info;
	d->BeforeDlg(Info);
	Status = GetOpenFileNameW(&Info) != 0;
	if (!Status && d->DoFallback(Info))
		Status = GetOpenFileNameW(&Info) != 0;
	d->AfterDlg(Info, Status);

	return Status && Length() > 0;
}

#include "shlobj.h"

int CALLBACK GFileSelectBrowseCallback(HWND hwnd, UINT uMsg, LPARAM lParam, LPARAM lpData)
{
	char *Name = (char*)lpData;
	if (uMsg == BFFM_INITIALIZED && Name)
	{
        PostMessage(hwnd, BFFM_SETSELECTION, true, (LMessage::Param)Name);
	}

	return 0;
}

bool LFileSelect::OpenFolder()
{
	bool Status = FALSE;

	Name("(folder selection)");

	OPENFILENAMEW	Info;
	d->BeforeDlg(Info);
	Info.Flags &= ~OFN_FILEMUSTEXIST;
	Info.Flags &= ~OFN_ALLOWMULTISELECT;
	Info.Flags |= OFN_NOVALIDATE;
	Info.Flags |= OFN_NOTESTFILECREATE;
	if (!Info.lpstrInitialDir && d->InitDir)
		Info.lpstrInitialDir = Utf8ToWide(d->InitDir);
	Status = GetSaveFileNameW(&Info) != 0;
	d->AfterDlg(Info, Status);
	
	auto f = d->Files.Length() ? d->Files[0] : NULL;
	if (f)
	{
		auto d = strrchr(f, DIR_CHAR);
		if (d && d > f)
		{
			if (d[-1] == ':')
				d[1] = 0;
			else
				*d = 0;
		}
	}

	return Status && Length() > 0;
}

bool LFileSelect::Save()
{
	bool Status = FALSE;

	OPENFILENAMEW	Info;
	d->BeforeDlg(Info);
	Status = GetSaveFileNameW(&Info) != 0;
	if (!Status && d->DoFallback(Info))
		Status = GetSaveFileNameW(&Info) != 0;		
	d->AfterDlg(Info, Status);

	return Status && Length() > 0;
}
