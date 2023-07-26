// Common Clipboard functions
#include "lgi/common/Lgi.h"
#include "lgi/common/ClipBoard.h"

bool LClipBoard::UnitTests()
{
	LClipBoard c(LAppInst->AppWnd);

	/* Set clipboard file test:
	LString::Array files;
	files.Add("/home/matthew/code/lgi/trunk/Ide/src/DebugContext.cpp");
	files.Add("/home/matthew/code/lgi/trunk/Ide/src/DebugContext.h");
	c.Files(files);
	*/
	
	/* Get clipboard formats test:
	LArray<LClipBoard::FormatType> Formats;
	if (c.EnumFormats(Formats))
	{
		for (auto s: Formats)
			LgiTrace("Fmt:%s\n", c.FmtToStr(s).Get());
	}
	*/

    return false;
}