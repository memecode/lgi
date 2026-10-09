#pragma once

#include "lgi/common/XmlTreeUi.h"

class OptionsDlg
    : public LDialog
    , public LXmlTreeUi
{
    LOptionsFile *options;

public:
	OptionsDlg(LView *Parent, LOptionsFile *Options)
        : options(Options)
    {
        LoadFromResource(IDD_OPTIONS);
        Map(DomGoogleCloudProject, ID_GOOGLE_CLOUD_PROJ, GV_STRING);
        Convert(options, this, true);
    }

    int OnNotify(LViewI *Ctrl, const LNotification &n) override
    {
        switch (Ctrl->GetId())
        {
            case IDOK:
        	    Convert(options, this, false);
                EndModal(1);
                break;
            case IDCANCEL:
                EndModal(0);
                break;
        }

        return 0;
    }
};