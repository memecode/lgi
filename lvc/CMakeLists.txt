
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
    find_path(LIBSSH_INCLUDE_DIR libssh.h
        PATH /usr/include/libssh
        PATH /system/develop/headers/libssh) # this is the HAIKU location
    if (NOT LIBSSH_INCLUDE_DIR)
        message(FATAL_ERROR "libssh not found.")
    endif()
    add_library(ssh_shared INTERFACE)
    set_target_properties(ssh_shared
        PROPERTIES
            INTERFACE_INCLUDE_DIRECTORIES ${LIBSSH_INCLUDE_DIR}
            INTERFACE_LINK_LIBRARIES "ssh")
endif()

add_executable(Lvc ${APP_TYPE}
    ../src/common/Lgi/LgiMain.cpp
    ../src/common/Text/DeEscape.cpp
    ../src/common/Text/XmlTreeUi.cpp
    ../src/common/Widgets/ControlTree.cpp
    src/BrowseUi.cpp
    src/DropDownBtn.cpp
    src/Main.cpp
    src/SshConnection.cpp
    src/VcCommit.cpp
    src/VcFile.cpp
    src/VcFolder.cpp
    src/PatchViewer.cpp
	${APP_ICON}
    ${APP_RESOURCES}
	${APP_MANIFEST}
	)

target_link_libraries(Lvc lgi libpng16 ssh_shared)
target_include_directories(Lvc PUBLIC resources)

if (APPLE)
    macBundlePostBuild(Lvc)
else()
    file(COPY ${CMAKE_CURRENT_LIST_DIR}/resources DESTINATION ${CMAKE_CURRENT_BINARY_DIR})
endif()

set_target_properties(Lvc PROPERTIES EXCLUDE_FROM_ALL TRUE)