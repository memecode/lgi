#include "lgi/common/Lgi.h"
#include "lgi/common/WebDav.h"

////////////////////////////////////////////////////////////////////////////////
// The namespace prefixes used in a WebDAV response vary between servers, so
// match on the local name of the element rather than the prefixed name.
static bool XmlNameIs(LXmlTag *t, const char *Name)
{
	if (!t || !Name)
		return false;

	const char *Tag = t->GetTag();
	if (!Tag)
		return false;

	const char *Colon = strchr(Tag, ':');
	if (Colon)
		Tag = Colon + 1;

	const char *Want = strchr(Name, ':');
	if (Want)
		Want = Want + 1;

	return !Stricmp(Tag, Want);
}

static LXmlTag *XmlChild(LXmlTag *t, const char *Name)
{
	if (!t)
		return NULL;

	for (auto c: t->Children)
		if (XmlNameIs(c, Name))
			return c;

	return NULL;
}

static bool XmlStatusOk(LXmlTag *PropStat)
{
	auto Status = XmlChild(PropStat, "status");
	return !Status || strstr(Status->GetContent(), "200") != NULL;
}

////////////////////////////////////////////////////////////////////////////////
bool LFindXml(LArray<LXmlTag*> &Results, LXmlTag *t, const char *Name)
{
	if (XmlNameIs(t, Name))
		Results.Add(t);

	for (auto i: t->Children)
		LFindXml(Results, i, Name);

	return Results.Length() > 0;
}

////////////////////////////////////////////////////////////////////////////////
LWebdav::LWebdav(LString endPoint, LString user, LString pass, LCancel *cancel) :
	Cancel(cancel)
{
	EndPoint = endPoint;
	User = user;
	Pass = pass;

	LUri u(EndPoint);
	EndPointPath = u.sPath;
}

void LWebdav::PrettyPrint(LXmlTag &x)
{
	LStringPipe p;
	LXmlTree t;
	t.Write(&x, &p);
	auto s = p.NewLStr();
	LgiTrace("Pretty: %s\n", s.Get());
}

LString LWebdav::RelativeHref(const LString &Href) const
{
	LString h = Href;

	// The server can report either the path or a full URL for each item...
	if (h.StartsWith("http"))
	{
		ssize_t p = h.Find("/", h.Find("//") + 2);
		h = p >= 0 ? h(p, -1) : LString("/");
	}

	if (EndPointPath && h.StartsWith(EndPointPath))
		h = h(EndPointPath.Length(), -1);

	if (h.IsEmpty())
		return "/";

	return h.StartsWith("/") ? h : LString("/") + h;
}

LString LWebdav::FullHref(const LString &Href) const
{
	LString Base = EndPointPath.RStrip("/");
	if (Base.IsEmpty())
		Base = "/";

	LString h = Href;
	if (!h.StartsWith("/"))
		h = LString("/") + h;

	return Base + h;
}

void LWebdav::ParseMultistatus(LXmlTag &Root, LArray<FileProps> &Files, const char *DataTag)
{
	LArray<LXmlTag*> Responses;
	if (!LFindXml(Responses, &Root, "response"))
		return;

	for (auto r: Responses)
	{
		auto Href = XmlChild(r, "href");
		if (!Href)
			continue;

		LArray<LXmlTag*> Props;
		LFindXml(Props, r, "prop");

		for (auto p: Props)
		{
			if (p->Parent && XmlNameIs(p->Parent, "propstat") && !XmlStatusOk(p->Parent))
				continue;

			auto Len = XmlChild(p, "getcontentlength");
			auto Data = DataTag ? XmlChild(p, DataTag) : NULL;
			if (!Len && !Data)
				continue;

			auto &f = Files.New();
			f.Href = RelativeHref(Href->GetContent());

			if (Len)
				f.Length = Atoi(Len->GetContent(), 10, 0);

			auto lastmodified = XmlChild(p, "getlastmodified");
			if (lastmodified) f.LastMod = lastmodified->GetContent();

			auto etag = XmlChild(p, "getetag");
			if (etag)
			{
				f.Tag = LString(etag->GetContent()).Strip("\"");
			}

			auto contentType = XmlChild(p, "getcontenttype");
			if (contentType) f.ContentType = contentType->GetContent();

			if (Data) f.Data = Data->GetContent();
		}
	}
}

