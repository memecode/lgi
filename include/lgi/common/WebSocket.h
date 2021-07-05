#ifndef _LWEBSOCKET_H_
#define _LWEBSOCKET_H_

#include <functional>
#include "lgi/common/Net.h"

class LSelect
{
protected:
	LArray<LSocket*> s;
	int Select(LArray<LSocket*> &Results, bool Rd, bool Wr, int TimeoutMs);

public:
	LSelect(LSocket *sock = NULL);
	
	LSelect &operator +=(LSocket *sock);
	
	LArray<LSocket*> Readable(int Timeout = -1);
	LArray<LSocket*> Writeable(int Timeout = -1);
};

class LWebSocket : public LSocket
{
	struct LWebSocketPriv *d;

public:
	typedef std::function<bool(char *Data, uint64 Len)> OnMsg;
	LWebSocket(bool Server = true, OnMsg onMsg = nullptr);
	~LWebSocket();

	bool InitFromHeaders(LString Data, OsSocket Sock);
	void ReceiveHandler(OnMsg onMsg);
	bool SendMessage(char *Data, uint64 Len);
	bool SendMessage(LString s) { return SendMessage(s.Get(), s.Length()); }
	bool OnData();
};

#endif