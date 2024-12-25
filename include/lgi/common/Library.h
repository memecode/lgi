#ifndef __GLIBRARY_H
#define __GLIBRARY_H

// Os specific wrapper typedefs/defines
#if defined(WINDOWS)
	typedef HMODULE OsLibHandle;
#elif defined(POSIX)
	typedef void *OsLibHandle;
#else // atheos/linux
	typedef int OsLibHandle;
#endif

// Generic shared library loader
class LgiClass LLibrary
{
	char *FileName = nullptr;
	OsLibHandle hLib = nullptr;

public:
	LLibrary(const char *File = 0, bool Quiet = false);
	virtual ~LLibrary();

	OsLibHandle Handle() { return hLib; }
	const char *GetFileName() { return FileName; }
	virtual bool IsLoaded()
	{
		return hLib != 0;
	}

	bool Load(const char *File, bool Quiet = false);
	bool Unload();
	void *GetAddress(const char *Resource);
	LString GetFullPath();
};

// Runtime linking macros
#define GLibBreakPoint	LAssert(0);
#define GLibCallType	// __stdcall

#define GLibFunc0(ret, func)							\
	ret func()											\
	{													\
		typedef ret (GLibCallType *p_##func)();			\
		p_##func p = (p_##func) GetAddress(#func);		\
		if (p)											\
		{												\
			return p();									\
		}												\
		GLibBreakPoint									\
		return 0;										\
	}

#define GLibFunc1(ret, func, argtype, argname)			\
	ret func(argtype argname)							\
	{													\
		typedef ret (GLibCallType *p_##func)(argtype);	\
		p_##func p = (p_##func) GetAddress(#func);		\
		if (p)											\
		{												\
			return p(argname);							\
		}												\
		GLibBreakPoint									\
		return (ret)0;									\
	}

#define GLibFunc2(ret, func, argtype1, argname1, argtype2, argname2) \
	ret func(argtype1 argname1, argtype2 argname2)		\
	{													\
		typedef ret (GLibCallType *p_##func)(argtype1, argtype2);	\
		p_##func p = (p_##func) GetAddress(#func);		\
		if (p)											\
		{												\
			return p(argname1, argname2);				\
		}												\
		GLibBreakPoint									\
		return (ret)0;									\
	}

#define GLibFunc3(ret, func, argtype1, argname1,		\
							argtype2, argname2,			\
							argtype3, argname3)			\
	ret func(argtype1 argname1,							\
			 argtype2 argname2,							\
			 argtype3 argname3)							\
	{													\
		typedef ret (GLibCallType *p_##func)(argtype1,	\
								argtype2,				\
								argtype3);				\
		p_##func p = (p_##func) GetAddress(#func);		\
		if (p)											\
		{												\
			return p(argname1, argname2, argname3);		\
		}												\
		GLibBreakPoint									\
		return 0;										\
	}

#define GLibFunc4(ret, func, argtype1, argname1,		\
							argtype2, argname2,			\
							argtype3, argname3,			\
							argtype4, argname4)			\
	ret func(argtype1 argname1,							\
			 argtype2 argname2,							\
			 argtype3 argname3,							\
			 argtype4 argname4)							\
	{													\
		typedef ret (GLibCallType *p_##func)(argtype1,	\
								argtype2,				\
								argtype3,				\
								argtype4);				\
		p_##func p = (p_##func) GetAddress(#func);		\
		if (p)											\
		{												\
			return p(argname1, argname2,				\
					 argname3, argname4);				\
		}												\
		GLibBreakPoint									\
		return 0;										\
	}

#define GLibFunc5(ret, func, argtype1, argname1,		\
							argtype2, argname2,			\
							argtype3, argname3,			\
							argtype4, argname4,			\
							argtype5, argname5)			\
	ret func(argtype1 argname1,							\
			 argtype2 argname2,							\
			 argtype3 argname3,							\
			 argtype4 argname4,							\
			 argtype5 argname5)							\
	{													\
		typedef ret (GLibCallType *p_##func)(argtype1,	\
								argtype2,				\
								argtype3,				\
								argtype4,				\
								argtype5);				\
		p_##func p = (p_##func) GetAddress(#func);		\
		if (p)											\
		{												\
			return p(argname1, argname2,				\
					 argname3, argname4,				\
					 argname5);							\
		}												\
		GLibBreakPoint									\
		return 0;										\
	}

#define GLibFunc6(ret, func, argtype1, argname1,		\
							argtype2, argname2,			\
							argtype3, argname3,			\
							argtype4, argname4,			\
							argtype5, argname5,			\
							argtype6, argname6)			\
	ret func(argtype1 argname1,							\
			 argtype2 argname2,							\
			 argtype3 argname3,							\
			 argtype4 argname4,							\
			 argtype5 argname5,							\
			 argtype6 argname6)							\
	{													\
		typedef ret (GLibCallType *p_##func)(argtype1,	\
								argtype2,				\
								argtype3,				\
								argtype4,				\
								argtype5,				\
								argtype6);				\
		p_##func p = (p_##func) GetAddress(#func);		\
		if (p)											\
		{												\
			return p(argname1, argname2,				\
					 argname3, argname4,				\
					 argname5, argname6);				\
		}												\
		GLibBreakPoint									\
		return 0;										\
	}

#define GLibFunc7(ret, func, argtype1, argname1,		\
							argtype2, argname2,			\
							argtype3, argname3,			\
							argtype4, argname4,			\
							argtype5, argname5,			\
							argtype6, argname6,			\
							argtype7, argname7)			\
	ret func(argtype1 argname1,							\
			 argtype2 argname2,							\
			 argtype3 argname3,							\
			 argtype4 argname4,							\
			 argtype5 argname5,							\
			 argtype6 argname6,							\
			 argtype7 argname7)							\
	{													\
		typedef ret (GLibCallType *p_##func)(argtype1,	\
								argtype2,				\
								argtype3,				\
								argtype4,				\
								argtype5,				\
								argtype6,				\
								argtype7);				\
		p_##func p = (p_##func) GetAddress(#func);		\
		if (p)											\
		{												\
			return p(argname1, argname2,				\
					 argname3, argname4,				\
					 argname5, argname6, argname7);		\
		}												\
		GLibBreakPoint									\
		return 0;										\
	}

#endif
