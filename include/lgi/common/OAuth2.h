#ifndef _LOAUTH2_H_
#define _LOAUTH2_H_

//////////////////////////////////////////////////////////////////
#include "lgi/common/Net.h"

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
		
		LString ClientID;
		LString ClientSecret;
		LString RedirURIs;
		LString AuthUri;
		LString ApiUri;
		// LString RevokeUri;
		LString Scope;
		LUri Proxy;
		
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

	LOAuth2(Params &params, const char *account, GDom *store, LCancel *cancel, LStream *log = NULL);
	virtual ~LOAuth2();

	LString GetAccessToken();
	bool Refresh();
};

#endif
