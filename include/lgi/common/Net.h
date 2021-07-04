/**
	\file
	\author Matthew Allen
	\date 28/5/1998
	\brief Network sockets classes
	Copyright (C) 1998, <a href="mailto:fret@memecode.com">Matthew Allen</a>
*/

#ifndef __INET_H
#define __INET_H

#include "lgi/common/LgiNetInc.h"
#include "lgi/common/LgiInterfaces.h"
#include "lgi/common/Mem.h"
#include "lgi/common/Containers.h"
#include "lgi/common/Stream.h"
#include "lgi/common/String.h"
#include "lgi/common/Cancel.h"
#include "lgi/common/HashTable.h"

#if defined WIN32
	
	#include "ws2ipdef.h"

#elif defined POSIX

    #include <netinet/in.h>

#elif defined BEOS

	#include <sys/socket.h>
	#include <netdb.h>

#else

typedef int SOCKET;

#endif

#define DEFAULT_TIMEOUT			30

// Standard ports
#define FTP_PORT				21
#define SSH_PORT				22
#define SMTP_PORT				25
#define HTTP_PORT				80
#define HTTPS_PORT				443
#define SMTP_SSL_PORT			465
#define POP3_PORT				110
#define POP3_SSL_PORT			995
#define IMAP_PORT				143
#define IMAP_SSL_PORT			993

// Parameters for passing to LSocket::SetVariant

/// Turn on/off logging. Used with LSocket::SetParameter.
#define LSocket_Log				"Log"
/// Set the progress object. Used with LSocket::SetParameter. Value = (LProgressView*)Prog
#define LSocket_Progress		"Progress"
/// Set the size of the transfer. Used with LSocket::SetParameter. Value = (int)Size
#define LSocket_TransferSize	"TransferSize"
/// Set the type of protocol. Used with LSocket::SetParameter. Value = (char*)Protocol
#define LSocket_Protocol		"Protocol"
#define LSocket_SetDelete		"SetDelete"

// Functions
LgiNetFunc bool HaveNetConnection();
LgiNetFunc bool WhatsMyIp(GAutoString &Ip);
LgiExtern GString LIpStr(uint32_t ip);
LgiExtern uint32_t LIpHostInt(GString str);

/// Make md5 hash
LgiNetFunc void MDStringToDigest
(
	/// Buffer to receive md5 hash
	unsigned char digest[16],
	/// Input string
	char *Str,
	/// Length of input string or -1 for null terminated
	int Len = -1
);


