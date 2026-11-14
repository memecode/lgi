#ifndef _LWEBDAV_H_
#define _LWEBDAV_H_

#include "lgi/common/Http.h"
#include "OpenSSLSocket.h"

extern bool LFindXml(LArray<LXmlTag*> &Results, LXmlTag *t, const char *Name);

class LWebdav
{
protected:
	LString EndPoint, EndPointPath, User, Pass;
	LCancel *Cancel = NULL;

	bool ReportChecked = false;
	bool ReportOk = false;

	LAutoPtr<LSocketI> GetSocket()
	{
		LAutoPtr<LSocketI> s;

		SslSocket *ssl = new SslSocket();
		if (ssl)
		{
			ssl->SetSslOnConnect(true);
			ssl->SetCancel(Cancel);
			s.Reset(ssl);
		}

		return s;
	}

	struct Req
	{
		bool Status;
		int ProtocolStatus;
		LHttp::TContentEncoding Encoding;
		LString InHdrs, InBody;
		LString OutHdrs, OutBody;

		Req()
		{
			Status = false;
			Encoding = LHttp::EncodeNone;
			ProtocolStatus = 0;
		}
	};

	void PrettyPrint(LXmlTag &x);
	bool Request(Req &r, const char *Name, LString Resource);

	LString FullHref(const LString &Href) const;
	LString RelativeHref(const LString &Href) const;

public:
	struct FileProps
	{
		LString Href, LastMod, Tag, ContentType, Data;
		int64_t Length;

		LString GetContentType() const
		{
			return ContentType.SplitDelimit(";")[0].Strip();
		}

		FileProps Copy() // Make a thread safe copy...
		{
			FileProps f;
			f.Href = Href.Get();
			f.LastMod = LastMod.Get();
			f.Tag = Tag.Get();
			f.ContentType = ContentType.Get();
			f.Data = Data.Get();
			f.Length = Length;
			return f;
		}
	};

protected:
	void ParseMultistatus(LXmlTag &Root, LArray<FileProps> &Files, const char *DataTag);

public:
	LWebdav(LString endPoint, LString user, LString pass, LCancel *cancel);

	LString::Array GetOptions(LString Resource);
	/// True when the server advertises the RFC 3258 REPORT method
	bool SupportsReport(LString Resource);
	bool PropFind(LArray<FileProps> &Files, LString Resource, int Depth = 1);
	/// Bulk fetch the content for every 'Hrefs' entry with a single request.
	/// 'ReportType' is 'calendar-multiget' or 'addressbook-multiget'
	bool Report(LArray<FileProps> &Files, LString Resource, const char *ReportType, LString::Array &Hrefs);
	bool Get(const char *Resource, LString &Data);
	bool Put(const char *Resource, LString &Data);
	bool Delete(const char *Resource);
};

#endif