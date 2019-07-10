# Linux:
# sudo apt install libgtk-3-dev

cmake_minimum_required (VERSION 3.12)

cmake_policy(SET CMP0054 NEW)
set(CMAKE_CXX_STANDARD 11)

project (Lgi)
set (LGI_VERSION_MAJOR 4)
set (LGI_VERSION_MINOR 0)
get_filename_component(ROOT "${CMAKE_CURRENT_LIST_DIR}/../.." REALPATH)
get_filename_component(CODELIB "${ROOT}/../../../CodeLib" REALPATH)
get_filename_component(VCPKG "${CODELIB}/vcpkg" REALPATH)

message(STATUS "ROOT=${ROOT}")
message(STATUS "VCPKG=${VCPKG}")

if (UNIX AND NOT APPLE)
	set(LINUX TRUE)
endif()

if (APPLE)

	set( CMAKE_SKIP_BUILD_RPATH true )
	add_definitions("-DMAC -DLGI_LIBRARY -D_DEBUG -DLGI_RES")
	set(GTK3_PATH "/Users/Shared/inst")
	if (NOT EXISTS ${GTK3_PATH})
		get_filename_component(GTK3_PATH "${ROOT}/../../inst" REALPATH)
		if (NOT EXISTS ${GTK3_PATH})
			message(ERROR "Where is GTK3?")
		endif()
	endif()
	message(STATUS "Lgi.GTK3_PATH=${GTK3_PATH}")

	set(GTK3_INCLUDE_DIRS
				"${GTK3_PATH}/include/gtk-3.0"
				"${GTK3_PATH}/include/glib-2.0"
				"${GTK3_PATH}/lib/glib-2.0/include"
				"${GTK3_PATH}/include/pango-1.0"
				"${GTK3_PATH}/include/cairo"
				"${GTK3_PATH}/include/gdk-pixbuf-2.0"
				"${GTK3_PATH}/include/atk-1.0")
	set(GTK3_LIBRARIES
				"cairo"
				"glib-2.0"
				"gobject-2.0.0"
				"gdk-3"
				"gdk_pixbuf-2.0"
				"gtk-3"
				"pango-1.0"
				"gio-2.0"
				"pangocairo-1.0")
	set(GTK3_LIB_PATH "${GTK3_PATH}/lib")
	list(TRANSFORM GTK3_LIBRARIES PREPEND "${GTK3_LIB_PATH}/lib")
	list(TRANSFORM GTK3_LIBRARIES APPEND ".dylib")
	set(GTK3_LIBRARIES ${GTK3_LIBRARIES} CACHE INTERNAL "")

elseif (LINUX)

	add_definitions("-DLINUX -DLGI_LIBRARY -D_DEBUG -DLGI_RES -fPIC -w -fno-inline -fpermissive")
	find_package(PkgConfig REQUIRED)
	pkg_check_modules(GTK3 REQUIRED gtk+-3.0)
	pkg_check_modules(GST REQUIRED gstreamer-1.0>=1.4)
	                               
elseif (MSVC)

	add_definitions("/W1 -DWINDOWS -DWIN32 -DLGI_LIBRARY -D_DEBUG -DLGI_RES -D_WINDLL -D_UNICODE -DUNICODE")

endif ()

