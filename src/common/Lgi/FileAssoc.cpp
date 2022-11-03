#include <stdio.h>

#include "Lgi.h"
#include "LFileAssoc.h"

/////////////////////////////////////////////////////////////////////
LFileAssocAction::LFileAssocAction()
{
	App = 0;
	Action = 0;
}

LFileAssocAction::~LFileAssocAction()
{
	DeleteArray(App);
	DeleteArray(Action);
}

/////////////////////////////////////////////////////////////////////
class LFileAssocPrivate
{
public:
	char *Mime;
	char *Ext;

	#ifdef WIN32
	char *Class;
	#endif

	LFileAssocPrivate(char *mime, char *ext)
	{
		Mime = NewStr(mime);
		Ext = NewStr(ext);

		#ifdef WIN32
		Class = 0;

		if (Mime AND !Ext)
		{
			char *d = strrchr(Mime, '/');
			if (d)
			{
				if (d[1] == '.')
				{
					Ext = NewStr(d + 1);
				}
				else
				{
					GRegKey k("HKEY_CLASSES_ROOT\\MIME\\Database\\Content Type\\%s", Mime);
					char *e = k.GetStr("Extension");
					if (e)
					{
						Ext = NewStr(e);
					}
				}
			}

			GRegKey k("HKEY_CLASSES_ROOT\\%s", Ext);
			Class = NewStr(k.GetStr());
		}
		else if (Ext AND !Mime)
		{
			GRegKey k("HKEY_CLASSES_ROOT\\%s", Ext);
			char *m = k.GetStr("Content Type");
			if (m)
			{
				Mime = NewStr(m);
			}
			else
			{
				char s[256];
				sprintf(s, "application/%s", Ext);
				Mime = NewStr(s);
			}
			Class = NewStr(k.GetStr());
		}
		#endif
	}

	~LFileAssocPrivate()
	{
		DeleteArray(Mime);
		DeleteArray(Ext);

		#ifdef WIN32
		DeleteArray(Class);
		#endif
	}
};

LFileAssoc::LFileAssoc(char *mime, char *ext)
{
	d = new LFileAssocPrivate(mime, ext);
}

LFileAssoc::~LFileAssoc()
{
	DeleteObj(d);
}

char *LFileAssoc::GetMimeType()
{
	return d->Mime;
}

char *LFileAssoc::GetExtension()
{
	return d->Ext;
}

bool LFileAssoc::GetExtensions(LArray<char*> &Ext)
{
	bool Status = false;

	#ifdef WIN32

	if (d->Ext)
	{
		Ext.Add(NewStr(d->Ext + 1));
		Status = true;
	}
	
	#endif

	return Status;
}

bool LFileAssoc::GetActions(LArray<LFileAssocAction*> &Actions)
{
	bool Status = false;

	#ifdef WIN32

	if (d->Class)
	{
		GRegKey k("HKEY_CLASSES_ROOT\\%s\\shell", d->Class);
		List<char> a;
		if (k.GetKeyNames(a))
		{
			for (char *s = a.First(); s; s = a.Next())
			{
				LFileAssocAction *Act = new LFileAssocAction;
				if (Act)
				{
					Actions.Add(Act);
					Act->Action = s;
					Status = true;

					GRegKey n("HKEY_CLASSES_ROOT\\%s\\shell\\%s\\command", d->Class, s);
					Act->App = NewStr(n.GetStr());
				}
			}
		}
	}
	
	#endif

	return Status;
}

bool LFileAssoc::SetAction(LFileAssocAction *Action)
{
	bool Status = false;

	#ifdef WIN32

	if (Action)
	{
		GRegKey k("HKEY_CLASSES_ROOT\\%s\\shell\\%s\\command", d->Class, Action->Action);
		if (!k.IsOk())
		{
			k.Create();
		}
		k.SetStr(0, Action->App);
		Status = true;
	}
	
	#endif

	return Status;
}

bool LFileAssoc::SetIcon(char *File, int Index)
{
	bool Status = false;

	#ifdef WIN32

	if (File)
	{
		GRegKey k("HKEY_CLASSES_ROOT\\%s\\DefaultIcon", d->Class);
		if (!k.IsOk())
		{
			k.Create();
		}

		char s[300];
		sprintf(s, "%s,%i", File, Index);
		k.SetStr(0, s);
		Status = true;
	}
	
	#endif

	return Status;
}