bool LWebdav::Request(Req &r, const char *Name, LString Resource)
{
	auto sock = GetSocket();
	LHttp http(Cancel);
	LUri u(EndPoint);
	if (!http.Open(sock, u.sHost, u.Port ? u.Port : HTTPS_PORT))
	{
		r.Status = false;
	}
	else
	{
		LStringPipe OutPipe, OutHdrsPipe;

		LString Delim("/");
		auto Path = u.sPath.SplitDelimit(Delim);
		Path += Resource.SplitDelimit(Delim);
		auto Res = Delim + Delim.Join(Path);
		u.sPath = Res;
		auto Full = u.ToString();
		LMemStream In(r.InBody.Get(), r.InBody.Length(), false);

		http.SetPlainAuth(User, Pass);
		r.Status = http.Request(Name, Full, &r.ProtocolStatus, r.InHdrs, r.InBody ? &In : NULL, &OutPipe, &OutHdrsPipe, &r.Encoding);
		r.OutHdrs = OutHdrsPipe.NewLStr();
		r.OutBody = OutPipe.NewLStr();
			
		// LgiTrace("%s\n%s\n", r.OutHdrs.Get(), r.OutBody.Get());
	}

	return r.Status;
}

LString::Array LWebdav::GetOptions(LString Resource)
{
	LString::Array opts;

	Req r;
	if (Request(r, "OPTIONS", Resource))
	{
		auto Lines = r.OutHdrs.SplitDelimit("\r\n");
		for (auto Ln: Lines)
		{
			auto p = Ln.Split(":", 1);
			if (p.Length() < 2)
				continue;
			if (p[0].Strip().Equals("Allow"))
			{
				auto o = p[1].Split(",");
				for (auto i: o)
					opts.Add(i.Strip());
			}
		}
	}

	for (auto o: opts) LgiTrace("opt=%s\n", o.Get());

	return opts;
}

bool LWebdav::PropFind(LArray<FileProps> &Files, LString Resource, int Depth)
{
	Req r;
	r.InBody = "<?xml version=\"1.0\" encoding=\"UTF-8\" ?>\n"
				"<propfind xmlns=\"DAV:\">\n"
				"	<allprop/>\n"
				"</propfind>";

	r.InHdrs.Printf("Depth: %i\r\n"
				"Content-Type: text/xml; charset=\"utf-8\"\r\n"
				"Content-Length: %i\r\n",
				Depth,
				(int)r.InBody.Length());

	if (!Request(r, "PROPFIND", Resource))
		return false;

	LXmlTag x;
	LXmlTree t;
	LMemStream m(r.OutBody.Get(), r.OutBody.Length(), false);
	if (!t.Read(&x, &m))
		return false;

	// PrettyPrint(&x);

	ParseMultistatus(x, Files, NULL);

	return true;
}

bool LWebdav::SupportsReport(LString Resource)
{
	if (ReportChecked)
		return ReportOk;

	ReportChecked = true;

	for (auto o: GetOptions(Resource))
		if (o.Equals("REPORT"))
		{
			ReportOk = true;
			break;
		}

	if (!ReportOk)
		LgiTrace("Server does not support REPORT, using one request per file.\n");

	return ReportOk;
}

