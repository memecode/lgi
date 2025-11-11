/*hdr
**		FILE:			INet.cpp
**		AUTHOR:			Matthew Allen
**		DATE:			28/5/98
**		DESCRIPTION:	Internet sockets
**
**		Copyright (C) 1998, Matthew Allen
**				fret@memecode.com
*/

#if defined(LINUX)
	#include <netinet/tcp.h>
	#include <unistd.h>
	#include <poll.h>
	#include <errno.h>
	#include <netdb.h>
#elif defined(MAC)
	#include <SystemConfiguration/SCDynamicStoreCopySpecific.h>
	#include <SystemConfiguration/SCSchemaDefinitions.h>
	#include <sys/types.h>
	#include <sys/socket.h>
	#include <ifaddrs.h>
#endif

#if defined(WINDOWS)
	#define _WINSOCK_DEPRECATED_NO_WARNINGS 1
	#include <winsock2.h>
	#include <ws2tcpip.h>
 	#include <stdio.h>
	#include <stdlib.h>
	#include <ras.h>

	typedef HOSTENT HostEnt;
	typedef int socklen_t;
	typedef unsigned long in_addr_t;
	typedef BOOL option_t;

	#define LPrintSock "%I64x"
	#define MSG_NOSIGNAL		0
	#ifndef EWOULDBLOCK
		#define EWOULDBLOCK		WSAEWOULDBLOCK
	#endif
	#ifndef EISCONN
		#define EISCONN			WSAEISCONN
	#endif

	#define OsAddr				S_un.S_addr
#else
	#include <sys/types.h>
	#include <ifaddrs.h>
#endif
#include <ctype.h>

#include "lgi/common/File.h"
#include "lgi/common/Net.h"
#include "lgi/common/LgiString.h"
#include "lgi/common/LgiCommon.h"
#include "lgi/common/RegKey.h"
#include "lgi/common/Window.h"
#include "LgiOsClasses.h"

#define USE_BSD_SOCKETS			1
#define DEBUG_CONNECT			0
#define ETIMEOUT				400
#define PROTO_UDP				0x100
#define PROTO_BROADCAST			0x200
#define IsNewLine(ch)			((ch) == '\r' || (ch) == '\n')

#if defined POSIX

	#include <stdio.h>
	#include <sys/types.h>
	#include <sys/socket.h>
	#include <netinet/in.h>
	#include <arpa/inet.h>
	#include <ctype.h>
	#include <netdb.h>
	#include <errno.h>
	#include <netinet/tcp.h>
	#include <ifaddrs.h>

	#define SOCKET_ERROR -1
	#define LPrintSock "%x"
	typedef hostent HostEnt;
	typedef int option_t;
	#define OsAddr				s_addr
	
#endif

///////////////////////////////////////////////////////////////////////////////
#ifdef WIN32
static bool SocketsOpen = false;
#endif

bool StartNetworkStack()
{
	#ifdef WIN32
		
		#ifndef WINSOCK_VERSION
		#define WINSOCK_VERSION MAKEWORD(2,2)
		#endif

		if (!SocketsOpen)
		{
			// Start sockets
			WSADATA WsaData;
			int Result = WSAStartup(WINSOCK_VERSION, &WsaData);
			if (!Result)
			{
				if (WsaData.wVersion == WINSOCK_VERSION)
				{
					SocketsOpen = Result == 0;
				}
				else
				{
					WSACleanup();
					return false;
				}
			}
		}

	#endif

	return true;
}

void StopNetworkStack()
{
	#ifdef WIN32
	if (SocketsOpen)
	{
		WSACleanup();
		SocketsOpen = false;
	}
	#endif
}

#ifdef WIN32
#include "..\..\Win\INet\MibAccess.h"
#include <iphlpapi.h>
#pragma comment(lib, "iphlpapi.lib")
#endif

bool LSocket::EnumInterfaces(LArray<Interface> &Out)
{
	bool Status = false;

	StartNetworkStack();

	#ifdef WIN32

		#if 1

			ULONG outBufLen = 0;
			DWORD dwRetVal = 0;
			IP_ADAPTER_INFO* pAdapterInfos = (IP_ADAPTER_INFO*)malloc(sizeof(IP_ADAPTER_INFO));

			// retry up to 5 times, to get the adapter infos needed
			for (int i = 0; i < 5 && (dwRetVal == ERROR_BUFFER_OVERFLOW || dwRetVal == NO_ERROR); ++i)
			{
				dwRetVal = GetAdaptersInfo(pAdapterInfos, &outBufLen);
				if (dwRetVal == NO_ERROR)
				{
					break;
				}
				else if (dwRetVal == ERROR_BUFFER_OVERFLOW)
				{
					free(pAdapterInfos);
					pAdapterInfos = (IP_ADAPTER_INFO*)malloc(outBufLen);
				}
				else
				{
					pAdapterInfos = 0;
					break;
				}
			}
			if (dwRetVal == NO_ERROR)
			{
				auto pAdapterInfo = pAdapterInfos;
				while (pAdapterInfo)
				{
					auto pIpAddress = &(pAdapterInfo->IpAddressList);
					while (pIpAddress != 0)
					{
						auto Ip4 = LIpToInt(pIpAddress->IpAddress.String);
						auto Netmask4 = LIpToInt(pIpAddress->IpMask.String);

						if (Ip4 && Netmask4)
						{
							auto &out = Out.New();
							out.Ip4 = Ip4;
							out.Netmask4 = Netmask4;

							LString keyName;
							keyName.Printf(	"HKEY_LOCAL_MACHINE\\SYSTEM\\CurrentControlSet\\Control\\Network\\{4D36E972-E325-11CE-BFC1-08002BE10318}\\%s\\Connection",
											pAdapterInfo->AdapterName);
							LRegKey key(false, keyName);
							if (key.IsOk())
								out.Name = key.GetStr("Name");
							else
								LgiTrace("%s:%i - Error getting '%s'\n", _FL, keyName.Get());
						}

						pIpAddress = pIpAddress->Next;
					}
					pAdapterInfo = pAdapterInfo->Next;
				}

				Status = true;
			}
			free(pAdapterInfos);

		#else

			MibII m;
			m.Init();

			MibInterface Intf[16] = {0};
			int Count = m.GetInterfaces(Intf, CountOf(Intf));
			if (Count)
			{
				for (int i=0; i<Count; i++)
				{
					auto &OutIntf = Out.New();
					OutIntf.Ip4 = htonl(Intf[i].Ip);
					OutIntf.Netmask4 = htonl(Intf[i].Netmask);
					OutIntf.Name = Intf[i].Name;
					Status = true;
				}
			}

		#endif

	#else
		// Posix
		struct ifaddrs *addrs = NULL;
		int r = getifaddrs(&addrs);
		if (r == 0)
		{
			for (ifaddrs *a = addrs; a; a = a->ifa_next)
			{
				if (a->ifa_addr &&
					a->ifa_addr->sa_family == AF_INET)
				{
					sockaddr_in *in = (sockaddr_in*)a->ifa_addr;
					sockaddr_in *mask = (sockaddr_in*)a->ifa_netmask;
				
					auto &Intf = Out.New();
					Intf.Ip4 = ntohl(in->sin_addr.s_addr);
					Intf.Netmask4 = ntohl(mask->sin_addr.s_addr);
					Intf.Name = a->ifa_name;

					Status = true;
				}
			}
		
			freeifaddrs(addrs);
		}
	#endif

	return Status;
}

////////////////////////////////////////////////////////////////////////////////////////////////
class LSocketImplPrivate : public LCancel
{
public:
	// Data
	int Blocking	: 1;
	int NoDelay		: 1;
	int Udp			: 1;
	int Broadcast	: 1;

	int			LogType		= NET_LOG_NONE;
	LString		LogFile;
	int			Timeout		= -1;
	OsSocket	Socket		= INVALID_SOCKET;
	int			LastError	= 0;
	LCancel		*Cancel		= NULL;
	LString		ErrStr;

	LSocketImplPrivate()
	{	
		Cancel = this;
		Blocking = true;
		NoDelay = false;
		Udp = false;
		Broadcast = false;
	}

	~LSocketImplPrivate()
	{
	}

	bool Select(int TimeoutMs, bool Read)
	{
		// Assign to local var to avoid a thread changing it
		// on us between the validity check and the select.
		// Which is important because a socket value of -1
		// (ie invalid) will crash the FD_SET macro.
		OsSocket s = Socket; 
		if (ValidSocket(s) && !Cancel->IsCancelled())
		{
			struct timeval t = {TimeoutMs / 1000, (TimeoutMs % 1000) * 1000};

			fd_set set;
			FD_ZERO(&set);
			FD_SET(s, &set);		
			
			int ret = select(	(int)s+1,
								Read ? &set : NULL,
								!Read ? &set : NULL,
								NULL,
								TimeoutMs >= 0 ? &t : NULL);
			
			if (ret > 0 && FD_ISSET(s, &set))
			{
				return true;
			}
		}

		return false;
	}

	// This is the timing granularity of the SelectWithCancel loop. Ie the 
	// number of milliseconds between checking the Cancel object.
	constexpr static int CancelCheckMs = 50;

	bool SelectWithCancel(int TimeoutMs, bool Read)
	{
		if (!Cancel)
			// Regular select where we just wait the whole timeout...
			return Select(TimeoutMs, false);

		// Because select can't check out 'Cancel' value during the waiting we run the select
		// call in a much smaller timeout and a loop so that we can respond to Cancel being set
		// in a timely manner.
		auto Now = LCurrentTime();
		auto End = Now + TimeoutMs;
		do
		{
			// Do the cancel check...
			if (Cancel->IsCancelled())
				break;

			// How many ms to wait?
			Now = LCurrentTime();
			auto Remain = MIN(CancelCheckMs, (int)(End - Now));
			if (Remain <= 0)
				break;
			
			if (Select(Remain, Read))
				return true;
		}
		while (Now < End);

		return false;
	}
};

LSocket::LSocket(LStreamI *logger, void *unused_param)
{
	StartNetworkStack();
	BytesWritten = 0;
	BytesRead = 0;
	d = new LSocketImplPrivate;
}

LSocket::~LSocket()
{
	Close();
	DeleteObj(d);
}

bool LSocket::IsOK()
{
	return
			#ifndef __llvm__
			this != 0 &&
			#endif
			d != 0;
}

LCancel *LSocket::GetCancel()
{
	return d->Cancel;
}

