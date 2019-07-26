# cmake -G "Visual Studio 14 2015 Win64" -DCMAKE_BUILD_TYPE=Debug ..
cmake_minimum_required (VERSION 3.12)

set(CMAKE_CXX_STANDARD 11)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)

project (HtmlTest)

if (UNIX AND NOT APPLE)
	set(LINUX TRUE)
endif()
add_subdirectory(../../src/linux LgiDir)
get_filename_component(HTMLTEST "${CMAKE_CURRENT_LIST_DIR}/.." REALPATH)
get_filename_component(LGI "${HTMLTEST}/.." REALPATH)
get_filename_component(CODELIB "${LGI}/../../../CodeLib" REALPATH)
message(STATUS "LGI=${LGI}")
message(STATUS "CODELIB=${CODELIB}")

if (APPLE)

	set(GTK3_PATH "/Users/Shared/inst")
	add_definitions("-DMAC -DLGI_LIBRARY -D_DEBUG -DLGI_RES")
	SET(GUI_TYPE MACOSX_BUNDLE)

elseif (LINUX)

	add_definitions("-DLINUX -DLGI_LIBRARY -D_DEBUG -DLGI_RES -DPOSIX -fPIC -w -fno-inline -fpermissive")
	find_package(PkgConfig REQUIRED)
	pkg_check_modules(GTK3 REQUIRED gtk+-3.0)
	find_package(PNG REQUIRED)
	
elseif (MSVC)

	enable_language(ASM_MASM)

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
	${HTMLTEST}/TestSuite.cpp
	${LGI}/src/common/Lgi/LgiMain.cpp
	${LGI}/src/common/Text/GHtml.cpp
	${LGI}/src/common/Text/GHtmlCommon.cpp
	${LGI}/src/common/Text/GHtmlParser.cpp
	${LGI}/src/common/Coding/ExternCall.asm
	${LGI}/src/common/Coding/GScriptCompiler.cpp
	${LGI}/src/common/Coding/GLexCpp.cpp
	${LGI}/src/common/Coding/GScriptLibrary.cpp
	${LGI}/src/common/Coding/GScriptVM.cpp
	${LGI}/src/common/Gdc2/Tools/GdcTools.cpp
	${LGI}/src/common/Images/Compare/ImageComparison.cpp
	${LGI}/src/common/Widgets/GZoomView.cpp
	)

set(CommonResourceFiles
	)

if (APPLE)
	set(MacResourceFiles
		# MacCarbon/English.lproj/InfoPlist.strings
		# MacCarbon/English.lproj/main.nib
		)
	list(TRANSFORM MacResourceFiles PREPEND ../)

	set_source_files_properties(${CommonResourceFiles} PROPERTIES MACOSX_PACKAGE_LOCATION "Resources")
	set_source_files_properties(${MacResourceFiles} PROPERTIES MACOSX_PACKAGE_LOCATION "Resources")

	add_executable(HtmlTest MACOSX_BUNDLE ${CommonSource} ${CommonResourceFiles} ${MacResourceFiles})
	
	# set(MACOSX_BUNDLE_ICON_FILE ".icns")
	add_custom_command(TARGET HtmlTest POST_BUILD 
					COMMAND "${CMAKE_COMMAND}" -E copy 
						"$<TARGET_FILE:Lgi>"
						"HtmlTest.app/Contents/MacOS/$<TARGET_FILE_NAME:Lgi>" 
					COMMENT "Copying to output directory")
elseif (LINUX)
	add_executable(HtmlTest ${CommonSource})
elseif (MSVC)
	add_executable(HtmlTest WIN32 ${CommonSource} ${CommonResourceFiles})
endif ()

if (APPLE)
	message(STATUS "${GTK3_PATH}/include")
	target_include_directories(HtmlTest PUBLIC
							"${LGI}/include/linux/Gtk"
							"${LGI}/include/linux"
							"${GTK3_PATH}/include")
	target_link_libraries(HtmlTest Lgi ${EXTRA_LIBS})
	set_target_properties(HtmlTest PROPERTIES
		MACOSX_BUNDLE TRUE
		MACOSX_FRAMEWORK_IDENTIFIER com.memecode.HtmlTest
		RESOURCE "${RESOURCE_FILES}"
		)
	set(RESOURCE_DEST "../Resources")

elseif (LINUX)
	target_include_directories(HtmlTest PUBLIC
								"${LGI}/include/linux/Gtk"
								"${LGI}/include/linux"
								${GTK3_INCLUDE_DIRS}
								${PNG_INCLUDE_DIR})
	target_link_libraries(HtmlTest Lgi pthread ${GTK2_LIBRARIES} ${PNG_LIBRARY})
	set(RESOURCE_DEST "Resources")
elseif (MSVC)
	target_include_directories(HtmlTest PUBLIC ${PNG_INCLUDE_DIRS} ${VCPKG}/installed/x64-windows/include)
	target_link_libraries(	HtmlTest
							Lgi)
	if (EXISTS ${PNG_DLL})
		add_custom_command(TARGET HtmlTest POST_BUILD 
					COMMAND "${CMAKE_COMMAND}" -E copy 
						"${PNG_DLL}"
						"$<TARGET_FILE_DIR:HtmlTest>/libpng${VS_VER}x${WORDSIZE}.dll" 
					COMMENT "Copying libpng to output directory")
	endif()
	set(RESOURCE_DEST "Resources")
endif ()

target_include_directories (HtmlTest PUBLIC
							"${LGI}/include/common"
							"${LGI}/include/linux/gtk"
							"../Resources"
							${PNG_INCLUDE_DIR}
							${OPENSSL_INCLUDE_DIR}
							"${CODELIB}/libiconv-1.14/include")
