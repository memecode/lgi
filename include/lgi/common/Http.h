#ifndef __IHTTP_H
#define __IHTTP_H

#include "lgi/common/Net.h"
#include "lgi/common/Uri.h"

// HTTP[S] client class:
class LHttp
{
public:
	enum TContentEncoding
	{
		EncodeNone,
		EncodeRaw,
		EncodeGZip,
	};
	
	enum TAuth
	{
		AuthPlain,
		AuthDigest,
	};

	enum TRequestType
	{
		HttpNone,
		HttpGet,
		HttpPost,
		HttpOther,
	};

protected:
	LString Proxy;
	int ProxyPort = 0;

	int BufferLen = 16 << 10;
	char *Buffer = nullptr;
	
	LCancel *Cancel = nullptr;
	LAutoPtr<LSocketI> Socket;	// commands
	size_t ResumeFrom = 0;
	LString FileLocation;
	char *Headers = nullptr;
	bool NoCache = false;
	LString AuthUser, AuthPassword, AuthRealm;
	TAuth AuthType = AuthPlain;
	LError err;
	LStream *log = nullptr;

	// Digest auth:
	struct DigestRealm {
		LString realm, nonce, opaque, algorithm, qop;
		DigestRealm(LString wwwAuthHdr);
	};
	LHashTbl<ConstStrKey<char,false>, DigestRealm*> realms;
	LHashTbl<ConstStrKey<char>, int> nonceCounter;

public:
	Progress *Meter = nullptr;

	LHttp(LCancel *cancel = nullptr);
	virtual ~LHttp();

	void SetResume(size_t i) { ResumeFrom = i; }
	void SetProxy(const char *p, int Port);
	void SetNoCache(bool i) { NoCache = i; }
	void SetPlainAuth(const char *User = nullptr, const char *Pass = nullptr);
	void SetDigestAuth(const char *User, const char *Pass);
	void SetLog(LStream *logger) { log = logger; }

	// Data
	LSocketI *Handle() { return Socket; }
	char *AlternateLocation() { return FileLocation; }
	char *GetHeaders() { return Headers; }
	
	// Connection
	bool Open(LAutoPtr<LSocketI> S, const char *RemoteHost, int Port = 0);
	bool Close();
	bool IsOpen();
	LSocketI *GetSocket() { return Socket; }
	LError &GetError() { return err; }

	// General
	bool Request(	const char *Method,
					const char *Uri,
					int *ProtocolStatus,
					const char *InHeaders,
					LStreamI *InBody,
					LStreamI *Out,
					LStreamI *OutHeaders,
					TContentEncoding *OutEncoding);

	bool Get(		const char *Uri,
					const char *InHeaders,
					int *ProtocolStatus,
					LStreamI *Out,
					TContentEncoding *OutEncoding,
					LStreamI *OutHeaders = 0)
	{
		return Request("GET",
						Uri,
						ProtocolStatus,
						InHeaders,
						NULL, // InBody
						Out,
						OutHeaders,
						OutEncoding);
	}
	

	bool Post(		char *Uri,
					LStreamI *In,
					int *ProtocolStatus,
					LStreamI *Out,
					LStreamI *OutHeaders = 0,
					const char *InHeaders = 0)
	{
		return Request("POST",
						Uri,
						ProtocolStatus,
						InHeaders,
						In,
						Out,
						OutHeaders,
						NULL);
	}

	// Helper function for reading chunked data:
	static LError ReadChunked(	LSocketI *sock,
								LStream *output,
								char *buf, // may contain 'used' bytes of data when called
								ssize_t bufLen,
								ssize_t used,
								LStream *log = nullptr);
};

/// This method will download a URI.
bool LGetUri
(
	/// Method for cancelling the call...
	LCancel *Cancel,
	/// The output stream to put the data
	LStreamI *Out,
	/// Any error message
	LError *OutError,
	/// The input URI to retreive
	const char *InUri,
	/// [Optional] Extra headers to use
	const char *InHeaders = NULL,
	/// [Optional] The proxy to use
	LUri *InProxy = NULL
);

#endif
