#include "lgi/common/Lgi.h"

#include <app/Messenger.h>
#include <private/tracker/tracker_private.h>

void LShowFileProperties(OsView Parent, const char *Filename)
{
    BMessenger tracker(kTrackerSignature);
    if (!tracker.IsValid())
    {
        LgiTrace("%s:%i - No tracker app '%s'\n", _FL, kTrackerSignature);
        return;
    }
    
	const uint32 kGetInfo = 'Tinf';
    BMessage msg(kGetInfo);

    BEntry entry(Filename);
    entry_ref ref;
    if (entry.GetRef(&ref) != B_OK)
    {
        LgiTrace("%s:%i - No entry for '%s'\n", _FL, Filename);
        return;
    }

    msg.AddRef("refs", &ref);
    auto result = tracker.SendMessage(&msg);
    if (result != B_OK)
    {
        LgiTrace("%s:%i - SendMessage result: %x\n", _FL, result);
    }
}

bool LBrowseToFile(const char *Filename)
{
    BMessenger tracker(kTrackerSignature);
    if (!tracker.IsValid())
    {
        LgiTrace("%s:%i - No tracker app '%s'\n", _FL, kTrackerSignature);
        return false;
    }

    LFile::Path p(Filename);

    bool isFile = p.IsFile();
    if (isFile)
        p.PopLast();

    BMessage msg(B_REFS_RECEIVED);
    BEntry entry(p.GetFull());
    entry_ref ref;
    if (entry.GetRef(&ref) != B_OK)
    {
        LgiTrace("%s:%i - No entry for '%s'\n", _FL, Filename);
        return false;
    }

    msg.AddRef("refs", &ref);
    auto result = tracker.SendMessage(&msg);
    if (result != B_OK)
    {
        LgiTrace("%s:%i - SendMessage result: %x\n", _FL, result);
    }

    if (isFile)
    {
        // Also select the file...
        BMessage sel(BPrivate::kSelect);
        BEntry fileEntry(Filename);
        if (fileEntry.GetRef(&ref) != B_OK)
        {
            LgiTrace("%s:%i - No entry for '%s'\n", _FL, Filename);
            return false;
        }

        sel.AddRef("refs", &ref);
        snooze(300000);
        auto result = tracker.SendMessage(&sel);
        if (result != B_OK)
        {
            LgiTrace("%s:%i - SendMessage result: %x\n", _FL, result);
        }
    }


    return result == B_OK;
}