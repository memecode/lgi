cmake_minimum_required (VERSION 3.18)

project(Lgi)

if (WIN32)
	set(CMAKE_SYSTEM_VERSION 10.0.19041.0)
endif()

if (FALSE)
	# dump all vars for debugging.
	get_cmake_property(_variableNames VARIABLES)
	list (SORT _variableNames)
	foreach (_variableName ${_variableNames})
		message(STATUS "${_variableName}=${${_variableName}}")
	endforeach()
endif()

include(build.cmake)

# set(CMAKE_OSX_ARCHITECTURES "x86_64")
set(CMAKE_CXX_STANDARD 14)
set(LGI_VERSION_MAJOR 1)
set(LGI_VERSION_MINOR 0)

if (UNIX AND NOT APPLE AND NOT HAIKU)
	set(LINUX TRUE)
endif()

if (WIN32)
	message("CMAKE_GENERATOR=${CMAKE_GENERATOR}")
	message("CMAKE_GENERATOR_PLATFORM=${CMAKE_GENERATOR_PLATFORM}")
	if (CMAKE_GENERATOR MATCHES "Win64" OR
		CMAKE_GENERATOR_PLATFORM MATCHES "x64")
		set(WORDSIZE 64)
		set(OPENSSL_DIR "c:/Program Files/OpenSSL-Win64")
	else()
		set(WORDSIZE 32)
		set(OPENSSL_DIR "c:/Program Files (x86)/OpenSSL-Win32")
	endif()
	set(OPENSSL_INCLUDE_DIR "${OPENSSL_DIR}/include")

	if (CMAKE_VS_WINDOWS_TARGET_PLATFORM_VERSION MATCHES "14393")
		message("SDK too old: ${CMAKE_VS_WINDOWS_TARGET_PLATFORM_VERSION}")
	endif()
endif()

get_directory_property(hasParent PARENT_DIRECTORY)
if(hasParent)
	set(LGI_DIR "${CMAKE_CURRENT_LIST_DIR}" PARENT_SCOPE)
endif()
set(LGI_DIR "${CMAKE_CURRENT_LIST_DIR}")

# External libs:
get_filename_component(CODE ../.. ABSOLUTE)
get_filename_component(DEPS ${CMAKE_CURRENT_LIST_DIR}/../deps ABSOLUTE)
get_filename_component(CODELIB ${CODE}/../codelib ABSOLUTE)
if(hasParent)
	set(CODELIB "${CODELIB}" PARENT_SCOPE)
endif()

if(WIN32)
	set(DEPS_BUILD "${DEPS}/build-x64")
	if (NOT EXISTS "${DEPS_BUILD}")
		message(WARNING "Expecting external libraries in: ${DEPS_BUILD}")
	endif()
else()
	if(LINUX OR HAIKU)
		set(DEPS_ARCH "-x64")
	else()
		set(DEPS_ARCH "")
	endif()
	set(DEPS_DEBUG "${DEPS}/build${DEPS_ARCH}-debug")
	if (NOT EXISTS "${DEPS_DEBUG}")
		message(WARNING "Expecting debug external libraries in: ${DEPS_DEBUG}")
	endif()
	set(DEPS_RELEASE "${DEPS}/build${DEPS_ARCH}-release")
	if (NOT EXISTS "${DEPS_RELEASE}")
		message(WARNING "Expecting release external libraries in: ${DEPS_RELEASE}")
	endif()
	if(hasParent)
		set(DEPS "${DEPS}" PARENT_SCOPE)
	endif()
endif()

if (APPLE)

	# SET(GUI_TYPE MACOSX_BUNDLE)
	find_library(COCOA_LIBRARY Cocoa)
	mark_as_advanced (COCOA_LIBRARY)
	set(PUBLIC_LIBS ${COCOA_LIBRARY})

elseif (LINUX)

	find_package(PkgConfig)
	pkg_check_modules(GTK3 REQUIRED gtk+-3.0)
	pkg_check_modules(GST REQUIRED gstreamer-1.0>=1.4)
	set(PUBLIC_LIBS pthread crypt z)

endif()

