#pragma once

#include <functional>
#include "lgi/common/Net.h"

typedef LAutoPtr<LSocketI> LAutoSocket;

class LWebSocketBase
{
public:
	constexpr static const char *Key = "258EAFA5-E914-47DA-95CA-C5AB0DC85B11";
	constexpr static int DefaultHttpResponse = 101;

	enum WsOpCode
	{
		WsContinue = 0,
		WsText     = 1,
		WsBinary   = 2,
		WsClose    = 8,
		WsPing     = 9,
		WsPong     = 10,
	};
	const char *ToString(WsOpCode op);
	LString ToString(uint64 i);

	typedef std::function<bool(WsOpCode op, char *ptr, ssize_t len)> OnMsg;
	typedef std::function<void()> OnClose;
	typedef std::function<LAutoSocket()> CreateSocket;

	virtual ~LWebSocketBase() {}
};

class LWsApplication : public LWebSocketBase
{
public:
};

// Web socket client class
class LWebSocket : public LWebSocketBase
{
protected:
	struct LWebSocketPriv *d;

public:
	OnMsg MsgCb;
	OnClose CloseCb;

	LWebSocket(LAutoSocket sock, bool Server = true, OnMsg onMsg = nullptr);
	virtual ~LWebSocket();

	bool Open(const char *uri, int port);
	LSocketI *GetSocket();
	bool SendMessage(WsOpCode Op, char *Data, uint64 Len);
	bool SendMessage(LString s) { return SendMessage(WsText, s.Get(), s.Length()); }
	bool Close();
	bool Read();
	bool IsConnected(); // Ready to send/receive messages
	
	virtual void OnHeaders() {};
};

// Server class
class LWebSocketServer : public LWebSocketBase
{
	struct LWebSocketServerPriv *d;

public:
	enum ConnectStatus
	{
		ConnectOk,
		ConnectError,
		ConnectClosed,
	};

	class Connection : public LWebSocket
	{
		friend struct LWebSocketServerPriv;
		
		LWebSocketServerPriv *d;
		size_t used = 0;	// bytes in 'read' that are used
		LArray<char> read;	// read buffer..
		LString write;		// write buffer...
		LString headers;
		LString method, path, protocol, ip;
		
		Connection(LWebSocketServerPriv *priv, LAutoSocket s);

		void OnHeaders();
		
	public:
		// Methods
		LString GetMethod() { return method; }
		LString GetPath() { return path; }
		LString GetIp() { return ip; }
		bool Send(LString msg);

		// The server may attach an application object
		LAutoPtr<LWsApplication> App;

		// Callbacks
		std::function<int(bool status)> ReceiveHdrsCb;
	};

	LWebSocketServer(LStream *log = NULL, int port = HTTPS_PORT);
	virtual ~LWebSocketServer();

	void SetCreateSocket(CreateSocket createSocketCb);
	void OnConnection(std::function<bool(Connection*)> onConnectCb);
};
