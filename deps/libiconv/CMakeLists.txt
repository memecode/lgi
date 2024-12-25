# libiconv isn't a cmake project... but this file wraps it up as one

set(SRC "${DEPS}/libiconv")
if(NOT EXISTS ${SRC})
    message(FATAL_ERROR "Expecting libiconv checkout at: ${SRC}")
endif()
message("libiconv SRC=${SRC}")

# include: .\..\..\source;.\..\..\source\lib;.\..\..\source\include
# defs: BUILDING_LIBICONV;BUILDING_DLL;PIC;HAVE_CONFIG_H;ENABLE_RELOCATABLE=1;
#   IN_LIBRARY;INSTALLPREFIX="c:\\\\program files\\iconv";
#   INSTALLDIR="c:\\\\program files\\iconv";
#   NO_XMALLOC;set_relocation_prefix=libiconv_set_relocation_prefix;
#   relocate=libiconv_relocate;
#   LIBDIR="c:\\\\program files\\iconv";
#   _CRT_SECURE_NO_WARNINGS
# source\lib\iconv.c
# source\libcharset\lib\localcharset.c
# source\lib\relocatable.c

# common headers and defs
configure_file(${SRC}/source/srclib/uniwidth.in.h ${CMAKE_CURRENT_BINARY_DIR}/include/uniwidth.h)
configure_file(${SRC}/source/srclib/unitypes.in.h ${CMAKE_CURRENT_BINARY_DIR}/include/unitypes.h)

set(DLL_VARIABLE "")
set(EILSEQ "B_TO_POSIX_ERROR(B_POSIX_ERROR_BASE + 38)")
set(USE_MBSTATE_T "0")
set(BROKEN_WCHAR_H "0")
set(HAVE_WCHAR_T "1")

# library
configure_file(${CMAKE_CURRENT_LIST_DIR}/source_config.h ${CMAKE_CURRENT_BINARY_DIR}/lib/config.h)
configure_file(${SRC}/source/include/iconv.h.in ${CMAKE_CURRENT_BINARY_DIR}/lib/iconv.h)
if(WIN32)
    configure_file(${CMAKE_CURRENT_LIST_DIR}/unistd.h ${CMAKE_CURRENT_BINARY_DIR}/lib/unistd.h)
endif()
add_library(libiconv SHARED
    ${SRC}/source/lib/iconv.c
    ${SRC}/source/libcharset/lib/localcharset.c
    ${SRC}/source/lib/relocatable.c
    )
target_include_directories(libiconv
    PUBLIC
        ${SRC}/source
        ${SRC}/source/lib
        ${SRC}/source/include
    PRIVATE
        ${CMAKE_CURRENT_BINARY_DIR}/lib
        ${CMAKE_CURRENT_BINARY_DIR}/include)
target_compile_definitions(libiconv
    PUBLIC
        BUILDING_DLL
    PRIVATE
        BUILDING_LIBICONV
        PIC
        HAVE_CONFIG_H
        ENABLE_RELOCATABLE=1
        IN_LIBRARY
        NO_XMALLOC
        _CRT_SECURE_NO_WARNINGS
        set_relocation_prefix=libiconv_set_relocation_prefix
        relocate=libiconv_relocate)
if(HAIKU)
    target_compile_options(iconv_srclib
        PRIVATE
            -Wno-pointer-to-int-cast)
endif()
    

install(TARGETS libiconv
    DESTINATION ${CMAKE_BINARY_DIR}/lib)
install(FILES
    ${SRC}/source/include/iconv.h
    DESTINATION ${CMAKE_BINARY_DIR}/include)