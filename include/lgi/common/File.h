/**
	\file
	\author Matthew Allen
	\date 24/5/2002
	\brief Common file system header
	Copyright (C) 1995-2002, <a href="mailto:fret@memecode.com">Matthew Allen</a>
*/

#pragma once

#include <fcntl.h>

#include "lgi/common/Mem.h"
#include "lgi/common/Stream.h"
#include "lgi/common/Array.h"
#include "lgi/common/RefCount.h"
#include "lgi/common/StringClass.h"
#include "lgi/common/Error.h"

#ifdef WIN32

	typedef HANDLE							OsFile;
	#define INVALID_HANDLE					INVALID_HANDLE_VALUE
	#define ValidHandle(hnd)				((hnd) != INVALID_HANDLE_VALUE)
	#define DIR_PATH_SIZE					512
	#define LFileCompare					_stricmp

	#define O_READ							GENERIC_READ
	#define O_WRITE							GENERIC_WRITE
	#define O_READWRITE						(GENERIC_READ | GENERIC_WRITE)
	#define O_SHARE							0x01000000
	#define O_NO_CACHE						0x00800000

#else

	#include <sys/types.h>
	#include <sys/stat.h>
	#include <dirent.h>
	#include <unistd.h>

	typedef int								OsFile;
	#define INVALID_HANDLE					-1
	#define ValidHandle(hnd)				((hnd) >= 0)
	#define LFileCompare					strcmp

	#define O_READ							O_RDONLY
	#define O_WRITE							O_WRONLY
	#ifdef MAC
		#define O_SHARE						O_SHLOCK
	#else
		#define O_SHARE						0
	#endif
	#define O_READWRITE						O_RDWR

#endif

/////////////////////////////////////////////////////////////////////
// Defines
#define FileDev								(LFileSystem::GetInstance())
#define MAX_PATH_LEN						512

// File system types (used by LDirectory and LVolume)
enum LVolumeTypes
{
	VT_NONE,
	VT_FLOPPY,
	VT_HARDDISK,
	VT_CDROM,
	VT_RAMDISK,
	VT_FOLDER,
	VT_FILE,
	VT_DESKTOP,
	VT_MUSIC,
	VT_PICTURES,
	VT_DOWNLOADS,
	VT_TRASH,
	VT_USB_FLASH,
	VT_APPLICATIONS,
	VT_NETWORK_NEIGHBOURHOOD,
	VT_NETWORK_MACHINE,
	VT_NETWORK_SHARE,
	VT_NETWORK_PRINTER,
	VT_NETWORK_GROUP,
	VT_MAX,
};

// Volume attributes
#define VA_CASE_SENSITIVE				0x0001
#define VA_CASE_PRESERVED				0x0002
#define VA_UNICODE_ON_DISK				0x0004
#define VA_LFN_API						0x4000
#define VA_COMPRESSED					0x8000

// File attributes
#define FA_NORMAL						0x0000
#define FA_READONLY						0x0001
#define FA_HIDDEN						0x0002
#define FA_SYSTEM						0x0004
#define FA_VOLUME						0x0008
#define FA_DIRECTORY					0x0010
#define FA_ARCHIVE						0x0020
#define FA_COMPUTER						0x0040

/////////////////////////////////////////////////////////////////////
// Abstract classes

/// Generic directory iterator
class LgiClass LDirectory
{
	struct LDirectoryPriv *d;

public:
	constexpr static int MaxPathLen = 512;

	LDirectory();
	virtual ~LDirectory();

	/// \brief Starts the search. The entries '.' and '..' are never returned.
	/// The default pattern returns all files.
	/// \return Non zero on success
	virtual int First
	(
		/// The path of the directory
		const char *Name,
		/// The pattern to match files against.
		/// \sa The default LGI_ALL_FILES matchs all files.
		const char *Pattern = LGI_ALL_FILES
	);
	
	/// \brief Get the next match
	/// \return Non zero on success
	virtual int Next();

	/// \brief Finish the search
	/// \return Non zero on success
	virtual int Close();

	/// \brief Constructs the full path of the current directory entry
	/// \return Non zero on success
	virtual bool Path
	(
		// The buffer to write to
		char *s,
		// The size of the output buffer in bytes
		int BufSize
	)	const;

