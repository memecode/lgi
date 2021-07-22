#include <stdio.h>
#include <ctype.h>

#include "lgi/common/Lgi.h"
#include "lgi/common/DocView.h"
#include "lgi/common/Token.h"
#include "lgi/common/Edit.h"
#include "lgi/common/CheckBox.h"
#include "lgi/common/TextLabel.h"
#include "lgi/common/FileSelect.h"

#include "LgiIde.h"

#define DEBUG_HIST		0

/////////////////////////////////////////////////////////////////////////////////////
FindInFiles::FindInFiles(AppWnd *app, FindParams *params)
{
	TypeHistory = 0;
	FolderHistory = 0;

	SetParent(App = app);
	
	if (params)
	{
		Params = params;
		OwnParams = false;
	}
	else
	{
		Params = new FindParams;
		OwnParams = true;
	}

	if (LoadFromResource(IDD_FIND_IN_FILES))
	{
		MoveToCenter();

		if (GetViewById(IDC_TYPE_HISTORY, TypeHistory))
			TypeHistory->SetTargetId(IDC_FILE_TYPES);

		if (GetViewById(IDC_FOLDER_HISTORY, FolderHistory))
			FolderHistory->SetTargetId(IDC_DIR);
	}
}

FindInFiles::~FindInFiles()
{
	if (OwnParams)
		DeleteObj(Params);
}

void SerializeHistory(GHistory *h, const char *opt, LOptionsFile *p, bool Write)
{
	if (h && p)
	{
		LString last;
		last.Printf("%sSelect", opt);
		LViewI *Edit = NULL;
		h->GetWindow()->GetViewById(h->GetTargetId(), Edit);
		
		#if DEBUG_HIST
		LgiTrace("%s:%i - SerializeHistory '%s', Write=%i\n", _FL, last.Get(), Write);
		#endif

		LVariant v;
		if (Write)
		{
			LString::Array a;
			a.SetFixedLength(false);
			int64 i = 0;
			int64 Selected = h->Value();
			LString EdTxt;
			if (Edit)
				EdTxt = Edit->Name();

			for (auto s: *h)
			{
				if (EdTxt && EdTxt.Equals(s))
				{
					Selected = i;
				}

				a.Add(s);
				#if DEBUG_HIST
				LgiTrace("\t[%i]='%s'\n", i, s);
				#endif
				i++;
			}

			LString strs = LString("|").Join(a);
			p->SetValue(opt, v = strs.Get());
			#if DEBUG_HIST
			LgiTrace("\tstrs='%s'\n", strs.Get());
			#endif
			
			p->SetValue(last, v = Selected);
			#if DEBUG_HIST
			LgiTrace("\tv=%i\n", v.CastInt32());
			#endif
		}
		else
		{
			if (p->GetValue(opt, v))
			{
				LString raw(v.Str());
				LString::Array t = raw.Split("|");
				h->DeleteArrays();
				for (unsigned i=0; i<t.Length(); i++)
				{
					h->Insert(NewStr(t[i]));
					#if DEBUG_HIST
					LgiTrace("\t[%i]='%s'\n", i, t[i].Get());
					#endif
				}
				h->Update();
				
				if (p->GetValue(last, v))
				{
					h->Value(v.CastInt64());
					#if DEBUG_HIST
					LgiTrace("\tValue=%i\n", v.CastInt32());
					#endif
				}
				else LgiTrace("%s:%i - No option '%s'\n", _FL, last.Get());
			}
		}
	}
}

void FindInFiles::OnCreate()
{
	if (Params)
	{
		SetCtrlValue(IDC_ENTIRE_SOLUTION, Params->Type == FifSearchSolution);
		SetCtrlValue(IDC_SEARCH_DIR, Params->Type == FifSearchDirectory);

		SetCtrlName(IDC_LOOK_FOR, Params->Text);
		SetCtrlName(IDC_FILE_TYPES, Params->Ext);
		SetCtrlName(IDC_DIR, Params->Dir);
		
		SetCtrlValue(IDC_WHOLE_WORD, Params->MatchWord);
		SetCtrlValue(IDC_CASE, Params->MatchCase);
		SetCtrlValue(IDC_SUB_DIRS, Params->SubDirs);
		
		SerializeHistory(TypeHistory, "TypeHist", App->GetOptions(), false);
		SerializeHistory(FolderHistory, "FolderHist", App->GetOptions(), false);

		LEdit *v;
		if (GetViewById(IDC_LOOK_FOR, v))
		{
			v->Focus(true);
			v->SelectAll();
		}
	}
}

