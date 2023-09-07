#pragma once

#include <functional>
#include "lgi/common/Net.h"

class LWebSocketBase
{
public:
	typedef std::function<bool(char *ptr, ssize_t len)> OnMsg;
	typedef std::function<LSocket*()> CreateSocket;

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
		LAutoPtr<LSocket> sock;
		ssize_t		 used = 0; // bytes in 'read' that are used
		LArray<char> read;     // read buffer..
		LString      write;    // write buffer...
		
		Connection(LWebSocketServerPriv *priv, LSocket *s);
		ConnectStatus OnRead();
		
	public:
		LString Context;
		
		LWebSocketBase::OnMsg MsgCb;
		std::function<void()> CloseCb;
	};

	LWebSocketServer(LStream *log = NULL, int port = HTTPS_PORT);
	virtual ~LWebSocketServer();

	void SetCreateSocket(CreateSocket createSocketCb);
	void OnConnection(std::function<bool(Connection*)> onConnectCb);
};