# Lgi source code:
set (CommonSource
	src/common/Gdc2/15Bit.cpp
	src/common/Gdc2/16Bit.cpp
	src/common/Gdc2/24Bit.cpp
	src/common/Gdc2/32Bit.cpp
	src/common/Gdc2/64Bit.cpp
	src/common/Gdc2/8Bit.cpp
	src/common/Gdc2/Alpha.cpp
	include/lgi/common/Filter.h
	src/common/Gdc2/Filters/Filter.cpp
	src/common/Gdc2/Filters/Gif.cpp
	src/common/Gdc2/Filters/Lzw.cpp
	src/common/Gdc2/Filters/Jpeg.cpp
	src/common/Gdc2/Filters/Tiff.cpp
	src/common/Gdc2/Filters/Png.cpp
	src/common/Gdc2/Filters/Svg.cpp
	include/lgi/common/DisplayString.h
	src/common/Gdc2/Font/DisplayString.cpp
	include/lgi/common/StringLayout.h
	src/common/Gdc2/Font/StringLayout.cpp
	include/lgi/common/Font.h
	src/common/Gdc2/Font/Font.cpp
	src/common/Gdc2/Font/FontSystem.cpp
	src/common/Gdc2/Font/FontType.cpp
	src/common/Gdc2/Font/TypeFace.cpp
	include/lgi/common/Charset.h
	src/common/Gdc2/Font/Charset.cpp
	include/lgi/common/Colour.h
	src/common/Gdc2/Colour.cpp
	include/lgi/common/Gdc2.h
	include/lgi/common/Palette.h
	include/lgi/common/MathTools.h
	src/common/Gdc2/GdcCommon.cpp
	include/lgi/common/Rect.h
	src/common/Gdc2/Rect.cpp
	src/common/Gdc2/Surface.cpp
	src/common/Gdc2/Tools/GdcTools.cpp
	include/lgi/common/Path.h
	src/common/Gdc2/Path/Path.cpp
	src/common/Gdc2/Tools/ColourReduce.cpp
	include/lgi/common/Containers.h
	src/common/General/Containers.cpp
	include/lgi/common/DateTime.h
	src/common/General/DateTime.cpp
	src/common/General/TimeZone.cpp
	src/common/General/ExeCheck.cpp
	include/lgi/common/File.h
	src/common/General/FileCommon.cpp
	include/lgi/common/Growl.h
	src/common/General/Growl.cpp
	src/common/General/Password.cpp
	src/common/Hash/md5/md5.c
	src/common/Hash/sha1/sha1.c
	src/common/Net/Base64.cpp
	src/common/Net/Uri.cpp
	include/lgi/common/Net.h
	src/common/Net/Net.cpp
	src/common/Net/MDStringToDigest.cpp
	src/common/Net/CommsBus.cpp
	src/common/Lgi/Alert.cpp
	include/lgi/common/Css.h
	src/common/Lgi/Css.cpp
	include/lgi/common/CssTools.h
	src/common/Lgi/CssTools.cpp
	include/lgi/common/FindReplaceDlg.h
	src/common/Lgi/FindReplace.cpp
	include/lgi/common/FontSelect.h
	src/common/Lgi/FontSelect.cpp
	src/common/Lgi/GuiUtils.cpp
	src/common/Lgi/Input.cpp
	include/lgi/common/Library.h
	src/common/Lgi/Library.cpp
	include/lgi/common/Mru.h
	src/common/Lgi/Mru.cpp
	include/lgi/common/Mutex.h
	src/common/Lgi/Mutex.cpp
	src/common/Lgi/Object.cpp
	include/lgi/common/OptionsFile.h
	src/common/Lgi/OptionsFile.cpp
	include/lgi/common/SubProcess.h
	src/common/General/SubProcessCommon.cpp
	include/lgi/common/Stream.h
	src/common/Lgi/Stream.cpp
	src/common/Lgi/MemStream.cpp
	include/lgi/common/Thread.h
	src/common/Lgi/ThreadCommon.cpp
	src/common/Lgi/ThreadEvent.cpp
	include/lgi/common/ToolTip.h
	src/common/Lgi/ToolTip.cpp
	include/lgi/common/TrayIcon.h
	src/common/Lgi/TrayIcon.cpp
	include/lgi/common/Variant.h
	src/common/Lgi/Variant.cpp
	include/lgi/common/App.h
	src/common/Lgi/AppCommon.cpp
	include/lgi/common/View.h
	src/common/Lgi/ViewCommon.cpp
	include/lgi/common/Window.h
	src/common/Lgi/WindowCommon.cpp
	include/lgi/common/DragAndDrop.h
	src/common/Lgi/DragAndDropCommon.cpp
	include/lgi/common/Lgi.h
	src/common/Lgi/LgiCommon.cpp
	src/common/Lgi/LMsg.cpp
	src/common/Lgi/Rand.cpp
	include/lgi/common/DropFiles.h
	src/common/Lgi/DropFiles.cpp
	include/lgi/common/Menu.h
	src/common/Lgi/MenuCommon.cpp
	include/lgi/common/LgiRes.h
	src/common/Resource/LgiRes.cpp
	src/common/Resource/Res.cpp
	src/common/Skins/Gel/Gel.cpp
	include/lgi/common/DocView.h
	src/common/Text/DocView.cpp
	include/lgi/common/StringClass.h
	include/lgi/common/LgiString.h
	src/common/Text/String.cpp
	include/lgi/common/TextView3.h
	src/common/Text/TextView3.cpp
	include/lgi/common/TextView4.h
	src/common/Text/TextView4.cpp
	include/lgi/common/Token.h
	src/common/Text/Token.cpp
	include/lgi/common/Unicode.h
	src/common/Text/Unicode.cpp
	include/lgi/common/Unicode.h
	src/common/Text/Utf8.cpp
	include/lgi/common/XmlTree.h
	src/common/Text/XmlTree.cpp
	include/lgi/common/Button.h
	src/common/Widgets/Button.cpp
	include/lgi/common/Bitmap.h
	src/common/Widgets/Bitmap.cpp
	include/lgi/common/Box.h
	src/common/Widgets/Box.cpp
	src/common/Widgets/ItemContainer.cpp
	include/lgi/common/List.h
	src/common/Widgets/List.cpp
	include/lgi/common/Panel.h
	src/common/Widgets/Panel.cpp
	include/lgi/common/Popup.h
	src/common/Widgets/Popup.cpp
	include/lgi/common/ProgressDlg.h
	src/common/Widgets/ProgressDlg.cpp
	include/lgi/common/RadioGroup.h
	src/common/Widgets/RadioGroup.cpp
	include/lgi/common/Splitter.h
	src/common/Widgets/Splitter.cpp
	include/lgi/common/StatusBar.h
	src/common/Widgets/StatusBar.cpp
	include/lgi/common/TableLayout.h
	src/common/Widgets/TableLayout.cpp
	include/lgi/common/TabView.h
	src/common/Widgets/TabView.cpp
	include/lgi/common/TextLabel.h
	src/common/Widgets/TextLabel.cpp
	include/lgi/common/ToolBar.h
	src/common/Widgets/ToolBar.cpp	
	include/lgi/common/Tree.h
	src/common/Widgets/Tree.cpp	
	include/lgi/common/CheckBox.h
	include/lgi/common/Combo.h
	include/lgi/common/Edit.h
	include/lgi/common/Layout.h
	include/lgi/common/Slider.h
	include/lgi/common/Progress.h
	include/lgi/common/ClipBoard.h
	include/lgi/common/FileSelect.h
	)