int FindInFiles::OnNotify(LViewI *v, int f)
{
	switch (v->GetId())
	{
		case IDC_SET_DIR:
		{
			LFileSelect s;
			s.Parent(this);
			s.InitialDir(GetCtrlName(IDC_DIR));
			if (s.OpenFolder())
			{
				int Idx = FolderHistory->Add(s.Name());
				if (Idx >= 0)
					FolderHistory->Value(Idx);
				else
					SetCtrlName(IDC_DIR, s.Name());
			}
			break;
		}
		case IDC_ENTIRE_SOLUTION:
		case IDC_SEARCH_DIR:
		{
			bool SearchSol = GetCtrlValue(IDC_ENTIRE_SOLUTION) != 0;
			SetCtrlEnabled(IDC_DIR, !SearchSol);
			SetCtrlEnabled(IDC_SET_DIR, !SearchSol);
			SetCtrlEnabled(IDC_FOLDER_HISTORY, !SearchSol);
			SetCtrlEnabled(IDC_SUB_DIRS, !SearchSol);
			break;
		}
		case IDOK:
		case IDCANCEL:
		{
			Params->Type = GetCtrlValue(IDC_ENTIRE_SOLUTION) != 0 ? FifSearchSolution :  FifSearchDirectory;
			Params->Text = GetCtrlName(IDC_LOOK_FOR);
			Params->Ext = GetCtrlName(IDC_FILE_TYPES);
			Params->Dir = GetCtrlName(IDC_DIR);
			
			Params->MatchWord = GetCtrlValue(IDC_WHOLE_WORD);
			Params->MatchCase = GetCtrlValue(IDC_CASE);
			Params->SubDirs = GetCtrlValue(IDC_SUB_DIRS);
		
			if (TypeHistory) TypeHistory->Add(Params->Ext);
			SerializeHistory(TypeHistory, "TypeHist", App->GetOptions(), true);
			if (FolderHistory) FolderHistory->Add(Params->Dir);
			SerializeHistory(FolderHistory, "FolderHist", App->GetOptions(), true);
			
			EndModal(v->GetId() == IDOK);
			break;
		}
	}
	
	return 0;
}

/////////////////////////////////////////////////////////////////////////////////////
class FindInFilesThreadPrivate
{
public:
	int AppHnd;
	LAutoPtr<FindParams> Params;
	bool Loop, Busy;
	LStringPipe Pipe;
	int64 Last;
};

FindInFilesThread::FindInFilesThread(int AppHnd) : LEventTargetThread("FindInFiles")
{
	d = new FindInFilesThreadPrivate;
	d->AppHnd = AppHnd;
	d->Loop = true;
	d->Busy = false;
	d->Last = 0;
}

FindInFilesThread::~FindInFilesThread()
{
	DeleteObj(d);
}

void FindInFilesThread::Log(const char *Str)
{
	PostThreadEvent(d->AppHnd, M_APPEND_TEXT, (GMessage::Param)Str, AppWnd::FindTab);
}

