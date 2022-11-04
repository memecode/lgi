
set(APP_RESOURCE Resources/Lvc.lr8)
set(APP_MANIFEST)
if (WIN32)
	set(APP_TYPE WIN32)
	set(APP_ICON resources/Lvc.rc)
	set(APP_MANIFEST ../src/win/General/Lgi.manifest)
elseif (APPLE)
	set(APP_TYPE MACOSX_BUNDLE)
	set(APP_ICON Resources/mac-icon.icns)
	set_source_files_properties(${APP_ICON} ${APP_RESOURCE}
		PROPERTIES
			MACOSX_PACKAGE_LOCATION "Resources")
elseif (LINUX)
	set(APP_ICON Resources/icon128.png)
endif()

get_filename_component(LIBSSH_DIR "${CODELIB}/libssh" ABSOLUTE)
add_subdirectory(${LIBSSH_DIR} libssh)

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
	${APP_MANIFEST}
	)

target_link_libraries(Lvc lgi libpng15 ssh_shared)

file(COPY ${CMAKE_CURRENT_LIST_DIR}/Resources DESTINATION ${CMAKE_CURRENT_BINARY_DIR})
