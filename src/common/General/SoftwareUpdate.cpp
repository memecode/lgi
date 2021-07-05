#include "lgi/common/Lgi.h"
#include "lgi/common/SoftwareUpdate.h"
#include "lgi/common/Net.h"
#include "lgi/common/Http.h"
#include "lgi/common/ProgressDlg.h"
#include "lgi/common/TextLabel.h"
#include "lgi/common/Button.h"
#include "lgi/common/LgiRes.h"
#include "lgi/common/Thread.h"

static char sHttpDownloadFailed[] = "HTTP download failed.";
static char sSocketConnectFailed[] = "Socket connect failed.";
static char sNoUpdateUri[] = "No update URI.";
static char sXmlParsingFailed[] = "XML parsing failed.";
static char sUnexpectedXml[] = "Unexpected XML.";
static char sUpdateError[] = "Update script error: %s";

struct LSoftwareUpdatePriv
{
	LAutoString Name;
	LAutoString UpdateUri;
	LAutoString Proxy;
	LAutoString Error;
	LAutoString TempPath;

	void SetError(int Id, const char *Def = 0)
	{
		Error.Reset(NewStr(LgiLoadString(Id, Def)));
	}

	class UpdateThread : public LThread
	{
		LSoftwareUpdatePriv *d;
		LSocket *s;
		LSoftwareUpdate::UpdateInfo *Info;
		IHttp Http;
		bool IncBetas;

	public:
		bool Status;

		UpdateThread(LSoftwareUpdatePriv *priv, LSoftwareUpdate::UpdateInfo *info, bool betas) :
		    LThread("SoftwareUpdateThread")
		{
			Info = info;
			d = priv;
			s = 0;
			IncBetas = betas;
			Status = false;

			Run();
		}

		~UpdateThread()
		{
			LgiAssert(IsExited());
		}

		void Cancel()
		{
			Http.Close();
			Info->Cancel = true;
		}

		int Main()
		{
			if (d->UpdateUri)
			{
				LUri Uri(d->UpdateUri);
				char Dir[256];
				int WordSize = sizeof(size_t) << 3;
				LString OsName = LGetOsName();
				int Os = LGetOs();
				if (Os == LGI_OS_WIN32 ||
					Os == LGI_OS_WIN64 ||
					Os == LGI_OS_WIN9X)
				{
					OsName.Printf("Win%i", WordSize);
				}

				sprintf_s(Dir, sizeof(Dir), "%s?name=%s&os=%s&betas=%i", Uri.sPath.Get(), (char*)d->Name, OsName.Get(), IncBetas);
				Uri.sPath = Dir;

				LString GetUri = Uri.ToString();
				
				#ifdef _DEBUG
				LgiTrace("UpdateURI=%s\n", GetUri.Get());
				#endif
				
				if (d->Proxy)
				{
				    LUri Proxy(d->Proxy);
				    if (Proxy.sHost)
					    Http.SetProxy(Proxy.sHost, Proxy.Port?Proxy.Port:HTTP_PORT);
				}
				
				LStringPipe RawXml;
				int ProtocolStatus = 0;
				LAutoPtr<LSocketI> s(new LSocket);
				if (Http.Open(s, Uri.sHost, Uri.Port))
				{
					IHttp::ContentEncoding Enc;
					if (Http.Get(GetUri, NULL, &ProtocolStatus, &RawXml, &Enc))
					{
						LXmlTree Tree;
						LXmlTag Root;
						if (Tree.Read(&Root, &RawXml))
						{
							LXmlTag *StatusCode;
							if (Root.IsTag("software") &&
								(StatusCode = Root.GetChildTag("status")))
							{
								if (StatusCode->GetContent() &&
									atoi(StatusCode->GetContent()) > 0)
								{
									LXmlTag *t;
									if ((t = Root.GetChildTag("version")))
										Info->Version.Reset(NewStr(t->GetContent()));
									if ((t = Root.GetChildTag("revision")))
										Info->Build.Reset(NewStr(t->GetContent()));
									if ((t = Root.GetChildTag("uri")))
										Info->Uri.Reset(NewStr(t->GetContent()));
									if ((t = Root.GetChildTag("date")))
									{
										Info->Date.SetFormat(GDTF_YEAR_MONTH_DAY);
										Info->Date.Set(t->GetContent());
									}

									Status = true;
								}
								else
								{
									LXmlTag *Msg = Root.GetChildTag("msg");
									LStringPipe p;
									p.Print(LgiLoadString(L_ERROR_UPDATE, sUpdateError), Msg?Msg->GetContent():(char*)"Unknown");
									d->Error.Reset(p.NewStr());
									LgiTrace("UpdateURI=%s\n", GetUri.Get());
								}
							}
							else d->SetError(L_ERROR_UNEXPECTED_XML, sUnexpectedXml);
						}
						else d->SetError(L_ERROR_XML_PARSE, sXmlParsingFailed);
					}
					else d->SetError(L_ERROR_HTTP_FAILED, sHttpDownloadFailed);
				}
				else d->SetError(L_ERROR_CONNECT_FAILED, sSocketConnectFailed);
			}
			else d->SetError(L_ERROR_NO_URI, sNoUpdateUri);

			return 0;
		}
	};