if (APPLE)

	set(Source
		${CommonSource}
		src/common/Widgets/CheckBox.cpp
		src/common/Widgets/Combo.cpp
		src/common/Widgets/ScrollBar.cpp
		src/common/Widgets/Edit.cpp
		src/common/Lgi/Layout.cpp
		src/common/Widgets/Progress.cpp
		src/common/Widgets/Slider.cpp
		src/mac/cocoa/ShowFileProp_Mac.mm
		src/mac/common/MemDC.cpp
		src/mac/common/PrintDC.cpp
		src/mac/common/ScreenDC.cpp
		src/mac/cocoa/File.mm
		src/mac/cocoa/Mem.mm
		src/mac/cocoa/App.mm
		src/mac/cocoa/ClipBoard.mm
		src/mac/cocoa/DragAndDrop.mm
		src/mac/cocoa/General.mm
		src/mac/cocoa/FileSelect.mm
		src/mac/cocoa/Menu.mm
		src/mac/cocoa/Printer.mm
		src/mac/cocoa/Thread.mm
		src/mac/cocoa/View.mm
		src/mac/cocoa/Widgets.mm
		src/mac/cocoa/Window.mm
		src/mac/cocoa/CocoaView.mm
		src/mac/cocoa/Gdc2.mm
		)