set (CommonSource
	include/common/LgiInc.h
	include/common/GString.h
	include/common/GClipBoard.h
	include/common/GArray.h
	src/common/Gdc2/15Bit.cpp
	src/common/Gdc2/16Bit.cpp
	src/common/Gdc2/24Bit.cpp
	src/common/Gdc2/32Bit.cpp
	src/common/Gdc2/64Bit.cpp
	src/common/Gdc2/8Bit.cpp
	src/common/Gdc2/Alpha.cpp
	include/common/GFilter.h
	src/common/Gdc2/Filters/GFilter.cpp
	include/common/GDisplayString.h
	src/common/Gdc2/Font/GDisplayString.cpp
	include/common/LStringLayout.h
	src/common/Gdc2/Font/LStringLayout.cpp
	include/common/GFont.h
	src/common/Gdc2/Font/GFont.cpp
	src/common/Gdc2/Font/GFont_A.cpp
	src/common/Gdc2/Font/GFont_W.cpp
	src/common/Gdc2/Font/GFontCodePages.cpp
	src/common/Gdc2/Font/GFontSystem.cpp
	include/common/GColour.h
	src/common/Gdc2/GColour.cpp
	src/common/Gdc2/GdcCommon.cpp
	include/common/GRect.h
	src/common/Gdc2/GRect.cpp
	src/common/Gdc2/GSurface.cpp
	include/common/GPath.h
	src/common/Gdc2/Path/GPath.cpp
	include/common/GPalette.h
	include/common/Gdc2.h
	src/common/Gdc2/Tools/GColourReduce.cpp
	include/common/GContainers.h
	src/common/General/GContainers.cpp
	include/common/LDateTime.h
	src/common/General/LDateTime.cpp
	include/common/GFile.h
	src/common/General/GExeCheck.cpp
	src/common/General/GFileCommon.cpp
	include/common/GGrowl.h
	src/common/General/GGrowl.cpp
	include/common/GPassword.h
	src/common/General/GPassword.cpp
	src/common/Hash/md5/md5.h
	src/common/Hash/md5/md5.c
	src/common/Hash/sha1/sha1.h
	src/common/Hash/sha1/sha1.c
	include/common/Base64.h
	src/common/INet/Base64.cpp
	src/common/INet/GUri.cpp
	include/common/INet.h
	src/common/INet/INet.cpp
	include/common/INetTools.h
	src/common/INet/INetTools.cpp
	src/common/INet/MDStringToDigest.cpp
	src/common/Lgi/GAlert.cpp
	include/common/GCss.h
	src/common/Lgi/GCss.cpp
	include/common/GCssTools.h
	src/common/Lgi/GCssTools.cpp
	include/common/GFindReplaceDlg.h
	src/common/Lgi/GFindReplace.cpp
	include/common/GFontSelect.h
	src/common/Lgi/GFontSelect.cpp
	src/common/Lgi/GGuiUtils.cpp
	include/common/GInput.h
	src/common/Lgi/GInput.cpp
	include/common/GLibrary.h
	src/common/Lgi/GLibrary.cpp
	include/common/GStream.h
	src/common/Lgi/GMemStream.cpp
	include/common/GMru.h
	src/common/Lgi/GMru.cpp
	include/common/LMutex.h
	src/common/Lgi/LMutex.cpp
	include/common/LgiClass.h
	src/common/Lgi/GObject.cpp
	include/common/GOptionsFile.h
	src/common/Lgi/GOptionsFile.cpp
	src/common/Lgi/GStream.cpp
	include/common/LThread.h
	src/common/Lgi/LThreadCommon.cpp
	include/common/LThreadEvent.h
	src/common/Lgi/LThreadEvent.cpp
	include/common/GSubProcess.h
	src/common/Lgi/GSubProcess.cpp
	src/common/Lgi/GToolTip.cpp
	src/common/Lgi/GTrayIcon.cpp
	include/common/GVariant.h
	src/common/Lgi/GVariant.cpp
	src/common/Lgi/GViewCommon.cpp
	src/common/Lgi/GWindowCommon.cpp
	include/common/LgiCommon.h
	src/common/Lgi/LgiCommon.cpp
	src/common/Lgi/LgiMsg.cpp
	src/common/Lgi/LgiRand.cpp
	src/common/Lgi/GMenuCommon.cpp
	include/common/LgiRes.h
	src/common/Resource/LgiRes.cpp
	include/common/Res.h
	src/common/Resource/Res.cpp
	src/common/Skins/Gel/Gel.cpp
	include/common/GDocView.h
	src/common/Text/GDocView.cpp
	include/common/GStringClass.h
	src/common/Text/GString.cpp
	include/common/GTextView3.h
	src/common/Text/GTextView3.cpp
	include/common/GToken.h
	src/common/Text/GToken.cpp
	include/common/GUnicode.h
	src/common/Text/GUnicode.cpp
	src/common/Text/GUtf8.cpp
	include/common/GXmlTree.h
	src/common/Text/GXmlTree.cpp
	include/common/GBitmap.h
	src/common/Widgets/GBitmap.cpp
	include/common/GBox.h
	src/common/Widgets/GBox.cpp
	include/common/GItemContainer.h
	src/common/Widgets/GItemContainer.cpp
	include/common/LList.h
	src/common/Widgets/LList.cpp
	include/common/GPanel.h
	src/common/Widgets/GPanel.cpp
	include/common/GPopup.h
	src/common/Widgets/GPopup.cpp
	include/common/GProgressDlg.h
	src/common/Widgets/GProgressDlg.cpp
	src/common/Widgets/GSplitter.cpp
	src/common/Widgets/GStatusBar.cpp
	include/common/GTableLayout.h
	src/common/Widgets/GTableLayout.cpp
	include/common/GTabView.h
	src/common/Widgets/GTabView.cpp
	include/common/GTextLabel.h
	src/common/Widgets/GTextLabel.cpp
	include/common/GToolBar.h
	src/common/Widgets/GToolBar.cpp	
	include/common/GTree.h
	src/common/Widgets/GTree.cpp
	include/common/GMessage.h
	include/common/LgiClasses.h
	)

