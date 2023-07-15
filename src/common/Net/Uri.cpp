#include "lgi/common/Lgi.h"
#include "lgi/common/Net.h"
#include "lgi/common/RegKey.h"
#include "lgi/common/Uri.h"

/////////////////////////////////////////////////////////////////////////////////
static const char *Ws = " \t\r\n";
#define SkipWs(s) while (*s && strchr(Ws, *s)) s++;

LUri::LUri(const char *uri)
{
	Port = 0;
	if (uri)
		Set(uri);
}

LUri::~LUri()
{
	Empty();
}

LUri &LUri::operator +=(const char *s)
{
	// Add segment to path
	if (!sPath.Length() || sPath(-1) != '/')
		sPath += IsFile() ? DIR_STR : "/";

	auto len = sPath.Length();
	sPath += s;

	if (IsFile())
	{
		char *c = sPath.Get();
		#if DIR_CHAR == '/'
		char from = '\\';
		#else
		char from = '/';
		#endif
		for (size_t i=len; c && i<sPath.Length(); i++)
		{
			if (c[i] == from)
				c[i] = DIR_CHAR;
		}
	}

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

const char *LUri::LocalPath()
{
	if (!IsFile())
		return NULL;
	auto s = sPath.Get();
	if (!s)
		return NULL;
	#ifdef WINDOWS
	if (*s == '/')
		s++;
	#endif
	return s;
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
	const char *p = s;
	while (*s && IsAlpha(*s)) s++;
	if (s[0] == ':' &&
		(s - p) > 1 &&
		s[1] == '/' &&
		s[2] == '/')
	{
		sProtocol.Set(p, s - p);
		s += 3;
	}
	else
	{
		// No protocol, so assume it's a host name or path
		s = p;
	}

	// Check for path
	bool HasPath = false;
	if
	(
		(s[0] && s[1] == ':')
		||
		(s[0] == '/')
		||
		(s[0] == '\\')
	)
	{
		HasPath = true;
	}
	else
	{
		// Scan over the host name
		p = s;
		while (	*s &&
				*s > ' ' &&
				*s < 127 &&
				*s != '/' &&
				*s != '\\')
		{
			s++;
		}

		sHost.Set(p, s - p);
		if (sHost)
		{
			char *At = strchr(sHost, '@');
			if (At)
			{
				*At++ = 0;
				char *Col = strchr(sHost, ':');
				if (Col)
				{
					*Col++ = 0;
					sPass = DecodeStr(Col);
				}

				sUser = DecodeStr(sHost);
				sHost = At;
			}

			char *Col = strchr(sHost, ':');
			if (Col)
			{
				Port = atoi(Col+1);
				sHost.Length(Col-sHost.Get());
			}
		}

		HasPath = *s == '/';
	}

	if (HasPath)
	{
		const char *Start = s;
		while (*s && *s != '#')
			s++;

		if (*s)
		{
			sPath.Set(Start, s - Start);
			sAnchor = s + 1;
		}
		else
		{
			sPath = Start;
		}
		
		#if 0
		// This decodes the path from %## encoding to raw characters.
		// However sometimes we need the encoded form. So instead of
		// doing the conversion here the caller has to do it now.
		char *i = Path, *o = Path;
		while (*i)
		{
			if (*i == '%' && i[1] && i[2])
			{
				char h[3] = {i[1], i[2], 0};
				*o++ = htoi(h);
				i+=2;
			}
			else
			{
				*o++ = *i;
			}
			i++;
		}
		*o = 0;
		#endif
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