void LSocket::SetCancel(LCancel *c)
{
	d->Cancel = c;
}

void LSocket::OnDisconnect()
{
}

OsSocket LSocket::ReleaseHandle()
{
	auto h = d->Socket;
	d->Socket = INVALID_SOCKET;
	return h;
}

OsSocket LSocket::Handle(OsSocket Set)
{
	if (Set != INVALID_SOCKET)
	{
		d->Socket = Set;
	}

	return d->Socket;
}

bool LSocket::IsOpen()
{
	if (ValidSocket(d->Socket) && !d->Cancel->IsCancelled())
	{
		return true;
	}

	return false;
}

int LSocket::GetTimeout()
{
	return d->Timeout;
}

void LSocket::SetTimeout(int ms)
{
	d->Timeout = ms;
}

bool LSocket::IsReadable(int TimeoutMs)
{
	// Assign to local var to avoid a thread changing it
	// on us between the validity check and the select.
	// Which is important because a socket value of -1
	// (ie invalid) will crash the FD_SET macro.
	OsSocket s = d->Socket; 
	if (ValidSocket(s) && !d->Cancel->IsCancelled())
	{
		#ifdef LINUX

			// Because Linux doesn't return from select() when the socket is
			// closed elsewhere we have to do something different... damn Linux,
			// why can't you just like do the right thing?
			
			struct pollfd fds;
			fds.fd = s;
			fds.events = POLLIN | POLLRDHUP | POLLERR;
			fds.revents = 0;

			int r = poll(&fds, 1, TimeoutMs);
			if (r > 0)
			{
				return fds.revents != 0;
			}
			else if (r < 0)
			{
				Error();
			}
		
		#else
		
			struct timeval t = {TimeoutMs / 1000, (TimeoutMs % 1000) * 1000};

			fd_set r;
			FD_ZERO(&r);
			FD_SET(s, &r);
			
			int v = select((int)s+1, &r, 0, 0, &t);
			if (v > 0 && FD_ISSET(s, &r))
			{
				return true;
			}
			else if (v < 0)
			{
				Error();
			}
		
		#endif
	}
	else
		LgiTrace("%s:%i - Not a valid socket.\n", _FL);

	return false;
}

bool LSocket::IsWritable(int TimeoutMs)
{
	return d->SelectWithCancel(TimeoutMs, false);
}

bool LSocket::CanAccept(int TimeoutMs)
{
	return IsReadable(TimeoutMs);
}

bool LSocket::IsBlocking()
{
	return d->Blocking != 0;
}

void LSocket::IsBlocking(bool block)
{
	if (d->Blocking ^ block)
	{
		d->Blocking = block;
	
		#if defined WIN32
		ulong NonBlocking = !block;
		ioctlsocket(d->Socket, FIONBIO, &NonBlocking);
		#elif defined POSIX
		fcntl(d->Socket, F_SETFL, d->Blocking ? 0 : O_NONBLOCK);
		#else
		#error Impl me.
		#endif
	}
}

bool LSocket::IsDelayed()
{
	return !d->NoDelay;
}

void LSocket::IsDelayed(bool Delay)
{
	bool NoDelay = !Delay;
	if (d->NoDelay ^ NoDelay)
	{
		d->NoDelay = NoDelay;
	
		option_t i = d->NoDelay != 0;
		setsockopt(d->Socket, IPPROTO_TCP, TCP_NODELAY, (const char*)&i, sizeof(i));
	}
}

int LSocket::GetLocalPort()
{
	struct sockaddr_in addr;
	socklen_t size;
	
	ZeroObj(addr);
	size = sizeof(addr);
	if ((getsockname(Handle(), (sockaddr*)&addr, &size)) >= 0)
	{
		return ntohs(addr.sin_port);
	}
	return 0;
}

bool LSocket::GetLocalIp(char *IpAddr)
{
	if (IpAddr)
	{
		struct sockaddr_in addr;
		socklen_t size;
		
		size = sizeof(addr);
		if ((getsockname(Handle(), (sockaddr*)&addr, &size)) < 0)
			return false;
		
		if (addr.sin_addr.s_addr == INADDR_ANY)
			return false;
		
		uchar *a = (uchar*)&addr.sin_addr.s_addr;
		sprintf_s(	IpAddr,
					16,
					"%i.%i.%i.%i",
					a[0],
					a[1],
					a[2],
					a[3]);
	
		return true;
	}

	return false;
}

bool LSocket::GetRemoteIp(uint32_t *IpAddr)
{
	if (IpAddr)
	{
		struct sockaddr_in a;
		socklen_t addrlen = sizeof(a);
		if (!getpeername(Handle(), (sockaddr*)&a, &addrlen))
		{
			*IpAddr = ntohl(a.sin_addr.s_addr);
			return true;
		}
	}

	return false;
}

bool LSocket::GetRemoteIp(char *IpAddr)
{
	if (!IpAddr)
		return false;

	uint32_t Ip = 0;
	if (!GetRemoteIp(&Ip))
		return false;

	sprintf_s(	IpAddr,
				16,
				"%u.%u.%u.%u",
				(Ip >> 24) & 0xff,
				(Ip >> 16) & 0xff,
				(Ip >> 8) & 0xff,
				(Ip) & 0xff);

	return false;
}

int LSocket::GetRemotePort()
{
	struct sockaddr_in a;
	socklen_t addrlen = sizeof(a);
	if (!getpeername(Handle(), (sockaddr*)&a, &addrlen))
	{
		return a.sin_port;
	}

	return 0;
}

int LSocket::Open(const char *HostAddr, int Port)
{
	int Status = -1;
	
	Close();

	if (HostAddr)
	{
		BytesWritten = 0;
		BytesRead = 0;
		
		sockaddr_in RemoteAddr;
		HostEnt *Host = 0;
		in_addr_t IpAddress = 0;

		ZeroObj(RemoteAddr);
		#ifdef WIN32
		d->Socket = WSASocket(AF_INET, SOCK_STREAM, IPPROTO_TCP, 0, 0, WSA_FLAG_OVERLAPPED);
		#else
		d->Socket = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
		#endif

		if (ValidSocket(d->Socket))
		{
			LArray<char> Buf(512);

			#if !defined(MAC)
			option_t i;
			socklen_t sz = sizeof(i);
			int r = getsockopt(d->Socket, IPPROTO_TCP, TCP_NODELAY, (char*)&i, &sz);
			if (d->NoDelay ^ i)
			{
				i = d->NoDelay != 0;
				setsockopt(d->Socket, IPPROTO_TCP, TCP_NODELAY, (char *) &i, sizeof(i));
			}
			#endif

			if (IsDigit(*HostAddr) && strchr(HostAddr, '.'))
			{
				// Ip address
				// IpAddress = inet_addr(HostAddr);
				if (!inet_pton(AF_INET, HostAddr, &IpAddress))
				{
					Error();
					return 0;
				}
				
				/* This seems complete unnecessary? -fret Dec 2018
				#if defined(WIN32)

				Host = c((const char*) &IpAddress, 4, AF_INET);
				if (!Host)
					Error();

				#else

				Host = gethostbyaddr
				(
					#ifdef MAC
					HostAddr,
					#else
					&IpAddress,
					#endif
					4,
					AF_INET
				);

				#endif
				*/
			}
			else
			{
				// Name address
				#ifdef LINUX

				Host = new HostEnt;
				if (Host)
				{
					memset(Host, 0, sizeof(*Host));
					
					HostEnt *Result = 0;
					int Err = 0;
					int Ret;
					while
					(
						(
							!GetCancel()
							||
							!GetCancel()->IsCancelled()
						)
						&&
						(
							Ret
							=
							gethostbyname_r(HostAddr,
											Host,
											&Buf[0], Buf.Length(),
											&Result,
											&Err)
						)
						==
						ERANGE
					)
					{
						Buf.Length(Buf.Length() << 1);
					}
					if (Ret)
					{
						auto ErrStr = LErrorCodeToString(Err);
						printf("%s:%i - gethostbyname_r('%s') returned %i, %i, %s\n",
							_FL, HostAddr, Ret, Err, ErrStr.Get());
						DeleteObj(Host);
					}
				}
				
				#if DEBUG_CONNECT
				printf("%s:%i - Host=%p\n", __FILE__, __LINE__, Host);
				#endif
				
				#else
				
				Host = gethostbyname(HostAddr);
				
				#endif
				
				if (!Host)
				{
					Error();
					Close();
					return false;
				}
			}
			
			if (1)
			{
				RemoteAddr.sin_family = AF_INET;
				RemoteAddr.sin_port = htons(Port);
				
				if (Host)
				{
					if (Host->h_addr_list && Host->h_addr_list[0])
					{
						memcpy(&RemoteAddr.sin_addr, Host->h_addr_list[0], sizeof(in_addr) );
					}
					else return false;
				}
				else
				{
					memcpy(&RemoteAddr.sin_addr, &IpAddress, sizeof(IpAddress) );
				}

				#ifdef WIN32
				if (d->Timeout < 0)
				{
					// Do blocking connect
					Status = connect(d->Socket, (sockaddr*) &RemoteAddr, sizeof(sockaddr_in));
				}
				else
				#endif
				{
					#define CONNECT_LOGGING			0
					
					// Setup the connect
					bool Block = IsBlocking();
					if (Block)
					{
						#if CONNECT_LOGGING
						LgiTrace(LPrintSock " - Setting non blocking\n", d->Socket);
						#endif
						IsBlocking(false);
					}
				
					// Do initial connect to kick things off..
					#if CONNECT_LOGGING
					LgiTrace(LPrintSock " - Doing initial connect to %s:%i\n", d->Socket, HostAddr, Port);
					#endif
					Status = connect(d->Socket, (sockaddr*) &RemoteAddr, sizeof(sockaddr_in));
					#if CONNECT_LOGGING
					LgiTrace(LPrintSock " - Initial connect=%i Block=%i\n", d->Socket, Status, Block);
					#endif

					// Wait for the connect to finish?
					if (Status && Block)
					{
						Error(Host);

						#ifdef WIN32
						// yeah I know... wtf? (http://itamarst.org/writings/win32sockets.html)
						#define IsWouldBlock() (d->LastError == EWOULDBLOCK || d->LastError == WSAEINVAL || d->LastError == WSAEWOULDBLOCK)
						#else
						#define IsWouldBlock() (d->LastError == EWOULDBLOCK || d->LastError == EINPROGRESS)
						#endif

						#if CONNECT_LOGGING
						LgiTrace(LPrintSock " - IsWouldBlock()=%i d->LastError=%i\n", d->Socket, IsWouldBlock(), d->LastError);
						#endif

						int64 End = LCurrentTime() + (d->Timeout > 0 ? d->Timeout : 30000);
						while (	!d->Cancel->IsCancelled() &&
								ValidSocket(d->Socket) &&
								IsWouldBlock())
						{
							int64 Remaining = End - LCurrentTime();

							#if CONNECT_LOGGING
							LgiTrace(LPrintSock " - Remaining " LPrintfInt64 "\n", d->Socket, Remaining);
							#endif

							if (Remaining < 0)
							{
								#if CONNECT_LOGGING
								LgiTrace(LPrintSock " - Leaving loop\n", d->Socket);
								#endif
								break;
							}
							
							if (IsWritable((int)MIN(Remaining, 1000)))
							{
								// Should be ready to connect now...
								#if CONNECT_LOGGING
								LgiTrace(LPrintSock " - Secondary connect...\n", d->Socket);
								#endif
								Status = connect(d->Socket, (sockaddr*) &RemoteAddr, sizeof(sockaddr_in));
								#if CONNECT_LOGGING
								LgiTrace(LPrintSock " - Secondary connect=%i\n", d->Socket, Status);
								#endif
								if (Status != 0)
								{
									Error(Host);
									
									if (d->LastError == EISCONN
										#ifdef WIN32
										|| d->LastError == WSAEISCONN // OMG windows, really?
										#endif
										)
									{
										Status = 0;
									}
									else
									{
										#if CONNECT_LOGGING
										LgiTrace(LPrintSock " - Connect=%i Err=%i\n", d->Socket, Status, d->LastError);
										#endif
										if (IsWouldBlock())
											continue;
									}
									break;
								}
								else
								{
									#if CONNECT_LOGGING
									LgiTrace(LPrintSock " - Connected...\n", d->Socket);
									#endif
									break;
								}
							}
							else
							{
								#if CONNECT_LOGGING
								LgiTrace(LPrintSock " - Timout...\n", d->Socket);
								#endif
							}
						}
					}

					if (Block)
						IsBlocking(true);
				}

				if (!Status)
				{
					char Info[256];
					sprintf_s(Info,
							sizeof(Info),
							"[INET] Socket Connect: %s [%i.%i.%i.%i], port: %i",
							HostAddr,
							(RemoteAddr.sin_addr.s_addr) & 0xFF,
							(RemoteAddr.sin_addr.s_addr >> 8) & 0xFF,
							(RemoteAddr.sin_addr.s_addr >> 16) & 0xFF,
							(RemoteAddr.sin_addr.s_addr >> 24) & 0xFF,
							Port);
					OnInformation(Info);
				}
				else
				{
					Error();
				}
			}

			if (Status)
			{
				#ifdef WIN32
				closesocket(d->Socket);
				#else
				close(d->Socket);
				#endif
				d->Socket = INVALID_SOCKET;
			}
		}
		else
		{
			Error();
		}
	}
	
	return Status == 0;
}

