#define __USE_GNU
#include <stdio.h>

#define DEBUG_LIB_MSGS		0
#define ALLOW_FALLBACK_PATH	0

#include "Lgi.h"
#if defined(LINUX) || defined(MAC)
#include <dlfcn.h>
#endif
#include "GToken.h"

GLibrary::GLibrary(const char *File, bool Quiet)
{
	FileName = 0;
	hLib = 0;
	if (File)
	{
		Load(File, Quiet);	
	}
}

GLibrary::~GLibrary()
{
	Unload();
}

bool GLibrary::Load(const char *File, bool Quiet)
{
	Unload();

	if (File)
	{
		char f[MAX_PATH];
		int ch = sprintf_s(f, sizeof(f), "%s", File);
		
		// check for default extension..
		// if you specify no extension in the application this will correctly set
		// it for the OS your running thus removing the need for #ifdef's in your
		// app. sweet!
		char *Ext = LgiGetExtension((char*)File);
		if (!Ext || _stricmp(Ext, LGI_LIBRARY_EXT))
		{
			ch += sprintf_s(f+ch, sizeof(f)-ch, ".%s", LGI_LIBRARY_EXT);
		}

		size_t Len = strlen(f) + 32;
		FileName = new char[Len];
		if (FileName)
		{
			memset(FileName, 0, Len);
			strcpy_s(FileName, Len, f); // FileName is always longer then 'f'
			
			#if defined WIN32

			SetErrorMode(SEM_NOOPENFILEERRORBOX | SEM_FAILCRITICALERRORS);

			GAutoWString w(Utf8ToWide(f));
			if (w)
			{
				hLib = LoadLibraryW(w);
				#if defined(_DEBUG) && DEBUG_LIB_MSGS
				if (!hLib)
					LgiTrace("LoadLibraryW(%S) failed with %i\n", w.Get(), GetLastError());
				#endif
			}
			else LgiTrace("%s:%i - Failed to convert string to wide.\n", _FL);

			SetErrorMode(0);


			#else
			
			char *f = strrchr(FileName, DIR_CHAR);
			if (f) f++;
			else f = FileName;
			bool IsLgi = stricmp(f, "lgilgi." LGI_LIBRARY_EXT) == 0 ||
						 stricmp(f, "lgilgid." LGI_LIBRARY_EXT) == 0;			
			if (!IsLgi) // Bad things happen when we load LGI again.
			{
				hLib = dlopen(FileName, RTLD_NOW);
				#if DEBUG_LIB_MSGS
				LgiTrace("%s:%i - dlopen('%s') = %p\n", _FL, FileName, hLib);
				#endif
				
				#ifdef LINUX
				if (!hLib)
				{
					// Try with an extra ".0"... just for fun.
					char *e = FileName + strlen(FileName);
					strcpy(e, ".0");
					hLib = dlopen(FileName, RTLD_NOW);
					#if DEBUG_LIB_MSGS
					LgiTrace("%s:%i - dlopen('%s') = %p\n", _FL, FileName, hLib);
					#endif
					*e = 0;
				}
				#endif	
				
				if (hLib)
				{
					#if 0 // defined(LINUX) && defined(__USE_GNU)
					char Path[MAX_PATH];
					int r = dlinfo(hLib, RTLD_DI_ORIGIN, Path);
					if (r == 0)
					{
						LgiMakePath(Path, sizeof(Path), Path, File);
						printf("GLibrary loaded: '%s'\n", Path);
					}
					else printf("%s:%i - dlinfo failed.\n", _FL);
					#endif
				}
				else
				{
					if (LgiIsRelativePath(FileName))
					{
						// Explicitly try the full path to the executable folder
						char p[MAX_PATH];
						#if LGI_COCOA || defined(__GTK_H__)
						LgiMakePath(p, sizeof(p), LgiArgsAppPath, "../../Frameworks");
						#else
						strcpy_s(p, sizeof(p), LGetExePath());
						#endif
						LgiMakePath(p, sizeof(p), p, FileName);
						hLib = dlopen(p, RTLD_NOW);
						#if DEBUG_LIB_MSGS
						printf("%s:%i - Trying '%s' = %p\n", _FL, p, hLib);
						#endif
					}
					if (!hLib)
					{
						#if DEBUG_LIB_MSGS
						char *e = dlerror();
						if
						(
							!stristr(e, "No such file or directory")
							&&
							!stristr(e, "image not found")
							&&
							!Quiet
						)
							LgiTrace("%s:%i - dlopen(%s) failed: %s\n", _FL, File, e);
						#endif

						#if ALLOW_FALLBACK_PATH
							GToken t("/opt/local/lib", ":");
							for (int i=0; i<t.Length(); i++)
							{
								char full[MAX_PATH];
								LgiMakePath(full, sizeof(full), t[i], f);
								if (FileExists(full))
								{
									hLib = dlopen(full, RTLD_NOW);
									#if DEBUG_LIB_MSGS
									if (!Quiet)
										LgiTrace("%s:%i - dlopen(%s)=%p\n", _FL, full, hLib);
									#endif
									if (hLib)
										break;
								}
								#if DEBUG_LIB_MSGS
								else if (!Quiet)
									LgiTrace("%s doesn't exist\n", full);
								#endif
							}
						#endif

						if (!hLib && !Quiet)
						{
							char *e = dlerror();
							if (!stristr(e, "No such file or directory"))
								LgiTrace("%s:%i - dlopen(%s) failed: %s\n", _FL, File, e);
						}
					}
				}
			}

			#endif
		}
	}

	return hLib != 0;
}