	virtual const char *FullPath();

	/// Gets the current entries attributes (platform specific)
	virtual long GetAttributes() const;
	
	/// Gets the name of the current entry. (Doesn't include the path).
	virtual char *GetName() const;
	virtual LString FileName() const;
	
	/// Gets the user id of the current entry. (Doesn't have any meaning on Win32).
	virtual int GetUser
	(
		/// If true gets the group id instead of the user id.
		bool Group
	) const;
	
	/// Gets the entries creation time. You can convert this to an easy to read for using LDateTime.
	virtual uint64 GetCreationTime() const;

	/// Gets the entries last access time. You can convert this to an easy to read for using LDateTime.
	virtual uint64 GetLastAccessTime() const;

	/// Gets the entries last modified time.  You can convert this to an easy to read for using LDateTime.
	virtual uint64 GetLastWriteTime() const;

	/// Returns the uncompressed size of the entry.
	virtual uint64 GetSize() const;
	
	/// Returns the size of file on disk. This can be both larger and smaller than the logical size.
	virtual int64 GetSizeOnDisk();

	/// Returns true if the entry is a sub-directory.
	virtual bool IsDir() const;
	
	/// Returns true if the entry is a symbolic link.
	virtual bool IsSymLink() const;
	
	/// Returns true if the entry is read only.
	virtual bool IsReadOnly() const;

	/// \brief Returns true if the entry is hidden.
	/// This is equivilant to a attribute flag on win32 and a leading '.' on unix.
	virtual bool IsHidden() const;

	/// Creates an copy of this type of LDirectory class.
	virtual LDirectory *Clone();
	
	/// Gets the type code of the current entry. See the VT_?? defines for possible values.
	virtual int GetType() const;

	/// Converts a string to the 64-bit value returned from the date functions.
	bool ConvertToTime(char *Str, int SLen, uint64 Time) const;

	/// Converts the 64-bit value returned from the date functions to a string.
	bool ConvertToDate(char *Str, int SLen, uint64 Time) const;
};

/// Describes a volume connected to the system
class LgiClass LVolume
{
	friend class LFileSystem;
	friend struct LVolumePriv;

protected:
	struct LVolumePriv *d;

public:
	LVolume(const char *Path = NULL);
	LVolume(LSystemPath SysPath, const char *Name);
	virtual ~LVolume();

	const char *Name() const;
	const char *Path() const;
	int Type() const;
	int Flags() const;
	uint64 Size() const;
	uint64 Free() const;
	LSurface *Icon() const;

	virtual bool IsMounted() const;
	virtual bool SetMounted(bool Mount);
	virtual LVolume *First();
	virtual LVolume *Next();
	virtual LDirectory *GetContents();
	virtual void Insert(LAutoPtr<LVolume> v);
};

typedef int (*CopyFileCallback)(void *token, int64 Done, int64 Total);

/// A singleton class for accessing the file system
class LgiClass LFileSystem
{
	friend class LFile;
	static LFileSystem *Instance;
	class LFileSystemPrivate *d;

	LVolume *Root = NULL;

public:
	LFileSystem();
	~LFileSystem();
	
	/// Return the current instance of the file system. The shorthand for this is "FileDev".
	static LFileSystem *GetInstance() { return Instance; }

	/// Call this when the devices on the system change. For instance on windows
	/// when you receive WM_DEVICECHANGE.
	void OnDeviceChange(char *Reserved = 0);

	/// Gets the root volume of the system.
	LVolume *GetRootVolume();
	
	/// Copies a file
	bool Copy
	(
		/// The file to copy from...
		const char *From,
		/// The file to copy to. Any existing file there will be overwritten without warning.
		const char *To,
		/// The error code or zero on success
		LError *Status = 0,
		/// Optional callback when some data is copied.
		CopyFileCallback Callback = 0,
		/// A user defined token passed to the callback function
		void *Token = 0
	);

	/// Delete file
	bool Delete(const char *FileName, bool ToTrash = true);
	/// Delete files
	bool Delete
	(
		/// The list of files to delete
		LArray<const char*> &Files,
		/// A list of status codes where 0 means success and non-zero is an error code, usually an OS error code. NULL if not required.
		LArray<LError> *Status = NULL,
		/// true if you want the files moved to the trash folder, false if you want them deleted directly
		bool ToTrash = true
	);
	
