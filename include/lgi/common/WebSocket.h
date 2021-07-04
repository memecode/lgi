#ifndef _LWEBSOCKET_H_
#define _LWEBSOCKET_H_

#include <functional>
#include "lgi/common/Net.h"

class LSelect
{
protected:
	GArray<LSocket*> s;
	int Select(GArray<LSocket*> &Results, bool Rd, bool Wr, int TimeoutMs);

public:
	LSelect(LSocket *sock = NULL);
	
	LSelect &operator +=(LSocket *sock);
	
	GArray<LSocket*> Readable(int Timeout = -1);
	GArray<LSocket*> Writeable(int Timeout = -1);
};

class LWebSocket : public LSocket
{
	struct LWebSocketPriv *d;

public:
	typedef std::function<bool(char *Data, uint64 Len)> OnMsg;
	LWebSocket(bool Server = true, OnMsg onMsg = nullptr);
	~LWebSocket();

	bool InitFromHeaders(GString Data, OsSocket Sock);
	void ReceiveHandler(OnMsg onMsg);
	bool SendMessage(char *Data, uint64 Len);
	bool SendMessage(GString s) { return SendMessage(s.Get(), s.Length()); }
	bool OnData();
};

#endif