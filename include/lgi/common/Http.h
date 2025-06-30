#ifndef __IHTTP_H
#define __IHTTP_H

#include "lgi/common/Net.h"
#include "lgi/common/Uri.h"

class LHttp
{
	LString Proxy;
	int ProxyPort = 0;

	int BufferLen = 16 << 10;
	char *Buffer = NULL;
	
	LCancel *Cancel = NULL;
	LAutoPtr<LSocketI> Socket;	// commands
	size_t ResumeFrom = 0;
	LString FileLocation;
	char *Headers = NULL;
	bool NoCache = false;
	LString AuthUser, AuthPassword;
	LString ErrorMsg;

public:
	enum ContentEncoding
	{
		EncodeNone,
		EncodeRaw,
		EncodeGZip,
	};

	Progress *Meter = NULL;

	LHttp(LCancel *cancel = NULL);
	virtual ~LHttp();

	void SetResume(size_t i) { ResumeFrom = i; }
	void SetProxy(char *p, int Port);
	void SetNoCache(bool i) { NoCache = i; }
	void SetAuth(char *User = 0, char *Pass = 0);

	// Data
	LSocketI *Handle() { return Socket; }
	char *AlternateLocation() { return FileLocation; }
	char *GetHeaders() { return Headers; }
	
	// Connection
	bool Open(LAutoPtr<LSocketI> S, const char *RemoteHost, int Port = 0);
	bool Close();
	bool IsOpen();
	LSocketI *GetSocket() { return Socket; }
	LString GetErrorString() { return ErrorMsg; }

	// General
	bool Request(	const char *Type,
					const char *Uri,
					int *ProtocolStatus,
					const char *InHeaders,
					LStreamI *InBody,
					LStreamI *Out,
					LStreamI *OutHeaders,
					ContentEncoding *OutEncoding);

	bool Get(		const char *Uri,
					const char *InHeaders,
					int *ProtocolStatus,
					LStreamI *Out,
					ContentEncoding *OutEncoding,
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
};

/// This method will download a URI.
bool LGetUri
(
	/// Method for cancelling the call...
	LCancel *Cancel,
	/// The output stream to put the data
	LStreamI *Out,
	/// Any error message
	LString *OutError,
	/// The input URI to retreive
	const char *InUri,
	/// [Optional] Extra headers to use
	const char *InHeaders = NULL,
	/// [Optional] The proxy to use
	LUri *InProxy = NULL
);

#endif
