

add_library(ntlm SHARED
	../../Hash/md5/md5.c
	lib/check-version.c
	lib/des.c
	lib/dummy.c
	lib/malloc.c
	lib/md4.c
	lib/random.c
	lib/strverscmp.c
	smbencrypt.c
	smbutil.c
	lib/byteswap.h
	lib/check-version.h
	lib/des.h
	lib/md4.h
	lib/strverscmp.h
	ntlm.h
	)

target_include_directories(ntlm
	PRIVATE
		.
		lib
		../../../../include)

target_compile_definitions(ntlm
	PRIVATE
		LGI_LIBRARY
		LIBNTLM_EXPORTS)

if (WIN32)

	target_sources(ntlm
		PRIVATE
			build-win32/stdbool.h
			build-win32/_stdint.h)


	target_include_directories(ntlm
		PRIVATE
			build-win32)

	target_compile_definitions(ntlm
		PUBLIC
			WINDOWS)

else()

	target_sources(ntlm
		PRIVATE
			lib/strdup.c)
			
endif()