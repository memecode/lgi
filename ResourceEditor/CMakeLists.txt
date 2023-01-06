

set(APP_RESOURCE Resources/lgires.lr8)
set(APP_MANIFEST)
if (WIN32)
	set(APP_TYPE WIN32)
	set(APP_ICON resources/Script1.rc)
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

add_executable(LgiRes ${APP_TYPE}

	Code/LgiResEdit.h
	Code/LgiRes_Dialog.h
	Code/LgiRes_Menu.h
	Code/LgiRes_String.h

	Code/LgiResApp.cpp
	Code/LgiRes_ControlTree.cpp
	Code/LgiRes_Css.cpp
	Code/LgiRes_Dialog.cpp
	Code/LgiRes_Menu.cpp
	Code/LgiRes_String.cpp
	Code/LgiRes_TableLayout.cpp
	Code/Search.cpp
	Code/ShowLanguages.cpp

	${LGI_DIR}/src/common/Gdc2/Filters/Gif.cpp
	${LGI_DIR}/src/common/Gdc2/Filters/Lzw.cpp
	${LGI_DIR}/src/common/Gdc2/Filters/Png.cpp
	${LGI_DIR}/src/common/Lgi/About.cpp
	${LGI_DIR}/src/common/Lgi/DocApp.cpp
	${LGI_DIR}/src/common/Lgi/LgiMain.cpp

	Resources/resdefs.h
	Resources/resource.h
	${APP_ICON})

target_include_directories(LgiRes PRIVATE Code Resources)
target_link_libraries(LgiRes lgi libpng16)

if (APPLE)
    macBundlePostBuild(LgiRes)
else()
	file(COPY ${CMAKE_CURRENT_LIST_DIR}/Resources DESTINATION ${CMAKE_CURRENT_BINARY_DIR})
endif()

set_target_properties(LgiRes PROPERTIES EXCLUDE_FROM_ALL TRUE)
