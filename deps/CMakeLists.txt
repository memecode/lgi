cmake_minimum_required(VERSION 3.20)

set(CMAKE_POLICY_DEFAULT_CMP0077 NEW)
project(lgiDeps)

get_filename_component(DEPS "${CMAKE_CURRENT_LIST_DIR}/../../deps" ABSOLUTE)

set(EXTRA_ARGS
    -DBUILD_SHARED_LIBS=ON
    -DCMAKE_INSTALL_PREFIX=${CMAKE_BINARY_DIR}
    )

if (APPLE)
    execute_process(COMMAND xcodebuild -version
        OUTPUT_VARIABLE XcodeOut)
    separate_arguments(XcodeParts UNIX_COMMAND ${XcodeOut})
    list(GET XcodeParts 1 XCODE_VER)
    if (${XCODE_VER} VERSION_GREATER_EQUAL "13.0")
	    set(CMAKE_OSX_ARCHITECTURES "x86_64;arm64")
    else()
	    set(CMAKE_OSX_ARCHITECTURES "x86_64")
    endif()
    message("XCODE_VER=${XCODE_VER} CMAKE_OSX_ARCHITECTURES=${CMAKE_OSX_ARCHITECTURES}")

    set(CMAKE_OSX_DEPLOYMENT_TARGET "10.15" CACHE STRING "Minimum OS X deployment version")
    string(REPLACE ";" "$<SEMICOLON>" CMAKE_OSX_ARCHITECTURES_ "${CMAKE_OSX_ARCHITECTURES}")
    list(APPEND EXTRA_ARGS "-DCMAKE_OSX_ARCHITECTURES=${CMAKE_OSX_ARCHITECTURES_}")
endif()

function(GitProject NAME URI BRANCH)

    set(BASE "${DEPS}/${NAME}")
    set(SRC "${BASE}/src")
    message(STATUS "${NAME} = ${SRC}")
    if(NOT EXISTS ${SRC})
        execute_process(COMMAND git clone -b ${BRANCH} ${URI} ${SRC}
            RESULT_VARIABLE clone_result
            OUTPUT_VARIABLE clone_out)
        if(${clone_result})
            message(FATAL_ERROR "${NAME} clone failed: ${clone_out}")
        endif()
    endif()

    add_subdirectory(${SRC} ${NAME})

endfunction()

GitProject(libjpeg "https://github.com/memecode/libjpeg.git" "memecode")
install(TARGETS jpeg
        DESTINATION ${CMAKE_BINARY_DIR}/lib)

GitProject(zlib    "https://github.com/madler/zlib.git" "develop")
install(TARGETS zlib
        DESTINATION ${CMAKE_BINARY_DIR}/lib)

set(PNG_ARM_NEON OFF)
set(PNG_STATIC OFF)
set(PNG_SHARED ON)
set(PNG_BUILD_ZLIB OFF)
set(ZLIB_INCLUDE_DIR
    ${DEPS}/zlib/src
    ${CMAKE_BINARY_DIR}/zlib)

GitProject(libpng  "https://github.com/memecode/libpng.git" "libpng16")
install(TARGETS png_shared
        DESTINATION ${CMAKE_BINARY_DIR}/lib)


#    INSTALL_COMMAND mkdir -p ${CMAKE_BINARY_DIR}/lib
#        COMMAND cp ${LIBJPEG_OUT} ${CMAKE_BINARY_DIR}/lib/${LIBJPEG_OUT}
#        COMMAND ln -sf ./${LIBJPEG_OUT} ${CMAKE_BINARY_DIR}/lib/libjpeg.1.dylib
#        COMMAND ln -sf ./libjpeg.1.dylib ${CMAKE_BINARY_DIR}/lib/libjpeg.dylib)
# add install rules for libjpeg

# FIXME: lunasvg

# FIXME: libiconv

# FIXME: libntlm