set (GtkSource
	include/linux/Gtk/LgiWidget.h
	src/linux/Gtk/Gdc2.cpp
	src/linux/Gtk/GMemDC.cpp
	src/linux/Gtk/GPrintDC.cpp
	src/linux/Gtk/GScreenDC.cpp
	src/linux/Gtk/LgiWidget.cpp
	src/linux/General/GMem.cpp
	src/linux/Lgi/GApp.cpp
	src/linux/Lgi/GClipBoard.cpp
	src/linux/Lgi/GDragAndDrop.cpp
	src/linux/Lgi/GLayout.cpp
	src/linux/Lgi/GMenuGtk.cpp
	src/linux/Lgi/GPrinter.cpp
	src/linux/Lgi/GView.cpp
	src/linux/Lgi/GWidgets.cpp
	src/linux/Lgi/GWindow.cpp
	src/common/Lgi/GFileSelect.cpp
	src/common/Widgets/GCheckBox.cpp
	src/common/Widgets/GEdit.cpp
	src/common/Widgets/GScrollBar.cpp
	src/common/Widgets/GCombo.cpp
	src/common/Widgets/GButton.cpp
	src/common/Widgets/GRadioGroup.cpp
	src/common/Widgets/GProgress.cpp
	)

set (WindowsSource
	src/win32/General/GFile.cpp
	src/win32/General/ShowFileProp_Win.cpp
	src/win32/Lgi/GGeneral.cpp
	src/win32/Lgi/LThread.cpp
	src/win32/INet/MibAccess.cpp
	)

set (LinuxSource
	src/linux/General/GFile.cpp
	src/linux/General/ShowFileProp_Linux.cpp
	src/linux/Lgi/GGeneral.cpp
	src/linux/Lgi/LThread.cpp
	include/linux/Gtk/LgiOsDefs.h
	)

set (MacSource
	src/mac/cocoa/General/ShowFileProp_Mac.mm
	src/mac/carbon/General/GFile.cpp
	src/mac/carbon/Lgi/GGeneral.cpp
	src/mac/carbon/Lgi/LThread.cpp
	)

list(TRANSFORM CommonSource PREPEND ${ROOT}/)
list(TRANSFORM GtkSource PREPEND ${ROOT}/)
list(TRANSFORM WindowsSource PREPEND ${ROOT}/)
list(TRANSFORM LinuxSource PREPEND ${ROOT}/)
list(TRANSFORM MacSource PREPEND ${ROOT}/)

if (APPLE)
	set(Source ${CommonSource} ${GtkSource} ${MacSource})
elseif (LINUX)
	set(Source ${CommonSource} ${GtkSource} ${LinuxSource})
elseif (MSVC)
	set(Source ${CommonSource} ${GtkSource} ${WindowsSource})
endif ()

add_library(Lgi SHARED ${Source})

if (APPLE)
	set(OBJCPP_SOURCE "${ROOT}/src/common/Lgi/LgiCommon.cpp")
	target_include_directories(Lgi PUBLIC
							"${ROOT}/include/linux/Gtk"
							"${ROOT}/include/linux"
							${GTK3_INCLUDE_DIRS}
							)
	target_link_libraries(Lgi ${GTK3_LIBRARIES})
	set_source_files_properties(${OBJCPP_SOURCE} PROPERTIES
                            COMPILE_FLAGS "-x objective-c++")
	set_target_properties(Lgi PROPERTIES
							BUILD_RPATH "{GTK3_PATH}/lib")
elseif (LINUX)
	target_include_directories(Lgi PUBLIC
							"${ROOT}/include/linux/Gtk"
							"${ROOT}/include/linux"
							${GTK3_INCLUDE_DIRS}
							${GST_INCLUDE_DIRS})
	target_link_libraries(Lgi
							${GTK3_LIBRARIES}
							${GST_LIBRARIES}
							magic)
elseif (MSVC)
	set(LIBPOSTFIX "d")
	target_include_directories(Lgi PUBLIC
							"${ROOT}/include/linux"
							"${ROOT}/include/linux/gtk"
							"${VCPKG}/installed/x64-windows/include")
	target_link_libraries(	Lgi							
							gdk-3.0.lib
							pango-1.0.lib
							glib-2.0.lib
							gtk-3.0.lib
							cairo${LIBPOSTFIX}.lib
							gobject-2.0.lib
							pangocairo-1.0.lib
							gdk_pixbuf-2.0.lib
							gio-2.0.lib
							Ws2_32.lib)
	# target_link_directories(Lgi PUBLIC ${VCPKG}/installed/x64-windows/lib)
	target_link_directories(Lgi PUBLIC ${VCPKG}/installed/x64-windows/debug/lib)
endif ()

target_include_directories (Lgi PUBLIC
							"${ROOT}/include/common"
							${PNGLIB_INCLUDE_DIR}
							"${CODELIB}/libiconv-1.14/include")



set_target_properties(Lgi PROPERTIES LINKER_LANGUAGE CXX)