	class Spinner : public LDialog
	{
		UpdateThread *Watch;

	public:
		Spinner(LViewI *par, UpdateThread *watch)
		{
			Watch = watch;
			SetParent(par);
			LRect r(0, 0, 330, 400);
			SetPos(r);
			Name("Software Update");

			LRect c = GetClient();
			LTextLabel *t = new LTextLabel(	-1,
											10, 10,
											c.X()-20,
											-1,
											LgiLoadString(L_SOFTUP_CHECKING, "Checking for software update..."));
			if (t)
			{
				AddView(t);
				LButton *btn = new LButton(	IDCANCEL,
											(c.X()-70)/2,
											t->GetPos().y2 + 10,
											70,
											-1,
											LgiLoadString(L_BTN_CANCEL, "Cancel"));
				if (btn)
				{
					AddView(btn);
					r.y2 = (r.Y()-c.Y()) + 30 + t->Y() + btn->Y();
					SetPos(r);
					MoveToCenter();
					DoModal();
				}
			}
		}

		void OnCreate()
		{
			SetPulse(100);
		}

		void OnPulse()
		{
			if (Watch->IsExited())
			{
				EndModal(0);
			}
		}

		int OnNotify(LViewI *c, int f)
		{
			switch (c->GetId())
			{
				case IDCANCEL:
				{
					Watch->Cancel();
					break;
				}
			}

			return LDialog::OnNotify(c, f);
		}
	};

	class UpdateDownload : public LThread, public GProxyStream
	{
		LSoftwareUpdate::UpdateInfo *Info;
		LUri *Uri;
		LUri *Proxy;
		LStream *Local;
		LAutoString *Err;
		int *Status;

	public:
		int64 Progress, Total;

		UpdateDownload( LSoftwareUpdate::UpdateInfo *info,
		                LUri *uri,
		                LUri *proxy,		                
		                LStream *local,
		                LAutoString *err,
		                int *status) : LThread("UpdateDownload"), GProxyStream(local)
		{
			Info = info;
			Uri = uri;
			Proxy = proxy;
			Local = local;
			Err = err;
			Status = status;
			Progress = Total = 0;

			Run();
		}

		int64 SetSize(int64 Size) override
		{
			Total = Size;
			return s->SetSize(Size);
		}

		ssize_t Write(const void *b, ssize_t l, int f = 0) override
		{
			auto ret = s->Write(b, l, f);
			Progress = s->GetPos();
			return ret;
		}

		int Main() override
		{
			IHttp Http;
			if (Proxy->sHost)
				Http.SetProxy(Proxy->sHost, Proxy->Port?Proxy->Port:HTTP_PORT);

			LAutoPtr<LSocketI> s(new LSocket);
			IHttp::ContentEncoding Enc;
			if (!Http.Open(s, Uri->sHost, Uri->Port))
			{
				Err->Reset(NewStr(LgiLoadString(L_ERROR_CONNECT_FAILED, sSocketConnectFailed)));
			}
			else if (!Http.Get(Info->Uri, 0, Status, this, &Enc))
			{
				Err->Reset(NewStr(LgiLoadString(L_ERROR_HTTP_FAILED, sHttpDownloadFailed)));
			}

			return 0;
		}
	};
};