/// Implementation of a network socket
class LgiNetClass LSocket :
	public LSocketI,
	public LStream
{
protected:
	class LSocketImplPrivate *d;

	// Methods
	void Log(const char *Msg, ssize_t Ret, const char *Buf, ssize_t Len);
	bool CreateUdpSocket();

public:
	ssize_t	BytesRead, BytesWritten;

	/// Creates the class
	LSocket(LStreamI *logger = 0, void *unused_param = 0);
	
	/// Destroys the class
	~LSocket();

	/// Gets the active cancellation object
	LCancel *GetCancel();

	/// Sets the active cancellation object
	void SetCancel(LCancel *c);

	/// Returns the operating system handle to the socket.
	OsSocket Handle(OsSocket Set = INVALID_SOCKET);
	OsSocket ReleaseHandle();

	/// Returns true if the internal state of the class is ok
	bool IsOK();
	
	/// Returns the IP address at this end of the socket.
	bool GetLocalIp(char *IpAddr);
	
	/// Returns the port at this end of the socket.
	int GetLocalPort();
	
	/// Gets the IP address at the remote end of the socket.
	bool GetRemoteIp(uint32_t *IpAddr);
	bool GetRemoteIp(char *IpAddr);

	/// Gets the IP address at the remote end of the socket.
	int GetRemotePort();

	/// Gets the current timeout for operations in ms
	int GetTimeout();

	/// Sets the current timeout for operations in ms
	void SetTimeout(int ms);

	/// Returns whether there is data available for reading.
	bool IsReadable(int TimeoutMs = 0);

	/// Returns whether there is data available for reading.
	bool IsWritable(int TimeoutMs = 0);

	/// Returns if the socket is ready to accept a connection
	bool CanAccept(int TimeoutMs = 0);

	/// Returns if the socket is set to block
	bool IsBlocking();

	/// Set the socket to block
	void IsBlocking(bool block);

	/// Get the send delay setting
	bool IsDelayed();

	/// Set the send delay setting
	void IsDelayed(bool Delay);

	/// Opens a connection.
	int Open
	(
		/// The name of the remote host.
		const char *HostAddr,
		/// The port on the remote host.
		int Port
	);
	
	/// Returns true if the socket is connected.
	bool IsOpen();
	
	/// Closes the connection to the remote host.
	int Close();

	/// Binds on a given port.
	bool Bind(int Port);

	/// Binds and listens on a given port for an incomming connection.
	bool Listen(int Port = 0);
	
	/// Accepts an incomming connection and connects the socket you pass in to the remote host.
	bool Accept
	(
		/// The socket to handle the connection.
		LSocketI *c
	);

	/// \brief Sends data to the remote host.
	/// \return the number of bytes written or <= 0 on error.
	ssize_t Write
	(
		/// Pointer to the data to write
		const void *Data,
		/// Numbers of bytes to write
		ssize_t Len,
		/// Flags to pass to send
		int Flags = 0
	);

	ssize_t Write(const GString &s) { return Write(s.Get(), s.Length()); }
	
	/// \brief Reads data from the remote host.
	/// \return the number of bytes read or <= 0 on error.
	///
	/// Generally the number of bytes returned is less than the buffer size. Depending on how much data
	/// you are expecting you will need to keep reading until you get and end of field marker or the number
	/// of bytes your looking for.
	ssize_t Read
	(
		/// Pointer to the buffer to write output to
		void *Data,
		/// The length of the receive buffer.
		ssize_t Len,
		/// The flags to pass to recv
		int Flags = 0
	);
	
	/// Returns the last error or 0.
	int Error(void *Param = 0);
	const char *GetErrorString();

	/// Not supported
	int64 GetSize() { return -1; }
	/// Not supported
	int64 SetSize(int64 Size) { return -1; }
	/// Not supported
	int64 GetPos() { return -1; }
	/// Not supported
	int64 SetPos(int64 Pos) { return -1; }

	/// Gets called when the connection is disconnected
	void OnDisconnect();
	
	/// Gets called when data is received.
	void OnRead(char *Data, ssize_t Len) {}

	/// Gets called when data is sent.
	void OnWrite(const char *Data, ssize_t Len) {}
	
	/// Gets called when an error occurs.
	void OnError(int ErrorCode, const char *ErrorDescription);
	
	/// Gets called when some information is available.
	void OnInformation(const char *Str) {}
	
	/// Parameter change handler.
	int SetParameter
	(
		/// e.g. #LSocket_Log
		int Param,
		int Value
	) { return false; }

	/// Get UPD mode
	bool GetUdp();
	/// Set UPD mode
	void SetUdp(bool b);
	/// Makes the socket able to broadcast
	void SetBroadcast();
	/// Read UPD packet
	int ReadUdp(void *Buffer, int Size, int Flags, uint32_t *Ip = 0, uint16_t *Port = 0);
	/// Write UPD packet
	int WriteUdp(void *Buffer, int Size, int Flags, uint32_t Ip, uint16_t Port);
	
	bool AddMulticastMember(uint32_t MulticastIp, uint32_t LocalInterface);
	bool SetMulticastInterface(uint32_t Interface);

	// Impl
	LStreamI *Clone()
	{
		LSocket *s = new LSocket;
		if (s)
			s->SetCancel(GetCancel());
		return s;
	}

	// Statics

	/// Enumerates the current interfaces
	struct Interface
	{
		GString Name;
		uint32_t Ip4; // Host order...
		uint32_t Netmask4;

		bool IsLoopBack()
		{
			return Ip4 == 0x7f000001;
		}
		
		bool IsPrivate()
		{
			uint8_t h1 = (Ip4 >> 24) & 0xff;
			if (h1 == 192)
			{
				uint8_t h2 = ((Ip4 >> 16) & 0xff);
				return h2 == 168;
			}
			else if (h1 == 10)
			{
				return true;
			}
			else if (h1 == 172)
			{
				uint8_t h2 = ((Ip4 >> 16) & 0xff);
				return h2 >= 16 && h2 <= 31;
			}
			
			return false;
		}
		
		bool IsLinkLocal()
		{
			uint8_t h1 = (Ip4 >> 24) & 0xff;
			if (h1 == 169)
			{
				uint8_t h2 = ((Ip4 >> 16) & 0xff);
				return h2 == 254;
			}
			
			return false;
		}
	};

	static bool EnumInterfaces(GArray<Interface> &Out);
};

class LgiNetClass LSocks5Socket : public LSocket
{
	GAutoString Proxy;
	int Port;
	GAutoString UserName;
	GAutoString Password;

protected:
	bool Socks5Connected;

public:
	LSocks5Socket();

	LSocks5Socket &operator=(const LSocks5Socket &s)
	{
		Proxy.Reset(NewStr(s.Proxy));
		UserName.Reset(NewStr(s.UserName));
		Password.Reset(NewStr(s.Password));
		Port = s.Port;
		return *this;
	}

	// Connection
	void SetProxy(char *proxy, int port, char *username, char *password);
	void SetProxy(const LSocks5Socket *s);
	int Open(const char *HostAddr, int port);

	// Server
	bool Listen(int Port) { return false; }
};

/// Uri parser
class LgiNetClass LUri
{
public:
	GString sProtocol;
	GString sUser;
	GString sPass;
	GString sHost;
	int Port;
	GString sPath;
	GString sAnchor;

	/// Parser for URI's.
	LUri
	(
		/// Optional URI to start parsing
		const char *uri = 0
	);
	~LUri();