bool LSocket::SetReuseAddress(bool reuse)
{
	if (!ValidSocket(d->Socket))
		d->Socket = socket(AF_INET, SOCK_STREAM, 0);
	if (!ValidSocket(d->Socket))
		return false;

	int so_reuseaddr = reuse;
	if (setsockopt(Handle(), SOL_SOCKET, SO_REUSEADDR, (const char *)&so_reuseaddr, sizeof so_reuseaddr))
	{
		OnError(0, "Attempt to set SO_REUSEADDR failed.");
		// This might not be fatal... so continue on.
		return false;
	}
	
	return true;
}

bool LSocket::Bind(int Port, bool reuseAddr)
{
	if (!ValidSocket(d->Socket))
	{
		OnError(0, "Attempt to use invalid socket to bind.");
		return false;
	}

	if (reuseAddr)
		SetReuseAddress(true);

	sockaddr_in add;
	add.sin_family = AF_INET;
	add.sin_addr.s_addr = htonl(INADDR_ANY);
	add.sin_port = htons(Port);
	int ret = bind(Handle(), (sockaddr*)&add, sizeof(add));
	if (ret)
	{
		Error();
	}

	return ret == 0;
}

bool LSocket::Listen(int Port)
{
	if (!ValidSocket(d->Socket))
		d->Socket = socket(AF_INET, SOCK_STREAM, 0);

	if (!ValidSocket(d->Socket))
	{
		Error();
		return false;
	}

	BytesWritten = 0;
	BytesRead = 0;

	sockaddr Addr;
	sockaddr_in *a = (sockaddr_in*) &Addr;
	ZeroObj(Addr);
	a->sin_family = AF_INET;
	a->sin_port = htons(Port);
	a->sin_addr.OsAddr = INADDR_ANY;

	if (bind(d->Socket, &Addr, sizeof(Addr)) < 0)
	{
		Error();
		return false;
	}
		
	if (listen(d->Socket, SOMAXCONN) == SOCKET_ERROR)
	{
		Error();
		return false;
	}

	return true;
}

bool LSocket::Accept(LSocketI *c)
{
	if (!c)
	{
		LAssert(0);
		return false;
	}
	
	OsSocket NewSocket = INVALID_SOCKET;
	sockaddr Address;
	
	socklen_t Length = sizeof(Address);
	uint64 Start = LCurrentTime();
	while (	!d->Cancel->IsCancelled() &&
			ValidSocket(d->Socket))
	{
		if (IsReadable(100))
		{
			NewSocket = accept(d->Socket, &Address, &Length);
			break;
		}
		else if (d->Timeout > 0)
		{
			uint64 Now = LCurrentTime();
			if (Now - Start >= d->Timeout)
			{
				LString s;
				s.Printf("Accept timeout after %.1f seconds.", ((double)(Now-Start)) / 1000.0);
				OnInformation(s);
				return false;
			}
		}
	}
	
	if (!ValidSocket(NewSocket))
		return false;

	return ValidSocket(c->Handle(NewSocket));
}

int LSocket::Close()
{
	if (ValidSocket(d->Socket))
	{
		#if defined WIN32
		closesocket(d->Socket);
		#else
		close(d->Socket);
		#endif
		d->Socket = INVALID_SOCKET;
		OnDisconnect();
	}

	return true;
}

void LSocket::Log(const char *Msg, ssize_t Ret, const char *Buf, ssize_t Len)
{
	if (d->LogFile)
	{
		LFile f;
		if (f.Open(d->LogFile, O_WRITE))
		{
			f.Seek(f.GetSize(), SEEK_SET);

			switch (d->LogType)
			{
				case NET_LOG_HEX_DUMP:
				{
					char s[256];

					f.Write(s, sprintf_s(s, sizeof(s), "%s = %i\r\n", Msg, (int)Ret));

					for (int i=0; i<Len;)
					{
						char Ascii[32], *a = Ascii;
						int Ch = sprintf_s(s, sizeof(s), "%8.8X\t", i);
						for (int n = i + 16; i<Len && i<n; i++)
						{
							*a = (uchar)Buf[i];
							if (*a < ' ') *a = '.';
							a++;

							Ch += sprintf_s(s+Ch, sizeof(s)-Ch, "%2.2X ", (uchar)Buf[i]);
						}
						*a++ = 0;
						while (Ch < 58)
						{
							Ch += sprintf_s(s+Ch, sizeof(s)-Ch, " ");
						}
						Ch += sprintf_s(s+Ch, sizeof(s)-Ch, "%s\r\n", Ascii);
						f.Write(s, Ch);
					}
					break;
				}
				case NET_LOG_ALL_BYTES:
				{
					f.Write(Buf, (int)Len);
					break;
				}
			}
		}
	}
}

ssize_t LSocket::Write(const void *Data, ssize_t Len, int Flags)
{
	if (!ValidSocket(d->Socket) || !Data || d->Cancel->IsCancelled())
		return -1;

	int Status = 0;

	if (d->Timeout < 0 || IsWritable(d->Timeout))
	{
		Status = (int)send
		(
			d->Socket,
			(char*)Data,
			(int) Len,
			Flags
			#ifndef MAC
			| MSG_NOSIGNAL
			#endif
		);
	}
	
	if (Status < 0)
		Error();
	else if (Status == 0)
		OnDisconnect();
	else
	{
		if (Status < Len)
		{
			// Just in case it's a string lets be safe
			((char*)Data)[Status] = 0;
		}

		BytesWritten += Status;
		OnWrite((char*)Data, Status);
	}

	return Status;
}

ssize_t LSocket::Read(void *Data, ssize_t Len, int Flags)
{
	if (!ValidSocket(d->Socket) || !Data || d->Cancel->IsCancelled())
		return -1;

	ssize_t Status = -1;

	if (!d->Blocking ||
		d->Timeout < 0 ||
		IsReadable(d->Timeout))
	{
		Status = recv(d->Socket, (char*)Data, (int) Len, Flags
			#ifdef MSG_NOSIGNAL
			| MSG_NOSIGNAL
			#endif
			);
	}

	Log("Read", (int)Status, (char*)Data, Status>0 ? Status : 0);

	if (Status < 0)
		Error();
	else if (Status == 0)
		OnDisconnect();
	else
	{
		if (Status < Len)
		{
			// Just in case it's a string lets be safe
			((char*)Data)[Status] = 0;
		}

		BytesRead += Status;
		OnRead((char*)Data, (int)Status);
	}

	return (int)Status;
}

void LSocket::OnError(int ErrorCode, const char *ErrorDescription)
{
	d->ErrStr.Printf("Error(%i): %s", ErrorCode, ErrorDescription);
}

const char *LSocket::GetErrorString()
{
	return d->ErrStr;
}

