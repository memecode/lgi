#pragma once

class LgiClass LCommsBus
{
	struct LCommsBusPriv *d;
	
public:
	enum TState
	{
		TDisconnectedClient,
		TDisconnectedServer,
		TConnectedClient,
		TConnectedServer,
	};

	using TCallback = std::function<void(TState)>;

	LCommsBus(LStream *log = NULL, LView *commsState = nullptr);
	~LCommsBus();
	
	bool IsServer() const;
	bool IsRunning() const;
	bool SendMsg(LString endPoint, LString msg);
	bool Listen(LString endPoint, std::function<void(LString)> cb);
	void SetCallback(TCallback cb);

	static void UnitTests(LStream *log);
};