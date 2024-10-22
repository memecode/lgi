#pragma once

class LCommsBus
{
	struct LCommsBusPriv *d;
	
public:
	LCommsBus(LStream *log = NULL);
	~LCommsBus();
	
	bool SendMsg(LString endPoint, LString msg);
	bool Listen(LString endPoint, std::function<void(LString)> cb);
};