	/// Create a directory
	bool CreateFolder(const char *PathName, bool CreateParentFoldersIfNeeded = false, LError *Err = NULL);
	
	/// Remove's a directory	
	bool RemoveFolder
	(
		/// The path to remove
		const char *PathName,
		/// True if you want this function to recursively delete all contents of the path passing in.
		bool Recurse = false
	);
	
	LString GetCurrentFolder();
	bool SetCurrentFolder(const char *PathName);

	/// Moves a file to a new location. Only works on the same device.
	bool Move(const char *OldName, const char *NewName, LError *Err = NULL);
};

#if defined(WINDOWS) || HAIKU32
	#define GFileOps()			\
		GFileOp(char)			\
		GFileOp(int8_t)			\
		GFileOp(uint8_t)		\
		GFileOp(int16_t)		\
		GFileOp(uint16_t)		\
		GFileOp(int32_t)		\
		GFileOp(uint32_t)		\
		GFileOp(int64_t)		\
		GFileOp(uint64_t)		\
		GFileOp(long)			\
		GFileOp(ulong)			\
		GFileOp(float)			\
		GFileOp(double)
#elif defined(LINUX) || HAIKU64
	#define GFileOps()			\
		GFileOp(char)			\
		GFileOp(int8_t)			\
		GFileOp(uint8_t)		\
		GFileOp(int16_t)		\
		GFileOp(uint16_t)		\
		GFileOp(int32_t)		\
		GFileOp(uint32_t)		\
		GFileOp(int64_t)		\
		GFileOp(uint64_t)		\
		GFileOp(float)			\
		GFileOp(double)
/*
		GFileOp(long)			\
		GFileOp(ulong)			\
*/
#else
	#define GFileOps()			\
		GFileOp(char)			\
		GFileOp(int8_t)			\
		GFileOp(uint8_t)		\
		GFileOp(int16_t)		\
		GFileOp(uint16_t)		\
		GFileOp(int32_t)		\
		GFileOp(uint32_t)		\
		GFileOp(int64_t)		\
		GFileOp(uint64_t)		\
		GFileOp(size_t)			\
		GFileOp(ssize_t)		\
		GFileOp(float)			\
		GFileOp(double)
#endif

/// Generic file access class
class LgiClass LFile : public LStream, public LRefCount
{
protected:
	class LFilePrivate *d;

	ssize_t SwapRead(uchar *Buf, ssize_t Size);
	ssize_t SwapWrite(uchar *Buf, ssize_t Size);

public:
	LFile(const char *Path = NULL, int Mode = O_READ);
	virtual ~LFile();

	OsFile Handle();
	void ChangeThread() override;
	operator bool() { return IsOpen(); }

	/// \brief Opens a file
	/// \return Non zero on success
	int Open
	(
		/// The path of the file to open
		const char *Name,
		/// The mode to open the file with. One of O_READ, O_WRITE or O_READWRITE.
		int Attrib
	)	override;
	
	/// Returns non zero if the class is associated with an open file handle.
	bool IsOpen() override;

	/// Returns the most recent error code encountered.
	int GetError();
	
	/// Closes the file.
	int Close() override;
	
	/// Gets the mode that the file was opened with.
	int GetOpenMode();

	/// Gets the block size
	int GetBlockSize();

	/// \brief Gets the current file pointer.
	/// \return The file pointer or -1 on error.
	int64 GetPos() override;
	
	/// \brief Sets the current file pointer.
	/// \return The new file pointer or -1 on error.
	int64 SetPos(int64 Pos) override;

	/// \brief Gets the file size.
	/// \return The file size or -1 on error.
	int64 GetSize() override;

	/// \brief Sets the file size.
	/// \return The new file size or -1 on error.
	int64 SetSize(int64 Size) override;

	/// \brief Reads bytes into memory from the current file pointer.
	/// \return The number of bytes read or <= 0.
	ssize_t Read(void *Buffer, ssize_t Size, int Flags = 0) override;
	
	/// \brief Writes bytes from memory to the current file pointer.
	/// \return The number of bytes written or <= 0.
	ssize_t Write(const void *Buffer, ssize_t Size, int Flags = 0) override;

