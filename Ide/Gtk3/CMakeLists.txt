# cmake -G "Visual Studio 14 2015 Win64" -DCMAKE_BUILD_TYPE=Debug ..
cmake_minimum_required (VERSION 3.7)

set(CMAKE_CXX_STANDARD 11)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)

project (LgiIde)

if (UNIX AND NOT APPLE)
	set(LINUX TRUE)
endif()
add_subdirectory(../../src/linux LgiDir)
get_filename_component(ROOT "${CMAKE_CURRENT_LIST_DIR}/../.." REALPATH)
get_filename_component(CODELIB "${ROOT}/../../../CodeLib" REALPATH)
message(STATUS "ROOT=${ROOT}")
message(STATUS "CODELIB=${CODELIB}")

if (APPLE)

	set(GTK3_PATH "/Users/Shared/inst")
	add_definitions("-DMAC -DLGI_LIBRARY -D_DEBUG -DLGI_RES")
	SET(GUI_TYPE MACOSX_BUNDLE)

	get_filename_component(LIBPNG_SEARCH "${CODELIB}/libpng" ABSOLUTE)
	get_filename_component(ZLIB_SEARCH "${CODELIB}/zlib" ABSOLUTE)
	add_subdirectory(${LIBPNG_SEARCH} libpng_dir)

