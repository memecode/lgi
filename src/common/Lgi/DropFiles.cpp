#include "lgi/common/Lgi.h"
#include "lgi/common/DropFiles.h"

#ifdef MAC
LgiFunc bool LMacFileToPath(LString &a)
{
	bool Status = true;

	#if LGI_COCOA

		auto *s = [[NSString alloc] initWithBytes:a.Get() length:a.Length() encoding:NSUTF8StringEncoding];
		if (s)
		{
			NSURL *url = [[NSURL alloc] initFileURLWithPath:s];
			if (url)
			{
				a = [url.path UTF8String];
				Status = !a.IsEmpty();
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

		LAssert(!"Impl me.");

	#endif

	return true;
}
#endif
