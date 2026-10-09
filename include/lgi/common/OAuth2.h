#ifndef _LOAUTH2_H_
#define _LOAUTH2_H_

//////////////////////////////////////////////////////////////////
#include "lgi/common/Net.h"
#include "lgi/common/Uri.h"

/* Do this somewhere?
LAutoString ErrorMsg;
StartSSL(ErrorMsg, NULL);
*/

class LOAuth2
{
	struct LOAuth2Priv *d;

public:
	struct Params
	{
		enum ServiceProvider
		{
			None,
			OAuthGoogle,
			OAuthMicrosoft,
		}	Provider;
		
		LString ClientID, ClientSecret;
		LString RedirURIs;
		LString AuthUri;
		LString ApiUri;
		LString Scope;
		LUri Proxy;
		
		LString SslKey, SslCert;
		bool ScanForKeyAndCert(const char *folder);
		
		// Requirements to function:
		constexpr static const char *CapMkcert = "mkcert";
		constexpr static const char *CapHttpsCert = "https-cert";
		LString::Array RequiredCapabilities;
		bool CheckRequirement(const char *req);

		Params()
		{
			Provider = None;
		}

		bool IsValid()
		{
			return	Provider != None &&
					ClientID &&
					ClientSecret &&
					RedirURIs &&
					AuthUri &&
					Scope &&
					ApiUri;
		}
	};

	LOAuth2(Params &params, const char *account, LDom *store, LCancel *cancel, LStream *log = NULL);
	virtual ~LOAuth2();

	LString GetAccessToken();
	bool Refresh();
	bool Restart();
	void OnLogin(); // successful login
};

#endif
