cmake_minimum_required(VERSION 3.20)

set(CMAKE_POLICY_DEFAULT_CMP0077 NEW)
project(lgiDeps)
enable_language( C CXX ASM )

get_filename_component(DEPS "${CMAKE_CURRENT_LIST_DIR}/../../deps" ABSOLUTE)
if(HAIKU OR APPLE)
    # I'm not sure why this needs to be here, but it gets libjpeg to build... so... 'eh'
    set(WITH_SIMD FALSE)
endif()

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
	    set(CMAKE_OSX_ARCHITECTURES "arm64;x86_64")
    else()
	    set(CMAKE_OSX_ARCHITECTURES "x86_64")
    endif()
    message("XCODE_VER=${XCODE_VER} CMAKE_OSX_ARCHITECTURES=${CMAKE_OSX_ARCHITECTURES}")

    set(CMAKE_OSX_DEPLOYMENT_TARGET "10.15" CACHE STRING "Minimum OS X deployment version")
    string(REPLACE ";" "$<SEMICOLON>" CMAKE_OSX_ARCHITECTURES_ "${CMAKE_OSX_ARCHITECTURES}")
    list(APPEND EXTRA_ARGS "-DCMAKE_OSX_ARCHITECTURES=${CMAKE_OSX_ARCHITECTURES_}")
endif()

function(GitProject NAME URI BRANCH)

    set(DIR "${DEPS}/${NAME}")
    message(STATUS "${NAME} = ${DIR}")
    if(NOT EXISTS ${DIR})
        execute_process(COMMAND git clone -b ${BRANCH} ${URI} ${DIR}
            RESULT_VARIABLE clone_result
            OUTPUT_VARIABLE clone_out)
        if(${clone_result})
            message(FATAL_ERROR "${NAME} clone failed: ${clone_out}")
        endif()
    endif()

    set(LISTS "${DIR}/CMakeLists.txt")
    if(EXISTS ${LISTS})
        add_subdirectory(${DIR} ${NAME})
    else()
        message(STATUS "${NAME} is not a cmake project")
    endif()

endfunction()

# libjpeg
if(APPLE)
    set(OSX_ARCHITECTURES ${CMAKE_OSX_ARCHITECTURES})
    set(CMAKE_OSX_ARCHITECTURES "x86_64")
endif()
GitProject(libjpeg "https://github.com/memecode/libjpeg.git" "memecode")
if(APPLE)
    set(CMAKE_OSX_ARCHITECTURES ${OSX_ARCHITECTURES})
endif()
install(TARGETS jpeg
        DESTINATION ${CMAKE_BINARY_DIR}/lib)

# zlib
GitProject(zlib    "https://github.com/madler/zlib.git" "develop")
install(TARGETS zlib
        DESTINATION ${CMAKE_BINARY_DIR}/lib)

# libpng
set(PNG_HARDWARE_OPTIMIZATIONS 0)
set(PNG_ARM_NEON off)
set(PNG_STATIC off)
set(PNG_SHARED on)
set(PNG_BUILD_ZLIB OFF)
set(ZLIB_INCLUDE_DIR
    ${DEPS}/zlib
    ${CMAKE_BINARY_DIR}/zlib)
GitProject(libpng  "https://github.com/memecode/libpng.git" "libpng16")
install(TARGETS png_shared
        DESTINATION ${CMAKE_BINARY_DIR}/lib)

# lunasvg
GitProject(lunasvg "https://github.com/sammycage/lunasvg.git" "master")

# libiconv
GitProject(libiconv "https://github.com/winlibs/libiconv.git" "master")
add_subdirectory(${CMAKE_CURRENT_LIST_DIR}/libiconv)

# libntlm
GitProject(libntlm "https://gitlab.com/gsasl/libntlm.git" "master")
add_subdirectory(${CMAKE_CURRENT_LIST_DIR}/libntlm)
