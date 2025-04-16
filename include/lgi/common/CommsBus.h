#pragma once

class LgiClass LCommsBus
{
	struct LCommsBusPriv *d;
	LStream *log = nullptr;

public:
	enum TState
	{
		TDisconnectedClient,
		TDisconnectedServer,
		TConnectedClient,
		TConnectedServer,
	};

	using TCallback = std::function<void(TState)>;

	LCommsBus(LStream *logger = NULL, LView *commsState = nullptr);
	~LCommsBus();

	LString GetHostName() const;	
	bool IsServer() const;
	bool IsRunning() const;
	bool SendMsg(LString endPoint, LString msg);
	bool Listen(LString endPoint, std::function<void(LString)> cb);
	void SetCallback(TCallback cb);

	static void UnitTests(LStream *log);
};