void FindInFilesThread::SearchFile(char *File)
{
	if (!File || !ValidStr(d->Params->Text))
		return;

	LFile f;
	if (!f.Open(File, O_READ))
	{
		LString s;
		s.Printf("Couldn't Read file '%s'\n", File);
		Log(NewStr(s));
		return;
	}

	auto Doc = f.Read();
	if (!Doc)
		return;

	auto Len = d->Params->Text.Length();
	const char *Text = d->Params->Text;
	
	char *LineStart = 0;
	int Line = 0;
	for (char *s = Doc; *s && d->Loop; s++)
	{
		if (*s == '\n')
		{
			Line++;
			LineStart = 0;
		}
		else
		{
			if (!LineStart)
				LineStart = s;
				
			bool Match = false;
			if (d->Params->MatchCase)
			{
				if (Text[0] == *s)
				{
					Match = strncmp(s, Text, Len) == 0;
				}
			}
			else
			{
				if (toupper(Text[0]) == toupper(*s))
				{
					Match = strnicmp(s, Text, Len) == 0;
				}
			}
			
			if (Match)
			{
				char *Eol = s + Len;
				while (*Eol && *Eol != '\n') Eol++;
				auto LineLen = Eol - LineStart;						
				
				bool StartOk = true;
				bool EndOk = true;
				if (d->Params->MatchWord)
				{
					if (s > Doc)
					{
						StartOk = IsWordBoundry(s[-1]);
					}
					
					EndOk = IsWordBoundry(s[Len]);
				}
				
				if (StartOk && EndOk)
				{
					static char Buf[1024];
					int Chars = snprintf(Buf, sizeof(Buf), "%s:%i:%.*s\n", File, Line + 1, (int)LineLen, LineStart);
					d->Pipe.Push(Buf, Chars);
					
					int64 Now = LCurrentTime();
					if (Now > d->Last  + 500)
						Log(d->Pipe.NewStr());
				}
				s = Eol - 1;
			}
		}
	}
}

bool FindInFilesCallback(void *UserData, char *Path, LDirectory *Dir)
{
	if (Dir->IsDir())
	{
		char *p = Dir->GetName();
		if
		(
			p
			&&
			(
				stricmp(p, ".svn") == 0
				||
				stricmp(p, ".git") == 0
			)
		)
			return false;
	}
	
	return true;
}

void FindInFilesThread::Stop()
{
	d->Loop = false;
	while (d->Busy)
		LgiSleep(1);
}

GMessage::Result FindInFilesThread::OnEvent(GMessage *Msg)
{
	switch (Msg->Msg())
	{
		case M_START_SEARCH:
		{
			d->Params.Reset((FindParams*)Msg->A());
			if (d->AppHnd &&
				d->Params &&
				ValidStr(d->Params->Text))
			{
				char Msg[256];

				PostThreadEvent(d->AppHnd, M_SELECT_TAB, AppWnd::FindTab);
				
				d->Loop = true;
				d->Busy = true;
				snprintf(Msg, sizeof(Msg), "Searching for '%s'...\n", d->Params->Text.Get());
				Log(NULL);
				Log(NewStr(Msg));

				LArray<const char*> Ext;
				GToken e(d->Params->Ext, ";, ");
				for (int i=0; i<e.Length(); i++)
				{
					Ext.Add(e[i]);
				}
		
				LArray<char*> Files;
				if (d->Params->Type == FifSearchSolution)
				{
					// Do the extension filtering...
					for (auto p: d->Params->ProjectFiles)
					{
						if (p)
						{
							const char *Leaf = LGetLeaf(p);
							for (auto e: Ext)
							{
								if (MatchStr(e, Leaf))
									Files.Add(NewStr(p));
							}
						}
						else LgiTrace("%s:%i - Null string in project files array.\n", _FL);
					}
				}
				else
				{
					// Find the files recursively...
					LRecursiveFileSearch(d->Params->Dir, &Ext, &Files, 0, 0, FindInFilesCallback);
				}

				if (Files.Length() > 0)
				{			
					sprintf(Msg, "in %i files...\n", (int)Files.Length());
					Log(NewStr(Msg));

					for (int i=0; i<Files.Length() && d->Loop; i++)
					{
						auto f = Files[i];
						if (f)
						{
							auto Dir = strrchr(f, DIR_CHAR);
							if (!Dir || Dir[1] != '.')
								SearchFile(f);
						}
						else LgiTrace("%s:%i - NULL Ptr in list????", _FL);
					}
			
					char *Str = d->Pipe.NewStr();
					if (Str)
						Log(Str);
			
					Files.DeleteArrays();

					Log(NewStr("Done.\n"));
				}
				else
				{
					Log(NewStr("No files matched.\n"));
				}
				
				d->Busy = false;
			}
			break;
		}
	}

	return 0;
}