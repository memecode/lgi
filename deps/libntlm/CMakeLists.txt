

set(SRC "${DEPS}/libntlm")
set(PACKAGE_VERSION "1.8")

# configure all the dependant headers and source:
file(GLOB HEADERS ${CMAKE_CURRENT_LIST_DIR}/*.h)
foreach(HDR ${HEADERS})
    get_filename_component(LEAF ${HDR} NAME)
    configure_file(${HDR} ${CMAKE_CURRENT_BINARY_DIR}/${LEAF})
endforeach()

file(GLOB SOURCES ${CMAKE_CURRENT_LIST_DIR}/*.c)
foreach(SRC ${SOURCES})
    get_filename_component(LEAF ${SRC} NAME)
    configure_file(${SRC} ${CMAKE_CURRENT_BINARY_DIR}/${LEAF})
endforeach()

configure_file(${SRC}/ntlm.h.in ${CMAKE_CURRENT_BINARY_DIR}/ntlm.h)

# create the library
if(WIN32)
    set(LIBNAME libntlm)
else()
    set(LIBNAME ntlm)
endif()
add_library(${LIBNAME} SHARED
    ${SRC}/smbutil.c
    ${SRC}/smbencrypt.c
    ${CMAKE_CURRENT_BINARY_DIR}/md4.c
    ${CMAKE_CURRENT_BINARY_DIR}/des.c)
target_include_directories(${LIBNAME}
    PRIVATE
        ${CMAKE_CURRENT_BINARY_DIR})
target_compile_definitions(${LIBNAME} PRIVATE PACKAGE_STRING="1.14")

# installation
install(TARGETS ${LIBNAME}
    DESTINATION ${CMAKE_BINARY_DIR}/lib)
install(FILES
    ${CMAKE_CURRENT_BINARY_DIR}/ntlm.h
    DESTINATION ${CMAKE_BINARY_DIR}/include)