	/// Modified time functions
	uint64_t GetModifiedTime();
	bool SetModifiedTime(uint64_t dt);

	/// Gets the path used to open the file
	virtual const char *GetName();
	
	/// Moves the current file pointer.
	virtual int64 Seek(int64 To, int Whence);
	
	/// Returns true if the current file pointer is at the end of the file.
	virtual bool Eof();

	/// Resets the status value.
	virtual void SetStatus(bool s = false);
	
	/// \brief Returns true if all operations were successful since the file was openned or SetStatus
	/// was used to reset the file's status.
	virtual bool GetStatus();

	/// \brief Sets the swap option. When switched on all integer reads/writes will have their bytes 
	/// swaped.
	virtual void SetSwap(bool s);
	
	/// Gets the current swap setting.
	virtual bool GetSwap();

	// String
	virtual ssize_t ReadStr(char *Buf, ssize_t Size);
	virtual ssize_t WriteStr(char *Buf, ssize_t Size);

	// Operators
	#define GFileOp(type)		virtual LFile &operator >> (type &i);
	GFileOps();
	#undef GFileOp

	#define GFileOp(type)		virtual LFile &operator << (type i);
	GFileOps();
	#undef GFileOp

	// LDom impl
	bool GetVariant(const char *Name, LVariant &Value, const char *Array = NULL) override;
	bool SetVariant(const char *Name, LVariant &Value, const char *Array = NULL) override;
	bool CallMethod(const char *Name, LScriptArguments &Args) override;

	// Path handling
	class LgiClass Path : public LString::Array
	{
		LString Full;
		
	public:
		enum State
		{
			TypeNone = 0,
			TypeFolder = 1,
			TypeFile = 2,
		};
		
		static LString Sep;

		Path(const char *init = NULL, const char *join = NULL)
		{
			SetFixedLength(false);
			if (init)
				*this = init;
			if (join)
				*this += join;
		}

		Path(LAutoString Init)
		{
			SetFixedLength(false);
			if (Init)
				*this = Init.Get();
		}
		
		Path(LString Init)
		{
			SetFixedLength(false);
			if (Init)
				*this = Init;
		}
		
		Path(LSystemPath Which, int WordSize = 0)
		{
			SetFixedLength(false);
			*this = GetSystem(Which, WordSize);
		}

		Path &operator =(const char *p)
		{
			LString s(p);
			*((LString::Array*)this) = s.SplitDelimit("\\/");
			SetFixedLength(false);
			return *this;
		}

		Path &operator =(const LString &s)
		{
			*((LString::Array*)this) = s.SplitDelimit("\\/");
			SetFixedLength(false);
			return *this;
		}
		
		Path &operator =(const LArray<LString> &p)
		{
			*((LArray<LString>*)this) = p;
			Full.Empty();
			SetFixedLength(false);
			return *this;
		}
		
		Path &operator +=(Path &a)
		{
			SetFixedLength(false);

			if (a.Length() == 0)
				return *this;

			for (unsigned i=0; i<a.Length(); i++)
			{
				LString &s = a[i];
				if (!_stricmp(s, "."))
					;
				else if (!_stricmp(s, ".."))
					(*this)--;
				else
					New() = s;
			}

			return *this;
		}

		Path Join(const LString p)
		{
			Path a(p);
			if (a.Length() == 0)
				return *this;
			if (!a[0u].Equals(".") && !a[0u].Equals(".."))
				Length(0);
			*this += a;
			return *this;
		}

		Path &operator +=(const LString &p)
		{
			Path a(p);
			*this += a;
			return *this;
		}
		
		Path GetParent()
		{
			Path p = *this;
			p.PopLast();
			return p;
		}

		Path &operator --()
		{
			PopLast();
			return *this;
		}

		Path &operator --(int i)
		{
			PopLast();
			return *this;
		}

		Path operator / (LString seg)
		{
			Path p(*this);
			p.Add(seg);
			return p;
		}
		
		operator const char *()
		{
			return GetFull();
		}

		LString GetFull()
		{
			#if !defined(WINDOWS)
			if (!IsRelative())
				Full = Sep + Sep.Join(*this);
			else
			#endif
				Full = Sep.Join(*this);
			return Full;
		}
		
