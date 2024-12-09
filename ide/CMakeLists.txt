cmake_minimum_required (VERSION 3.18)

set(CMAKE_CXX_STANDARD 14)

project (LgiIde)

if (NOT DEFINED LIBPNG_TARGET OR NOT TARGET ${LIBPNG_TARGET})
	message(FATAL_ERROR "Find the libpng target (see parent folder cmake)")
endif()

if (APPLE)

	set(CMAKE_OSX_ARCHITECTURES "x86_64")
	set(GUI_TYPE MACOSX_BUNDLE)
	find_library(COCOA_LIBRARY Cocoa)
	mark_as_advanced(COCOA_LIBRARY)
	set(EXTRA_LIBS ${COCOA_LIBRARY})

elseif (LINUX)

	add_definitions("-fPIC -w -fno-inline -fpermissive")

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

if (NOT MSVC)
	find_package(ZLIB REQUIRED)
	set(ZLIB_TARGET ZLIB::ZLIB)
else()
	set(ZLIB_TARGET zlib)
endif()

set (CommonSource
	src/SimpleCppParser.cpp
	src/WebFldDlg.cpp
	src/IdeCommon.cpp
	src/AddFtpFile.cpp
	src/ProjectNode.cpp
	src/IdeDoc.cpp
	src/IdeProject.cpp
	src/IdeProjectSettings.cpp
	src/LgiIde.cpp
	src/LgiUtils.cpp
	src/MemDumpViewer.cpp
	src/SpaceTabConv.cpp
	src/SysCharSupport.cpp
	src/DebugContext.cpp
	src/Debugger.cpp
	src/History.cpp
	src/IdeFindInFiles.cpp
	src/FindSymbol.cpp
	src/FtpThread.cpp
	src/DocEdit.cpp
	src/PythonParser.cpp
	src/MissingFiles.cpp
	src/DocEditStyling.cpp
	src/JavascriptParser.cpp
	src/NewProjectFromTemplate.cpp
	../src/common/Net/OpenSSLSocket.cpp
	../src/common/Net/Http.cpp
	../src/common/Net/Ftp.cpp
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
		mac/Info.plist
		Resources/mac-icon.icns)
	set(APP_TYPE MACOSX_BUNDLE)
	set_source_files_properties(${MacResourceFiles} PROPERTIES MACOSX_PACKAGE_LOCATION "Resources")
	list(APPEND ResourceFiles ${MacResourceFiles})

elseif (MSVC)

	set(APP_TYPE WIN32)

endif ()

add_executable(LgiIde ${APP_TYPE} ${CommonSource} ${ResourceFiles})
target_link_libraries(LgiIde lgi ${LIBPNG_TARGET} ${ZLIB_TARGET})
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

if (NOT MSVC)
	target_compile_definitions(LgiIde PRIVATE LIBPNG_SHARED=0)
endif()

# Create a resources sub-folder and copy over the resource files (images and lr8)
file(COPY ${CMAKE_CURRENT_LIST_DIR}/Resources DESTINATION ${CMAKE_CURRENT_BINARY_DIR})

target_include_directories (LgiIde PUBLIC
							Code
							"Resources"
							${PNG_INCLUDE_DIR}
							${OPENSSL_INCLUDE_DIR}
							"${CODELIB}/libiconv-1.14/include")

set_target_properties(LgiIde PROPERTIES EXCLUDE_FROM_ALL TRUE)