LSoftwareUpdate::LSoftwareUpdate(const char *SoftwareName,
								const char *UpdateUri,
								const char *ProxyUri,
								const char *OptionalTempPath)
{
	d = new LSoftwareUpdatePriv;
	d->Name.Reset(NewStr(SoftwareName));
	d->UpdateUri.Reset(NewStr(UpdateUri));
	d->Proxy.Reset(NewStr(ProxyUri));
	d->TempPath.Reset(NewStr(OptionalTempPath));
}

LSoftwareUpdate::~LSoftwareUpdate()
{
	DeleteObj(d);
}

bool LSoftwareUpdate::CheckForUpdate(UpdateInfo &Info, LViewI *WithUi, bool IncBetas)
{
	LSoftwareUpdatePriv::UpdateThread Update(d, &Info, IncBetas);
	
	if (WithUi)
	{
		LSoftwareUpdatePriv::Spinner s(WithUi, &Update);
	}
	else
	{
		while (!Update.IsExited())
		{
			LgiYield();
			LgiSleep(10);
		}
	}

	return Update.Status;
}

bool LSoftwareUpdate::ApplyUpdate(UpdateInfo &Info, bool DownloadOnly, LViewI *WithUi)
{
	if (!Info.Uri)
	{
		d->SetError(L_ERROR_NO_URI, sNoUpdateUri);
		return false;
	}

	LUri Uri(Info.Uri);
	if (!Uri.sPath)
	{
		d->SetError(L_ERROR_URI_ERROR, "No path in URI.");
		return false;
	}

	char *File = strrchr(Uri.sPath, '/');
	if (!File) File = Uri.sPath;
	else File++;

	char Tmp[MAX_PATH];
	if (d->TempPath)
		LgiMakePath(Tmp, sizeof(Tmp), d->TempPath, File);
	else
		LgiMakePath(Tmp, sizeof(Tmp), LGetSystemPath(LSP_TEMP), File);

	GFile Local;
	if (!Local.Open(Tmp, O_WRITE))
	{
		d->SetError(L_ERROR_OPENING_TEMP_FILE, "Can't open local temp file.");
		return false;
	}
	Local.SetSize(0);

	LProgressDlg *Dlg = new LProgressDlg;
	Dlg->SetDescription(LgiLoadString(L_SOFTUP_DOWNLOADING, "Downloading..."));
	Dlg->SetType("KiB");
	Dlg->SetScale(1.0 / 1024.0);

	int HttpStatus = 0;
	int64 Size = 0;
	LUri Proxy(d->Proxy);
	LSoftwareUpdatePriv::UpdateDownload Thread(&Info, &Uri, &Proxy, &Local, &d->Error, &HttpStatus);
	while (!Thread.IsExited())
	{
		LgiYield();
		LgiSleep(50);

		if (!Size)
		{
			if (Thread.Total)
				Dlg->SetLimits(0, Size = Thread.Total);
		}
		else
		{
			if (Thread.Progress > Dlg->Value())
				Dlg->Value(Thread.Progress);
		}
	}

	Local.Close();
	Dlg->EndModeless();

	if (HttpStatus != 200)
	{
		FileDev->Delete(Tmp);
		d->SetError(L_ERROR_HTTP_FAILED, sHttpDownloadFailed);
		return false;
	}

	if (!DownloadOnly)
	{
		char *Ext = LgiGetExtension(Tmp);
		if (Ext)
		{
			if (!_stricmp(Ext, "exe"))
			{
				// Execute to install...
				LExecute(Tmp);
				return true;
			}
			else
			{
				// Calculate the local path...
				char Path[MAX_PATH];
				LgiMakePath(Path, sizeof(Path), LGetExePath(), File);

				if (!_stricmp(Ext, "dll"))
				{
					// Copy to local folder...
					LError Err;
					if (!FileDev->Copy(Tmp, Path, &Err))
					{
						d->SetError(L_ERROR_COPY_FAILED, "Failed to copy file from temp folder to local folder.");
					}
				}
				else if (!_stricmp(Ext, "gz"))
				{
					// Unpack to local folder...
				}

				// Cleanup
				FileDev->Delete(Tmp);
			}
		}
	}

	return false;
}

char *LSoftwareUpdate::GetErrorMessage()
{
	return d->Error;
}
