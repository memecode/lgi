#include <stdio.h>

#include "lgi/common/Lgi.h"
#include "lgi/common/DocView.h"

#define SubtractPtr(a, b)		((a)-(b))

const char *LDocView::WhiteSpace		= " \t\r\n";
const char *LDocView::Delimiters		= "!@#$%^&*()'\":;,.<>/?[]{}-=+\\|`~";
const char *LDocView::UrlDelim			= "!~/:%+-?@&$#._=,;*()|";

LDocumentEnv::LDocumentEnv(LDocView *v)
{
	if (v)
	{
		Viewers.Add(v);
	}
}

LDocumentEnv::~LDocumentEnv()
{
	for (uint32_t i=0; i<Viewers.Length(); i++)
	{
		Viewers[i]->Environment = 0;
	}
}

int LDocumentEnv::NextUid()
{
	if (!Lock(_FL))
		return -1;
	int Uid = 0;
	for (auto v : Viewers)
		Uid = MAX(Uid, v->GetDocumentUid() + 1);
	Unlock();
	return Uid;
}

void LDocumentEnv::OnDone(LAutoPtr<LThreadJob> j)
{
	LoadJob *ld = dynamic_cast<LoadJob*>(j.Get());
	if (ld)
	{
		if (Lock(_FL))
		{
			LDocView *View = NULL;
			for (unsigned i=0; i<Viewers.Length(); i++)
			{
				auto Uid = Viewers[i]->GetDocumentUid();
				if (Uid == ld->UserUid)
				{
					View = Viewers[i];
					break;
				}
			}
			
			if (View)
			{
				View->OnContent(ld);
				j.Release();
			}
			Unlock();
		}
	}
	else LAssert(!"RTTI failed.");
}

//////////////////////////////////////////////////////////////////////////////////
char16 *ConvertToCrLf(char16 *Text)
{
	if (Text)
	{
		// add '\r's
		int Lfs = 0;
		int Len = 0;
		char16 *s=Text;
		for (; *s; s++)
		{
			if (*s == '\n') Lfs++;
			Len++;
		}

		char16 *Temp = new char16[Len+Lfs+1];
		if (Temp)
		{
			char16 *d=Temp;
			s = Text;
			for (; *s; s++)
			{
				if (*s == '\n')
				{
					*d++ = 0x0d;
					*d++ = 0x0a;
				}
				else if (*s == '\r')
				{
					// ignore
				}
				else
				{
					*d++ = *s;
				}
			}
			*d++ = 0;

			DeleteObj(Text);
			return Temp;
		}
	}

	return Text;
}

//////////////////////////////////////////////////////////////////////////////////
#include "lgi/common/Net.h"
LDocumentEnv::LoadType LDefaultDocumentEnv::GetContent(LoadJob *&j)
{
	if (!j || !ValidStr(j->Uri))
		return LoadError;

	char p[MAX_PATH_LEN];
	char *FullPath = NULL;
	char *FileName = NULL;
	LUri u(j->Uri);
	if (u.sProtocol && !_stricmp(u.sProtocol, "file"))
		FileName = u.sPath;
	else
		FileName = j->Uri;

	if (FileName)
	{
		LGetSystemPath(LSP_APP_INSTALL, p, sizeof(p));
		LMakePath(p, sizeof(p), p, FileName);
		if (LFileExists(p))
		{
			FullPath = p;
		}
		else
		{
			auto f = LFindFile(FileName);
			if (f)
				strcpy_s(FullPath = p, sizeof(p), f);
		}
	}			

	if (LFileExists(FullPath))
	{
		LString Mt = LGetFileMimeType(FullPath);
		if (Mt.Find("image/") == 0)
		{
			j->pDC.Reset(GdcD->Load(p));
			return LoadImmediate;
		}
		
		j->Filename = FullPath;
		return LoadImmediate;
	}

	return LoadError;
}

bool LDefaultDocumentEnv::OnNavigate(LDocView *Parent, const char *Uri)
{
	if (Uri)
	{
		if
		(
			_strnicmp(Uri, "mailto:", 7) == 0
			||
			(
				strchr(Uri, '@') != 0
				&&
				strchr(Uri, '/') == 0
			)
		)
		{
			// email
			LArray<LAppInfo*> Apps;
			if (LGetAppsForMimeType("application/email", Apps))
			{
				LAppInfo *First = Apps[0];
				LStringPipe a;

				char *Arg = First->Params ? strstr(First->Params, "%1") : 0;
				if (Arg)
				{
					// change '%1' into the email address in question
					a.Write(First->Params, (int) (Arg-First->Params));
					a.Print("%s%s", Uri, Arg + 2);
				}
				else
				{
					// no argument place holder... just pass as a normal arg
					a.Print(" %s", Uri);
				}

				LAutoString Exe(TrimStr(First->Path, "\"\'"));
				LAutoString Args(a.NewStr());

				LString ErrorMsg;
				if (LExecute(Exe, Args, ".", &ErrorMsg))
					return true;

				LgiMsg(Parent, "Failed to open '%s':\n%s", LAppInst->LBase::Name(), MB_OK, Exe.Get(), ErrorMsg.Get());
			}
			else
			{
				LgiMsg(Parent, "Couldn't get app to handle email.", LAppInst->LBase::Name(), MB_OK);
			}
		}
		else
		{
			// webpage
			LString ErrorMsg;
			if (LExecute(Uri, NULL, NULL, &ErrorMsg))
				return true;

			LgiMsg(Parent, "Failed to open '%s':\n%s", LAppInst->LBase::Name(), MB_OK, Uri, ErrorMsg.Get());
		}
	}

	return false;
}

