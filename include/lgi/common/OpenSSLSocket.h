#ifndef _OPEN_SSL_SOCKET_H_
#define _OPEN_SSL_SOCKET_H_

#include "lgi/common/LibraryUtils.h"
#include "lgi/common/Cancel.h"

// If you get a compile error on Linux:
//		sudo apt-get install libssl-dev
//
// On windows:
//		https://slproweb.com/products/Win32OpenSSL.html
#include "openssl/bio.h"
#include "openssl/ssl.h"
#include "openssl/err.h"

#define SslSocket_LogFile				"LogFile"
#define SslSocket_LogFormat				"LogFmt"

class SslSocket :
	public LSocketI,
	virtual public LDom
{
	friend class OpenSSL;
	struct SslSocketPriv *d;
	
	LMutex Lock;
	BIO *Bio = NULL;
	SSL *Ssl = NULL;
	LString ErrMsg;

	// Local stuff
	virtual void Log(const char *Str, ssize_t Bytes, SocketMsgType Type);
	void SslError(const char *file, int line, const char *Msg);
	LStream *GetLogStream();
	void DebugTrace(const char *fmt, ...);

public:
	static bool DebugLogging;
	static LString Random(int Len);

	SslSocket(LStreamI *logger = NULL, LCapabilityClient *caps = NULL, bool SslOnConnect = false, bool RawLFCheck = false, bool banner = true);
	~SslSocket();

	void SetLogger(LStreamI *logger);
	LStreamI *GetLog() override;
	void SetSslOnConnect(bool b);
	LCancel *GetCancel() override;
	void SetCancel(LCancel *c) override;
	
	// Socket
	OsSocket Handle(OsSocket Set = INVALID_SOCKET) override;
	bool IsOpen() override;
	int Open(const char *HostAddr, int Port) override;
	int Close() override;
	bool Listen(int Port = 0) override;
	void OnError(int ErrorCode, const char *ErrorDescription) override;
	void OnInformation(const char *Str) override;
	int GetTimeout() override;
	void SetTimeout(int ms) override;

	ssize_t Write(const void *Data, ssize_t Len, int Flags = 0) override;
	ssize_t Read(void *Data, ssize_t Len, int Flags = 0) override;
	void OnWrite(const char *Data, ssize_t Len) override;
	void OnRead(char *Data, ssize_t Len) override;

	bool IsReadable(int TimeoutMs = 0) override;
	bool IsWritable(int TimeoutMs = 0) override;
	bool IsBlocking() override;
	void IsBlocking(bool block) override;

	bool SetVariant(const char *Name, LVariant &Val, const char *Arr = NULL) override;
	bool GetVariant(const char *Name, LVariant &Val, const char *Arr = NULL) override;

	LStreamI *Clone() override;
	const char *GetErrorString() override;
};

extern bool StartSSL(LString &ErrorMsg, SslSocket *Sock);
extern void EndSSL();

#endif
