#include "lgi/common/Lgi.h"
#include <stdlib.h>
#if !defined(__GTK_H__)
#import <Cocoa/Cocoa.h>
#endif

void LShowFileProperties(OsView Parent, const char *Filename)
{
	if (!Filename)
		return;

	#if !defined(__GTK_H__)

	LString FileUri;
	FileUri.Printf("file://%s", Filename);

	NSPasteboard *pboard = [NSPasteboard pasteboardWithUniqueName];
	[pboard declareTypes:[NSArray arrayWithObject:NSPasteboardTypeFileURL] owner:nil];

	NSMutableArray *fileList = [NSMutableArray new];

	// Add as many as file's path in the fileList array
	NSString *fn = [NSString stringWithUTF8String:FileUri.Get()];
	[fileList addObject:fn];

	[pboard setPropertyList:fileList forType:NSPasteboardTypeFileURL];
	NSPerformService(@"Finder/Show Info", pboard);
	#endif
}

bool LBrowseToFile(const char *Filename)
{
	LString s;
	s.Printf("/usr/bin/osascript "
			"-e 'set asd to POSIX file \"%s\" as string\n"
			"tell application \"Finder\"\n"
			"	reveal asd\n"
			"	activate\n"
			"end tell'",
			Filename);
	// printf("s='%s'\n", s.Get());
	return system(s) >= 0;
}
