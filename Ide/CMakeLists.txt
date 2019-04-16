cmake_minimum_required (VERSION 3.5)

SET (CMAKE_OSX_ARCHITECTURES "i386")
set (CMAKE_CXX_STANDARD 11)

project (LgiIde)

if (UNIX AND NOT APPLE)
	set(LINUX TRUE)
endif()
add_subdirectory(.. LgiDir)

if (APPLE)

	add_definitions("-DMAC -DLGI_LIBRARY -D_DEBUG -DLGI_RES")
	SET(GUI_TYPE MACOSX_BUNDLE)
	INCLUDE_DIRECTORIES ( /Developer/Headers/FlatCarbon )
	FIND_LIBRARY(CARBON_LIBRARY Carbon)
	MARK_AS_ADVANCED (CARBON_LIBRARY)
	SET(EXTRA_LIBS ${CARBON_LIBRARY})

elseif (LINUX)

	add_definitions("-DLINUX -DLGI_LIBRARY -D_DEBUG -DLGI_RES -DPOSIX -fPIC -w -fno-inline -fpermissive")
	find_package(GTK2 COMPONENTS gtk)
	find_package(PNG REQUIRED)

elseif (MSVC)

	if ("${CMAKE_GENERATOR}" MATCHES "(Win64|IA64)")
		set(WORDSIZE 64)
		set(OPENSSL_INCLUDE_DIR "c:/Program Files/OpenSSL-Win64/include")
	else()
		set(WORDSIZE 32)
		set(OPENSSL_INCLUDE_DIR "c:/Program Files (x86)/OpenSSL-Win32/include")
	endif()
	
	if (NOT EXISTS ${OPENSSL_INCLUDE_DIR})
		message(FATAL_ERROR "OpenSSL not found @ ${OPENSSL_INCLUDE_DIR}")
	endif()
	
	if (${MSVC_VERSION} GREATER_EQUAL 1900)
		set(VS_VER 14)
	elseif (${MSVC_VERSION} EQUAL 1800)
		set(VS_VER 12)
	else()
		message(FATAL_ERROR "Unknown Visual Studio version: ${MSVC_VERSION}")
	endif()

	add_definitions("/W2 -DWINDOWS -DWIN32 -D_DEBUG -DLGI_RES -D_UNICODE -DUNICODE")
	get_filename_component(CODELIB "${CMAKE_CURRENT_SOURCE_DIR}/../../../../CodeLib" ABSOLUTE)
	get_filename_component(LIBPNG_SEARCH "${CODELIB}/libpng" ABSOLUTE)
	get_filename_component(ZLIB_SEARCH "${CODELIB}/zlib" ABSOLUTE)
	set(_FIND_ZLIB_ARG HINTS ${LIBPNG_SEARCH})
	include(FindPNG)
	if (NOT PNG_FOUND)
		set(PNG_CMAKE "${LIBPNG_SEARCH}/CMakeLists.txt")
		if (EXISTS "${PNG_CMAKE}")
			set(PNG_INCLUDE_DIR
				${LIBPNG_SEARCH}
				${LIBPNG_SEARCH}/build${WORDSIZE}
				${ZLIB_SEARCH}
				${ZLIB_SEARCH}/build${WORDSIZE})
			set(PNG_DLL
				${LIBPNG_SEARCH}/build${WORDSIZE}/Release/libpng15.dll)
		else()
			message(FATAL_ERROR "PNG not found @ ${LIBPNG_SEARCH}")
		endif()
	endif ()
	
	
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
	Code/GDebugContext.cpp
	Code/GDebugger.cpp
	Code/GHistory.cpp
	Code/FindInFiles.cpp
	Code/FindSymbol.cpp
	Code/FtpThread.cpp
	Code/DocEdit.cpp
	Code/PythonParser.cpp
	Code/MissingFiles.cpp
	Code/DocEditStyling.cpp
	Code/levenshtein.c
	../src/common/INet/OpenSSLSocket.cpp
	../src/common/INet/IHttp.cpp
	../src/common/INet/IFtp.cpp
	../src/common/Gdc2/Filters/Png.cpp
	../src/common/Lgi/GAbout.cpp
	../src/common/Lgi/GSubProcess.cpp
	../src/common/Lgi/GMdi.cpp
	../src/common/Lgi/LgiMain.cpp
	../src/common/Widgets/GControlTree.cpp
	../src/common/Coding/GParseCpp.cpp
	../src/common/Coding/GLexCpp.cpp
	)

