// Common Clipboard functions
#include "lgi/common/Lgi.h"
#include "lgi/common/ClipBoard.h"

bool LClipBoard::UnitTests()
{
	auto testName = "LClipBoard.UnitTests";
	auto parent = LAppInst->AppWnd;
	LClipBoard c(parent);

	#if 0 // The 'files' list tests:

		auto app = LGetSystemPath(LSP_APP_INSTALL);
		LFile::Path p(app);
		p--;

		LString::Array files;
		files.Add((p / "src" / "AddFtpFile.h").GetFull());
		files.Add((p / "src" / "AddFtpFile.cpp").GetFull());

		// Set clipboard file test:
		c.Files(files);

		LgiMsg(parent, "Paste the files on the clipboard now.", testName);

		LgiMsg(parent, "Copy some files now.", testName);
		c.Files([parent, testName](auto files, auto err)
		{
			if (files.Length())
				LgiMsg(parent, LString("Files on the clipboard are:\n") + LString("\n").Join(files), testName);
			else
				LgiMsg(parent, LString("No files, err: ") + err, testName);
		});
	
	#endif

	#if 1 // The 'html' tests:

		/*
		auto InDoc = "<html>\n"
					"<body>\n"
					"This is a <b>html</b> test.\n"
					"</body>\n"
					"</html>\n";
		c.Html(InDoc);

		LgiMsg(parent, "Paste the test HTML somewhere that accepts rich text.", testName);
		*/

		LArray<LClipBoard::FormatType> types;
		if (c.EnumFormats(types))
		{
			for (auto t: types)
				LgiTrace("type=%s\n", LClipBoard::FmtToStr(t).Get());
		}

		auto Html = c.Html();
		printf("Html=%s\n", Html.Get());

	#endif
	
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