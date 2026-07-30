
set(APP_RESOURCES
    resources/Lvc.lr8
    resources/image-list.png)
set(APP_MANIFEST)
if (WIN32)
	set(APP_TYPE WIN32)
	set(APP_ICON resources/Lvc.rc)
	set(APP_MANIFEST ../src/win/General/Lgi.manifest)
elseif (APPLE)
	set(APP_TYPE MACOSX_BUNDLE)
	set(APP_ICON Resources/mac-icon.icns)
	set_source_files_properties(
        ${APP_ICON}
        ${APP_RESOURCES}
		PROPERTIES
			MACOSX_PACKAGE_LOCATION "Resources")
elseif (LINUX)
	set(APP_ICON resources/icon128.png)
endif()

if (NOT TARGET ssh_shared)
    find_path(LIBSSH_HEADER libssh.h
        PATH /usr/include/libssh                # this is the Linux location
        PATH /system/develop/headers/libssh     # this is the Haiku location
        PATH ${LGI_DEPS_BUILD}/include/libssh)  # this is the Windows location
    if (NOT LIBSSH_HEADER)
        message(FATAL_ERROR "libssh not found.")
    endif()
    add_library(ssh_shared INTERFACE)
    cmake_path( GET LIBSSH_HEADER
                PARENT_PATH LIBSSH_INCLUDE_DIR)
    set_target_properties(ssh_shared
        PROPERTIES
            INTERFACE_INCLUDE_DIRECTORIES ${LIBSSH_INCLUDE_DIR}
            INTERFACE_LINK_LIBRARIES "ssh")
    # message("LIBSSH_INCLUDE_DIR: ${LIBSSH_INCLUDE_DIR}")
endif()

add_executable(Lvc
    ${APP_TYPE}
        src/BrowseUi.cpp
        src/DropDownBtn.cpp
        src/Main.cpp
        src/SshConnection.cpp
        src/VcCommit.cpp
        src/VcFile.cpp
        src/VcFolder.cpp
        src/PatchViewer.cpp
        
        ${LGI_DIR}/src/common/Net/Ssh.cpp
        ${LGI_DIR}/src/common/Text/RemoveAnsi.cpp
        ${LGI_DIR}/src/common/Net/Http.cpp
        ${LGI_DIR}/src/common/Lgi/LgiMain.cpp
        ${LGI_DIR}/src/common/Text/DeEscape.cpp
        ${LGI_DIR}/src/common/Text/XmlTreeUi.cpp
        ${LGI_DIR}/src/common/Widgets/ControlTree.cpp
        ${LGI_DIR}/src/common/Net/OpenSSLSocket.cpp
        
        ${APP_ICON}
        ${APP_RESOURCES}
        ${APP_MANIFEST})

target_link_libraries(Lvc
    PRIVATE
        lgi
        ssh_shared)

target_include_directories(Lvc
    PUBLIC
        resources)
        
target_compile_definitions(Lvc
    PRIVATE
        HAS_LIBSSH=1)

if(WIN32)
    target_link_directories(Lvc
		PRIVATE
			${LGI_DEPS_BUILD}/lib
			"C:\\Program Files\\OpenSSL-Win64\\lib")
endif()

if (APPLE)
    macBundlePostBuild(Lvc)
else()
    file(COPY ${CMAKE_CURRENT_LIST_DIR}/resources DESTINATION ${CMAKE_CURRENT_BINARY_DIR})
endif()

set_target_properties(Lvc PROPERTIES EXCLUDE_FROM_ALL TRUE)