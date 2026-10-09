#include "lgi/common/Lgi.h"
#include "lgi/common/Library.h"
#include "lgi/common/App.h"
#include "lgi/common/Dom.h"

#include "../LgiResEdit.h"

// Linux:
//		https://github.com/googleapis/google-cloud-cpp
//		git clone https://github.com/microsoft/vcpkg.git
//		cd vcpkg
//		./bootstrap-vcpkg.sh -disableMetrics
//		./vcpkg install google-cloud-cpp[translate]

// Mac:
// brew install google-cloud-cpp

// Windows:
// vcpkg install google-cloud-cpp[translate]

class GoogleTranslate :
    public LAutoTranslate,
    public LDom
{
	LAutoPtr<LLibrary> lib;
	LAutoPtr<LTranslationPlugin> plugin;

public:
	GoogleTranslate()
	{
		name = "Google Translate";
		
		LFile::Path p(LSP_APP_INSTALL);
		auto pluginPath = p / "src" / "translate" / "build" / "libGoogleTranslatePlugin.so";
		auto path = pluginPath.GetFull();
		if (pluginPath.Exists())
		{
			if (lib.Reset(new LLibrary(path)))
			{
				if (lib->IsLoaded())
					printf("%s:%i - loaded plugin '%s'\n", _FL, path.Get());
				else
					printf("%s:%i - err loading '%s'\n", _FL, path.Get());
			}
		}
		else printf("%s:%i - '%s' not found\n", _FL, path.Get());
		
		if (lib)
		{
			auto pCreate = (pCreateTranslator) lib->GetAddress("CreateTranslator");
			if (pCreate)
			{
				plugin.Reset(pCreate(this));
				if (plugin)
                {
					printf("%s:%i - created plugin instance\n", _FL);
                }
                else
				{
					printf("%s:%i - failed to create plugin instance\n", _FL);
				}
			}
			else
			{
				printf("%s:%i - entry point not found\n", _FL);
			}
		}
		else
		{
			engines.Delete((LAutoTranslate*)this);
			LAssert(engines.Length() == 0);
		}
	}
	
	const char *GetClass() { return "GoogleTranslate"; }

	bool Translate(LString english, LString newLang, std::function<void(bool, LString)> callback) override
	{
		if (plugin)
			return plugin->Translate(english, newLang, callback);
		return false;
	}

	bool GetVariant(const char *Name, LVariant &Value, const char *Array = NULL) override
    {
        if (!Stricmp(Name, DomGoogleCloudProject))
        {
            if (auto app = dynamic_cast<AppWnd*>(LAppInst->AppWnd))
            {
                auto res = app->GetOptions()->GetValue(DomGoogleCloudProject, Value);
                printf("%s:%i - got cloud '%s' = %i\n", _FL, Value.Str(), res);
                return res;
            }
            else printf("%s:%i - failed to find app wnd.\n", _FL);
        }
        return false;
    }
};

GoogleTranslate GoogleTranslateEngine;