int LSocket::Error(void *Param)
{
	// Get the most recent error.
	if (!(d->LastError =
		#ifdef WIN32
		WSAGetLastError()
		#else
		errno
		#endif
		))
		return 0;

	// These are not really errors...
	if (d->LastError == EWOULDBLOCK ||
		d->LastError == EISCONN)
		return 0;

	static class ErrorMsg {
	public:
		int Code;
		const char *Msg;
	}
	ErrorCodes[] =
	{
		{0,					"Socket disconnected."},

		#if defined WIN32
		{WSAEACCES,			"Permission denied."},
		{WSAEADDRINUSE,		"Address already in use."},
		{WSAEADDRNOTAVAIL,	"Cannot assign requested address."},
		{WSAEAFNOSUPPORT,	"Address family not supported by protocol family."},
		{WSAEALREADY,		"Operation already in progress."},
		{WSAECONNABORTED,	"Software caused connection abort."},
		{WSAECONNREFUSED,	"Connection refused."},
		{WSAECONNRESET,		"Connection reset by peer."},
		{WSAEDESTADDRREQ,	"Destination address required."},
		{WSAEFAULT,			"Bad address."},
		{WSAEHOSTDOWN,		"Host is down."},
		{WSAEHOSTUNREACH,	"No route to host."},
		{WSAEINPROGRESS,	"Operation now in progress."},
		{WSAEINTR,			"Interrupted function call."},
		{WSAEINVAL,			"Invalid argument."},
		{WSAEISCONN,		"Socket is already connected."},
		{WSAEMFILE,			"Too many open files."},
		{WSAEMSGSIZE,		"Message too long."},
		{WSAENETDOWN,		"Network is down."},
		{WSAENETRESET,		"Network dropped connection on reset."},
		{WSAENETUNREACH,	"Network is unreachable."},
		{WSAENOBUFS,		"No buffer space available."},
		{WSAENOPROTOOPT,	"Bad protocol option."},
		{WSAENOTCONN,		"Socket is not connected."},
		{WSAENOTSOCK,		"Socket operation on non-socket."},
		{WSAEOPNOTSUPP,		"Operation not supported."},
		{WSAEPFNOSUPPORT,	"Protocol family not supported."},
		{WSAEPROCLIM,		"Too many processes."},
		{WSAEPROTONOSUPPORT,"Protocol not supported."},
		{WSAEPROTOTYPE,		"Protocol wrong type for socket."},
		{WSAESHUTDOWN,		"Cannot send after socket shutdown."},
		{WSAESOCKTNOSUPPORT,"Socket type not supported."},
		{WSAETIMEDOUT,		"Connection timed out."},
		{WSAEWOULDBLOCK,	"Operation would block."},
		{WSAHOST_NOT_FOUND,	"Host not found."},
		{WSANOTINITIALISED,	"Successful WSAStartup not yet performed."},
		{WSANO_DATA,		"Valid name, no data record of requested type."},
		{WSANO_RECOVERY,	"This is a non-recoverable error."},
		{WSASYSNOTREADY,	"Network subsystem is unavailable."},
		{WSATRY_AGAIN,		"Non-authoritative host not found."},
		{WSAVERNOTSUPPORTED,"WINSOCK.DLL version out of range."},
		{WSAEDISCON,		"Graceful shutdown in progress."},
		#else
		{EACCES,			"Permission denied."},
		{EADDRINUSE,		"Address already in use."},
		{EADDRNOTAVAIL,		"Cannot assign requested address."},
		{EAFNOSUPPORT,		"Address family not supported by protocol family."},
		{EALREADY,			"Operation already in progress."},
		{ECONNABORTED,		"Software caused connection abort."},
		{ECONNREFUSED,		"Connection refused."},
		{ECONNRESET,		"Connection reset by peer."},
		{EFAULT,			"Bad address."},
		{EHOSTUNREACH,		"No route to host."},
		{EINPROGRESS,		"Operation now in progress."},
		{EINTR,				"Interrupted function call."},
		{EINVAL,			"Invalid argument."},
		{EISCONN,			"Socket is already connected."},
		{EMFILE,			"Too many open files."},
		{EMSGSIZE,			"Message too long."},
		{ENETDOWN,			"Network is down."},
		{ENETRESET,			"Network dropped connection on reset."},
		{ENETUNREACH,		"Network is unreachable."},
		{ENOBUFS,			"No buffer space available."},
		{ENOPROTOOPT,		"Bad protocol option."},
		{ENOTCONN,			"Socket is not connected."},
		{ENOTSOCK,			"Socket operation on non-socket."},
		{EOPNOTSUPP,		"Operation not supported."},
		{EPFNOSUPPORT,		"Protocol family not supported."},
		{EPROTONOSUPPORT,	"Protocol not supported."},
		{EPROTOTYPE,		"Protocol wrong type for socket."},
		{ESHUTDOWN,			"Cannot send after socket shutdown."},
		{ETIMEDOUT,			"Connection timed out."},
		{EWOULDBLOCK,		"Resource temporarily unavailable."},
		{HOST_NOT_FOUND,	"Host not found."},
		{NO_DATA,			"Valid name, no data record of requested type."},
		{NO_RECOVERY,		"This is a non-recoverable error."},
		{TRY_AGAIN,			"Non-authoritative host not found."},
		{ETIMEOUT,			"Operation timed out."},
		{EDESTADDRREQ,		"Destination address required."},
		{EHOSTDOWN,			"Host is down."},
		#ifndef HAIKU
		{ESOCKTNOSUPPORT,	"Socket type not supported."},
		#endif
		
		#endif
		{-1, 0}
	};

	ErrorMsg *Error = ErrorCodes;
	while (Error->Code >= 0 && Error->Code != d->LastError)
	{
		Error++;
	}

	if (d->LastError == 10060 && Param)
	{
		HostEnt *He = (HostEnt*)Param;
		char s[256];
		sprintf_s(s, sizeof(s), "%s (gethostbyname returned '%s')", Error->Msg, He->h_name);
		OnError(d->LastError, s);
	}
	else
		#if defined(MAC)
		if (d->LastError != 36)
		#endif
	{
		OnError(d->LastError, (Error->Code >= 0) ? Error->Msg : "<unknown error>");
	}


	switch (d->LastError)
	{
		case 0:
		#ifdef WIN32
		case 183: // I think this is a XP 'disconnect' ???
		case WSAECONNABORTED:
		case WSAECONNRESET:
		case WSAENETRESET:
		#else
		case ECONNABORTED:
		case ECONNRESET:
		case ENETRESET:
		#endif
		{
			Close();
			break;
		}
	}

	return d->LastError;
}

bool LSocket::GetUdp()
{
	return d->Udp != 0;
}

void LSocket::SetUdp(bool isUdp)
{
	if (d->Udp ^ isUdp)
	{
		d->Udp = isUdp;
		if (!ValidSocket(d->Socket))
		{
			if (d->Udp)
				d->Socket = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
			else
				d->Socket = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
		}

		if (d->Broadcast)
		{
			option_t enabled = d->Broadcast != 0;
			setsockopt(Handle(), SOL_SOCKET, SO_BROADCAST, (char*)&enabled, sizeof(enabled));
		}
	}
}

void LSocket::SetBroadcast(bool isBroadcast)
{
	d->Broadcast = isBroadcast;
}

bool LSocket::AddMulticastMember(uint32_t MulticastIp, uint32_t LocalInterface)
{
	if (!MulticastIp || !LocalInterface)
		return false;

	struct ip_mreq mreq;
	mreq.imr_multiaddr.s_addr = htonl(MulticastIp);		// your multicast address
	mreq.imr_interface.s_addr = htonl(LocalInterface);	// your incoming interface IP
	int r = setsockopt(Handle(), IPPROTO_IP, IP_ADD_MEMBERSHIP, (const char*) &mreq, sizeof(mreq));
	if (!r)
		return true;

	Error();
	return false;
}

bool LSocket::SetMulticastInterface(uint32_t Interface)
{
	if (!Interface)
		return false;

	struct sockaddr_in addr;
	addr.sin_addr.s_addr = Interface;
	auto r = setsockopt(Handle(), IPPROTO_IP, IP_MULTICAST_IF, (const char*) &addr, sizeof(addr));
	if (!r)
		return true;

	Error();
	return false;
}

bool LSocket::CreateUdpSocket()
{
	if (!ValidSocket(d->Socket))
	{
		d->Socket = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
		if (ValidSocket(d->Socket))
		{
			if (d->Broadcast)
			{
				option_t enabled = d->Broadcast != 0;
				auto r = setsockopt(Handle(), SOL_SOCKET, SO_BROADCAST, (char*)&enabled, sizeof(enabled));
				if (r)
					Error();
			}
		}
	}

	return ValidSocket(d->Socket);
}

int LSocket::ReadUdp(void *Buffer, int Size, int Flags, uint32_t *Ip, uint16_t *Port)
{
	if (!Buffer || Size < 0)
		return -1;

	CreateUdpSocket();

	sockaddr_in a;
	socklen_t AddrSize = sizeof(a);
	ZeroObj(a);
	a.sin_family = AF_INET;
	if (Port)
		a.sin_port = htons(*Port);
	#if defined(WINDOWS)
		a.sin_addr.S_un.S_addr = INADDR_ANY;
	#else
		a.sin_addr.s_addr = INADDR_ANY;
	#endif

	auto b = recvfrom(d->Socket, (char*)Buffer, Size, Flags, (sockaddr*)&a, &AddrSize);
	if (b > 0)
	{
		OnRead((char*)Buffer, (int)b);

		if (Ip)
			*Ip = ntohl(a.sin_addr.OsAddr);
		if (Port)
			*Port = ntohs(a.sin_port);
	}

	return (int)b;
}

int LSocket::WriteUdp(void *Buffer, int Size, int Flags, uint32_t Ip, uint16_t Port, int ttl)
{
	if (!Buffer || Size < 0)
		return -1;

	CreateUdpSocket();

	if (ttl > 0)
		setsockopt(d->Socket, IPPROTO_IP, IP_TTL, (const char*)&ttl, sizeof(ttl));

	sockaddr_in a;
	ZeroObj(a);
	a.sin_family = AF_INET;
	a.sin_port = htons(Port);
	a.sin_addr.OsAddr = Ip == 0xffffffff ? INADDR_ANY : htonl(Ip);
	ssize_t b = sendto(d->Socket, (char*)Buffer, Size, Flags, (sockaddr*)&a, sizeof(a));
	if (b > 0)
	{
		OnWrite((char*)Buffer, (int)b);
	}
	else
	{
		printf("%s:%i - sendto failed with %i.\n", _FL, errno);
	}
	return (int)b;
}

