#ifndef _LWEBDAV_H_
#define _LWEBDAV_H_

#include "IHttp.h"
#include "OpenSSLSocket.h"

extern bool LFindXml(GArray<GXmlTag*> &Results, GXmlTag *t, const char *Name);

class LWebdav
{
	GString EndPoint, EndPointPath, User, Pass;

	GAutoPtr<GSocketI> GetSocket()
	{
		GAutoPtr<GSocketI> s;

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
		GString InHdrs, InBody;
		GString OutHdrs, OutBody;

		Req()
		{
			Status = false;
			Encoding = IHttp::EncodeNone;
			ProtocolStatus = 0;
		}
	};

	void PrettyPrint(GXmlTag &x);
	bool Request(Req &r, const char *Name, GString Resource);

public:
	struct FileProps
	{
		GString Href, LastMod, Tag, ContentType, Data;
		int64_t Length;

		GString GetContentType()
		{
			return ContentType.SplitDelimit(";")[0].Strip();
		}

		FileProps Copy()
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

	LWebdav(GString endPoint, GString user, GString pass);

	GString::Array GetOptions(GString Resource);
	bool PropFind(GArray<FileProps> &Files, GString Resource, int Depth = 1);
	bool Get(GString &Data, const char *Resource);
	bool Put(GString &Data, const char *Resource);
	bool Delete(const char *Resource);
};

#endif