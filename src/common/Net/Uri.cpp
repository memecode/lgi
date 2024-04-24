#include "lgi/common/Lgi.h"
#include "lgi/common/Net.h"
#include "lgi/common/RegKey.h"
#include "lgi/common/Uri.h"

/////////////////////////////////////////////////////////////////////////////////
static const char *Ws = " \t\r\n";
#define SkipWs(s) while (*s && strchr(Ws, *s)) s++;

LUri::LUri(const char *uri)
{
	if (uri)
		Set(uri);
}

LUri::LUri
(
	const char *proto,
	const char *user,
	const char *pass,
	const char *host,
	int port,
	const char *path,
	const char *anchor
)
{
	sProtocol = proto;
	sUser = user;
	sPass = pass;
	sHost = host;
	Port = port;
	sPath = path;
	sAnchor = anchor;
}

LUri::~LUri()
{
	Empty();
}

LUri &LUri::operator +=(const char *s)
{
	// Add segment to path
	if (!s)
		return *this;

	if (*s == '/')
		sPath.Empty(); // reset

	ssize_t lastSep;
	for (lastSep=sPath.Length()-1; lastSep>=0; lastSep--)
	{
		auto c = sPath(lastSep);
		if (c == '/' || c == '\\')
			break;
	}
	auto path = lastSep >= 0 ? sPath(0, lastSep) : sPath;
	auto parts = path.SplitDelimit("/\\");
	parts.SetFixedLength(false);

	for (auto p: LString(s).SplitDelimit("/\\"))
	{
		if (p.Equals(".."))
			parts.PopLast();
		else if (p.Equals("."))
			;
		else
			parts.Add(p);
	}

	sPath = LString("/").Join(parts);
	return *this;
}

LUri &LUri::operator =(const LUri &u)
{
	Empty();
	sProtocol = u.sProtocol;
	sUser = u.sUser;
	sPass = u.sPass;
	sHost = u.sHost;
	sPath = u.sPath;
	sAnchor = u.sAnchor;
	Port = u.Port;
	return *this;
}

void LUri::Empty()
{
	Port = 0;
	sProtocol.Empty();
	sUser.Empty();
	sPass.Empty();
	sHost.Empty();
	sPath.Empty();
	sAnchor.Empty();
}

LUri::operator bool()
{
	return IsFile() ? !sPath.IsEmpty() : !sHost.IsEmpty();
}

LString LUri::LocalPath()
{
	if (!IsFile())
		return LString();

	#ifdef WINDOWS
	if (sPath.Length() > 0 &&
		sPath(0) == '/')
		return sPath(1, -1).Replace("/", DIR_STR);
	#endif

	return sPath.Replace("/", DIR_STR);
}

LString LUri::ToString()
{
	LStringPipe p;
	if (sProtocol)
		p.Print("%s://", sProtocol.Get());

	if (sUser || sPass)
	{
		auto UserEnc = EncodeStr(sUser, "@:");
		auto PassEnc = EncodeStr(sPass, "@:");
		p.Print("%s:%s@", UserEnc?UserEnc.Get():"", PassEnc?PassEnc.Get():"");
	}
	if (sHost)
		p.Write(sHost);
	if (Port)
		p.Print(":%i", Port);
	if (sPath)
	{
		auto e = EncodeStr(sPath);
		char *s = e ? e : sPath;
		p.Print("%s%s", *s == '/' ? "" : "/", s);
	}
	if (sAnchor)
		p.Print("#%s", sAnchor.Get());
		
	return p.NewLStr();
}