//////////////////////////////////////////////////////////////////////////////
bool LHaveNetConnection()
{
	bool Status = false;

	// Check for dial up connection
	#if defined WIN32
	typedef DWORD (__stdcall *RasEnumConnections_Proc)(LPRASCONN lprasconn, LPDWORD lpcb, LPDWORD lpcConnections); 
	typedef DWORD (__stdcall *RasGetConnectStatus_Proc)(HRASCONN hrasconn, LPRASCONNSTATUS lprasconnstatus);

	HMODULE hRas = (HMODULE) LoadLibraryA("rasapi32.dll");
	if (hRas)
	{
		RASCONN Con[10];
		DWORD Connections = 0;
		DWORD Bytes = sizeof(Con);

		ZeroObj(Con);
		Con[0].dwSize = sizeof(Con[0]);

		RasEnumConnections_Proc pRasEnumConnections = (RasEnumConnections_Proc) GetProcAddress(hRas, "RasEnumConnectionsA"); 
		RasGetConnectStatus_Proc pRasGetConnectStatus = (RasGetConnectStatus_Proc) GetProcAddress(hRas, "RasGetConnectStatusA"); 

		if (pRasEnumConnections &&
			pRasGetConnectStatus)
		{
			pRasEnumConnections(Con, &Bytes, &Connections);

			for (unsigned i=0; i<Connections; i++)
			{
				RASCONNSTATUS Stat;
				char *Err = "";

				ZeroObj(Stat);
				Stat.dwSize = sizeof(Stat);

				DWORD Error = 0;				
				if (!(Error=pRasGetConnectStatus(Con[i].hrasconn, &Stat)))
				{
					if (Stat.rasconnstate == RASCS_Connected)
					{
						Status = true;
					}
				}

				/*
				else
				{
					switch (Error)
					{
						case ERROR_NOT_ENOUGH_MEMORY:
						{
							Err = "Not enuf mem.";
							break;
						}
						case ERROR_INVALID_HANDLE:
						{
							Err = "Invalid Handle.";
							break;
						}
					}
				}

				int n=0;
				*/
			}
		}

		FreeLibrary(hRas);
	}
	#endif

	// Check for ethernet connection
	if (!Status)
	{
	}

	return Status;
}

///////////////////////////////////////////////////////////////////////////////////////////
// Gets a field value
void InetGetField(	const char *start,
					const char *end, // maybe NULL is not known, in which case scan till the end of the whole header value
					std::function<char*(size_t)> alloc,
					std::function<void(size_t)> setLength)
{
	LAssert(start != nullptr);
	const char *s = start, *e = start;
	static const char *WhiteSpace = " \r\t\n";
	LArray<LRange> parts;
	size_t totalLen = 0;

	// Look for the end of the string
	while (true)
	{
		if (*e == 0 || (end && e >= end))
		{
			auto len = e - s;
			totalLen += len;
			parts.New().Set(s - start, len);
			break;
		}
		else if (IsNewLine(*e))
		{
			auto len = e - s;
			totalLen += len;
			parts.New().Set(s - start, len);

			if (*e == '\r' &&
				(!end || end - e >= 3) &&
				e[1] == '\n' &&
				IsWhite(e[2]))
			{
				e += 3; // \r\n case
			}
			else if (*e == '\n' &&
				(!end || end - e >= 2) &&
				IsWhite(e[1]))
			{
				e += 2; // no '\r' case...
			}
			else break; // normal end of header value

			s = e;
		}
		else e++;
	}

	// Build the string...
	if (auto str = alloc(totalLen))
	{
		size_t offset = 0;
		for (auto &r: parts)
		{
			memcpy(str + offset, start + r.Start, r.Len);
			offset += r.Len;
		}
		str[totalLen] = 0;
	}
}

template<typename T>
T *SeekNextLine(T *s, T *End)
{
	if (!s)
		return s;

	for (; *s && *s != '\n' && (!End || s < End); s++)
		;
	if (*s == '\n' && (!End || s < End))
		s++;

	return s;
}

// Search through headers for a field
char *InetGetHeaderField(	// Returns an allocated string or NULL on failure
	const char *Headers,	// Pointer to all the headers
	const char *Field,		// The field your after
	ssize_t Len)			// Maximum len to run, or -1 for NULL terminated
{
	if (Headers && Field)
	{
		// for all lines
		const char *End = Len < 0 ? nullptr : Headers + Len;
		size_t FldLen = strlen(Field);
		
		for (const char *s = Headers;
			*s && (!End || s < End);
			s = SeekNextLine(s, End))
		{
			if (*s != 9 &&
				_strnicmp(s, Field, FldLen) == 0)
			{
				// found a match
				s += FldLen;
				if (*s == ':')
				{
					s++;

					// Skip over leading whitespace:
					while (*s)
					{
						if (strchr(" \t\r", *s))
						{
							s++;
						}
						else if (*s == '\n')
						{
							if (strchr(" \r\t", s[1]))
								s += 2;
							else
								break;
						}
						else break;							
					}
					
					char *value = NULL;
					InetGetField(s, End,
						[&value](auto sz)
						{
							return value = new char[sz + 1];
						},
						[&value](auto sz)
						{
							value[sz] = 0;
						});
					return value;
				}
			}
		}
	}

	return NULL;
}

struct LHeader
{
	char *field;
	size_t fieldLen;
	char *value;
	size_t valueLen;

	bool Is(LString f)
	{
		if (f.Length() != fieldLen)
			return false;
		return !Strnicmp(field, f.Get(), fieldLen);
	}

	LString UnwrapValue()
	{
		LStringPipe p(256);
		char *end = value + valueLen;
		for (char *s = value; s < end; )
		{
			char *e = s;
			while (e < end && !IsNewLine(*e))
				e++;

			p.Write(s, e - s);
			if (e >= end)
				break;

			// Skip over the wrapping marker
			if (e < end && *e == '\r') e++;
			if (e < end && *e == '\n') e++;
			if (e < end && IsWhite(*e)) e++;
			s = e;
		}
		return p.NewLStr();
	}
};

void IterateHeaders(
	/// Input headers
	LString &headers,
	/// Output array
	LArray<LHeader> &a,
	/// [Optional] filter for specfic field
	LString filterField = LString(),
	/// [Optional] max fields to return (0 = all)
	int limitCount = 0)
{
	if (!headers)
		return;

	if (filterField.Equals("List-Owner"))
	{
		int asd=0;
	}

	auto end = headers.Get() + headers.Length();
	for (auto s = headers.Get(); s < end; )
	{
		while (s < end && IsWhite(*s)) s++;
		auto field = s;
		
		while (s < end && !strchr("\r\n:", *s)) s++;
		auto fieldLen = s - field;

		if (s < end && *s == ':')
		{
			// has value
			s++;
			while (s < end && (*s == ' ' || *s == '\t'))
				s++;
			auto value = s;
			while (true)
			{
				if (s >= end   ||
					*s == 0    ||
					*s == '\r' ||
					*s == '\n')
				{
					// is it a multiline value?
					if (end - s >= 3 &&
						IsNewLine(s[0]) &&
						IsNewLine(s[1]) &&
						IsWhite(s[2]))
					{
						// yes.. keep going
						s += 2;
					}
					else
					{
						if
						(
							filterField.IsEmpty()
							||
							(
								filterField.Length() == fieldLen
								&&
								!Strnicmp(filterField.Get(), field, fieldLen)
							)
						)
						{
							// no... emit header
							auto &hdr = a.New();
							hdr.field = field;
							hdr.fieldLen = fieldLen;
							hdr.value = value;
							hdr.valueLen = s - value;
							
							if (limitCount > 0 &&
								a.Length() >= limitCount)
								return;
						}
						break;
					}
				}
				s++;
			}
		}
		else
		{
			while (s < end && !strchr("\r\n", *s)) s++;
		}
		if (s < end && *s == '\r') s++;
		if (s < end && *s == '\n') s++;
	}
}

LString LGetHeaderField(LString Headers, const char *Field)
{
	if (!Headers || !Field)
		return LString();

	LArray<LHeader> parsed;
	IterateHeaders(Headers, parsed, Field, 1);
	if (parsed.Length() > 0)
		return parsed[0].UnwrapValue();
	
	return LString();
}

static LString WrapValue(LString hdr, LString val)
{
	const int MaxLine = 76;
	LStringPipe out;
	for (size_t i = 0; i < val.Length(); )
	{
		auto remaining = val.Length() - i;
		auto avail = i ? MaxLine - 1 : MaxLine - hdr.Length() - 2; 
		auto bytes = MIN(remaining, avail);

		out.Write(val.Get() + i, bytes);
		if (bytes < remaining)
			out.Write("\r\n ", 3);

		i += bytes;
	}
	return out.NewLStr();
}

bool LSetHeaderFeild(LString &headers, LString field, LString value)
{
	LArray<LHeader> parsed;
	IterateHeaders(headers, parsed, LString());
	
	// Check if the header already exists...
	for (auto &h: parsed)
	{
		if (h.Is(field))
		{
			// It does... rewrite the value
			auto before = headers(0, h.value - headers.Get());
			auto after = headers(h.value + h.valueLen - headers.Get(), -1);
			headers = before + WrapValue(field, value) + after;
			return true;
		}
	}

	// Not already part of the header list... append it
	bool hasTrailingEol = headers.Length() >= 2 &&
		headers(-2) == '\r' &&
		headers(-1) == '\n';
	headers += LString::Fmt("%s%s: %s\r\n", hasTrailingEol ? "" : "\r\n", field.Get(), WrapValue(field, value).Get());

	return true;
}

