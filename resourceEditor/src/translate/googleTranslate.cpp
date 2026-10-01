#include "../LgiResEdit.h"

// Linux:
//      https://github.com/googleapis/google-cloud-cpp
//      git clone https://github.com/microsoft/vcpkg.git
//      cd vcpkg
//      ./bootstrap-vcpkg.sh -disableMetrics
//      ./vcpkg install google-cloud-cpp[translate]

// Mac:
// brew install google-cloud-cpp

// Windows:
// vcpkg install google-cloud-cpp[translate]

#if __has_include("google/cloud/translate/v3/translation_client.h")

#include "google/cloud/translate/v3/translation_client.h"
#include "google/cloud/project.h"
#include <cstdlib>
#include <iostream>
#include <string>
#include <vector>

class GoogleTranslate : public LAutoTranslate
{
public:
    GoogleTranslate()
    {
        name = "Google Translate";
        LAutoTranslate::engines.Add(this);
    }

    bool Translate(LString english, LString newLang, std::function<void(bool, LString)> callback) override
    {
        auto Fail = [&callback](const char *Message)
        {
            if (callback)
                callback(false, Message);
            return false;
        };

        const char *ProjectId = std::getenv("GOOGLE_CLOUD_PROJECT");
        if (!ProjectId || !*ProjectId)
            return Fail("GOOGLE_CLOUD_PROJECT is not set");

        namespace translate = ::google::cloud::translate_v3;
        auto Client = translate::TranslationServiceClient(
            translate::MakeTranslationServiceConnection());

        google::cloud::translate::v3::TranslateTextRequest Request;
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

GoogleTranslate GoogleTranslateEngine;

#endif