#pragma once

class LCommsBus
{
	struct LCommsBusPriv *d;
	
public:
	LCommsBus(int Uid, LStream *log = NULL);
	~LCommsBus();
	
	bool IsServer() const;
	bool SendMsg(LString endPoint, LString msg);
	bool Listen(LString endPoint, std::function<void(LString)> cb);

	static bool UnitTests();
};