bool LHeaderUnitTests()
{
	const char *testHdrs = "Content-Type: text/html\r\n"
		"Content-Length: 12345\r\n"
		"Cookie: someData";
	const char *testHdrs2 =
		"Thread-Index: AdweBDOj\r\n"
		"Content-Class: \r\n"
		"Date: Fri, 5 Sep 2025 03:58:07 +0000\r\n"
		"Message-ID: \r\n"
		" <JH0PR03MB8021DD51331.outlook.com>\r\n";
	const char *contentType = "text/html";
	const char *contentLen = "12345";
	const char *cookie = "someData";

	#define CHECK(val) if (!(val)) { LAssert(0); return false; }

	auto date = LGetHeaderField(testHdrs2, "Date");
	CHECK(date == "Fri, 5 Sep 2025 03:58:07 +0000");
	auto msgId = LGetHeaderField(testHdrs2, "Message-ID");
	CHECK(msgId == "<JH0PR03MB8021DD51331.outlook.com>");

	CHECK(LGetHeaderField(testHdrs, "Content-Type") == contentType);
	CHECK(LGetHeaderField(testHdrs, "Content-Length") == contentLen);
	CHECK(LGetHeaderField(testHdrs, "Cookie") == cookie);

	// Test setting the len to a shorter value:
	LString in = testHdrs;
	const char *shortVal = "321";
	LSetHeaderFeild(in, "Content-Length", shortVal);
	CHECK(LGetHeaderField(in, "Content-Type") == contentType);
	CHECK(LGetHeaderField(in, "Content-Length") == shortVal);
	CHECK(LGetHeaderField(in, "Cookie") == cookie);
	
	// Test setting the len to a longer value:
	in = testHdrs;
	const char *longVal = "5675689569234";
	LSetHeaderFeild(in, "Content-Length", longVal);
	CHECK(LGetHeaderField(in, "Content-Type") == contentType);
	CHECK(LGetHeaderField(in, "Content-Length") == longVal);
	CHECK(LGetHeaderField(in, "Cookie") == cookie);

	// Test setting the last value:
	in = testHdrs;
	const char *newCookie = "thisIsTheOtherData";
	LSetHeaderFeild(in, "Cookie", newCookie);
	CHECK(LGetHeaderField(in, "Content-Type") == contentType);
	CHECK(LGetHeaderField(in, "Content-Length") == contentLen);
	CHECK(LGetHeaderField(in, "Cookie") == newCookie);

	// Test setting a wrapped value
	in = testHdrs;
	const char *newType = "0----=====1----=====2----=====3----=====4----=====5----=====6----=====7----=====8----=====9----=====10---=====";
	LSetHeaderFeild(in, "Content-Type", newType);
	CHECK(LGetHeaderField(in, "Content-Type") == newType);
	CHECK(LGetHeaderField(in, "Content-Length") == contentLen);
	CHECK(LGetHeaderField(in, "Cookie") == cookie);
	for (auto ln: in.SplitDelimit("\r\n"))
		CHECK(ln.Length() <= 76);

	return true;
}

void InetGetSubField_Impl(	const char *s,
							const char *Field,
							std::function<void(const char*, size_t)> output)
{
	if (!s || !Field)
		return;

	s = strchr(s, ';');
	if (!s)
		return;
	s++;

	auto FieldLen = strlen(Field);
	auto White = " \t\r\n";
	while (*s)
	{
		// Skip leading whitespace
		while (*s && (strchr(White, *s) || *s == ';')) s++;

		// Parse field name
		if (IsAlpha((uint8_t)*s))
		{
			const char *f = s;
			while (*s && (IsAlpha(*s) || *s == '-')) s++;
			bool HasField = ((s-f) == FieldLen) && (_strnicmp(Field, f, FieldLen) == 0);
			while (*s && strchr(White, *s)) s++;
			if (*s == '=')
			{
				s++;
				while (*s && strchr(White, *s)) s++;
				if (*s && strchr("\'\"", *s))
				{
					// Quote Delimited Field
					char d = *s++;
					char *e = strchr((char*)s, d);
					if (e)
					{
						if (HasField)
						{
							output(s, e - s);
							break;
						}

						s = e + 1;
					}
					else break;
				}
				else
				{
					// Space Delimited Field
					const char *e = s;
					while (*e && !strchr(White, *e) && *e != ';') e++;

					if (HasField)
					{
						output(s, e - s);
						break;
					}

					s = e;
				}
			}
			else break;
		}
		else break;
	}
}

char *InetGetSubField(const char *HeaderValue, const char *Field)
{
	char *result = NULL;
	InetGetSubField_Impl(HeaderValue, Field,
						[&result](auto str, auto sz)
						{
							result = NewStr(str, sz);
						});
	return result;
}

LString LGetSubField(LString HeaderValue, const char *Field)
{
	LString result;
	InetGetSubField_Impl(HeaderValue, Field,
						[&result](auto str, auto sz)
						{
							result.Set(str, sz);
						});
	return result;
}

LString LIpToStr(uint32_t ip)
{
	LString s;
	s.Printf("%i.%i.%i.%i",
			(ip>>24)&0xff,
			(ip>>16)&0xff,
			(ip>>8)&0xff,
			(ip)&0xff);
	return s;
}

uint32_t LIpToInt(LString str)
{
	auto p = str.Split(".");
	if (p.Length() != 4)
		return 0;
	
	uint32_t ip = 0;
	for (auto &s : p)
	{
		ip <<= 8;
		auto n = s.Int();
		if (n > 255)
		{
			LAssert(0);
			return 0;
		}
		ip |= (uint8_t)s.Int();
	}
	return ip;
}

uint32_t LHostnameToIp(const char *Host)
{
	if (!Host)
		return 0;
		
	// struct addrinfo hints = {};
	struct addrinfo *res = NULL;
	getaddrinfo(Host, NULL, NULL, &res);
	if (!res)
		return 0;

	uint32_t ip = 0;
	if (res->ai_addr)
	{
		auto fam = res->ai_addr->sa_family;
		if (fam == AF_INET)
		{
			auto a = (sockaddr_in*)res->ai_addr;
			ip = ntohl(a->sin_addr.s_addr);
		}
	}
	freeaddrinfo(res);
	
	return ip;
}

#if WINDOWS
	#define M_ASYNC_HOSTNAME		M_USER + 200
	LRESULT CALLBACK LHostnameAsyncPriv_Proc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam);
	#define PORTABLE_GETADDR		0
#elif defined(MAC) || defined(HAIKU)
	#define PORTABLE_GETADDR		1
	#include "lgi/common/Thread.h"
	#include "lgi/common/ThreadEvent.h"
#else
	#define PORTABLE_GETADDR		0
#endif

