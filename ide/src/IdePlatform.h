#pragma once

//////////////////////////////////////////////////////////////////////
// Platform stuff
enum IdePlatform
{
	PlatformUnknown = -2,
	PlatformCurrent = -1,
	PlatformWin = 0,		// 0x1
	PlatformLinux,			// 0x2
	PlatformMac,			// 0x4
	PlatformHaiku,			// 0x8
	PlatformMax,
};
#define PLATFORM_WIN32			(1 << PlatformWin)
#define PLATFORM_LINUX			(1 << PlatformLinux)
#define PLATFORM_MAC			(1 << PlatformMac)
#define PLATFORM_HAIKU			(1 << PlatformHaiku)
#define PLATFORM_ALL			(PLATFORM_WIN32|PLATFORM_LINUX|PLATFORM_MAC|PLATFORM_HAIKU)
extern IdePlatform PlatformFlagsToEnum(int flags);
extern LString PlatformFlagsToStr(int flags);
extern const char *ToString(IdePlatform p);

#if defined(_WIN32)
#define PLATFORM_CURRENT		PLATFORM_WIN32
#elif defined(MAC)
#define PLATFORM_CURRENT		PLATFORM_MAC
#elif defined(LINUX)
#define PLATFORM_CURRENT		PLATFORM_LINUX
#elif defined(HAIKU)
#define PLATFORM_CURRENT		PLATFORM_HAIKU
#endif

extern const char *PlatformNames[];
extern const char sCurrentPlatform[];
extern const char *Untitled;
extern const char SourcePatterns[];

