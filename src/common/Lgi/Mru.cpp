#include <stdio.h>

#include "lgi/common/Lgi.h"
#include "lgi/common/Mru.h"
#include "lgi/common/Variant.h"
#include "lgi/common/Menu.h"

////////////////////////////////////////////////////////////////////
#define DEBUG_LOG				0
#define M_MRU_BASE				(M_USER+0x3500)

struct GMruEntry
{
	LString Display;
	LString Raw;
	
	GMruEntry &operator =(const GMruEntry &e)
	{
		Display = e.Display;
		Raw = e.Raw;
		return *this;
	}
};

class GMruPrivate
{
public:
	int Size;
	LArray<GMruEntry*> Items;
	LSubMenu *Parent;
	LFileType *SelectedType;

	GMruPrivate()	
	{
		Parent = 0;
		Size = 10;
		SelectedType = 0;
	}

	~GMruPrivate()	
	{
		Items.DeleteObjects();
	}
};	

////////////////////////////////////////////////////////////////////
GMru::GMru()
{
	d = new GMruPrivate;
}

GMru::~GMru()
{
	DeleteObj(d);
}

bool GMru::SerializeEntry
(
	/// The displayable version of the reference (this should have any passwords blanked out)
	LString *Display,
	/// The form passed to the client software to open/save. (passwords NOT blanked)
	LString *Raw,
	/// The form safe to write to disk, if a password is present it must be encrypted.
	LString *Stored
)
{
	if (Raw && Raw->Get())
	{
		if (Stored)
			*Stored = *Raw;
		if (Display)
			*Display = *Raw;
	}
	else if (Stored && Stored->Get())
	{
		if (Display)
			*Display = *Stored;
		if (Raw)
			*Raw = *Stored;
	}
	
	return true;
}

void GMru::GetFileTypes(LFileSelect *Dlg, bool Write)
{
	Dlg->Type("All Files", LGI_ALL_FILES);
}

char *GMru::_GetCurFile()
{
	if (d->Items.Length())
		return d->Items[0]->Raw;
	return NULL;
}

LFileType *GMru::GetSelectedType()
{
	return d->SelectedType;
}

bool GMru::_OpenFile(const char *File, bool ReadOnly)
{
	bool Status = OpenFile(File, ReadOnly);

	if (Status)
	{
		AddFile(File, true);
	}
	else
	{
		RemoveFile(File);
	}

	return Status;
}

bool GMru::_SaveFile(const char *FileName)
{
	bool Status = false;

	if (FileName)
	{
		char File[MAX_PATH_LEN];
		strcpy_s(File, sizeof(File), FileName);
		
		LFileType *st;
		if (!LFileExists(File) &&
			(st = GetSelectedType()) &&
			st->Extension())
		{
			char *Cur = LGetExtension(File);
			if (!Cur)
			{
				// extract extension
				LString::Array a = LString(st->Extension()).Split(LGI_PATH_SEPARATOR);
				for (auto e: a)
				{
					LString::Array p = e.RSplit(".", 1);
					if (!p.Last().Equals("*"))
					{
						// bung the extension from the file type if not there
						char *Dot = strrchr(File, '.');
						if (Dot)
							Dot++;
						else
						{
							Dot = File + strlen(File);
							*Dot++ = '.';
						}

						strcpy_s(Dot, File+sizeof(File)-Dot, p.Last());
						break;
					}
				}
			}
		}
		
		Status = SaveFile(File);

		if (Status)
		{
			AddFile(File);
		}
		else
		{
			RemoveFile(File);
		}
	}

	return Status;
}

void GMru::_Update()
{
	if (d->Items.Length() > d->Size)
	{
		d->Items.Length(d->Size);
	}

	if (d->Parent)
	{
		// remove existing items.
		d->Parent->Empty();

		// add current items
		if (d->Items.Length() > 0)
		{
			for (int i=0; i<d->Items.Length(); i++)
			{
				GMruEntry *c = d->Items[i];
				d->Parent->AppendItem(c->Display ? c->Display : c->Raw, M_MRU_BASE + i, true);
			}
		}
		else
		{
			d->Parent->AppendItem("(none)", -1, false);
		}
	}
}

bool GMru::Set(LSubMenu *parent, int size)
{
	d->Parent = parent;
	if (size > 0)
	{
		d->Size = size;
	}

	_Update();

	return true;
}

const char *GMru::AddFile(const char *FileName, bool Update)
{
	#if DEBUG_LOG
	LgiTrace("%s:%i - AddFile(%s,%i)\n", _FL, FileName, Update);
	#endif
	if (!FileName)
		return NULL;

	auto Status = FileName;
	GMruEntry *c = NULL;
	for (int i=0; i<d->Items.Length(); i++)
	{
		GMruEntry *e = d->Items[i];
		#if DEBUG_LOG
		LgiTrace("[%i] cmp '%s' '%s'\n", i, e->Raw.Get(), FileName);
		#endif
		if (!LFileCompare(e->Raw, FileName))
		{
			// exact string being added.. just move to the top
			// no need to reallocate
			
			if (strcmp(e->Raw, FileName) &&
				e->Raw.Length() == strlen(FileName))
			{
				e->Raw = FileName; // This fixes any changes in case...
				#if DEBUG_LOG
				LgiTrace("Updating raw case\n");
				#endif
			}
			else
			{
				#if DEBUG_LOG
				LgiTrace("Moving to the top\n");
				#endif
			}
			
			d->Items.DeleteAt(i, true);
			d->Items.AddAt(0, e);
			c = e;
			break;
		}
	}

	if (!c)
	{
		c = new GMruEntry;
		c->Raw = FileName;
		if (SerializeEntry(&c->Display, &c->Raw, NULL))
		{
			#if DEBUG_LOG
			LgiTrace("Adding new entry %s %s\n", c->Raw.Get(), c->Display.Get());
			#endif
			d->Items.AddAt(0, c);
		}
		else
		{
			LAssert(0);
		}
	}

	if (Update)
	{
		// update
		_Update();
	}

	return Status;
}

