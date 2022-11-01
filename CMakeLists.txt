cmake_minimum_required (VERSION 3.18)

set(CMAKE_OSX_ARCHITECTURES "x86_64")
set(CMAKE_CXX_STANDARD 14)

project (Lgi)
set(LGI_VERSION_MAJOR 1)
set(LGI_VERSION_MINOR 0)
set(LGI_DIR "${CMAKE_CURRENT_LIST_DIR}" PARENT_SCOPE)

if (UNIX AND NOT APPLE)
	set(LINUX TRUE)
endif()

get_filename_component(CODE ../.. ABSOLUTE)
get_filename_component(CODELIB ${CODE}/../codelib ABSOLUTE)
if (NOT EXISTS "${CODELIB}")
	message(FATAL_ERROR "Expecting external libraries in: ${CODELIB}")
else()
	message("CODELIB=${CODELIB}")
endif()
set(ICONV_PATH "${CODELIB}/libiconv")
if (NOT EXISTS "${ICONV_PATH}/include")
	if (APPLE)
		if (EXISTS "/opt/local/include/iconv.h")
			set(ICONV_PATH "/opt/local")
		else()
			find_package(Iconv REQUIRED)
		endif()
	elseif (LINUX)
		find_package(Iconv REQUIRED)
	else()
		message(FATAL_ERROR "Missing iconv at: ${ICONV_PATH}")
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
	set(PUBLIC_LIBS pthread crypt)
	                               
endif()

set (CommonSource
	src/common/Gdc2/15Bit.cpp
	src/common/Gdc2/16Bit.cpp
	src/common/Gdc2/24Bit.cpp
	src/common/Gdc2/32Bit.cpp
	src/common/Gdc2/64Bit.cpp
	src/common/Gdc2/8Bit.cpp
	src/common/Gdc2/Alpha.cpp
	src/common/Gdc2/Filters/Filter.cpp
	src/common/Gdc2/Font/DisplayString.cpp
	src/common/Gdc2/Font/StringLayout.cpp
	src/common/Gdc2/Font/Font.cpp
	src/common/Gdc2/Font/FontSystem.cpp
	src/common/Gdc2/Font/FontType.cpp
	src/common/Gdc2/Font/TypeFace.cpp
	src/common/Gdc2/Font/Charset.cpp
	src/common/Gdc2/Colour.cpp
	src/common/Gdc2/GdcCommon.cpp
	src/common/Gdc2/Rect.cpp
	src/common/Gdc2/Surface.cpp
	src/common/Gdc2/Path/Path.cpp
	src/common/Gdc2/Tools/ColourReduce.cpp
	src/common/General/Containers.cpp
	src/common/General/DateTime.cpp
	src/common/General/ExeCheck.cpp
	src/common/General/FileCommon.cpp
	src/common/General/Growl.cpp
	src/common/General/Password.cpp
	src/common/Hash/md5/md5.c
	src/common/Hash/sha1/sha1.c
	src/common/Net/Base64.cpp
	src/common/Net/Uri.cpp
	src/common/Net/Net.cpp
	src/common/Net/NetTools.cpp
	src/common/Net/MDStringToDigest.cpp
	src/common/Lgi/Alert.cpp
	src/common/Lgi/Css.cpp
	src/common/Lgi/CssTools.cpp
	src/common/Lgi/FindReplace.cpp
	src/common/Lgi/FontSelect.cpp
	src/common/Lgi/GuiUtils.cpp
	src/common/Lgi/Input.cpp
	src/common/Lgi/Library.cpp
	src/common/Lgi/MemStream.cpp
	src/common/Lgi/Mru.cpp
	src/common/Lgi/Mutex.cpp
	src/common/Lgi/Object.cpp
	src/common/Lgi/OptionsFile.cpp
	src/common/Lgi/SubProcess.cpp
	src/common/Lgi/Stream.cpp
	src/common/Lgi/ThreadCommon.cpp
	src/common/Lgi/ThreadEvent.cpp
	src/common/Lgi/ToolTip.cpp
	src/common/Lgi/TrayIcon.cpp
	src/common/Lgi/Variant.cpp
	src/common/Lgi/AppCommon.cpp
	src/common/Lgi/ViewCommon.cpp
	src/common/Lgi/WindowCommon.cpp
	src/common/Lgi/DragAndDropCommon.cpp
	src/common/Lgi/LgiCommon.cpp
	src/common/Lgi/LMsg.cpp
	src/common/Lgi/Rand.cpp
	src/common/Lgi/DropFiles.cpp
	src/common/Lgi/MenuCommon.cpp
	src/common/Resource/LgiRes.cpp
	src/common/Resource/Res.cpp
	src/common/Skins/Gel/Gel.cpp
	src/common/Text/DocView.cpp
	src/common/Text/String.cpp
	src/common/Text/TextView3.cpp
	src/common/Text/Token.cpp
	src/common/Text/Unicode.cpp
	src/common/Text/Utf8.cpp
	src/common/Text/XmlTree.cpp
	src/common/Widgets/Button.cpp
	src/common/Widgets/Bitmap.cpp
	src/common/Widgets/Box.cpp
	src/common/Widgets/ItemContainer.cpp
	src/common/Widgets/List.cpp
	src/common/Widgets/Panel.cpp
	src/common/Widgets/Popup.cpp
	src/common/Widgets/ProgressDlg.cpp
	src/common/Widgets/RadioGroup.cpp
	src/common/Widgets/Splitter.cpp
	src/common/Widgets/StatusBar.cpp
	src/common/Widgets/TableLayout.cpp
	src/common/Widgets/TabView.cpp
	src/common/Widgets/TextLabel.cpp
	src/common/Widgets/ToolBar.cpp	
	src/common/Widgets/Tree.cpp	
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
		src/linux/Lgi/Widgets.cpp
		src/linux/Lgi/Window.cpp
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

endif ()

add_library(lgi SHARED ${Source})

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

	target_include_directories(lgi
		PUBLIC
			include/lgi/mac/cocoa
			/opt/local/include
			${CODELIB}/libpng
			${CODELIB}/libpng/build
		PRIVATE
			private/mac
			)

	target_link_libraries(lgi
		PUBLIC
			${EXTRA_LIBS})

	target_compile_options(lgi
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
		src/common/Lgi/SubProcess.cpp
		src/common/Lgi/AppCommon.cpp
		src/common/Lgi/DropFiles.cpp
		src/common/Widgets/Popup.cpp
		src/common/General/DateTime.cpp
		src/mac/common/MemDC.cpp
		src/mac/common/ScreenDC.cpp
		src/mac/cocoa/General.cpp		
		)

	set_source_files_properties(${OBJCPP_SOURCE} PROPERTIES 
                            COMPILE_FLAGS "-x objective-c++")

	target_compile_definitions(lgi
		PUBLIC
			MAC
			LGI_COCOA
		)

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
		)

elseif (MSVC)

	target_compile_definitions(lgi
		PUBLIC
			WINDOWS
			WIN32
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

endif()

target_link_libraries(lgi
	PUBLIC
		${PUBLIC_LIBS})

target_compile_definitions(lgi
	PUBLIC
		LGI_RES
	PRIVATE
		LGI_LIBRARY
	)

target_include_directories(lgi
	PUBLIC
		include
	PRIVATE
		private/common
		${PNGLIB_INCLUDE_DIR}
		${ICONV_PATH}/include
	)

set_target_properties(lgi PROPERTIES LINKER_LANGUAGE CXX)
