cmake_minimum_required (VERSION 3.18)

set(CMAKE_OSX_ARCHITECTURES "x86_64")
set(CMAKE_CXX_STANDARD 14)

project (LgiIde)

if (UNIX AND NOT APPLE)
	set(LINUX TRUE)
endif()

if (NOT TARGET libpng15)
    get_filename_component(LIBPNG_DIR "${CODELIB}/libpng" ABSOLUTE)
    add_subdirectory(${LIBPNG_DIR} libpng)
endif()

if (APPLE)

	set(GUI_TYPE MACOSX_BUNDLE)
	find_library(COCOA_LIBRARY Cocoa)
	mark_as_advanced(COCOA_LIBRARY)
	set(EXTRA_LIBS ${COCOA_LIBRARY})

elseif (LINUX)

	add_definitions("-fPIC -w -fno-inline -fpermissive")
	find_package(PNG REQUIRED)

elseif (MSVC)

	set(ResourceFiles
		../src/win/General/Lgi.manifest
		Resources/Script1.rc)
	if (NOT EXISTS ${OPENSSL_INCLUDE_DIR})
		message(FATAL_ERROR "OpenSSL not found: '${OPENSSL_INCLUDE_DIR}'")
	endif()
	if (NOT EXISTS ${CODELIB})
		message(FATAL_ERROR "Codelib not found: '${CODELIB}'")
	endif()
	
	if (${MSVC_VERSION} GREATER_EQUAL 1900)
		set(VS_VER 14)
	elseif (${MSVC_VERSION} EQUAL 1800)
		set(VS_VER 12)
	else()
		message(FATAL_ERROR "Unknown Visual Studio version: ${MSVC_VERSION}")
	endif()

endif ()

set (CommonSource
	Code/SimpleCppParser.cpp
	Code/WebFldDlg.cpp
	Code/IdeCommon.cpp
	Code/AddFtpFile.cpp
	Code/ProjectNode.cpp
	Code/IdeDoc.cpp
	Code/IdeProject.cpp
	Code/IdeProjectSettings.cpp
	Code/LgiIde.cpp
	Code/LgiUtils.cpp
	Code/MemDumpViewer.cpp
	Code/SpaceTabConv.cpp
	Code/SysCharSupport.cpp
	Code/DebugContext.cpp
	Code/Debugger.cpp
	Code/History.cpp
	Code/FindInFiles.cpp
	Code/FindSymbol.cpp
	Code/FtpThread.cpp
	Code/DocEdit.cpp
	Code/PythonParser.cpp
	Code/MissingFiles.cpp
	Code/DocEditStyling.cpp
	Code/levenshtein.c
	Code/JavascriptParser.cpp
	Code/NewProjectFromTemplate.cpp
	../src/common/Net/OpenSSLSocket.cpp
	../src/common/Net/Http.cpp
	../src/common/Net/Ftp.cpp
	../src/common/Gdc2/Filters/Png.cpp
	../src/common/Lgi/About.cpp
	../src/common/Lgi/Mdi.cpp
	../src/common/Lgi/LgiMain.cpp
	../src/common/Widgets/ControlTree.cpp
	../src/common/Coding/ParseCpp.cpp
	../src/common/Coding/LexCpp.cpp
	)

list(APPEND ResourceFiles
	Resources/cmds-32px.png
	Resources/icons.png
	Resources/cmds-16px.png
	Resources/LgiIde.lr8
	Resources/icon64.png
	)

if (APPLE)
	set(MacResourceFiles
		MacCocoa/Info.plist
		Resources/mac-icon.icns)
	set(APP_TYPE MACOSX_BUNDLE)
	set_source_files_properties(${MacResourceFiles} PROPERTIES MACOSX_PACKAGE_LOCATION "Resources")
	list(APPEND ResourceFiles ${MacResourceFiles})
elseif (MSVC)
	set(APP_TYPE WIN32)
endif ()

add_executable(LgiIde ${APP_TYPE} ${CommonSource} ${ResourceFiles})
target_link_libraries(LgiIde lgi libpng15)
target_include_directories(LgiIde PRIVATE Code Resources)

if (APPLE)

	target_link_libraries(LgiIde ${EXTRA_LIBS})
	set_target_properties(LgiIde PROPERTIES
		MACOSX_BUNDLE TRUE
		MACOSX_FRAMEWORK_IDENTIFIER com.memecode.LgiIde
		RESOURCE "${ResourceFiles}"
		)
	message("macBundlePostBuild(LgiIde)")
	macBundlePostBuild(LgiIde)

elseif (MSVC)

	target_link_libraries(LgiIde ComCtl32.lib Ws2_32.lib UxTheme.lib imm32.lib)
	add_custom_command(TARGET LgiIde POST_BUILD 
					COMMAND "${CMAKE_COMMAND}" -E copy 
						"$<TARGET_FILE:lgi>"
						"$<TARGET_FILE_DIR:LgiIde>/$<TARGET_FILE_NAME:lgi>" 
					COMMENT "Copying Lgi to output directory")
	if (EXISTS ${PNG_DLL})
		add_custom_command(TARGET LgiIde POST_BUILD 
					COMMAND "${CMAKE_COMMAND}" -E copy 
						"${PNG_DLL}"
						"$<TARGET_FILE_DIR:LgiIde>/libpng${VS_VER}x${WORDSIZE}.dll" 
					COMMENT "Copying Lgi to output directory")
	endif()

endif ()

# Create a resources sub-folder and copy over the resource files (images and lr8)
file(COPY ${CMAKE_CURRENT_LIST_DIR}/Resources DESTINATION ${CMAKE_CURRENT_BINARY_DIR})

target_include_directories (LgiIde PUBLIC
							Code
							"Resources"
							${PNG_INCLUDE_DIR}
							${OPENSSL_INCLUDE_DIR}
							"${CODELIB}/libiconv-1.14/include")