set(CommonResourceFiles
	Resources/cmds-32px.png
	Resources/icons.png
	Resources/cmds-16px.png
	Resources/LgiIde.lr8
	)


if (APPLE)
	set(MacResourceFiles
		Mac/Info.plist
		Mac/English.lproj/InfoPlist.strings
		Mac/English.lproj/main.nib)
	set_source_files_properties(${MacResourceFiles} PROPERTIES MACOSX_PACKAGE_LOCATION "Resources")

	add_executable(LgiIde MACOSX_BUNDLE ${CommonSource} ${CommonResourceFiles} ${MacResourceFiles})
	# set(MACOSX_BUNDLE_ICON_FILE ".icns")
	add_custom_command(TARGET LgiIde POST_BUILD 
					COMMAND "${CMAKE_COMMAND}" -E copy 
						"$<TARGET_FILE:Lgi>"
						"LgiIde.app/Contents/MacOS/$<TARGET_FILE_NAME:Lgi>" 
					COMMENT "Copying to output directory")
elseif (LINUX)
	add_executable(LgiIde ${CommonSource})
elseif (MSVC)
	add_executable(LgiIde WIN32 ${CommonSource} ${CommonResourceFiles})
endif ()

if (APPLE)
	target_include_directories(LgiIde PUBLIC "../include/mac/carbon" "../include/posix")
	target_link_libraries(LgiIde Lgi ${EXTRA_LIBS})
	set_target_properties(LgiIde PROPERTIES
		MACOSX_BUNDLE TRUE
		MACOSX_FRAMEWORK_IDENTIFIER com.memecode.LgiIde
		RESOURCE "${RESOURCE_FILES}"
		)
elseif (LINUX)
	target_include_directories(LgiIde PUBLIC "../include/linux/Gtk" "../include/linux" ${GTK2_INCLUDE_DIRS} ${PNG_INCLUDE_DIR})
	target_link_libraries(LgiIde Lgi pthread ${GTK2_LIBRARIES} ${PNG_LIBRARY})
elseif (MSVC)
	target_include_directories(LgiIde PUBLIC "../include/win32" ${PNG_INCLUDE_DIRS})
	target_link_libraries(	LgiIde
							Lgi
							ComCtl32.lib
							Ws2_32.lib
							UxTheme.lib
							imm32.lib)
	foreach(infileName ${CommonResourceFiles})
		add_custom_command(TARGET LgiIde POST_BUILD COMMAND ${CMAKE_COMMAND} -E copy ${CMAKE_CURRENT_SOURCE_DIR}/${infileName} $<TARGET_FILE_DIR:LgiIde>)
	endforeach()
	add_custom_command(TARGET LgiIde POST_BUILD 
					COMMAND "${CMAKE_COMMAND}" -E copy 
						"$<TARGET_FILE:Lgi>"
						"$<TARGET_FILE_DIR:LgiIde>/$<TARGET_FILE_NAME:Lgi>" 
					COMMENT "Copying Lgi to output directory")
	if (EXISTS ${PNG_DLL})
		add_custom_command(TARGET LgiIde POST_BUILD 
					COMMAND "${CMAKE_COMMAND}" -E copy 
						"${PNG_DLL}"
						"$<TARGET_FILE_DIR:LgiIde>/libpng${VS_VER}x${WORDSIZE}.dll" 
					COMMENT "Copying Lgi to output directory")
	endif()
endif ()

target_include_directories (LgiIde PUBLIC
							"../include/common"
							"Resources"
							${PNG_INCLUDE_DIR}
							${OPENSSL_INCLUDE_DIR}
							"${CMAKE_CURRENT_SOURCE_DIR}/../../../../CodeLib/libiconv-1.14/include")
# "${CMAKE_CURRENT_SOURCE_DIR}/../../../../CodeLib/libpng"
# "${CMAKE_CURRENT_SOURCE_DIR}/../../../../CodeLib/libpng-1.6.26"

