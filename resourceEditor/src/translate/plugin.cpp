#include "../LgiResEdit.h"

#include "google/cloud/translate/v3/translation_client.h"
#include "google/cloud/project.h"
#include <cstdlib>
#include <iostream>
#include <string>
#include <vector>

class LGoogleTranslationPlugin : public LTranslationPlugin
{
    LDom *params;

public:
    LGoogleTranslationPlugin(LDom *params)
    {
        this->params = params;
    }

	~LGoogleTranslationPlugin()
    {
    }
	
    bool Translate(LString english, LString newLang, TStrCallback callback) override
    {
        auto Fail = [&callback](const char *Message)
        {
            if (callback)
                callback(false, Message);
            return false;
        };

        LVariant v;
        const char *ProjectId = std::getenv("GOOGLE_CLOUD_PROJECT");
        if (!ProjectId || !*ProjectId)
        {
            if (params)
            {
                params->GetValue(DomGoogleCloudProject, v);
                ProjectId = v.Str();
            }

            if (!ProjectId || !*ProjectId)
                return Fail("GOOGLE_CLOUD_PROJECT is not set");
        }

        namespace translate_v3 = ::google::cloud::translate_v3;
        auto Client = translate_v3::TranslationServiceClient(
            translate_v3::MakeTranslationServiceConnection());

        google::cloud::translation::v3::TranslateTextRequest Request;
        Request.set_parent(std::string("projects/") + ProjectId + "/locations/global");
        Request.set_target_language_code(newLang.Get());
        Request.set_mime_type("text/plain");
        Request.add_contents(english.Get());

        auto Response = Client.TranslateText(Request);
        if (!Response || Response->translations().empty())
        {
            if (!Response)
                return Fail(Response.status().message().c_str());
            return Fail("Google Translate returned no translations");
        }

        if (callback)
            callback(true, Response->translations()[0].translated_text().c_str());

        return true;
    }
};

extern "C"
LTranslationPlugin *CreateTranslator(LDom *params)
{
    return new LGoogleTranslationPlugin(params);
}