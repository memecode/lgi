#include "Lgi.h"
#include "GDropFiles.h"

#ifdef MAC
LgiFunc bool LMacFileToPath(GAutoString &a)
{
	bool Status = true;
	#if LGI_COCOA
		auto *s = [[NSString alloc] initWithBytes:a.Get() length:strlen(a) encoding:NSUTF8StringEncoding];
		if (s)
		{
			NSURL *url = [[NSURL alloc] initFileURLWithPath:s];
			if (url)
			{
				Status = a.Reset(NewStr([url.path UTF8String]));
				[url release];
			}
			else Status = false;
			[s release];
		}
		else Status = false;
	#else
		LgiAssert(!"Impl me.");
	#endif

	return true;
}
#endif