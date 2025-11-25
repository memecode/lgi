cmake_minimum_required(VERSION 3.20)

set(CMAKE_POLICY_DEFAULT_CMP0077 NEW)
project(lgiDeps)
enable_language( C CXX ASM )
if(UNIX AND NOT APPLE)
    set(LINUX TRUE)
endif()

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

function(GitProject NAME URI BRANCH ADDTARGET)

    set(DIR "${DEPS}/${NAME}")
    message(STATUS "${NAME} = ${DIR}")
    if(NOT EXISTS ${DIR})
        execute_process(COMMAND git clone --recurse-submodules -b ${BRANCH} ${URI} ${DIR}
            RESULT_VARIABLE clone_result
            OUTPUT_VARIABLE clone_out)
        if(NOT ${clone_result} MATCHES "^[0-9]+$" OR ${clone_result})
            message("git clone --recurse-submodules -b ${BRANCH} ${URI} ${DIR}")
            message(FATAL_ERROR "${NAME} clone failed: ${clone_result} ${clone_out}")
        endif()
    endif()

    if(${ADDTARGET})
        set(LISTS "${DIR}/CMakeLists.txt")
        message("adding cmake: ${DIR}/CMakeLists.txt")
        if(EXISTS ${LISTS})
            add_subdirectory(${DIR} ${NAME})
        else()
            message(STATUS "${NAME} is not a cmake project")
        endif()
    endif()

endfunction()

set(LIB_FOLDER ${CMAKE_BINARY_DIR}/lib)
file(MAKE_DIRECTORY ${LIB_FOLDER})

if(WIN32)
    # '/FS' - Forces writes to the PDB file to be serialized through MSPDBSRV.EXE.
    add_compile_options(/FS)
endif()

if (1)
    # libjpeg
    if(APPLE)
        set(ADD_JPEG_TARGET 0)
    else()
        set(ADD_JPEG_TARGET 1)
    endif()
    GitProject(libjpeg "https://github.com/memecode/libjpeg.git" "memecode" ADD_JPEG_TARGET)
    if(APPLE)
        
        # this is a hack to build each architecture of libjpeg separately and then combine them into a universal dylib
        set(ARCHS ${CMAKE_OSX_ARCHITECTURES})
        set(LIBJPEG_DYLIB "libjpeg.62.4.0.dylib")

        foreach(ARCH ${ARCHS})
            set(JPEG_BIN ${CMAKE_BINARY_DIR}/jpeg-${ARCH})
            message("JPEG_BIN=${JPEG_BIN}")
            file(MAKE_DIRECTORY ${JPEG_BIN})
            execute_process(COMMAND cmake -G Ninja -DCMAKE_OSX_ARCHITECTURES=${ARCH} ${DEPS}/libjpeg
                WORKING_DIRECTORY ${JPEG_BIN}
                RESULT_VARIABLE ConfigureResult
                COMMAND_ERROR_IS_FATAL ANY)
            message("ConfigureResult=${ConfigureResult}")
            execute_process(COMMAND ninja
                WORKING_DIRECTORY ${JPEG_BIN}
                RESULT_VARIABLE BuildResult
                COMMAND_ERROR_IS_FATAL ANY)
            message("BuildResult=${BuildResult}")
        endforeach()

        # create a universal binary out of the per-architecture builds:
        execute_process(COMMAND lipo
                ${CMAKE_BINARY_DIR}/jpeg-arm64/${LIBJPEG_DYLIB}
                ${CMAKE_BINARY_DIR}/jpeg-x86_64/${LIBJPEG_DYLIB}
                -output ${LIB_FOLDER}/${LIBJPEG_DYLIB}
                -create
            RESULT_VARIABLE LipoResult
            COMMAND_ERROR_IS_FATAL ANY)
        message("LipoResult=${LipoResult}")

        # create some symlinks for them:
        file(CREATE_LINK ./${LIBJPEG_DYLIB} ${LIB_FOLDER}/libjpeg.62.dylib  SYMBOLIC)
        file(CREATE_LINK ./libjpeg.62.dylib ${LIB_FOLDER}/libjpeg.dylib SYMBOLIC)

    else()
        install(TARGETS jpeg
            RUNTIME
                DESTINATION ${CMAKE_BINARY_DIR}/bin
            LIBRARY
                DESTINATION ${CMAKE_BINARY_DIR}/lib)

    endif()
endif()

# zlib
GitProject(zlib "https://github.com/madler/zlib.git" "develop" TRUE)
install(TARGETS zlib
    RUNTIME
        DESTINATION ${CMAKE_BINARY_DIR}/bin
    LIBRARY
        DESTINATION ${CMAKE_BINARY_DIR}/lib)
# fix the output name
set_target_properties(
    zlib
    PROPERTIES OUTPUT_NAME zlib)
if(LINUX)
    target_compile_options(zlibstatic PRIVATE -fPIC)
endif()

# libpng
set(PNG_HARDWARE_OPTIMIZATIONS 0)
set(PNG_ARM_NEON off)
set(PNG_STATIC off)
set(PNG_SHARED on)
set(PNG_BUILD_ZLIB OFF)
set(ZLIB_INCLUDE_DIR
    ${DEPS}/zlib
    ${CMAKE_BINARY_DIR}/zlib)
GitProject(libpng  "https://github.com/memecode/libpng.git" "libpng16" TRUE)
install(TARGETS png_shared
    RUNTIME
        DESTINATION ${CMAKE_BINARY_DIR}/bin
    LIBRARY
        DESTINATION ${CMAKE_BINARY_DIR}/lib)

if (1)
    # lunasvg
    GitProject(lunasvg "https://github.com/sammycage/lunasvg.git" "master" TRUE)
    if(LINUX)
        target_compile_options(lunasvg PRIVATE -fPIC)
        target_compile_options(plutovg PRIVATE -fPIC)
    endif()

    # libiconv
    GitProject(libiconv "https://github.com/winlibs/libiconv.git" "master" TRUE)
    add_subdirectory(${CMAKE_CURRENT_LIST_DIR}/libiconv)

    # libntlm
    GitProject(libntlm "https://gitlab.com/gsasl/libntlm.git" "master" TRUE)
    add_subdirectory(${CMAKE_CURRENT_LIST_DIR}/libntlm)
endif()