#include "Lgi.h"
#include "LWebdav.h"

////////////////////////////////////////////////////////////////////////////////
bool LFindXml(GArray<GXmlTag*> &Results, GXmlTag *t, const char *Name)
{
	if (t->IsTag(Name))
		Results.Add(t);

	for (auto i: t->Children)
		LFindXml(Results, i, Name);

	return Results.Length() > 0;
}

////////////////////////////////////////////////////////////////////////////////
LWebdav::LWebdav(GString endPoint, GString user, GString pass)
{
	EndPoint = endPoint;
	User = user;
	Pass = pass;

	GUri u(EndPoint);
	EndPointPath = u.Path;
}

void LWebdav::PrettyPrint(GXmlTag &x)
{
	GStringPipe p;
	GXmlTree t;
	t.Write(&x, &p);
	auto s = p.NewGStr();
	LgiTrace("Pretty: %s\n", s.Get());
}

bool LWebdav::Request(Req &r, const char *Name, GString Resource)
{
	auto sock = GetSocket();
	IHttp http;
	GUri u(EndPoint);
	if (!http.Open(sock, u.Host, u.Port ? u.Port : HTTPS_PORT))
	{
		r.Status = false;
	}
	else
	{
		GStringPipe OutPipe, OutHdrsPipe;

		GString Delim("/");
		auto Path = GString(u.Path).SplitDelimit(Delim);
		Path += Resource.SplitDelimit(Delim);
		auto Res = Delim + Delim.Join(Path);
		DeleteArray(u.Path);
		u.Path = NewStr(Res);
		auto Full = u.GetUri();
		GMemStream In(r.InBody.Get(), r.InBody.Length(), false);

		http.SetAuth(User, Pass);
		r.Status = http.Request(Name, Full, &r.ProtocolStatus, r.InHdrs, r.InBody ? &In : NULL, &OutPipe, &OutHdrsPipe, &r.Encoding);
		r.OutHdrs = OutHdrsPipe.NewGStr();
		r.OutBody = OutPipe.NewGStr();
			
		// LgiTrace("%s\n%s\n", r.OutHdrs.Get(), r.OutBody.Get());
	}

	return r.Status;
}

GString::Array LWebdav::GetOptions(GString Resource)
{
	GString::Array opts;

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

bool LWebdav::PropFind(GArray<FileProps> &Files, GString Resource, int Depth)
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

	GXmlTag x;
	GXmlTree t;
	GMemStream m(r.OutBody.Get(), r.OutBody.Length(), false);
	if (!t.Read(&x, &m))
		return false;

	// PrettyPrint(&x);

	GArray<GXmlTag*> Responses;
	if (!LFindXml(Responses, &x, "d:response"))
		return false;

	for (auto r: Responses)
	{
		auto Href = r->GetChildTag("d:href");
		GXmlTag *FileProps = NULL;
		GArray<GXmlTag*> Props;
		if (LFindXml(Props, r, "d:prop"))
		{
			for (auto p: Props)
			{
				auto Len = p->GetChildTag("d:getcontentlength");
				if (Len)
					FileProps = p;
			}
		}

		if (Href && FileProps)
		{
			auto &f = Files.New();
			f.Href = Href->GetContent();
			if (f.Href.Find(EndPointPath) == 0)
			{
				f.Href = f.Href(EndPointPath.Length(),-1);
			}

			auto len = FileProps->GetChildTag("d:getcontentlength");
			f.Length = Atoi(len->GetContent(), 10, 0);
			auto lastmodified = FileProps->GetChildTag("d:getlastmodified");
			if (lastmodified) f.LastMod = lastmodified->GetContent();
			auto etag = FileProps->GetChildTag("d:getetag");
			if (etag)
			{
				f.Tag = etag->GetContent();
				f.Tag = f.Tag.Strip("\"");
			}
			auto contentType = FileProps->GetChildTag("d:getcontenttype");
			if (contentType) f.ContentType = contentType->GetContent();
		}
	}

	return true;
}

bool LWebdav::Get(GString &Data, const char *Resource)
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

bool LWebdav::Put(GString &Data, const char *Resource)
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
		LgiTrace("DeleteFailed: %i\n%s\n%s\n", r.ProtocolStatus, r.OutHdrs.Get(), r.OutBody.Get());
		return false;
	}

	return true;
}
