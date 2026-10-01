#include "../LgiResEdit.h"

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

    bool Translate(LString english, LString newLang, std::function<void(LString)> callback) override
    {
        const char *ProjectId = std::getenv("GOOGLE_CLOUD_PROJECT");
        if (!ProjectId || !*ProjectId)
            return false;

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
            return false;

        if (callback)
            callback(Response->translations()[0].translated_text().c_str());

        return true;
    }
};

GoogleTranslate GoogleTranslateEngine;

#endif