bool GLibrary::Unload()
{
	if (hLib)
	{
		#if defined WIN32
		FreeLibrary(hLib);
		#elif defined(LINUX) || defined(MAC)
		dlclose(hLib);
		#else
		LgiAssert(0);
		#endif
		hLib = 0;
	}

	DeleteArray(FileName);

	return true;
}

void *GLibrary::GetAddress(const char *Resource)
{
	void *p = NULL;
	
	if (Resource && hLib)
	{
		#if defined WIN32
		p = (void*) GetProcAddress(hLib, Resource);
		#elif defined(LINUX) || defined(MAC)
		p = dlsym(hLib, Resource);
		#else
		LgiAssert(0);
		#endif

		#if DEBUG_LIB_MSGS
		if (!p)
			LgiTrace("%s:%i - GetAddress('%s')=%p\n", _FL, Resource, p);
		#endif
	}

	return p;
}

#ifdef MAC
#import <dlfcn.h>
#import <mach-o/dyld.h>
#import <mach-o/nlist.h>
#import <stdio.h>
#import <string.h>

#ifdef __LP64__
typedef struct mach_header_64 mach_header_t;
typedef struct segment_command_64 segment_command_t;
typedef struct nlist_64 nlist_t;
#else
typedef struct mach_header mach_header_t;
typedef struct segment_command segment_command_t;
typedef struct nlist nlist_t;
#endif

static const char * first_external_symbol_for_image(const mach_header_t *header)
{
    Dl_info info;
    if (dladdr(header, &info) == 0)
        return NULL;

    segment_command_t *seg_linkedit = NULL;
    segment_command_t *seg_text = NULL;
    struct symtab_command *symtab = NULL;

    struct load_command *cmd = (struct load_command *)((intptr_t)header + sizeof(mach_header_t));
    for (uint32_t i = 0; i < header->ncmds; i++, cmd = (struct load_command *)((intptr_t)cmd + cmd->cmdsize))
    {
        switch(cmd->cmd)
        {
            case LC_SEGMENT:
            case LC_SEGMENT_64:
                if (!strcmp(((segment_command_t *)cmd)->segname, SEG_TEXT))
                    seg_text = (segment_command_t *)cmd;
                else if (!strcmp(((segment_command_t *)cmd)->segname, SEG_LINKEDIT))
                    seg_linkedit = (segment_command_t *)cmd;
                break;

            case LC_SYMTAB:
                symtab = (struct symtab_command *)cmd;
                break;
        }
    }

    if ((seg_text == NULL) || (seg_linkedit == NULL) || (symtab == NULL))
        return NULL;

    intptr_t file_slide = ((intptr_t)seg_linkedit->vmaddr - (intptr_t)seg_text->vmaddr) - seg_linkedit->fileoff;
    intptr_t strings = (intptr_t)header + (symtab->stroff + file_slide);
    nlist_t *sym = (nlist_t *)((intptr_t)header + (symtab->symoff + file_slide));

    for (uint32_t i = 0; i < symtab->nsyms; i++, sym++)
    {
        if ((sym->n_type & N_EXT) != N_EXT || !sym->n_value)
            continue;

        return (const char *)strings + sym->n_un.n_strx;
    }

    return NULL;
}

const char * pathname_for_handle(void *handle)
{
    for (int32_t i = _dyld_image_count(); i >= 0 ; i--)
    {
        const char *first_symbol = first_external_symbol_for_image((const mach_header_t *)_dyld_get_image_header(i));
        if (first_symbol && strlen(first_symbol) > 1)
        {
            handle = (void *)((intptr_t)handle | 1); // in order to trigger findExportedSymbol instead of findExportedSymbolInImageOrDependentImages. See `dlsym` implementation at http://opensource.apple.com/source/dyld/dyld-239.3/src/dyldAPIs.cpp
            first_symbol++; // in order to remove the leading underscore
            void *address = dlsym(handle, first_symbol);
            Dl_info info;
            if (dladdr(address, &info))
                return info.dli_fname;
        }
    }
    return NULL;
}
#elif defined(LINUX)
#include <link.h>
#endif

GString GLibrary::GetFullPath()
{
	#if defined(MAC)
		if (hLib)
			return pathname_for_handle(hLib);
	#elif defined(WINDOWS)
		if (hLib)
		{
			wchar_t File[MAX_PATH] = L"";
			GetModuleFileNameW(Handle(), File, sizeof(File));
			return File;
		}
	#elif defined(LINUX)
		if (hLib)
		{
			struct link_map *map = NULL;
			dlinfo(hLib, RTLD_DI_LINKMAP, &map);
			if (map)
				return realpath(map->l_name, NULL);
		}
	#else
		LgiAssert(!"Impl me.");
	#endif
	return NULL;
}
