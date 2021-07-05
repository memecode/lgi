#ifndef _LWEBDAV_H_
#define _LWEBDAV_H_

#include "lgi/common/Http.h"
#include "OpenSSLSocket.h"

extern bool LFindXml(LArray<LXmlTag*> &Results, LXmlTag *t, const char *Name);

class LWebdav
{
protected:
	LString EndPoint, EndPointPath, User, Pass;

	LAutoPtr<LSocketI> GetSocket()
	{
		LAutoPtr<LSocketI> s;

		SslSocket *ssl = new SslSocket();
		if (ssl)
		{
			ssl->SetSslOnConnect(true);
			s.Reset(ssl);
		}

		return s;
	}

	struct Req
	{
		bool Status;
		int ProtocolStatus;
		IHttp::ContentEncoding Encoding;
		LString InHdrs, InBody;
		LString OutHdrs, OutBody;

		Req()
		{
			Status = false;
			Encoding = IHttp::EncodeNone;
			ProtocolStatus = 0;
		}
	};

	void PrettyPrint(LXmlTag &x);
	bool Request(Req &r, const char *Name, LString Resource);

public:
	struct FileProps
	{
		LString Href, LastMod, Tag, ContentType, Data;
		int64_t Length;

		LString GetContentType()
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

	LWebdav(LString endPoint, LString user, LString pass);

	LString::Array GetOptions(LString Resource);
	bool PropFind(LArray<FileProps> &Files, LString Resource, int Depth = 1);
	bool Get(const char *Resource, LString &Data);
	bool Put(const char *Resource, LString &Data);
	bool Delete(const char *Resource);
};

#endif