elseif (LINUX)

	set(Source
		${CommonSource}
		src/linux/Gtk/Gdc2.cpp
		src/linux/Gtk/MemDC.cpp
		src/linux/Gtk/PrintDC.cpp
		src/linux/Gtk/ScreenDC.cpp
		src/linux/Gtk/LgiWidget.cpp
		src/linux/General/File.cpp
		src/linux/General/Mem.cpp
		src/linux/General/ShowFileProp_Linux.cpp
		src/linux/Lgi/App.cpp
		src/linux/Lgi/ClipBoard.cpp
		src/linux/Lgi/DragAndDrop.cpp
		src/linux/Lgi/General.cpp
		src/linux/Lgi/Layout.cpp
		src/linux/Lgi/Menu.cpp
		src/linux/Lgi/Printer.cpp
		src/linux/Lgi/Thread.cpp
		src/linux/Lgi/View.cpp
		src/linux/Lgi/Window.cpp
		src/linux/Lgi/Dialog.cpp
		src/common/Lgi/FileSelect.cpp
		src/common/Widgets/CheckBox.cpp
		src/common/Widgets/Edit.cpp
		src/common/Widgets/ScrollBar.cpp
		src/common/Widgets/Combo.cpp
		src/common/Widgets/RadioGroup.cpp
		src/common/Widgets/Progress.cpp
		src/common/Widgets/Slider.cpp
		)

elseif (MSVC)

	set(Source
		${CommonSource}
		src/win/General/Lgi.manifest
		src/win/Gdc2/Gdc2.cpp
		src/win/Gdc2/GdiLeak.cpp
		src/win/Gdc2/MemDC.cpp
		src/win/Gdc2/PrintDC.cpp
		src/win/Gdc2/ScreenDC.cpp
		src/win/General/File.cpp
		src/win/General/Mem.cpp
		src/win/General/ShowFileProp_Win.cpp
		src/win/INet/MibAccess.cpp
		src/win/Lgi/App.cpp
		src/win/Lgi/ClipBoard.cpp
		src/win/Lgi/DragAndDrop.cpp
		src/win/Lgi/FileSelect.cpp
		src/win/Lgi/General.cpp
		src/win/Lgi/Layout.cpp
		src/win/Lgi/Menu.cpp
		src/win/Lgi/Printer.cpp
		src/win/Lgi/ScrollBar.cpp
		src/win/Lgi/Thread.cpp
		src/win/Lgi/View.cpp
		src/win/Lgi/Window.cpp
		src/win/Lgi/LgiException.cpp
		src/win/Widgets/Button_Win.cpp
		src/common/Widgets/CheckBox.cpp # Use XP check box
		src/win/Widgets/Combo_Win.cpp
		src/win/Widgets/Dialog_Win.cpp
		src/win/Widgets/Edit_Win.cpp
		src/win/Widgets/RadioGroup_Win.cpp
		src/win/Widgets/Slider_Win.cpp
		src/win/Widgets/Progress_Win.cpp)

elseif (HAIKU)

	set(Source
		${CommonSource}
		src/common/Lgi/FileSelect.cpp
		src/common/Widgets/Combo.cpp
		src/common/Widgets/Edit.cpp
		src/common/Widgets/CheckBox.cpp
		src/common/Widgets/ScrollBar.cpp
		src/common/Widgets/Progress.cpp
		src/common/Widgets/Slider.cpp
		src/haiku/App.cpp
		src/haiku/ClipBoard.cpp
		src/haiku/DragAndDrop.cpp
		src/haiku/File.cpp
		src/haiku/Gdc2.cpp
		src/haiku/General.cpp
		src/haiku/Layout.cpp
		src/haiku/Mem.cpp
		src/haiku/MemDC.cpp
		src/haiku/Menu.cpp
		src/haiku/PrintDC.cpp
		src/haiku/Printer.cpp
		src/haiku/ScreenDC.cpp
		src/haiku/ShowFileProp_Haiku.cpp
		src/haiku/Thread.cpp
		src/haiku/View.cpp
		src/haiku/Widgets.cpp
		src/haiku/Window.cpp)

