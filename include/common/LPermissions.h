#ifndef _LPERM_H_
#define _LPERM_H_

#include "LHashTable.h"

struct LPermissions
{
	struct _Win
	{
		uint32_t ReadOnly : 1;	// FILE_ATTRIBUTE_READONLY
		uint32_t Hidden : 1;
		uint32_t System : 1;
		uint32_t Reserved1 : 1;

		uint32_t Folder : 1;		// FILE_ATTRIBUTE_DIRECTORY
		uint32_t Archive : 1;
		uint32_t Device : 1;
		uint32_t Normal : 1;

		uint32_t Temporary : 1;	// FILE_ATTRIBUTE_TEMPORARY
		uint32_t Sparse : 1;
		uint32_t ReparsePoint : 1;
		uint32_t Compressed : 1;

		uint32_t Offline : 1;		// FILE_ATTRIBUTE_OFFLINE
		uint32_t ContentNotIndexed : 1;
		uint32_t Encrypted : 1;
		uint32_t IntegrityStream : 1;

		uint32_t Virtual : 1;		// FILE_ATTRIBUTE_VIRTUAL
		uint32_t NoScrub : 1;
		uint32_t Ea : 1;
	};

	struct _Unix
	{
		uint32_t GlobalExecute : 1;	// S_IXOTH
		uint32_t GlobalWrite : 1;		// S_IWOTH
		uint32_t GlobalRead : 1;		// S_IROTH
		uint32_t GlobalReserved : 1;
		
		uint32_t GroupExecute : 1;	// S_IXGRP
		uint32_t GroupWrite : 1;		// S_IWGRP
		uint32_t GroupRead : 1;		// S_IRGRP
		uint32_t GroupReserved : 1;

		uint32_t UserExecute : 1;		// S_IXUSR
		uint32_t UserWrite : 1;		// S_IWUSR
		uint32_t UserRead : 1;		// S_IRUSR
		uint32_t UserReserved : 1;

		uint32_t Sticky : 1;
		uint32_t SetGid : 1;
		uint32_t SetUid : 1;
	};

	bool IsWindows; // switch between Win and Unix members in the union
	union
	{
		uint32_t u32;
		_Win Win;
		_Unix Unix;
	};

	LPermissions()
	{
		IsWindows = false;
		u32 = 0;
	}

	typedef LHashTbl<IntKey<uint32_t>,const char*> FlagMap;
	FlagMap &EnumFlags()
	{
		static FlagMap fWin, fUnix;
		FlagMap &m = IsWindows ? fWin : fUnix;
		if (m.Length() == 0)
		{
			for
			(
				auto s : GString
						(
							IsWindows
							?
							"0x1,ReadOnly\n0x2,Hidden\n0x4,System\n"
							"0x10,Folder\n0x20,Archive\n0x40,Device\n0x80,Normal\n"
							"0x100,Temporary\n0x200,Sparse\n0x400,ReparsePoint\n0x800,Compressed\n"
							"0x1000,Offline\n0x2000,ContentNotIndexed\n0x4000,Encrypted\n0x8000,IntegrityStream\n"
							"0x10000,Virtual\n0x20000,NoScrub\n0x40000,Ea\n"
							:
							"0x1,GlobalExecute\n0x2,GlobalWrite\n0x4,GlobalRead\n"
							"0x10,GroupExecute\n0x20,GroupWrite\n0x40,GroupRead\n"
							"0x100,UserExecute\n0x200,UserWrite\n0x400,UserRead\n"
							"0x1000,Sticky\n0x2000,SetGid\n0x4000,SetUid"
						).Split("\n")
			)
			{
				auto p = s.Split(",");
				fWin.Add(htoi(p[0].Get()),p[1]);
			}
		}
		return m;
	}

	bool UnitTest()
	{
		#ifdef WINDOWS
		u32 = FILE_ATTRIBUTE_READONLY;
		#else
		u32 = 1;
		#endif
		if (Win.System || !Win.ReadOnly || Win.Hidden) return false;
		#ifdef WINDOWS
		u32 = FILE_ATTRIBUTE_HIDDEN;
		if (Win.System || Win.ReadOnly || !Win.Hidden) return false;
		u32 = FILE_ATTRIBUTE_SYSTEM;
		if (!Win.System || Win.ReadOnly || Win.Hidden) return false;
		#endif

		u32 = 0x400;
		if (!Unix.UserRead || Unix.UserWrite || Unix.UserExecute) return false;

		return true;
	}
};

#endif
