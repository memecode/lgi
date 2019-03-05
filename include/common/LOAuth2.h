#ifndef _LOAUTH2_H_
#define _LOAUTH2_H_

//////////////////////////////////////////////////////////////////

/* Do this somewhere?
GAutoString ErrorMsg;
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
		
		GString ClientID;
		GString ClientSecret;
		GString RedirURIs;
		GString AuthUri;
		GString ApiUri;
		// GString RevokeUri;
		GString Scope;
		GUri Proxy;
		
		// GString AccessToken;
		// GString RefreshToken;
		// int ExpiresIn;
		
		Params()
		{
			Provider = None;
			// ExpiresIn = 0;
		}

		bool IsValid()
		{
			return	Provider != None &&
					ClientID &&
					ClientSecret &&
					RedirURIs &&
					AuthUri &&
					// RevokeUri &&
					Scope &&
					ApiUri;
		}
	};


	LOAuth2(Params &params, const char *account, GDom *store, GStream *log = NULL);
	virtual ~LOAuth2();

	GString GetAccessToken();
};

#endif