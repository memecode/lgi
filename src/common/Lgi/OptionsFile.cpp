#include "lgi/common/Lgi.h"
#include "lgi/common/OptionsFile.h"

#define DEBUG_OPTS_FILE		0


void LOptionsFile::_Init()
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

LOptionsFile::LOptionsFile(const char *FileName) : LMutex("LOptionsFile")
{
	_Init();

	if (LFileExists(FileName))
		File = FileName;
	else
		SetMode(GuessMode(), FileName);
}

LOptionsFile::LOptionsFile(PortableType Mode, const char *BaseName) : LMutex("LOptionsFile")
{
	_Init();

	if (Mode != UnknownMode)
		SetMode(Mode, BaseName);
}

LOptionsFile::~LOptionsFile()
{
}

LOptionsFile::PortableType LOptionsFile::GuessMode()
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

bool LOptionsFile::SetMode(PortableType mode, const char *BaseName)
{
	char FullPath[MAX_PATH_LEN];

	if (!mode)
		mode = GuessMode();
	Mode = mode;

	if (!LGetSystemPath(Mode == DesktopMode ? LSP_APP_ROOT : LSP_APP_INSTALL, FullPath, sizeof(FullPath)))
		return false;

	if (!LDirExists(FullPath))
		FileDev->CreateFolder(FullPath);

	LMakePath(FullPath, sizeof(FullPath), FullPath, BaseName ? BaseName : (char*)"Options");
	if (!LGetExtension(FullPath))
		strcat_s(FullPath, sizeof(FullPath), ".xml");
	File = FullPath;
	Dirty = !LFileExists(File);

	return true;
}

bool LOptionsFile::_OnAccess(bool Start)
{
	if (Start)
	{
		return Lock(_FL);
	}

	Unlock();
	return true;
}

bool LOptionsFile::IsValid()
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

void LOptionsFile::SetFile(const char *f)
{
	File = f;
	Dirty = true;
}

bool LOptionsFile::SerializeFile(bool Write)
{
	if (!File)
	{
		Error = "No file name provided.";
		return false;
	}
		
	LMutex::Auto Lck(this, _FL);
	if (!Lck)
	{
		Error = "Failed to lock mutex.";
		return false;
	}

	LFile f;
	#if DEBUG_OPTS_FILE
	LgiTrace("%s:%i - LOptionsFile::Serialize(%i) File='%s'\n",
		_FL, Write, File.Get());
	#endif

	LXmlTree Tree(GXT_PRETTY_WHITESPACE);
	if (Write)
	{
		if (f.Open(File, O_WRITE))
		{
			f.SetSize(0);
			if (!Tree.Write(this, &f))
			{
				Error.Printf("%s:%i - Failed encode xml to '%s'", _FL, File.Get());
				LgiTrace("%s\n", Error.Get());
				LAssert("Xml write failed.");
				return false;
			}
		}
		else
		{
			Error.Printf("%s:%i - Failed to open '%s' for writing", _FL, File.Get());
			LgiTrace("%s\n", Error.Get());
			return false;
		}
	}
	else // Read
	{
		if (!LFileExists(File))
		{
			// Potentially we're installed to a read-only location...
			// Check that here and relocate options file path to a RW location.
			if (!f.Open(File, O_WRITE))
			{
				if (Mode == PortableMode)
				{
					// Change to desktop mode...
					// FYI: This happens when running in an AppImage.
					SetMode(DesktopMode);
				}
				else
				{
					Error.Printf("%s:%i - Write check for '%s' failed. Mode=%i", _FL, File.Get(), Mode);
					LgiTrace("%s\n", Error.Get());
					return false;
				}
			}
		}
		
		if (LFileExists(File))
		{
			if (f.Open(File, O_READ))
			{
				Empty(true);

				auto Status = Tree.Read(this, &f, 0);
				if (!Status)
				{
					_Init(); // re init

					Error.Printf("%s:%i - Xml parse of '%s' failed", _FL, File.Get());
					LgiTrace("%s\n", Error.Get());
				}

				_Defaults();				
				return Status;
			}
			else
			{
				Error.Printf("%s:%i - Failed to open '%s' for reading", _FL, File.Get());
				LgiTrace("%s\n", Error.Get());
				return false;
			}
		}
	}

	return true;
}

bool LOptionsFile::DeleteValue(const char *Name)
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

bool LOptionsFile::CreateTag(const char *Name)
{
	LXmlTag *Status = 0;

	if (Name && Lock(_FL))
	{
		Status = GetChildTag(Name, true);
		Unlock();
	}

	return Status != 0;
}

bool LOptionsFile::DeleteTag(const char *Name)
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

LXmlTag *LOptionsFile::LockTag(const char *Name, const char *File, int Line)
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