endif()

if (WIN32)
    list(APPEND Source src/win/Lgi/SubProcess.cpp)
else()
    list(APPEND Source src/posix/SubProcess.cpp)
endif()

add_library(lgi SHARED ${Source})

add_library(lgi-stub STATIC
    src/common/Gdc2/Font/Charset.cpp
    src/common/General/Containers.cpp
    src/common/General/DateTime.cpp
	src/common/General/TimeZone.cpp
    src/common/General/FileCommon.cpp
    src/common/Lgi/Library.cpp
    src/common/Lgi/LgiCommon.cpp
    src/common/Lgi/Mutex.cpp
    src/common/Lgi/Stream.cpp
    src/common/Lgi/ThreadCommon.cpp
    src/common/Lgi/Variant.cpp
    src/common/Lgi/LMsg.cpp
    src/common/Lgi/Rand.cpp
    src/common/Text/String.cpp
    src/common/Text/Token.cpp
    src/common/Text/Unicode.cpp)

target_compile_definitions(lgi-stub
	PRIVATE
		LGI_LIBRARY
	PUBLIC
		LGI_STATIC)

target_include_directories(lgi-stub
	PUBLIC
		include)

if (DEFINED LGI_LIB_POSTFIX)
	set_target_properties(lgi
		PROPERTIES 
			DEBUG_POSTFIX
				${LGI_LIB_POSTFIX}d
			RELEASE_POSTFIX
				${LGI_LIB_POSTFIX})
endif()

if (APPLE)

	set_target_properties(lgi
		PROPERTIES
		XCODE_ATTRIBUTE_CLANG_ENABLE_OBJC_WEAK "YES"
		)
	
	set_target_properties(lgi PROPERTIES FRAMEWORK true)
	set_target_properties(lgi PROPERTIES
		XCODE_ATTRIBUTE_INSTALL_PATH @executable_path/../Frameworks/)
	target_include_directories(lgi
		PUBLIC
			include/lgi/mac/cocoa
			/opt/local/include
		PRIVATE
			private/mac
			)
	target_compile_definitions(lgi
		PUBLIC
			MAC
			LGI_COCOA
		)
	target_link_libraries(lgi
		PUBLIC
			${EXTRA_LIBS})
	target_compile_options(lgi
		PUBLIC
			-Wno-nullability-completeness)

	# lgi stub settings
	target_include_directories(lgi-stub
		PUBLIC
			include/lgi/mac/cocoa
			/opt/local/include
		PRIVATE
			private/mac)
	target_compile_definitions(lgi-stub
		PUBLIC
			MAC
			LGI_COCOA
		)
	target_compile_options(lgi-stub
		PUBLIC
			-Wno-nullability-completeness)

	set(OBJCPP_SOURCE
		src/common/Gdc2/Font/FontSystem.cpp
		src/common/Gdc2/GdcCommon.cpp
		src/common/Lgi/LgiCommon.cpp
		src/common/Lgi/GuiUtils.cpp
		src/common/Lgi/Process.cpp
		src/common/Lgi/ViewCommon.cpp
		src/common/Lgi/TrayIcon.cpp
		src/common/Lgi/LMsg.cpp
		src/common/Lgi/ViewCommon.cpp
		src/common/Lgi/AppCommon.cpp
		src/common/Lgi/DropFiles.cpp
		src/common/Widgets/Popup.cpp
		src/common/General/DateTime.cpp
		src/common/General/TimeZone.cpp
		src/mac/common/MemDC.cpp
		src/mac/common/ScreenDC.cpp
		src/mac/cocoa/General.cpp
		src/posix/SubProcess.cpp
		)

	set_source_files_properties(${OBJCPP_SOURCE} PROPERTIES 
                            COMPILE_FLAGS "-x objective-c++")