elseif (LINUX)

	add_definitions("-DLINUX -DLGI_LIBRARY -D_DEBUG -DLGI_RES -DPOSIX -fPIC -w -fno-inline -fpermissive")
	find_package(PkgConfig REQUIRED)
	pkg_check_modules(GTK3 REQUIRED gtk+-3.0)
	pkg_check_modules(LIBPNG REQUIRED libpng) # use the system libpng
	add_definitions(-DLIBPNG_VERSION=\"${LIBPNG_VERSION}\")
	
	# get_filename_component(LIBPNG_SEARCH "${CODELIB}/libpng" ABSOLUTE)
	# add_subdirectory(${LIBPNG_SEARCH} libpng_dir)
	
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

	add_definitions("/W1 -DWINDOWS -DWIN32 -D_DEBUG -DLGI_RES -D_UNICODE -DUNICODE")
	get_filename_component(VCPKG "${CODELIB}/vcpkg" ABSOLUTE)
	get_filename_component(LIBPNG_SEARCH "${CODELIB}/libpng" ABSOLUTE)
	get_filename_component(ZLIB_SEARCH "${CODELIB}/zlib" ABSOLUTE)
	
	add_subdirectory(${LIBPNG_SEARCH} libpng_dir)
	
endif ()

set (CommonSource
	Code/AddFtpFile.h
	Code/AddFtpFile.cpp
	Code/DocEdit.h
	Code/DocEdit.cpp
	Code/DocEditStyling.cpp
	Code/FindInFiles.h
	Code/FindInFiles.cpp
	Code/FindSymbol.h
	Code/FindSymbol.cpp
	Code/FtpThread.h
	Code/FtpThread.cpp
	Code/GDebugContext.h
	Code/GDebugContext.cpp
	Code/GDebugger.h
	Code/GDebugger.cpp
	Code/GHistory.h
	Code/GHistory.cpp
	Code/IdeDoc.h
	Code/IdeDocPrivate.h
	Code/IdeDoc.cpp
	Code/IdeProject.h
	Code/IdeProject.cpp
	Code/IdeProjectSettings.h
	Code/IdeProjectSettings.cpp
	Code/levenshtein.h
	Code/levenshtein.c
	Code/LgiIde.h
	Code/LgiIde.cpp
	Code/LgiUtils.cpp
	Code/MemDumpViewer.cpp
	Code/MissingFiles.cpp
	Code/ParserCommon.h
	Code/SimpleCppParser.cpp
	Code/PythonParser.cpp
	Code/IdeCommon.cpp
	Code/ProjectNode.h
	Code/ProjectNode.cpp
	Code/SpaceTabConv.h
	Code/SpaceTabConv.cpp
	Code/SysCharSupport.cpp
	Code/WebFldDlg.h
	Code/WebFldDlg.cpp
	../src/common/INet/OpenSSLSocket.cpp
	../src/common/INet/IHttp.cpp
	../src/common/INet/IFtp.cpp
	../src/common/Gdc2/Filters/Png.cpp
	../src/common/Lgi/GAbout.cpp
	../src/common/Lgi/GMdi.cpp
	../src/common/Lgi/LgiMain.cpp
	../src/common/Widgets/GControlTree.cpp
	../src/common/Coding/GParseCpp.cpp
	../src/common/Coding/GLexCpp.cpp
	)
list(TRANSFORM CommonSource PREPEND ../)

set(CommonResourceFiles
	Resources/cmds-32px.png
	Resources/icons.png
	Resources/cmds-16px.png
	Resources/LgiIde.lr8
	Resources/icon64.png
	)
list(TRANSFORM CommonResourceFiles PREPEND ../)

if (APPLE)
	set(MacResourceFiles
		Resources/mac-icon.icns
		MacCarbon/English.lproj/InfoPlist.strings
		MacCarbon/English.lproj/main.nib)
	list(TRANSFORM MacResourceFiles PREPEND ../)

	set(MACOSX_BUNDLE_ICON_FILE mac-icon.icns)
	set_source_files_properties(${CommonResourceFiles} PROPERTIES MACOSX_PACKAGE_LOCATION "Resources")
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
	message(STATUS "${GTK3_PATH}/include")
	target_include_directories(LgiIde PUBLIC
							"${ROOT}/include/linux/Gtk"
							"${ROOT}/include/linux"
							"${GTK3_PATH}/include")
	target_link_libraries(LgiIde Lgi ${EXTRA_LIBS})
	set_target_properties(LgiIde PROPERTIES
		MACOSX_BUNDLE TRUE
		MACOSX_FRAMEWORK_IDENTIFIER com.memecode.LgiIde
		RESOURCE "${RESOURCE_FILES}"
		)
	set(RESOURCE_DEST "../Resources")

	if (0)
		get_cmake_property(_variableNames VARIABLES)
		list (SORT _variableNames)
		foreach (_variableName ${_variableNames})
			message(STATUS "${_variableName}=${${_variableName}}")
		endforeach()
	endif()

	add_custom_command(TARGET LgiIde POST_BUILD
				COMMAND "${CMAKE_COMMAND}" -E copy
					"$<TARGET_FILE:libpng15>"
					"$<TARGET_FILE_DIR:LgiIde>"
				COMMENT "Copying Lgi to output directory")

elseif (LINUX)

	target_include_directories(LgiIde PUBLIC
								"${ROOT}/include/linux/Gtk"
								"${ROOT}/include/linux"
								${GTK3_INCLUDE_DIRS}
								${PNG_INCLUDE_DIR})
	target_link_libraries(LgiIde Lgi pthread ${GTK3_LIBRARIES})
	set(RESOURCE_DEST "Resources")
#	add_custom_command(TARGET LgiIde POST_BUILD
#				COMMAND "${CMAKE_COMMAND}" -E copy
#					"$<TARGET_FILE:libpng15>"
#					"$<TARGET_FILE_DIR:LgiIde>"
#				COMMENT "Copying libpng to output directory")

elseif (MSVC)

	target_include_directories(LgiIde PUBLIC ${PNG_INCLUDE_DIRS} ${VCPKG}/installed/x64-windows/include)
	target_link_libraries(	LgiIde
							Lgi)
	if (EXISTS ${PNG_DLL})
		add_custom_command(TARGET LgiIde POST_BUILD 
					COMMAND "${CMAKE_COMMAND}" -E copy 
						"${PNG_DLL}"
						"$<TARGET_FILE_DIR:LgiIde>/libpng${VS_VER}x${WORDSIZE}.dll" 
					COMMENT "Copying Lgi to output directory")
	endif()
	set(RESOURCE_DEST "Resources")

endif ()

# Create a resources sub-folder and copy over the resource files (images and lr8)
add_custom_command(	TARGET LgiIde
					POST_BUILD
					COMMAND ${CMAKE_COMMAND} -E copy_directory ${CMAKE_CURRENT_LIST_DIR}/../Resources $<TARGET_FILE_DIR:LgiIde>/${RESOURCE_DEST})

target_include_directories (LgiIde PUBLIC
							"${ROOT}/include/common"
							"${ROOT}/include/linux/gtk"
							"../Resources"
							${PNG_INCLUDE_DIR}
							${OPENSSL_INCLUDE_DIR}
							"${CODELIB}/libiconv-1.14/include")
