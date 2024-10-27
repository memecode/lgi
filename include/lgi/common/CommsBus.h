#pragma once

class LgiClass LCommsBus
{
	struct LCommsBusPriv *d;
	
public:
	LCommsBus(LStream *log = NULL);
	~LCommsBus();
	
	bool IsServer() const;
	bool IsRunning() const;
	bool SendMsg(LString endPoint, LString msg);
	bool Listen(LString endPoint, std::function<void(LString)> cb);

	static void UnitTests(LStream *log);
};