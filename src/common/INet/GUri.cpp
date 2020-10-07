#include "Lgi.h"
#include "INet.h"
#include "GRegKey.h"


/////////////////////////////////////////////////////////////////////////////////
static const char *Ws = " \t\r\n";
#define SkipWs(s) while (*s && strchr(Ws, *s)) s++;

GUri::GUri(const char *uri)
{
	Port = 0;
	if (uri)
		Set(uri);
}

GUri::~GUri()
{
	Empty();
}

GUri &GUri::operator +=(const char *s)
{
	// Add segment to path
	if (!sPath.Length() || sPath(-1) != '/')
		sPath += IsFile() ? DIR_STR : "/";

	auto len = sPath.Length();
	sPath += s;

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

	return *this;
}

GUri &GUri::operator =(const GUri &u)
{
	Empty();
	sProtocol = u.sProtocol;
	sUser = u.sUser;
	sPass = u.sPass;
	sHost = u.sHost;
	sPath = u.sPath;
	sAnchor = u.sAnchor;
	return *this;
}

void GUri::Empty()
{
	Port = 0;
	sProtocol.Empty();
	sUser.Empty();
	sPass.Empty();
	sHost.Empty();
	sPath.Empty();
	sAnchor.Empty();
}

GUri::operator bool()
{
	return IsFile() ? !sPath.IsEmpty() : !sHost.IsEmpty();
}

const char *GUri::LocalPath()
{
	if (!IsFile())
		return NULL;
	auto s = sPath.Get();
	if (!s)
		return NULL;
	if (*s == '/')
		s++;
	return s;
}

GString GUri::ToString()
{
	GStringPipe p;
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
		
	return p.NewGStr();
}

bool GUri::Set(const char *uri)
{
	if (!uri)
		return false;

	Empty();

	const char *s = uri;
	SkipWs(s);

	// Scan ahead and check for protocol...
	const char *p = s;
	while (*s && IsAlpha(*s)) s++;
	if (s[0] == ':' && (s - p) > 1)
	{
		sProtocol.Set(p, s - p);
		s++;
		if (*s == '/') s++;
		if (*s == '/') s++;
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

GString GUri::EncodeStr(const char *s, const char *ExtraCharsToEncode)
{
	GStringPipe p(256);
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
	
	return p.NewGStr();
}

GUri::StrMap GUri::Params()
{
	StrMap m;

	if (sPath)
	{
		const char *q = strchr(sPath, '?');
		if (q++)
		{
			auto Parts = GString(q).SplitDelimit("&");
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

GString GUri::DecodeStr(const char *s)
{
	GStringPipe p(256);
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

	return p.NewGStr();
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

GProxyUri::GProxyUri()
{
	#if defined(WIN32)

	GRegKey k(false, "HKEY_CURRENT_USER\\Software\\Microsoft\\Windows\\CurrentVersion\\Internet Settings");
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
//			LgiAssert(!"Fixme");
//			#else
//			Host = CFStringToUtf8((CFStringRef) CFDictionaryGetValue(Proxies, kSCPropNetProxiesHTTPProxy));
//			#endif
//			Port = CFNumberRefToInt((CFNumberRef) CFDictionaryGetValue(Proxies, kSCPropNetProxiesHTTPPort));
//		}
//		
//		CFRelease(Proxies);
//	}

	#else

	#warning "Impl getting OS proxy here."

	#endif
}