void GMru::RemoveFile(const char *FileName, bool Update)
{
	// remove from list if there
	for (int i=0; i<d->Items.Length(); i++)
	{
		GMruEntry *e = d->Items[i];
		if (stricmp(e->Raw, FileName) == 0)
		{
			d->Items.DeleteAt(i);
			DeleteObj(e);
			break;
		}
	}

	if (Update)
	{
		_Update();
	}
}

bool GMru::DoFileDlg(LFileSelect &Select, bool Open)
{
	GetFileTypes(&Select, false);
	Select.ShowReadOnly(Open);
	if (Open ? Select.Open() : Select.Save())
	{
		d->SelectedType = Select.TypeAt(Select.SelectedType());
		if (Open)
			_OpenFile(Select.Name(), Select.ReadOnly());
		else
			_SaveFile(Select.Name());
	}
	else return false;

	return true;
}

bool GMru::OnCommand(int Cmd)
{
	bool Status = false;

	LViewI *Wnd = d->Parent->GetMenu() ? d->Parent->GetMenu()->WindowHandle() : 0;
	if (Wnd)
	{
		LFileSelect Select;
		Select.Parent(Wnd);
		Select.ClearTypes();
		d->SelectedType = 0;
		
		if (_GetCurFile())
		{
			if (LFileExists(_GetCurFile()))
				Select.Name(_GetCurFile());
			
			char Path[256];
			strcpy_s(Path, sizeof(Path), _GetCurFile());
			LTrimDir(Path);
			if (LDirExists(Path))
				Select.InitialDir(Path);
		}

		if (Cmd == IDM_OPEN)
		{
			Status = DoFileDlg(Select, true);
		}
		else if (Cmd == IDM_SAVEAS)
		{
			Status = DoFileDlg(Select, false);
		}
	}

	if (Cmd >= M_MRU_BASE &&
		Cmd < M_MRU_BASE + d->Items.Length())
	{
		int Index = Cmd - M_MRU_BASE;
		GMruEntry *c = d->Items[Index];
		if (c)
		{
			Status &= _OpenFile(c->Raw, false);
		}
	}

	return Status;
}

LMessage::Result GMru::OnEvent(LMessage *Msg)
{
	/*
	if (d->Parent &&
		MsgCode(Msg) == M_COMMAND)
	{
		#ifdef BEOS
		int32 Cmd = 0;
		int32 Event = 0;
		Msg->FindInt32("Cmd", &Cmd);
		Msg->FindInt32("Event", &Event);
		#else
		int Cmd = MsgA(Msg) & 0xffff;
		#endif

		OnCommand(Cmd);
	}
	*/

	return false;
}

bool GMru::Serialize(LDom *Store, const char *Prefix, bool Write)
{
	bool Status = false;
	LVariant v;

	if (Store && Prefix)
	{
		if (Write)
		{
			// add our keys
			int Idx = 0;
			char Key[64];
			LHashTbl<StrKey<char>, bool> Saved;

			for (int i=0; i<d->Items.Length(); i++)
			{
				GMruEntry *e = d->Items[i];
				LAssert(e->Raw.Get() != NULL);
				if (!Saved.Find(e->Raw))
				{
					Saved.Add(e->Raw, true);
					
					LString Stored;
					if (SerializeEntry(NULL, &e->Raw, &Stored)) // Convert Raw -> Stored
					{
						sprintf_s(Key, sizeof(Key), "%s.Item%i", Prefix, Idx++);
						Store->SetValue(Key, v = Stored.Get());
					}
					else LAssert(0);
				}
			}

			sprintf_s(Key, sizeof(Key), "%s.Items", Prefix);
			Store->SetValue(Key, v = (int)Idx);
		}
		else
		{
			// clear ourself
			d->Items.DeleteObjects();

			// read our keys in
			char Key[64];
			sprintf_s(Key, sizeof(Key), "%s.Items", Prefix);
			LVariant i;
			if (Store->GetValue(Key, i))
			{
				for (int n=0; n<i.CastInt32(); n++)
				{
					sprintf_s(Key, sizeof(Key), "%s.Item%i", Prefix, n);

					LVariant File;
					if (Store->GetValue(Key, File))
					{
						LString Stored = File.Str();
						LAssert(Stored.Get() != NULL);
						
						LAutoPtr<GMruEntry> e(new GMruEntry);
						if (SerializeEntry(&e->Display, &e->Raw, &Stored)) // Convert Stored -> Raw
						{
							d->Items.Add(e.Release());
						}
					}
				}
			}

			_Update();
		}
	}

	return Status;
}