bool LUri::Set(const char *uri)
{
	if (!uri)
		return false;

	Empty();

	const char *s = uri;
	SkipWs(s);

	// Scan ahead and check for protocol...
	const char *hasProto = NULL;
	const char *hasAuth = NULL;
	const char *hasAt = NULL;
	const char *hasPath = NULL;
	const char *hasColon = NULL;

	for (auto c = s; *c; c++)
	{
		if (c[0] == ':')
		{
			if (!hasProto && !hasAt)
				hasProto = c;
			else
				hasColon = c;
		}
		else if (c[0] == '/' &&
				 c[1] == '/')
		{
			hasAuth = c++;
		}
		else if (c[0] == '@' && !hasAt)
		{
			hasAt = c; // keep the first '@'
		}
		else if ((c[0] == '/' || c[0] == '\\') && !hasPath)
		{
			hasPath = c;
			break; // anything after this is path...
		}
	}

	if (hasProto)
	{
		sProtocol.Set(s, hasProto - s);
		if (hasAuth)
			s = hasAuth + 2;
		else
			s = hasProto + 1;
	}
	
	if (hasAt)
	{
		if (hasAt >= s)
		{
			auto p = LString(s, hasAt - s).SplitDelimit(":", 1);
			if (p.Length() == 2)
			{
				sUser = DecodeStr(p[0]);
				sPass = DecodeStr(p[1]);
			}
			else if (p.Length() == 1)
			{
				sUser = DecodeStr(p[0]);
			}

			s = hasAt + 1;
		}
		else
		{
			LAssert(!"hasAt should be > s");
			return false;
		}
	}

	bool hasHost = hasProto || hasAt || hasColon || !hasPath;
	if (hasHost)
	{
		auto p = LString(s, hasPath ? hasPath - s : -1).SplitDelimit(":", 1);
		if (p.Length() == 2)
		{
			sHost = p[0];
			Port = (int)p[1].Int();
		}
		else if (p.Length() == 1)
		{
			sHost = p[0];
		}
	}
	else
	{
		hasPath = s;
	}

	if (hasPath)
	{
		sPath = LString(hasPath).Replace("\\", "/");
		if (IsFile() &&
			LDirExists(LocalPath()))
		{
			// Folders should have a trailing slash
			if (sPath(-1) != '/')
				sPath += "/";
		}
	}

	if (sPath)
	{
		auto anchor = sPath.Find("#");
		if (anchor >= 0)
		{
			sAnchor = sPath(anchor, -1);
			sPath.Length(anchor);
		}
	}
		
	return sHost || sPath;
}

LString LUri::EncodeStr(const char *s, const char *ExtraCharsToEncode)
{
	LStringPipe p(256);
	if (s)
	{
		while (*s)
		{
			if (*s == ' ' || (ExtraCharsToEncode && strchr(ExtraCharsToEncode, *s)))
			{
				char h[4];
				sprintf_s(h, sizeof(h), "%%%2.2X", (uint32_t)(uchar)*s++);
				p.Write(h, 3);
			}
			else
			{
				p.Write(s++, 1);
			}
		}
	}
	
	return p.NewLStr();
}

LUri::StrMap LUri::Params()
{
	StrMap m;

	if (sPath)
	{
		const char *q = strchr(sPath, '?');
		if (q++)
		{
			auto Parts = LString(q).SplitDelimit("&");
			for (auto p : Parts)
			{
				auto Var = p.Split("=", 1);
				if (Var.Length() == 2)
					m.Add(Var[0], Var[1]);
			}
		}
	}

	return m;
}

LString LUri::DecodeStr(const char *s)
{
	LStringPipe p(256);
	if (s)
	{
		while (*s)
		{
			if (s[0] == '%' && s[1] && s[2])
			{
				char h[3] = { s[1], s[2], 0 };
				char c = htoi(h);
				p.Write(&c, 1);
				s += 3;
			}
			else
			{
				p.Write(s++, 1);
			}
		}
	}

	return p.NewLStr();
}

struct UriUnitCase
{
	const char *str;
	LUri uri;
};

