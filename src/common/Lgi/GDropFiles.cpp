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
			else
			{
				printf("%s:%i - initFileURLWithPath failed for %s\n", _FL, a.Get());
				Status = false;
			}
			[s release];
		}
		else
		{
			printf("%s:%i - initWithBytes failed for %s\n", _FL, a.Get());
			Status = false;
		}

	#else

		LgiAssert(!"Impl me.");

	#endif

	return true;
}
#endif
