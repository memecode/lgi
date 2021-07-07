#include "lgi/common/Lgi.h"
#include "lgi/common/OptionsFile.h"

#define DEBUG_OPTS_FILE		0


void GOptionsFile::_Init()
{
	Dirty = false;
	LockFile = NULL;
	LockLine = -1;
	Tag = Allocator->Alloc("Options");
	
	if (Lock(_FL))
	{
		_Defaults();
		Unlock();
	}
}

GOptionsFile::GOptionsFile(const char *FileName) : LMutex("GOptionsFile")
{
	_Init();

	if (LFileExists(FileName))
		File = FileName;
	else
		SetMode(GuessMode(), FileName);
}

GOptionsFile::GOptionsFile(PortableType Mode, const char *BaseName) : LMutex("GOptionsFile")
{
	_Init();

	SetMode(Mode, BaseName);
}

GOptionsFile::~GOptionsFile()
{
}

GOptionsFile::PortableType GOptionsFile::GuessMode()
{
	auto a = LGetExeFile().SplitDelimit(DIR_STR);
	if (a.Length() == 0)
		return UnknownMode;

	#if defined(MAC)
	if (a[0].Equals("Applications"))
		return DesktopMode;
	#elif defined(WINDOWS)
	if (a.Length() > 1 && (a[1].Equals("Program Files") || a[1].Equals("Program Files (x86)")))
		return DesktopMode;
	#else
	#warning "Impl me.""
	#endif

	return PortableMode;
}		

bool GOptionsFile::SetMode(PortableType mode, const char *BaseName)
{
	char FullPath[MAX_PATH];

	if (!mode)
		mode = GuessMode();
	Mode = mode;

	if (!LGetSystemPath(Mode == DesktopMode ? LSP_APP_ROOT : LSP_APP_INSTALL, FullPath, sizeof(FullPath)))
		return false;

	if (!LDirExists(FullPath))
		FileDev->CreateFolder(FullPath);

	LgiMakePath(FullPath, sizeof(FullPath), FullPath, BaseName ? BaseName : (char*)"Options");
	if (!LgiGetExtension(FullPath))
		strcat_s(FullPath, sizeof(FullPath), ".xml");
	File = FullPath;
	Dirty = !LFileExists(File);

	return true;
}

bool GOptionsFile::_OnAccess(bool Start)
{
	if (Start)
	{
		return Lock(_FL);
	}

	Unlock();
	return true;
}

bool GOptionsFile::IsValid()
{
	bool Status = false;

	if (Lock(_FL))
	{
		if (Attr.Length() > 0 &&
			Children.Length() > 0)
		{
			Status = true;
		}

		Unlock();
	}

	return Status;
}

void GOptionsFile::SetFile(const char *f)
{
	File = f;
	Dirty = true;
}

bool GOptionsFile::SerializeFile(bool Write)
{
	bool Status = false;

	if (File && Lock(_FL))
	{
		LFile f;
		if
		(
			(
				Write
				||
				LFileExists(File)
			)
			&&
			f.Open(File, Write ? O_WRITE : O_READ)
		)
		{
			#if DEBUG_OPTS_FILE
			LgiTrace("%s:%i - GOptionsFile::Serialize(%i) File='%s'\n", 
				_FL, Write, File.Get());
			#endif
			
			LXmlTree Tree(GXT_PRETTY_WHITESPACE);
			if (Write)
			{
				f.SetSize(0);
				Status = Tree.Write(this, &f);
			}
			else
			{
				Empty(true);
				if ((Status = Tree.Read(this, &f, 0)))
				{
					_Defaults();
				}
			}
		}
		else if (Write)
		{
			LgiTrace("%s:%i - Failed to open '%s'\n", _FL, File.Get());
			LgiAssert(!"Failed to open file.");
		}

		Unlock();
	}

	return Status;
}

bool GOptionsFile::DeleteValue(const char *Name)
{
	bool Status = false;

	if (Name && Lock(_FL))
	{
		LVariant v;
		SetValue(Name, v);
		Unlock();
	}

	return Status;
}

bool GOptionsFile::CreateTag(const char *Name)
{
	LXmlTag *Status = 0;

	if (Name && Lock(_FL))
	{
		Status = GetChildTag(Name, true);
		Unlock();
	}

	return Status != 0;
}

bool GOptionsFile::DeleteTag(const char *Name)
{
	bool Status = false;

	if (Name && Lock(_FL))
	{
		LXmlTag *t = GetChildTag(Name);
		if (t)
		{
			Status = t->RemoveTag();
			DeleteObj(t);
		}
		Unlock();
	}

	return Status;
}

LXmlTag *GOptionsFile::LockTag(const char *Name, const char *File, int Line)
{
	LXmlTag *t = 0;

	if (Lock(File, Line))
	{
		t = Name ? LXmlTag::GetChildTag(Name) : this;
		if (!t)
		{
			Unlock();
		}
	}

	return t;
}