struct LHostnameAsyncPriv
	: public LRefCount
	#if PORTABLE_GETADDR
	, public LThread
	, public LCancel
	#endif
{
	LHostnameAsync::TCallback callback;
	LError err;

	#if PORTABLE_GETADDR
		LString::Array nameServers;
		LString searchDomain;
		LString name;
		uint64_t endTime = 0;
		static uint16_t nextId;
		uint16_t transId = 0;
		uint32_t result_ip4 = 0;
		LString result_name;
	#elif WINDOWS
		PADDRINFOEX	addrInfo = nullptr;
		OVERLAPPED	overlapped = {};
		HANDLE		completeEvent = nullptr;
		HANDLE		cancelHandle = nullptr;
		LHostnameAsyncPriv()
		{
			completeEvent = CreateEvent(NULL, TRUE, FALSE, NULL);
		}
		~LHostnameAsyncPriv()
		{
		}
	#else // posixy?
		struct gaicb req = {};
		struct sigevent event = {};
		bool done = false;
	#endif
	uint64_t startTs = 0;

	#if PORTABLE_GETADDR
		LHostnameAsyncPriv() : LThread("LHostnameAsyncPriv")
		{
			nameServers.Add("192.168.0.1");
			
			// Look up the DNS servers
			LFile file("/etc/resolv.conf", O_READ);
			if (file)
			{
				for (auto &ln: file.Read().SplitDelimit("\n"))
				{
					if (ln(0) == '#')
						continue;
					auto parts = ln.SplitDelimit();
					if (parts.Length() == 2)
					{
						if (parts[0].Equals("nameserver"))
							nameServers.Add(parts[1]);
						else if (parts[0].Equals("search"))
							searchDomain = parts[1];
					}
				}
			}
			
			printf("resolve: %s\n", LString(",").Join(nameServers).Get());
		}
		
		~LHostnameAsyncPriv()
		{
			Cancel();
			WaitForExit();
		}
		
		void DnsLookup(const char *addr, int timeoutMs)
		{
			IncRef();
			name = addr;
			endTime = timeoutMs >= 0 ? LCurrentTime() + timeoutMs : 0;
			Run();
		}
		
		enum QTypes {
			QT_A		= 1,  // a host address
			QT_NS		= 2,  // an authoritative name server
			QT_MD		= 3,  // a mail destination (Obsolete - use MX)
			QT_MF		= 4,  // a mail forwarder (Obsolete - use MX)
			QT_CNAME	= 5,  // the canonical name for an alias
			QT_SOA		= 6,  // marks the start of a zone of authority
			QT_MB		= 7,  // a mailbox domain name (EXPERIMENTAL)
			QT_MG	 	= 8,  // a mail group member (EXPERIMENTAL)
			QT_MR		= 9,  // a mail rename domain name (EXPERIMENTAL)
			QT_NULL		= 10, // a null RR (EXPERIMENTAL)
			QT_WKS		= 11, // a well known service description
			QT_PTR		= 12, // a domain name pointer
			QT_HINFO	= 13, // host information
			QT_MINFO	= 14, // mailbox or mail list information
			QT_MX		= 15, // mail exchange
			QT_TXT		= 16, // text strings
		};
		
		enum QClasses {
			QC_IN		= 1, // the Internet
			QC_CS		= 2, // the CSNET class (Obsolete - used only for examples in some obsolete RFCs)
			QC_CH		= 3, // the CHAOS class
			QC_HS		= 4, // Hesiod [Dyer 87]
		};

		constexpr static int BUF_LEN = 1500;
		
		LString ReadStr(LPointer &p, char *msgStart)
		{
			if ((*p.u8 & 0xc0) == 0xc0)
			{
				// Reference...
				auto u16 = ntohs(*p.u16++);
				auto offset = u16 & ~0xc000;
				if (offset < 0 || offset >= BUF_LEN)
				{
					printf("readstr invalid offset=%i\n", offset);
					return LString();
				}
				
				LPointer ptr;
				ptr.c = msgStart + offset;
				return ReadStr(ptr, msgStart);
			}

			// Literal:
			LString::Array a;			
			while (*p.u8 > 0)
			{
				if (*p.u8 >= 64)
				{
					LAssert(!"label too long.");
					return LString();
				}
				
				auto &s = a.New();
				if (!s.Length(*p.u8++))
					return LString();
				memcpy(s.Get(), p.c, s.Length());
				p.c += s.Length();
			}
			p.c++;
			return LString(".").Join(a);
		}
		
		bool WriteStr(LPointer &p, LString &s)
		{
			auto parts = s.SplitDelimit(".");
			for (auto &part: parts)
			{
				if (part.Length() >= 64)
				{
					LAssert(!"label too long.");
					return false;
				}
				*p.u8++ = (uint8_t)part.Length();
				memcpy(p.c, part.Get(), part.Length());
				p.c += part.Length();
			}
			*p.c++ = 0;
			return true;
		}
		
		int Main() override
		{
			printf("in main.\n");

			LSocket sock;
			sock.SetUdp(true);

			while (!IsCancelled())
			{
				if (endTime && LCurrentTime() >= endTime)
				{
					printf("main timeout reached.\n");
					break;
				}
			
				char msg[BUF_LEN];
				LPointer p;
				p.c = msg;
					
				if (transId)
				{
					// Check for incomming replies...
					uint32_t read_ip = 0;
					uint16_t read_port = 0;
					if (auto rd = sock.ReadUdp(msg, sizeof(msg), 0, &read_ip, &read_port))
					{
						printf("rd=%i ip=%s port=%i\n", (int)rd, LIpToStr(read_ip).Get(), read_port);

						#define ERR(...) { printf(__VA_ARGS__); continue; }
						
						auto id = ntohs(*p.u16++);
						if (id != transId)
							ERR("%s:%i - not the right transaction id: %i/%i\n", _FL, id, transId);
						
						auto flags = ntohs(*p.u16++);
						if (!(flags & 0x8000))
							ERR("%s:%i - not a response\n", _FL)
						auto questions = ntohs(*p.u16++);
						auto answers = ntohs(*p.u16++);
						auto auth_rec = ntohs(*p.u16++);
						auto add_rec = ntohs(*p.u16++);
						LString q;
						for (int i=0; i<questions; i++)
						{
							// printf("read q:\n");
							q = ReadStr(p, msg);
							QTypes type = (QTypes) ntohs(*p.u16++);
							QClasses cls = (QClasses) ntohs(*p.u16++);
							// printf("got q: '%s' %i,%i\n", q.Get(), type, cls);
						}
						for (int i=0; i<answers; i++)
						{
							// printf("read a:\n");
							auto aName = ReadStr(p, msg);
							QTypes type = (QTypes) ntohs(*p.u16++);
							QClasses cls = (QClasses) ntohs(*p.u16++);
							auto ttl = ntohl(*p.u32++);
							auto len = ntohs(*p.u16++);
							if (type == QT_A)
							{
								if (len == 4)
								{
									result_name = aName;
									result_ip4 = ntohl(*p.u32);
									printf("a='%s' ip=%s type=%i cls=%i\n",
										aName.Get(), LIpToStr(result_ip4).Get(), type, cls);
								}
								else ERR("%s:%i - invalid A record len: %i\n", _FL, len);
							}
							p.c += len;
						}						
						break;
					}
				}
				else
				{
					if (nameServers.Length() == 0)
					{
						err.Set(LErrorInvalidParam, "no dns server");
						Complete();
						break;
					}

					// Send UDP request packet
					auto dnsServer = LIpToInt(nameServers[0]);

					*p.u16++ = htons(transId = nextId++);
					*p.u16++ = htons(0x0100); // query, stardard, not trunc, recurse
					*p.u16++ = htons(1); // questions
					*p.u16++ = htons(0); // answers
					*p.u16++ = htons(0); // nameserver records
					*p.u16++ = htons(1); // additional records
					
					// question record:
					if (!WriteStr(p, name))
						break;
					*p.u16++ = htons(QT_A);
					*p.u16++ = htons(QC_IN);
					
					// additional record:
					*p.u8++ = 0; // root
					*p.u16++ = htons(41); // OPT
					*p.u16++ = htons(1472); // UDP payload size
					*p.u8++ = 0; // ext rcode
					*p.u8++ = 0; // EDNS0 ver
					*p.u16++ = htons(0); // Z: cannot handle DNSSEC
					*p.u16++ = htons(0); // data len?
					
					int len = p.c - msg;
					int wr = sock.WriteUdp(msg, len, 0, dnsServer, DNS_PORT);
					printf("dns write=%i\n", wr);
				}
			}
			
			printf("%s:%i - call complete\n", _FL);
			Complete();
			printf("%s:%i - exit thread\n", _FL);
			return 0;
		}
	#endif
	
	void Complete()
	{
		#if PORTABLE_GETADDR
			if (callback)
			{
				if (!result_ip4)
				{
					err.Set(LErrorFuncFailed, "ip not found");
				}
				
				printf("%s:%i - calling cb\n", _FL);
				callback(result_ip4, err, LCurrentTime() - startTs);
			}
			else printf("%s:%i - no callback in complete\n", _FL);
		#elif WINDOWS
			uint32_t ip = 0;

			if (!err)
			{
				if (addrInfo)
				{
					if (addrInfo->ai_addr->sa_family == AF_INET)
					{
						auto a = (sockaddr_in*)addrInfo->ai_addr;
						ip = ntohl(a->sin_addr.s_addr);					
						// LgiTrace("%s:%i - complete called: %s (after %i ms)\n", _FL, LIpToStr(ip).Get(), (int)(LCurrentTime()-startTs));
					}
					else err.Set(LErrorInvalidParam, "not AF_INET family");
				}
				else
				{
					err.Set(LErrorInvalidParam, "no addrInfo");
				}
			}

			if (callback)
				callback(ip, err, LCurrentTime() - startTs);

			if (completeEvent)
			{
				CloseHandle(completeEvent);
				completeEvent = nullptr;
			}
		#else
			done = true;
			auto result = gai_error(&req);
			LError err;
			if (result)
			{
				err.Set(result, "gai_error failed");
				if (callback)
					callback(0, err, LCurrentTime() - startTs);
			}
			else
			{			
				auto out = req.ar_result;
				uint32_t ip = 0;
			
				#if 0
				printf(	"out:\n"
						"	ai_flags=%x\n"
						"	ai_family=%i\n"
						"	ai_socktype=%i\n"
						"	ai_protocol=%i\n"
						" 	ai_addrlen=%i\n"
						"	ai_addr=%p\n"
						"	ai_canonname='%s'\n"
						"	ai_next=%p\n",
						out->ai_flags,
						out->ai_family,
						out->ai_socktype,
						out->ai_protocol,
						(int)out->ai_addrlen,
						out->ai_addr,
						out->ai_canonname,
						out->ai_next);
				#endif
			
				if (out->ai_addr)
				{
					auto fam = out->ai_addr->sa_family;
					if (fam == AF_INET)
					{
						auto a = (sockaddr_in*)out->ai_addr;
						ip = ntohl(a->sin_addr.s_addr);
					}
					else err.Set(LErrorInvalidParam, "Not AF_INET.");
				}
				else err.Set(LErrorInvalidParam, "No ai_addr.");

				if (callback)
					callback(ip, err, LCurrentTime() - startTs);
			}
		#endif

		DecRef();
	}

	bool Cancel(bool b = true)
		#if PORTABLE_GETADDR
		override
		#endif
	{
		#if PORTABLE_GETADDR
			return LCancel::Cancel(b);
		#elif WINDOWS
			if (completeEvent)
			{
				// It didn't finish yet, so cancel it...
				GetAddrInfoExCancel(&cancelHandle);
				
				// And wait...
				WaitForSingleObject(completeEvent, INFINITE);
				
				// And cleanup
				CloseHandle(completeEvent);
			}
			return true;
		#else
			if (!done)
			{
				done = true;
				gai_cancel(&req);
			}
			return true;
		#endif		
	}
};

#if PORTABLE_GETADDR
	uint16_t LHostnameAsyncPriv::nextId = 1;
#endif

LHostnameAsync::LHostnameAsync(const char *host, TCallback callback, int timeoutMs)
{
	if ((d = new LHostnameAsyncPriv))
	{
		d->IncRef();
		d->callback = callback;
		d->startTs = LCurrentTime();

		#if PORTABLE_GETADDR
			d->DnsLookup(host, timeoutMs);
		#elif WINDOWS
			LAutoWString wHost(Utf8ToWide(host));
			d->IncRef();
			struct timeval timeout = {};
			if (timeoutMs >= 0)
			{
				timeout.tv_sec = timeoutMs / 1000;
				timeout.tv_usec = (timeoutMs % 1000) * 1000;
			}
			auto result = GetAddrInfoEx(wHost,
										nullptr, // service name
										NS_DNS,
										nullptr, // nspid
										nullptr, // hints
										&d->addrInfo,
										timeoutMs >= 0 ? &timeout : nullptr,
										&d->overlapped,
										[](auto error, auto bytes, auto overlapped)
										{
											auto ctx = CONTAINING_RECORD(overlapped, LHostnameAsyncPriv, overlapped);
											if (error != ERROR_SUCCESS)
												ctx->err.Set(error);
											ctx->Complete();
										},
										&d->cancelHandle);
			if (result != NO_ERROR &&
				result != WSA_IO_PENDING)
			{
				LError err(result);
				LgiTrace("%s:%i - GetAddrInfoExA err: %s\n", _FL, err.ToString().Get());
				if (callback)
					callback(0, err, LCurrentTime() - d->startTs);
				d->DecRef();
			}
		#else
			struct gaicb *reqs[1] = {&d->req};
			d->req.ar_name = host;

			d->event.sigev_notify = Gtk::SIGEV_THREAD;
			d->event.sigev_value.sival_ptr = d;
			d->event.sigev_notify_function = [](auto param)
				{
					if (auto obj = (LHostnameAsyncPriv*)param.sival_ptr)
						obj->Complete();
					else
						printf("%s:%i - Invalid param.\n", _FL);
				};
			int r = getaddrinfo_a(GAI_NOWAIT, reqs, 1, &d->event);

			d->IncRef();
			if (r)
				d->DecRef();
		#endif
	}
}

LHostnameAsync::~LHostnameAsync()
{
	d->Cancel();
	d->DecRef();
}

LString LHostName()
{
	char hostname[256];
	if (gethostname(hostname, sizeof(hostname)) != SOCKET_ERROR)
		return hostname;
	return LString();
}

const char *SocketFamilyToString(int fam)
{
	switch (fam)
	{
		case AF_UNSPEC: return "AF_UNSPEC";
		case AF_INET: return "AF_INET";
		case AF_APPLETALK: return "AF_APPLETALK";
		#ifdef AF_ROUTE
		case AF_ROUTE: return "AF_ROUTE";
		#endif
		#ifdef AF_LINK
		case AF_LINK: return "AF_LINK";
		#endif
		case AF_INET6: return "AF_INET6";
		#ifdef AF_DLI
		case AF_DLI: return "AF_DLI";
		#endif
		case AF_IPX: return "AF_IPX";
		#ifdef AF_NOTIFY
		case AF_NOTIFY: return "AF_NOTIFY";
		#endif
		case AF_UNIX: return "AF_UNIX";
		#ifdef AF_BLUETOOTH
		case AF_BLUETOOTH: return "AF_BLUETOOTH";
		#endif
	}
	
	static char num[16];
	sprintf_s(num, sizeof(num), "%i", fam);
	return num;
}

