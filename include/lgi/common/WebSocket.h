#pragma once

#include <functional>
#include "lgi/common/Net.h"

class LWebSocketBase
{
public:
	constexpr static const char *Key = "258EAFA5-E914-47DA-95CA-C5AB0DC85B11";

	typedef std::function<bool(char *ptr, ssize_t len)> OnMsg;
	typedef std::function<LSocketI*()> CreateSocket;

	virtual ~LWebSocketBase() {}
};

// Web socket client class
class LWebSocket : public LSocket, public LWebSocketBase
{
	struct LWebSocketPriv *d;

public:
	LWebSocket(bool Server = true, OnMsg onMsg = nullptr);
	~LWebSocket();

	bool InitFromHeaders(LString Data, OsSocket Sock);
	void ReceiveHandler(OnMsg onMsg);
	bool SendMessage(char *Data, uint64 Len);
	bool SendMessage(LString s) { return SendMessage(s.Get(), s.Length()); }
	bool OnData();
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

	class Connection
	{
		friend struct LWebSocketServerPriv;
		
		LWebSocketServerPriv *d;
		LAutoPtr<LSocketI>	sock;
		size_t used = 0; // bytes in 'read' that are used
		LArray<char> read;     // read buffer..
		LString write;    // write buffer...
		LString headers;
		LString method, path, protocol;
		
		Connection(LWebSocketServerPriv *priv, LSocketI *s);
		ConnectStatus OnRead();

		LString ConsumeBytes(size_t bytes);
		
	public:
		// Methods
		LString GetMethod() { return method; }
		LString GetPath() { return path; }

		// Callbacks
		std::function<int(bool status)> ReceiveHdrsCb;
		LWebSocketBase::OnMsg MsgCb;
		std::function<void()> CloseCb;
	};

	LWebSocketServer(LStream *log = NULL, int port = HTTPS_PORT);
	virtual ~LWebSocketServer();

	void SetCreateSocket(CreateSocket createSocketCb);
	void OnConnection(std::function<bool(Connection*)> onConnectCb);
};