elseif (LINUX)

	target_compile_definitions(lgi
		PUBLIC
			LINUX
			POSIX
		)

	target_compile_options(lgi
		PUBLIC
			-fPIC
			-w
			-fno-inline
			-fpermissive
		)

	target_include_directories(lgi
		PUBLIC
			include/lgi/linux/Gtk
			include/lgi/linux
			${GTK3_INCLUDE_DIRS}
			${GST_INCLUDE_DIRS}
		PRIVATE
			private/linux
		)
	
	target_link_libraries(lgi
		PUBLIC
			${GTK3_LIBRARIES}
			${GST_LIBRARIES}
			magic
		PRIVATE
			iconv
		)
	target_link_directories(lgi
		PRIVATE
			${DEPS}/build-x64-release/lib
		)

	target_include_directories(lgi-stub
		PUBLIC
			include/lgi/linux/Gtk
			include/lgi/linux
			${GTK3_INCLUDE_DIRS}
			${GST_INCLUDE_DIRS}
		PRIVATE
			private/linux)
	target_compile_definitions(lgi-stub
		PUBLIC
			LINUX)

elseif (MSVC)

	target_compile_definitions(lgi
		PUBLIC
			WINDOWS
			WIN64
			_WINDLL
			_UNICODE
			UNICODE
		)

	target_compile_options(lgi
		PRIVATE
			/W2
		)

	target_include_directories(lgi
		PUBLIC
			include/lgi/win
		PRIVATE
			private/win)
	
	target_link_libraries(lgi
		PUBLIC
			ComCtl32.lib
			Ws2_32.lib
			UxTheme.lib
			imm32.lib)

	target_sources(lgi-stub
		PRIVATE
		    src/win/General/File.cpp
			src/win/General/Mem.cpp
			src/win/Lgi/General.cpp
			src/win/Lgi/Thread.cpp)
	target_include_directories(lgi-stub
		PUBLIC
			include/lgi/win)

	target_compile_definitions(lgi-stub
		PUBLIC
			WINDOWS
			_UNICODE
			UNICODE)

elseif (HAIKU)

	target_include_directories(lgi-stub
		PUBLIC
			include/lgi/haiku
		PRIVATE
			private/haiku)
	target_compile_definitions(lgi-stub
		PUBLIC
			HAIKU
			_GNU_SOURCE)

	target_include_directories(lgi
		PUBLIC
			include/lgi/haiku
		PRIVATE
			private/haiku)
	target_compile_definitions(lgi
		PUBLIC
			HAIKU
			_GNU_SOURCE)
	target_link_libraries(lgi
		PUBLIC
			gnu
			network
			be)

endif()

target_link_libraries(lgi
	PUBLIC
		${PUBLIC_LIBS}
	PRIVATE
		lunasvg
		plutovg)

target_compile_definitions(lgi
	PUBLIC
		LGI_RES
	PRIVATE
		LGI_LIBRARY
		LUNASVG_BUILD_STATIC
	)

target_include_directories(lgi
	PUBLIC
		include
	PRIVATE
		private/common)

if(WIN32)
	target_link_directories(lgi
		PRIVATE
			${DEPS_BUILD}/lunasvg/$<IF:$<CONFIG:Debug>,Debug,Release>
			${DEPS_BUILD}/lib)
	target_include_directories(lgi
		PUBLIC
			${OPENSSL_INCLUDE_DIR}
		PRIVATE
			${DEPS_BUILD}/include)
else()
	# config specific dependency include and lib folders
	target_link_directories(lgi
		PRIVATE
			$<IF:$<CONFIG:Debug>,${DEPS_DEBUG}/lib,${DEPS_RELEASE}/lib>)
	target_include_directories(lgi
		PRIVATE
			$<IF:$<CONFIG:Debug>,${DEPS_DEBUG}/include,${DEPS_RELEASE}/include>)
endif()

set_target_properties(lgi PROPERTIES LINKER_LANGUAGE CXX)

if(LINUX)
	add_subdirectory(src/linux/CrashHandler)
endif()

if (LGI_LIB_ONLY)
	message(STATUS "Skipping LGI apps, tests and utils")
else()
	# main apps
	add_subdirectory(ide)
	add_subdirectory(lvc)
	add_subdirectory(resourceEditor)

	# other subfolders of smaller utils and tests
	add_subdirectory(utils)
	add_subdirectory(test)
	add_subdirectory(src/common/Net/libntlm-0.4.2)
endif()