LArray<uint32_t> LWhatsMyIp()
{
	LArray<uint32_t> addr = 0;

	StartNetworkStack();

	#if defined WIN32
		
		char hostname[256];
		HostEnt *e = NULL;
		if (gethostname(hostname, sizeof(hostname)) != SOCKET_ERROR)
		{
			if (auto e = gethostbyname(hostname))
			{
				for (int i=0; e->h_addr_list[i]; i++)
					addr.Add(htonl(*(uint32_t*)e->h_addr_list[i]));
			}
		}
	
	#else // unixy interfaces?

		struct ifaddrs *ifap = nullptr;
		int r = getifaddrs(&ifap);
		if (!r && ifap)
		{
			auto local_host = 0x7f000001;
			for (auto p = ifap; p; p = p->ifa_next)
			{
				if (!p->ifa_addr)
					continue;
				if (p->ifa_addr->sa_family == AF_INET)
				{
					auto in = (sockaddr_in*)p->ifa_addr;
					auto ip = htonl(in->sin_addr.s_addr);
					if (ip)
					{
						if (ip == local_host)
							addr.AddAt(0, ip);
						else
							addr.Add(ip);
						// printf("name=%s ip.addr=%x, %s\n", p->ifa_name, ip, LIpToStr(ip).Get());
					}
				}
			}
		}
		if (ifap)
	   		freeifaddrs(ifap);

	#endif

	return addr;
}

//////////////////////////////////////////////////////////////////////
#define SOCKS5_VER					5

#define SOCKS5_CMD_CONNECT			1
#define SOCKS5_CMD_BIND				2
#define SOCKS5_CMD_ASSOCIATE		3

#define SOCKS5_ADDR_IPV4			1
#define SOCKS5_ADDR_DOMAIN			3
#define SOCKS5_ADDR_IPV6			4

#define SOCKS5_AUTH_NONE			0
#define SOCKS5_AUTH_GSSAPI			1
#define SOCKS5_AUTH_USER_PASS		2

LSocks5Socket::LSocks5Socket()
{
	Socks5Connected = false;
}

void LSocks5Socket::SetProxy(const LSocks5Socket *s)
{
	Proxy.Reset(s ? NewStr(s->Proxy) : 0);
	Port = s ? s->Port : 0;
	UserName.Reset(s ? NewStr(s->UserName) : 0);
	Password.Reset(s ? NewStr(s->Password) : 0);
}

void LSocks5Socket::SetProxy(char *proxy, int port, char *username, char *password)
{
	Proxy.Reset(NewStr(proxy));
	Port = port;
	UserName.Reset(NewStr(username));
	Password.Reset(NewStr(password));
}

int LSocks5Socket::Open(const char *HostAddr, int port)
{
	bool Status = false;
	if (HostAddr)
	{
		char Msg[256];
		sprintf_s(Msg, sizeof(Msg), "[SOCKS5] Connecting to proxy server '%s'", HostAddr);
		OnInformation(Msg);

		Status = LSocket::Open(Proxy, Port) != 0;
		if (Status)
		{
			char Buf[1024];
			Buf[0] = SOCKS5_VER;
			Buf[1] = 2; // methods
			Buf[2] = SOCKS5_AUTH_NONE;
			Buf[3] = SOCKS5_AUTH_USER_PASS;
			
			// No idea how to implement this.
			// AuthReq[3] = SOCKS5_AUTH_GSSAPI;

			OnInformation("[SOCKS5] Connected, Requesting authentication type.");
			LSocket::Write(Buf, 4, 0);
			if (LSocket::Read(Buf, 2, 0) == 2)
			{
				if (Buf[0] == SOCKS5_VER)
				{
					bool Authenticated = false;
					switch (Buf[1])
					{
						case SOCKS5_AUTH_NONE:
						{
							Authenticated = true;
							OnInformation("[SOCKS5] No authentication needed.");
							break;
						}
						case SOCKS5_AUTH_USER_PASS:
						{
							OnInformation("[SOCKS5] User/Pass authentication needed.");
							if (UserName && Password)
							{
								char *b = Buf;
								*b++ = 1; // ver of sub-negotiation ??
								
								size_t NameLen = strlen(UserName);
								LAssert(NameLen < 0x80);
								*b++ = (char)NameLen;
								b += sprintf_s(b, NameLen+1, "%s", UserName.Get());

								size_t PassLen = strlen(Password);
								LAssert(PassLen < 0x80);
								*b++ = (char)PassLen;
								b += sprintf_s(b, PassLen+1, "%s", Password.Get());

								LSocket::Write(Buf, (int)(3 + NameLen + PassLen));
								if (LSocket::Read(Buf, 2, 0) == 2)
								{
									Authenticated = (Buf[0] == 1 && Buf[1] == 0);
								}

								if (!Authenticated)
								{
									OnInformation("[SOCKS5] User/Pass authentication failed.");
								}
							}
							break;
						}
					}

					if (Authenticated)
					{
						OnInformation("[SOCKS5] Authentication successful.");

						int HostPort = htons(port);
						
						// Header
						char *b = Buf;
						*b++ = SOCKS5_VER;
						*b++ = SOCKS5_CMD_CONNECT;
						*b++ = 0; // reserved

						long IpAddr = inet_addr(HostAddr);
						if (IpAddr != -1)
						{
							// Ip
							*b++ = SOCKS5_ADDR_IPV4;
							memcpy(b, &IpAddr, 4);
							b += 4;
						}
						else
						{
							// Domain Name
							*b++ = SOCKS5_ADDR_DOMAIN;
							size_t Len = strlen(HostAddr);
							LAssert(Len < 0x80);
							*b++ = (char)Len;
							strcpy_s(b, Buf+sizeof(Buf)-b, HostAddr);
							b += Len;
						}

						// Port
						memcpy(b, &HostPort, 2);
						b += 2;

						LSocket::Write(Buf, (int)(b - Buf), 0);
						if (LSocket::Read(Buf, 10, 0) == 10)
						{
							if (Buf[0] == SOCKS5_VER)
							{
								switch (Buf[1])
								{
									case 0:
										Socks5Connected = true;
										OnInformation("[SOCKS5] Connected!");
										break;
									case 1:
										OnInformation("[SOCKS5] General SOCKS server failure");
										break;
									case 2:
										OnInformation("[SOCKS5] Connection not allowed by ruleset");
										break;
									case 3:
										OnInformation("[SOCKS5] Network unreachable");
										break;
									case 4:
										OnInformation("[SOCKS5] Host unreachable");
										break;
									case 5:
										OnInformation("[SOCKS5] Connection refused");
										break;
									case 6:
										OnInformation("[SOCKS5] TTL expired");
										break;
									case 7:
										OnInformation("[SOCKS5] Command not supported");
										break;
									case 8:
										OnInformation("[SOCKS5] Address type not supported");
										break;
									default:
										OnInformation("[SOCKS5] Unknown SOCKS server failure");
										break;
								}
							}
							else
							{
								OnInformation("[SOCKS5] Wrong socks version.");
							}
						}
						else
						{
							OnInformation("[SOCKS5] Connection request read failed.");
						}
					}
					else
					{
						OnInformation("[SOCKS5] Not authenticated.");
					}
				}
				else
				{
					OnInformation("[SOCKS5] Wrong socks version.");
				}
			}
			else
			{
				OnInformation("[SOCKS5] Authentication type read failed.");
			}

			Status = Socks5Connected;
			if (!Status)
			{
				LSocket::Close();
				OnInformation("[SOCKS5] Failure: Disconnecting.");
			}
		}
	}

	return Status;
}

////////////////////////////////////////////////////////////////////////////
LSelect::LSelect(LSocket *sock)
{
	if (sock)
		*this += sock;
}
	
LSelect &LSelect::operator +=(LSocketI *sock)
{
	if (sock)
		s.Add(sock);
	return *this;
}
	
int LSelect::Select(LArray<LSocketI*> &Results, bool Rd, bool Wr, int TimeoutMs)
{
	if (s.Length() == 0)
		return 0;

	#ifdef LINUX

	// Because Linux doesn't return from select() when the socket is
	// closed elsewhere we have to do something different... damn Linux,
	// why can't you just like do the right thing?
		
	LArray<struct pollfd> fds;
	fds.Length(s.Length());
	for (unsigned i=0; i<s.Length(); i++)
	{
		auto in = s[i];
		auto &out = fds[i];
		out.fd = in->Handle();
		if (out.fd < 0)
		{
			LgiTrace("%s:%i - Warning: invalid socket!\n", _FL);
			out.fd = in->Handle();
		}
		out.events =	(Wr ? POLLOUT : 0) |
						(Rd ? POLLIN : 0) |
						POLLRDHUP |
						POLLERR;
		out.revents = 0;
	}

	int r = poll(fds.AddressOf(), fds.Length(), TimeoutMs);
	int Signalled = 0;
	if (r > 0)
	{
		for (unsigned i=0; i<fds.Length(); i++)
		{
			auto &f = fds[i];
			if (f.revents != 0)
			{
				Signalled++;
								
				if (f.fd == s[i]->Handle())
				{
					// printf("Poll[%i] = %x (flags=%x)\n", i, f.revents, Flags);
					Results.Add(s[i]);
				}
				else LAssert(0);
			}
		}
	}
	
	return Signalled;
		
	#else
		
	struct timeval t = {TimeoutMs / 1000, (TimeoutMs % 1000) * 1000};

	fd_set r;
	FD_ZERO(&r);
	OsSocket Max = 0;
	for (auto Sock : s)
	{
		auto h = Sock->Handle();
		if (Max < h)
			Max = h;
		FD_SET(h, &r);
	}
		
	int v = select(	(int)Max+1,
					Rd ? &r : NULL,
					Wr ? &r : NULL,
					NULL, TimeoutMs >= 0 ? &t : NULL);
	if (v > 0)
	{
		for (auto Sock : s)
		{
			if (FD_ISSET(Sock->Handle(), &r))
				Results.Add(Sock);
		}
	}

	return v;

	#endif
}

LArray<LSocketI*> LSelect::Readable(int TimeoutMs)
{
	LArray<LSocketI*> r;
	Select(r, true, false, TimeoutMs);
	return r;
}

LArray<LSocketI*> LSelect::Writeable(int TimeoutMs)
{
	LArray<LSocketI*> r;
	Select(r, false, true, TimeoutMs);
	return r;
}

