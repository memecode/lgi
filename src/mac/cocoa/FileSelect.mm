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

	LView *Parent;
	LFileSelect *Select;

	char *Title;
	char *DefExt;
	bool MultiSelect;
	List<char> Files;
	int CurrentType;
	List<LFileType> Types;
	List<char> History;
	bool ShowReadOnly;
	bool ReadOnly;
	
	bool EatClose;

public:
	static char *InitPath;
	static bool InitShowHiddenFiles;
	static LRect InitSize;

	LFileSelectPrivate(LFileSelect *select)
	{
		ShowReadOnly = false;
		ReadOnly = false;
		EatClose = false;
		Select = select;
		Title = 0;
		DefExt = 0;
		MultiSelect = false;
		Parent = 0;
		CurrentType = -1;
	}

	virtual ~LFileSelectPrivate()
	{
		DeleteArray(Title);
		DeleteArray(DefExt);

		Types.DeleteObjects();
		Files.DeleteArrays();
		History.DeleteArrays();
	}


};

char *LFileSelectPrivate::InitPath = 0;

//////////////////////////////////////////////////////////////////////////
LFileSelect::LFileSelect()
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
	d->Files.DeleteArrays();
	if (n)
	{
		d->Files.Insert(NewStr(n));
	}

	return true;
}

char *LFileSelect::operator [](size_t i)
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
	char *LFileSelect::Func()				\
	{										\
		return Var;							\
	}										\
	void LFileSelect::Func(const char *i)	\
	{										\
		DeleteArray(Var);					\
		if (i)								\
		{									\
			Var = NewStr(i);				\
		}									\
	}

CharPropImpl(InitialDir, d->InitPath);
CharPropImpl(Title, d->Title);
CharPropImpl(DefaultExtension, d->DefExt);
   
bool LFileSelect::Open()
{
	bool Status = false;
	

	return Status;
}

bool LFileSelect::OpenFolder()
{
	bool Status = false;
	

	return Status;
}

bool LFileSelect::Save()
{
	bool Status = false;
	

	return Status;
}

