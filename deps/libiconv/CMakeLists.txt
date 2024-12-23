# libiconv isn't a cmake project... but this file wraps it up as one

set(SRC "${DEPS}/libiconv")
if(NOT EXISTS ${SRC})
    message(FATAL_ERROR "Expecting libiconv checkout at: ${SRC}")
endif()
message("libiconv SRC=${SRC}")

# common headers
configure_file(${SRC}/source/srclib/uniwidth.in.h ${CMAKE_CURRENT_BINARY_DIR}/include/uniwidth.h)
configure_file(${SRC}/source/srclib/unitypes.in.h ${CMAKE_CURRENT_BINARY_DIR}/include/unitypes.h)

# library
configure_file(${CMAKE_CURRENT_LIST_DIR}/source_config.h ${CMAKE_CURRENT_BINARY_DIR}/lib/config.h)
#configure_file(${CMAKE_CURRENT_LIST_DIR}/config.h ${CMAKE_CURRENT_BINARY_DIR}/lib/config.h)
configure_file(${CMAKE_CURRENT_LIST_DIR}/unistd.h ${CMAKE_CURRENT_BINARY_DIR}/lib/unistd.h)
add_library(iconv_srclib STATIC
    ${SRC}/source/lib/iconv.c
    ${SRC}/source/srclib/progname.c
    ${SRC}/source/srclib/progreloc.c
    ${SRC}/source/srclib/safe-read.c
    ${SRC}/source/srclib/uniwidth/width.c
    ${SRC}/source/libcharset/lib/localcharset.c)
target_include_directories(iconv_srclib
    PUBLIC
        ${SRC}/source/include
        ${SRC}/source/srclib
    PRIVATE
        ${CMAKE_CURRENT_BINARY_DIR}/lib
        ${CMAKE_CURRENT_BINARY_DIR}/include)
target_compile_definitions(iconv_srclib
    PUBLIC
        NO_I18N
        LIBICONV_DLL
    PRIVATE
        BUILDING_LIBICONV)
    
# iconv itself
configure_file(${SRC}/source/config.h.in ${CMAKE_CURRENT_BINARY_DIR}/iconv/config.h)
add_library(libiconv SHARED
    ${SRC}/source/src/iconv.c
    )
target_compile_definitions(libiconv
    PUBLIC
        NO_I18N
        LIBICONV_DLL
    PRIVATE
        BUILDING_LIBICONV)
target_include_directories(libiconv
    PUBLIC
        ${SRC}/source/include
        ${SRC}/source/srclib
    PRIVATE
        ${CMAKE_CURRENT_BINARY_DIR}/iconv
        ${CMAKE_CURRENT_BINARY_DIR}/include)
target_link_libraries(libiconv PRIVATE iconv_srclib)
install(TARGETS libiconv
    DESTINATION ${CMAKE_BINARY_DIR}/lib)
install(FILES
    ${SRC}/source/include/iconv.h
    DESTINATION ${CMAKE_BINARY_DIR}/include)