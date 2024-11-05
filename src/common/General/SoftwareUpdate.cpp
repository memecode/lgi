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
	LString Name;
	LString UpdateUri;
	LString Proxy;
	LString Error;
	LString TempPath;

	bool SetError(int Id, const char *Def = 0)
	{
		Error = LLoadString(Id, Def);
		return false;
	}

	class UpdateThread :
		public LThread
	{
		LSoftwareUpdate *update = NULL;
		LSoftwareUpdatePriv *d = NULL;
		LSocket *s = NULL;
		LHttp http;
		bool incBetas = false;

	public:
		LSoftwareUpdate::UpdateInfo info;
		LSoftwareUpdate::UpdateCb callback;
		bool Status = false;

		UpdateThread(LSoftwareUpdate *softwareUpdate,
					bool betas,
					LSoftwareUpdate::UpdateCb cb) :
		    LThread("SoftwareUpdateThread")
		{
			update = softwareUpdate;
			d = update->d;
			incBetas = betas;
			callback = cb;

			Run();
		}

		~UpdateThread()
		{
			LAssert(IsExited());
		}

		void Cancel()
		{
			http.Close();
			info.Cancel = true;
		}

		int Complete(int Ret)
		{
			info.HasUpdate = Status;

			if (callback)
			{
				callback(Ret == 0 ? &info : NULL, update->GetErrorMessage());
				DeleteOnExit = true;
				delete update;
			}
			
			return Ret;
		}

		int Main()
		{
			if (!d->UpdateUri)
			{
				d->SetError(L_ERROR_NO_URI, sNoUpdateUri);
				return Complete(-1);
			}

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

			sprintf_s(Dir, sizeof(Dir), "%s?name=%s&os=%s&betas=%i", Uri.sPath.Get(), (char*)d->Name, OsName.Get(), incBetas);
			Uri.sPath = Dir;

			LString GetUri = Uri.ToString();
				
			#ifdef _DEBUG
			LgiTrace("UpdateURI=%s\n", GetUri.Get());
			#endif
				
			if (d->Proxy)
			{
				LUri Proxy(d->Proxy);
				if (Proxy.sHost)
					http.SetProxy(Proxy.sHost, Proxy.Port?Proxy.Port:HTTP_PORT);
			}
				
			LStringPipe RawXml;
			int ProtocolStatus = 0;
			LAutoPtr<LSocketI> s(new LSocket);
			if (!http.Open(s, Uri.sHost, Uri.Port))
			{
				d->SetError(L_ERROR_CONNECT_FAILED, sSocketConnectFailed);
				LgiTrace("%s:%i - Bad connect: %s:%i\n", _FL, Uri.sHost.Get(), Uri.Port);
				return Complete(-2);
			}

			LHttp::ContentEncoding Enc;
			if (!http.Get(GetUri, NULL, &ProtocolStatus, &RawXml, &Enc))
			{
				d->SetError(L_ERROR_HTTP_FAILED, sHttpDownloadFailed);
				LgiTrace("%s:%i - Bad URI: %s\n", _FL, GetUri.Get());
				return Complete(-3);
			}

			auto Xml = RawXml.NewLStr();
			LMemStream XmlStream(Xml.Get(), Xml.Length(), false);
			LXmlTree Tree;
			LXmlTag Root;
			if (!Tree.Read(&Root, &XmlStream))
			{
				d->SetError(L_ERROR_XML_PARSE, sXmlParsingFailed);
				LgiTrace("%s:%i - Bad XML: %s\n", _FL, Xml.Get());
				return Complete(-4);
			}

			LXmlTag *StatusCode;
			if (!Root.IsTag("software") ||
				!(StatusCode = Root.GetChildTag("status")))
			{
				d->SetError(L_ERROR_UNEXPECTED_XML, sUnexpectedXml);
				LgiTrace("%s:%i - Bad XML: %s\n", _FL, Xml.Get());
				return Complete(-5);
			}

			if (StatusCode->GetContent() &&
				atoi(StatusCode->GetContent()) > 0)
			{
				LXmlTag *t;
				if ((t = Root.GetChildTag("version")))
					info.Version = t->GetContent();
				if ((t = Root.GetChildTag("revision")))
					info.Build = t->GetContent();
				if ((t = Root.GetChildTag("uri")))
					info.Uri = t->GetContent();
				if ((t = Root.GetChildTag("date")))
				{
					info.Date.SetFormat(GDTF_YEAR_MONTH_DAY);
					info.Date.Set(t->GetContent());
				}

				Status = true;
			}
			else
			{
				LXmlTag *Msg = Root.GetChildTag("msg");
				LStringPipe p;
				p.Print(LLoadString(L_ERROR_UPDATE, sUpdateError), Msg?Msg->GetContent():(char*)"Unknown");
				d->Error = p.NewLStr();
				LgiTrace("UpdateURI=%s\n", GetUri.Get());
			}

			return Complete(0);
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
											LLoadString(L_SOFTUP_CHECKING, "Checking for software update..."));
			if (t)
			{
				AddView(t);
				LButton *btn = new LButton(	IDCANCEL,
											(c.X()-70)/2,
											t->GetPos().y2 + 10,
											70,
											-1,
											LLoadString(L_BTN_CANCEL, "Cancel"));
				if (btn)
				{
					AddView(btn);
					r.y2 = (r.Y()-c.Y()) + 30 + t->Y() + btn->Y();
					SetPos(r);
					MoveToCenter();
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
				SetPulse();
				EndModal(0);
			}
		}

		int OnNotify(LViewI *c, const LNotification &n) override
		{
			switch (c->GetId())
			{
				case IDCANCEL:
				{
					Watch->Cancel();
					break;
				}
			}

			return LDialog::OnNotify(c, n);
		}
	};

	class UpdateDownload : public LThread, public LProxyStream
	{
		const LSoftwareUpdate::UpdateInfo *Info;
		LUri *Uri;
		LUri *Proxy;
		LStream *Local;
		LString *Err;
		int *Status;

	public:
		int64 Progress, Total;

		UpdateDownload( const LSoftwareUpdate::UpdateInfo *info,
		                LUri *uri,
		                LUri *proxy,		                
		                LStream *local,
		                LString *err,
		                int *status) : LThread("UpdateDownload"), LProxyStream(local)
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
			LHttp Http;
			if (Proxy->sHost)
				Http.SetProxy(Proxy->sHost, Proxy->Port?Proxy->Port:HTTP_PORT);

			LAutoPtr<LSocketI> s(new LSocket);
			LHttp::ContentEncoding Enc;
			if (!Http.Open(s, Uri->sHost, Uri->Port))
			{
				*Err = LLoadString(L_ERROR_CONNECT_FAILED, sSocketConnectFailed);
			}
			else if (!Http.Get(Info->Uri, 0, Status, this, &Enc))
			{
				*Err = LLoadString(L_ERROR_HTTP_FAILED, sHttpDownloadFailed);
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
	d->Name = SoftwareName;
	d->UpdateUri = UpdateUri;
	d->Proxy = ProxyUri;
	d->TempPath = OptionalTempPath;
}

LSoftwareUpdate::~LSoftwareUpdate()
{
	DeleteObj(d);
}

void LSoftwareUpdate::CheckForUpdate(UpdateCb callback,
									LViewI *WithUi,
									bool IncBetas)
{
	auto thread = new LSoftwareUpdatePriv::UpdateThread(this, IncBetas, WithUi ? NULL/* UI will call the callback */ : callback);	
	if (WithUi)
	{
		auto s = new LSoftwareUpdatePriv::Spinner(WithUi, thread);
		s->DoModal([callback, thread, this](auto dlg, auto code)
		{
			if (callback)
				callback(&thread->info, d->Error);

			delete thread;
			delete this;
		});
	}
}

class ApplyUpdateState : public LProgressDlg
{
	LSoftwareUpdatePriv *d;

	const LSoftwareUpdate::UpdateInfo *Info = NULL;
	LUri Uri;
	LFile Local;
	char *File = NULL;
	char Tmp[MAX_PATH_LEN];
	bool ApplyStatus = false;
	bool DownloadOnly = false;
	int HttpStatus = 0;
	int64 Size = 0;
	LAutoPtr<LSoftwareUpdatePriv::UpdateDownload> Thread;

	std::function<void(bool)> Callback;

public:
	ApplyUpdateState(LSoftwareUpdatePriv *priv, const LSoftwareUpdate::UpdateInfo *info, LUri uri, bool downloadOnly, std::function<void(bool)> callback) :
		d(priv),
		Info(info),
		Uri(uri),
		DownloadOnly(downloadOnly),
		Callback(callback)
	{
		File = strrchr(Uri.sPath, '/');
		if (!File) File = Uri.sPath;
		else File++;

		if (d->TempPath)
			LMakePath(Tmp, sizeof(Tmp), d->TempPath, File);
		else
			LMakePath(Tmp, sizeof(Tmp), LGetSystemPath(LSP_TEMP), File);

		if (!Local.Open(Tmp, O_WRITE))
		{
			d->SetError(L_ERROR_OPENING_TEMP_FILE, "Can't open local temp file.");
			if (Callback) Callback(false);
		}
		else
		{
			Local.SetSize(0);

			SetDescription(LLoadString(L_SOFTUP_DOWNLOADING, "Downloading..."));
			SetType("KiB");
			SetScale(1.0 / 1024.0);

			LUri Proxy(d->Proxy);
			
			if (!Thread.Reset(new LSoftwareUpdatePriv::UpdateDownload(Info, &Uri, &Proxy, &Local, &d->Error, &HttpStatus)))
			{
				d->SetError(L_ERROR_NO_MEMORY, "Alloc failed.");
				if (Callback) Callback(false);
				Thread.Reset(); // Just bail
			}
		}
	}
	
	~ApplyUpdateState()
	{
		if (Callback)
			Callback(ApplyStatus);
	}

	void OnPulse()
	{
		if (Thread)
		{
			if (Thread->IsExited())
			{
				Thread.Reset();
				AfterThread();
			}
			else
			{
				if (!Size)
				{
					if (Thread->Total)
						SetRange(Size = Thread->Total);
				}
				else
				{
					if (Thread->Progress > Value())
						Value(Thread->Progress);
				}
			}
		}
		else
		{
			Quit();
			return;
		}
		
		LProgressDlg::OnPulse();
	}
	
	void AfterThread()
	{
		Local.Close();
		EndModeless();

		if (HttpStatus != 200)
		{
			FileDev->Delete(Tmp);
			d->SetError(L_ERROR_HTTP_FAILED, sHttpDownloadFailed);
			return;
		}

		if (!DownloadOnly)
		{
			char *Ext = LGetExtension(Tmp);
			if (Ext)
			{
				if (!_stricmp(Ext, "exe"))
				{
					// Execute to install...
					LExecute(Tmp);
					ApplyStatus = true;
					return;
				}
				else
				{
					// Calculate the local path...
					char Path[MAX_PATH_LEN];
					LMakePath(Path, sizeof(Path), LGetExePath(), File);

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
	}
};

void LSoftwareUpdate::ApplyUpdate(	const UpdateInfo *Info,
									bool DownloadOnly,
									LViewI *WithUi,
									std::function<void(bool)> Callback)
{
	if (!Info || !Info->Uri)
	{
		d->SetError(L_ERROR_NO_URI, sNoUpdateUri);
		if (Callback) Callback(false);
		return;
	}		

	LUri Uri(Info->Uri);
	if (!Uri.sPath)
	{
		d->SetError(L_ERROR_URI_ERROR, "No path in URI.");
		if (Callback) Callback(false);
		return;
	}

	new ApplyUpdateState(d, Info, Uri, DownloadOnly, Callback);
}

const char *LSoftwareUpdate::GetErrorMessage()
{
	return d->Error;
}