		bool IsRelative() const
		{
			if (Length() == 0)
				return false;
			auto &f = ItemAt(0);
			if (f.Equals("~") || f.Equals(".") || f.Equals(".."))
				return true;
			return false;
		}
		
		LFile::Path Absolute()
		{
			LFile::Path p = *this;
			p.SetFixedLength(false);
			
			for (size_t i=0; i<p.Length(); i++)
			{
				if (p[i].Equals("~"))
				{
					auto h = GetSystem(LSP_HOME, 0).SplitDelimit(DIR_STR);
					p.DeleteAt(i, true);
					p.AddAt(i, h);
					break;
				}
			}
			return p;
		}
		
		bool FixCase();
		State Exists();
		bool IsFile() { return Exists() == TypeFile; }
		bool IsFolder() { return Exists() == TypeFolder; }
		static LString GetSystem(LSystemPath Which, int WordSize = 0);
		static LString PrintAll();
	};

	/// Read the whole file into a string
	LString Read()
	{
		LString s;
		int64 sz = GetSize();
		NativeInt nsz = (NativeInt)sz;
		if (nsz > 0 &&
			nsz == sz &&
			s.Set(NULL, nsz))
		{
			int Block = 1 << 30;
			NativeInt i;
			for (i = 0; i < nsz; )
			{
				int Len = MIN(Block, (int) (nsz - i));
				ssize_t rd = Read(s.Get() + i, Len);
				if (rd <= 0)
					break;
				i += rd;
			}

			s.Get()[i] = 0;
		}
		return s;
	}

	/// Write a string
	bool Write(const LString &s)
	{
		return Write(s.Get(), s.Length()) == s.Length();
	}

	/// Sets the file to zero size.
	/// Useful for this sort of thing:
	/// LFile(MyPath, O_WRITE).Empty().Write(MyData);
	LFile &Empty()
	{
		SetSize(0);
		return *this;
	}
};

// Functions
LgiFunc int64 LFileSize(const char *FileName);

/// This function checks for the existence of a file (will return false for a folder).
LgiFunc bool LFileExists(const char *File, char *CorrectCase = NULL);

/// This function checks for the existence of a directory.
LgiFunc bool LDirExists(const char *Dir, char *CorrectCase = NULL);

/// Looks up the target of a link or shortcut file.
LgiFunc bool LResolveShortcut(const char *LinkFile, char *Path, ssize_t Len);

/// Reads in a text file to a dynamically allocated string
LgiFunc char *LReadTextFile(const char *File);

/// Trims off a path segment
LgiFunc bool LTrimDir(char *Path);

/// Gets the file name part of the path
LgiExtern const char *LGetLeaf(const char *Path);

/// Gets the file name part of the path
LgiExtern char *LGetLeaf(char *Path);

/// /returns true if the path is relative as opposed to absolute.
LgiFunc bool LIsRelativePath(const char *Path);

/// Creates a relative path
LgiClass LString LMakeRelativePath(const char *Base, const char *Path);

/// Appends 'File' to 'Dir' and puts the result in 'Str'. Dir and Str can be the same buffer.
LgiFunc bool LMakePath(char *Str, int StrBufLen, const char *Dir, const char *File);

/// Gets the file name's extension.
LgiFunc char *LGetExtension(const char *File);

/// \returns true if 'FileName' is an executable of some kind (looks at file name only).
LgiFunc bool LIsFileNameExecutable(const char *FileName);

/// \returns true if 'FileName' is an executable of some kind (looks at content).
LgiFunc bool LIsFileExecutable(const char *FileName, LStreamI *f, int64 Start, int64 Len);

/// Get information about the disk that a file resides on.
LgiFunc bool LGetDriveInfo(char *Path, uint64 *Free, uint64 *Size = 0, uint64 *Available = 0);

/// Shows the file's properties dialog
LgiFunc void LShowFileProperties(OsView Parent, const char *Filename);

/// Opens to the file or folder in the OS file browser (Explorer/Finder etc)
LgiFunc bool LBrowseToFile(const char *Filename);

/// Returns the physical device a file resides on
LgiExtern LString LGetPhysicalDevice(const char *Path);
