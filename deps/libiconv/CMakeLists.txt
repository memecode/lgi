# libiconv isn't a cmake project... but this file wraps it up as one

set(SRC "${DEPS}/libiconv")
if(NOT EXISTS ${SRC})
    message(FATAL_ERROR "Expecting libiconv checkout at: ${SRC}")
endif()
message("libiconv SRC=${SRC}")

# common headers and defs
configure_file(${SRC}/source/srclib/uniwidth.in.h ${CMAKE_CURRENT_BINARY_DIR}/include/uniwidth.h)
configure_file(${SRC}/source/srclib/unitypes.in.h ${CMAKE_CURRENT_BINARY_DIR}/include/unitypes.h)

set(DLL_VARIABLE "")
set(EILSEQ "B_TO_POSIX_ERROR(B_POSIX_ERROR_BASE + 38)")
set(USE_MBSTATE_T "0")
set(BROKEN_WCHAR_H "0")
set(HAVE_WCHAR_T "1")

if(WIN32)
    set(PUBLIC_DEFS BUILDING_DLL)
    set(PRIVATE_DEFS _CRT_SECURE_NO_WARNINGS)
else()
    set(PUBLIC_DEFS)
    set(PRIVATE_DEFS)
endif()

# library
configure_file(${CMAKE_CURRENT_LIST_DIR}/source_config.h ${CMAKE_CURRENT_BINARY_DIR}/include/config.h)
configure_file(${SRC}/source/include/iconv.h.in ${CMAKE_CURRENT_BINARY_DIR}/include/iconv.h)
if(WIN32)
    configure_file(${CMAKE_CURRENT_LIST_DIR}/unistd.h ${CMAKE_CURRENT_BINARY_DIR}/include/unistd.h)
    set(LIBNAME libiconv)
else()    
    set(LIBNAME iconv)
endif()
add_library(${LIBNAME} SHARED
    ${SRC}/source/lib/iconv.c
    ${SRC}/source/libcharset/lib/localcharset.c
    ${SRC}/source/lib/relocatable.c
    )
target_include_directories(${LIBNAME}
    BEFORE PRIVATE
        ${CMAKE_CURRENT_BINARY_DIR}/include)
target_include_directories(${LIBNAME}
    PUBLIC
        ${SRC}/source
        ${SRC}/source/lib
        # ${SRC}/source/include
        )
target_compile_definitions(${LIBNAME}
    PUBLIC
        ${PUBLIC_DEFS}
    PRIVATE
        BUILDING_LIBICONV
        PIC
        HAVE_CONFIG_H
        ENABLE_RELOCATABLE=1
        IN_LIBRARY
        NO_XMALLOC        
        set_relocation_prefix=libiconv_set_relocation_prefix
        relocate=libiconv_relocate
        ${PRIVATE_DEFS})
if(HAIKU)
    target_compile_options(${LIBNAME}
        PRIVATE
            -Wno-pointer-to-int-cast)
endif()
    
install(TARGETS ${LIBNAME}
    RUNTIME
        DESTINATION ${CMAKE_BINARY_DIR}/bin
    LIBRARY
        DESTINATION ${CMAKE_BINARY_DIR}/lib)
install(FILES
    ${SRC}/source/include/iconv.h
    DESTINATION ${CMAKE_BINARY_DIR}/include)