// RFC 3258 bulk fetch. Ask the server for the content of every file in the
// collection with a single request, rather than a GET for each one.
bool LWebdav::Report(LArray<FileProps> &Files, LString Resource, const char *ReportType, LString::Array &Hrefs)
{
	if (!Hrefs.Length())
		return false;

	bool Calendar = Stricmp(ReportType, "addressbook-multiget") != 0;
	const char *DataTag = Calendar ? "calendar-data" : "address-data";
	const char *Namespace = Calendar ? "urn:ietf:params:xml:ns:caldav" : "urn:ietf:params:xml:ns:vcard-4.0";

	// A very large collection can make a request the server will reject, so ask
	// for the content a batch at a time...
	const size_t Batch = 100;

	bool GotData = false;
	LXmlTree t;

	for (size_t i = 0; i < Hrefs.Length(); i += Batch)
	{
		if (Cancel && Cancel->IsCancelled())
			break;

		size_t End = i + Batch;
		if (End > Hrefs.Length())
			End = Hrefs.Length();

		LString Body;
		Body.Printf("<?xml version=\"1.0\" encoding=\"UTF-8\" ?>\n"
					"<N:%s xmlns:D=\"DAV:\" xmlns:N=\"%s\">\n"
					"	<D:prop>\n"
					"		<D:getetag/>\n"
					"		<N:%s/>\n",
					ReportType,
					Namespace,
					DataTag);

		// The hrefs must be the full path to each file, as the server reported them...
		LString Line;
		for (size_t j = i; j < End; j++)
		{
			Line.Printf("	<D:href>%s</D:href>\n", t.EncodeEntities(FullHref(Hrefs[j]).Get()));
			Body += Line;
		}

		Line.Printf("</N:%s>", ReportType);
		Body += Line;

		Req r;
		r.InBody = Body;
		r.InHdrs.Printf("Depth: 1\r\n"
						"Content-Type: application/xml; charset=\"utf-8\"\r\n"
						"Content-Length: %i\r\n",
						(int)Body.Length());

		if (!Request(r, "REPORT", Resource))
			break;

		// Some servers answer 207 Multi-Status, others plain 200...
		if (r.ProtocolStatus != 200 && r.ProtocolStatus != 207)
		{
			LgiTrace("ReportFailed: %i\n%s\n%s\n", r.ProtocolStatus, r.OutHdrs.Get(), r.OutBody.Get());
			break;
		}

		LXmlTag x;
		LMemStream m(r.OutBody.Get(), r.OutBody.Length(), false);
		if (!t.Read(&x, &m))
			break;

		size_t Before = Files.Length();
		ParseMultistatus(x, Files, DataTag);
		if (Files.Length() > Before)
			GotData = true;
	}

	return GotData;
}

bool LWebdav::Get(const char *Resource, LString &Data)
{
	Req r;

	if (!Request(r, "GET", Resource))
		return false;

	if (r.ProtocolStatus != 200)
	{
		LgiTrace("GetFailed: %i\n%s\n%s\n", r.ProtocolStatus, r.OutHdrs.Get(), r.OutBody.Get());
		return false;
	}

	Data = r.OutBody;
		
	// LgiTrace("Webdav.data=%s\n", Data.Get());
		
	return true;
}

bool LWebdav::Put(const char *Resource, LString &Data)
{
	Req r;
	r.InBody = Data;
	r.InHdrs.Printf("Content-Type: text/calendar\r\n"
					"Content-Length: " LPrintfInt64 "\r\n",
					Data.Length());

	if (!Request(r, "PUT", Resource))
		return false;

	if (r.ProtocolStatus / 100 != 2)
	{
		LgiTrace("PutFailed: %i\n%s\n%s\n%s", r.ProtocolStatus, r.OutHdrs.Get(), r.OutBody.Get(), Data.Get());
		return false;
	}

	Data = r.OutBody;
	return true;
}

bool LWebdav::Delete(const char *Resource)
{
	Req r;

	if (!Request(r, "DELETE", Resource))
		return false;

	if (r.ProtocolStatus / 100 != 2)
	{
		LgiTrace("DeleteFailed: %s = %i\n%s\n%s\n", 
			Resource,
			r.ProtocolStatus,
			r.OutHdrs.Get(),
			r.OutBody.Get());
		return false;
	}

	return true;
}