	bool IsProtocol(const char *p) { return sProtocol.Equals(p); }
	bool IsHttp() { return sProtocol.Equals("http") || sProtocol.Equals("https"); }
	bool IsFile() { return sProtocol.Equals("file"); }
	void SetFile(GString Path) { Empty(); sProtocol = "file"; sPath = Path; }
	const char *LocalPath();
	operator bool();

	/// Parse a URI into it's sub fields...
	bool Set(const char *uri);

	/// Re-constructs the URI
	GString ToString();

	/// Empty this object...
	void Empty();

	/// URL encode
	GString EncodeStr
	(
		/// The string to encode
		const char *s,
		/// [Optional] Any extra characters you want encoded
		const char *ExtraCharsToEncode = 0
	);

	/// URL decode
	GString DecodeStr(const char *s);

	/// Separate args into map
	typedef LHashTbl<StrKey<char,false>,GString> StrMap;
	StrMap Params();

	LUri &operator =(const LUri &u);
	LUri &operator =(const char *s) { Set(s); return *this; }
	LUri &operator +=(const char *s);
};

/// Proxy settings lookup
class LgiNetClass LProxyUri : public LUri
{
public:
	LProxyUri();
};

#define MAX_UDP_SIZE		512

class LUdpListener : public LSocket
{
	LStream *Log;
	GString Context;

public:
	LUdpListener(GArray<uint32_t> interface_ips, uint32_t mc_ip, uint16_t port, LStream *log = NULL) : Log(log)
	{
		//SetBroadcast();
		SetUdp(true);

		struct sockaddr_in addr;
		ZeroObj(addr);
		addr.sin_family = AF_INET;
		addr.sin_port = htons(port);
		#ifdef WINDOWS
		addr.sin_addr.S_un.S_addr = INADDR_ANY;
		#elif defined(MAC)
		addr.sin_addr.s_addr = htonl(mc_ip);
		#else
		addr.sin_addr.s_addr = INADDR_ANY;
		#endif

		if (mc_ip)
		{
			for (auto ip : interface_ips)
			{
				printf("AddMulticastMember(%s, %s)\n", LIpStr(mc_ip).Get(), LIpStr(ip).Get());
				AddMulticastMember(mc_ip, ip);
			}
		}
		
		int r = bind(Handle(), (struct sockaddr*)&addr, sizeof(addr));
		if (r)
		{
			#ifdef WIN32
			int err = WSAGetLastError();
			OnError(err, NULL);
			#endif

			printf("Error: Bind on %s:%i\n", LIpStr(ntohl(addr.sin_addr.s_addr)).Get(), port);
		}
		else
		{
			printf("Ok: Bind on %s:%i\n", LIpStr(ntohl(addr.sin_addr.s_addr)).Get(), port);
		}

	}

	bool ReadPacket(GString &d, uint32_t &Ip, uint16_t &Port)
	{
		if (!IsReadable(10))
			return false;

		char Data[MAX_UDP_SIZE];
		int Rd = ReadUdp(Data, sizeof(Data), 0, &Ip, &Port);
		if (Rd <= 0)
			return false;

		d.Set(Data, Rd);
		return true;
	}

	void OnError(int ErrorCode, const char *ErrorDescription)
	{
		if (Log)
			Log->Print("Error: %s - %i, %s\n", Context.Get(), ErrorCode, ErrorDescription);
	}
};

class LUdpBroadcast : public LSocket
{
	GArray<Interface> Intf;
	uint32_t SelectIf;

public:
	LUdpBroadcast(uint32_t selectIf) : SelectIf(selectIf)
	{
        SetBroadcast();
		SetUdp(true);
		EnumInterfaces(Intf);
	}

	bool BroadcastPacket(GString Data, uint32_t Ip, uint16_t Port)
	{
		return BroadcastPacket(Data.Get(), Data.Length(), Ip, Port);
	}

	bool BroadcastPacket(void *Ptr, size_t Size, uint32_t Ip, uint16_t Port)
	{
		if (Size > MAX_UDP_SIZE)
			return false;
		
		if (SelectIf)
		{
			struct in_addr addr;
			addr.s_addr = htonl(SelectIf);
			auto r = setsockopt(Handle(), IPPROTO_IP, IP_MULTICAST_IF, (char*)&addr, sizeof(addr));
			if (r)
				printf("%s:%i - set IP_MULTICAST_IF failed.\n", _FL);
			SelectIf = 0;
		}
		
		/*
		uint32 Netmask = 0xffffff00;
		Interface *Cur = NULL;
		for (auto &i : Intf)
		{
			if (i.Ip4 == Ip)
			{
				Netmask = i.Netmask4;
				Cur = &i;
				break;
			}
		}
		*/
		
		uint32_t BroadcastIp = Ip;
		#if 0
		printf("Broadcast %i.%i.%i.%i\n", 
			(BroadcastIp >> 24) & 0xff,
			(BroadcastIp >> 16) & 0xff,
			(BroadcastIp >> 8) & 0xff,
			(BroadcastIp) & 0xff);
		#endif
		int wr = WriteUdp(Ptr, (int)Size, 0, BroadcastIp, Port);
		return wr == Size;
	}
};

#endif

