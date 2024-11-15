/// \file
/// \author Matthew Allen
/// \brief Native mac file open dialog.
#include <stdio.h>
#include <stdlib.h>

#include "lgi/common/Lgi.h"
#include "lgi/common/Token.h"
#include "lgi/common/FileSelect.h"

//////////////////////////////////////////////////////////////////////////
// This is just a private data container to make it easier to change the
// implementation of this class without effecting headers and applications.
class LFileSelectPrivate
{
	friend class LFileSelect;
	friend class LFileSelectDlg;
	friend class GFolderList;

	LView *Parent = NULL;
	LFileSelect *Select = NULL;

	LString Title;
	LString DefExt;
	bool MultiSelect = false;
	LString::Array Files;
	int CurrentType = -1;
	List<LFileType> Types;
	LString::Array History;
	bool ShowReadOnly = false;
	bool ReadOnly = false;
	bool EatClose = false;

public:
	static LString InitPath;
	static bool InitShowHiddenFiles;
	static LRect InitSize;

	LFileSelectPrivate(LFileSelect *select)
	{
		Select = select;
	}

	virtual ~LFileSelectPrivate()
	{
		Types.DeleteObjects();
	}
};

LString LFileSelectPrivate::InitPath;

//////////////////////////////////////////////////////////////////////////
LFileSelect::LFileSelect(LViewI *Window, IFileSelectSystem *System)
{
	d = new LFileSelectPrivate(this);
}

LFileSelect::~LFileSelect()
{
	DeleteObj(d);
}

void LFileSelect::ShowReadOnly(bool b)
{
	d->ShowReadOnly = b;;
}

bool LFileSelect::ReadOnly()
{
	return d->ReadOnly;
}

const char *LFileSelect::Name()
{
	return d->Files[0];
}

bool LFileSelect::Name(const char *n)
{
	d->Files.Empty();
	if (n)
		d->Files.New() = n;

	return true;
}

const char *LFileSelect::operator [](size_t i)
{
	return d->Files.ItemAt(i);
}

size_t LFileSelect::Length()
{
	return d->Files.Length();
}

size_t LFileSelect::Types()
{
	return d->Types.Length();
}

void LFileSelect::ClearTypes()
{
	d->Types.DeleteObjects();
}

LFileType *LFileSelect::TypeAt(ssize_t n)
{
	return d->Types.ItemAt(n);
}

bool LFileSelect::Type(const char *Description, const char *Extension, int Data)
{
	LFileType *Type = new LFileType;
	if (Type)
	{
		Type->Description(Description);
		Type->Extension(Extension);
		d->Types.Insert(Type);
	}

	return Type != 0;
}

ssize_t LFileSelect::SelectedType()
{
	return d->CurrentType;
}

LViewI *LFileSelect::Parent()
{
	return d->Parent;
}

void LFileSelect::Parent(LViewI *Window)
{
	d->Parent = dynamic_cast<LView*>(Window);
}

bool LFileSelect::MultiSelect()
{
	return d->MultiSelect;
}

void LFileSelect::MultiSelect(bool Multi)
{
	d->MultiSelect = Multi;
}

#define CharPropImpl(Func, Var)				\
	const char *LFileSelect::Func()			\
	{										\
		return Var;							\
	}										\
	void LFileSelect::Func(const char *i)	\
	{										\
		Var = i;							\
	}

CharPropImpl(InitialDir, d->InitPath);
CharPropImpl(Title, d->Title);
CharPropImpl(DefaultExtension, d->DefExt);
   
void LFileSelect::Open(SelectCb Cb)
{
	auto openDlg = [NSOpenPanel openPanel];

	[openDlg setCanChooseFiles:YES];
	[openDlg setAllowsMultipleSelection:d->MultiSelect];
	if (d->InitPath)
		[openDlg setDirectoryURL:[NSURL fileURLWithPath:d->InitPath.NsStr() isDirectory:true]];

	NSModalResponse result = [openDlg runModal];
	if (result == NSModalResponseOK)
	{
		d->Files.Empty();
		
		auto urls = [openDlg URLs];
		for (unsigned i=0; i<[urls count]; i++)
		{
			auto url = [urls objectAtIndex:i];
			d->Files.New() = [url path];
		}
	}

	return result == NSModalResponseOK;
}

void LFileSelect::OpenFolder(SelectCb Cb)
{
	NSOpenPanel* openDlg = [NSOpenPanel openPanel];

	[openDlg setCanChooseDirectories:YES];
	if (d->InitPath)
		[openDlg setDirectoryURL:[NSURL fileURLWithPath:d->InitPath.NsStr() isDirectory:true]];
	
	NSModalResponse result = [openDlg runModal];
	if (result == NSModalResponseOK)
	{
		d->Files.Empty();
		
		auto urls = [openDlg URLs];
		for (unsigned i=0; i<[urls count]; i++)
		{
			auto url = [urls objectAtIndex:i];
			d->Files.New() = [url path];
		}
	}

	return result == NSModalResponseOK;
}

void LFileSelect::Save(SelectCb Cb)
{
	auto saveDlg = [NSSavePanel savePanel];

	if (d->InitPath)
		[saveDlg setDirectoryURL:[NSURL fileURLWithPath:d->InitPath.NsStr() isDirectory:true]];

	NSModalResponse result = [saveDlg runModal];
	if (result == NSModalResponseOK)
	{
		d->Files.Empty();
		d->Files.New() = [[saveDlg URL] path];
	}

	return result == NSModalResponseOK;
}

