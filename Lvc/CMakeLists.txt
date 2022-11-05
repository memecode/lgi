
set(APP_RESOURCES
    Resources/Lvc.lr8
    Resources/image-list.png)
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
	set(APP_ICON Resources/icon128.png)
endif()

if (NOT TARGET ssh_shared)
    get_filename_component(LIBSSH_DIR "${CODELIB}/libssh" ABSOLUTE)
    add_subdirectory(${LIBSSH_DIR} libssh)
endif()
if (NOT TARGET libpng15)
    get_filename_component(LIBPNG_DIR "${CODELIB}/libpng" ABSOLUTE)
    add_subdirectory(${LIBPNG_DIR} libpng)
endif()

add_executable(Lvc ${APP_TYPE}
    ../src/common/Gdc2/Filters/Png.cpp
    ../src/common/Lgi/LgiMain.cpp
    ../src/common/Text/DeEscape.cpp
    ../src/common/Text/XmlTreeUi.cpp
    ../src/common/Widgets/ControlTree.cpp
    Src/BlameUi.cpp
    Src/DropDownBtn.cpp
    Src/Main.cpp
    Src/SshConnection.cpp
    Src/VcCommit.cpp
    Src/VcFile.cpp
    Src/VcFolder.cpp
	${APP_ICON}
    ${APP_RESOURCES}
	${APP_MANIFEST}
	)

target_link_libraries(Lvc lgi libpng15 ssh_shared)

if (APPLE)
    macBundlePostBuild(Lvc)
else()
    file(COPY ${CMAKE_CURRENT_LIST_DIR}/Resources DESTINATION ${CMAKE_CURRENT_BINARY_DIR})
endif()