bool LUri::UnitTests()
{
	UriUnitCase Parse[] = {
		{"http://user:pass@host:1234/somePath/seg/file.png", LUri("http", "user", "pass", "host", 1234, "somePath/seg/file.png")},
		{"user:pass@host:1234/somePath/seg/file.png",        LUri(NULL, "user", "pass", "host", 1234, "somePath/seg/file.png")  },
		{"user@host:1234/somePath/seg/file.png",             LUri(NULL, "user", NULL, "host", 1234, "somePath/seg/file.png")    },
		{"user@host/somePath/seg/file.png",                  LUri(NULL, "user", NULL, "host", 0, "somePath/seg/file.png")       },
		{"user@host",                                        LUri(NULL, "user", NULL, "host", 0, NULL)                          },
		{"host",                                             LUri(NULL, NULL, NULL, "host", 0, NULL)                            },
		{"host:1234",                                        LUri(NULL, NULL, NULL, "host", 1234, NULL)                         },
		{"somePath/seg/file.png",                            LUri(NULL, NULL, NULL, NULL, 0, "somePath/seg/file.png")           },
		{"mailto:user@host.com",                             LUri("mailto", "user", NULL, "host.com", 0, NULL)                  },
	};

	for (auto &test: Parse)
	{
		LUri u(test.str);
		if (u != test.uri)
		{
			LAssert(!"test failed");
			return false;
		}
	}

	return true;
}

///////////////////////////////////////////////////////////////////////////////////////////////////////
#if defined LGI_CARBON
int CFNumberRefToInt(CFNumberRef r, int Default = 0)
{
	int i = Default;
	if (r && 
		CFGetTypeID(r) == CFNumberGetTypeID())
	{
		CFNumberGetValue(r, kCFNumberIntType, &r);
	}
	return i;
}
#endif

LProxyUri::LProxyUri()
{
	#if defined(WIN32)

		LRegKey k(false, "HKEY_CURRENT_USER\\Software\\Microsoft\\Windows\\CurrentVersion\\Internet Settings");
		if (k.IsOk())
		{
		    uint32_t Enabled = 0;
		    if (k.GetInt("ProxyEnable", Enabled) && Enabled)
		    {
			    char *p = k.GetStr("ProxyServer");
			    if (p)
			    {
				    Set(p);
			    }
			}
		}

	#elif defined LINUX
	
		char *HttpProxy = getenv("http_proxy");
		if (HttpProxy)
		{
			Set(HttpProxy);
		}
	
	#elif defined MAC
	
		//	CFDictionaryRef Proxies = SCDynamicStoreCopyProxies(0);
		//	if (!Proxies)
		//		LgiTrace("%s:%i - SCDynamicStoreCopyProxies failed.\n", _FL);
		//	else
		//	{
		//		int enable = CFNumberRefToInt((CFNumberRef) CFDictionaryGetValue(Proxies, kSCPropNetProxiesHTTPEnable));
		//		if (enable)
		//		{
		//			#ifdef LGI_COCOA
		//			LAssert(!"Fixme");
		//			#else
		//			Host = CFStringToUtf8((CFStringRef) CFDictionaryGetValue(Proxies, kSCPropNetProxiesHTTPProxy));
		//			#endif
		//			Port = CFNumberRefToInt((CFNumberRef) CFDictionaryGetValue(Proxies, kSCPropNetProxiesHTTPPort));
		//		}
		//		
		//		CFRelease(Proxies);
		//	}

	#elif defined(HAIKU)

		// There doesn't seem to be a system wide proxy setting, so for the time being
		// lets just put a setting in the Lgi config and use that:
		if (!LAppInst)
		{
			LgiTrace("%s:%i - No LApp instance yet?\n", _FL);
		}
		else
		{
			auto p = LAppInst->GetConfig(LApp::CfgNetworkHttpProxy);
			if (p)
			{
				Set(p);
			}
			else
			{
				static bool First = true;
				if (First)
				{
					First = false;
					LgiTrace("%s:%i No HTTP Proxy configured in '%s'.\n", _FL, LAppInst->GetConfigPath().Get());
				}
			}
		}

	#else

		#warning "Impl getting OS proxy here."

	#endif
}
