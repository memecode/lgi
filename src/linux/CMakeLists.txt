cmake_minimum_required (VERSION 3.12)

set (CMAKE_OSX_ARCHITECTURES "i386")
set (CMAKE_CXX_STANDARD 11)

project (Lgi)
set (LGI_VERSION_MAJOR 1)
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

	add_definitions("-DMAC -DLGI_LIBRARY -D_DEBUG -DLGI_RES")
	# SET(GUI_TYPE MACOSX_BUNDLE)
	include_directories ( /Developer/Headers/FlatCarbon )
	find_library(CARBON_LIBRARY Carbon)
	find_library(COCOA_LIBRARY Cocoa)
	mark_as_advanced (CARBON_LIBRARY COCOA_LIBRARY)
	set(EXTRA_LIBS ${CARBON_LIBRARY} ${COCOA_LIBRARY})

elseif (LINUX)

	add_definitions("-DLINUX -DLGI_LIBRARY -D_DEBUG -DLGI_RES -fPIC -w -fno-inline -fpermissive")
	find_package(GTK2 COMPONENTS gtk)
	find_package(PkgConfig)
	pkg_check_modules(GST REQUIRED gstreamer-1.0>=1.4)
	                               
elseif (MSVC)

	add_definitions("/W1 -DWINDOWS -DWIN32 -DLGI_LIBRARY -D_DEBUG -DLGI_RES -D_WINDLL -D_UNICODE -DUNICODE")

endif ()

set (CommonSource
	src/common/Gdc2/15Bit.cpp
	src/common/Gdc2/16Bit.cpp
	src/common/Gdc2/24Bit.cpp
	src/common/Gdc2/32Bit.cpp
	src/common/Gdc2/64Bit.cpp
	src/common/Gdc2/8Bit.cpp
	src/common/Gdc2/Alpha.cpp
	src/common/Gdc2/Filters/GFilter.cpp
	src/common/Gdc2/Font/GDisplayString.cpp
	src/common/Gdc2/Font/LStringLayout.cpp
	src/common/Gdc2/Font/GFont.cpp
	src/common/Gdc2/Font/GFont_A.cpp
	src/common/Gdc2/Font/GFont_W.cpp
	src/common/Gdc2/Font/GFontCodePages.cpp
	src/common/Gdc2/Font/GFontSystem.cpp
	src/common/Gdc2/GColour.cpp
	src/common/Gdc2/GdcCommon.cpp
	src/common/Gdc2/GRect.cpp
	src/common/Gdc2/GSurface.cpp
	src/common/Gdc2/Path/GPath.cpp
	src/common/Gdc2/Tools/GColourReduce.cpp
	src/common/General/GContainers.cpp
	src/common/General/LDateTime.cpp
	src/common/General/GExeCheck.cpp
	src/common/General/GFileCommon.cpp
	src/common/General/GGrowl.cpp
	src/common/General/GPassword.cpp
	src/common/Hash/md5/md5.c
	src/common/Hash/sha1/sha1.c
	src/common/INet/Base64.cpp
	src/common/INet/GUri.cpp
	src/common/INet/INet.cpp
	src/common/INet/INetTools.cpp
	src/common/INet/MDStringToDigest.cpp
	src/common/Lgi/GAlert.cpp
	src/common/Lgi/GCss.cpp
	src/common/Lgi/GCssTools.cpp
	src/common/Lgi/GFindReplace.cpp
	src/common/Lgi/GFontSelect.cpp
	src/common/Lgi/GGuiUtils.cpp
	src/common/Lgi/GInput.cpp
	src/common/Lgi/GLibrary.cpp
	src/common/Lgi/GMemStream.cpp
	src/common/Lgi/GMru.cpp
	src/common/Lgi/LMutex.cpp
	src/common/Lgi/GObject.cpp
	src/common/Lgi/GOptionsFile.cpp
	src/common/Lgi/GProcess.cpp
	src/common/Lgi/GStream.cpp
	src/common/Lgi/LThreadCommon.cpp
	src/common/Lgi/LThreadEvent.cpp
	src/common/Lgi/GToolTip.cpp
	src/common/Lgi/GTrayIcon.cpp
	src/common/Lgi/GVariant.cpp
	src/common/Lgi/GViewCommon.cpp
	src/common/Lgi/GWindowCommon.cpp
	src/common/Lgi/LgiCommon.cpp
	src/common/Lgi/LgiMsg.cpp
	src/common/Lgi/LgiRand.cpp
	src/common/Lgi/GMenuCommon.cpp
	src/common/Resource/LgiRes.cpp
	src/common/Resource/Res.cpp
	src/common/Skins/Gel/Gel.cpp
	src/common/Text/GDocView.cpp
	src/common/Text/GString.cpp
	src/common/Text/GTextView3.cpp
	src/common/Text/GToken.cpp
	src/common/Text/GUnicode.cpp
	src/common/Text/GUtf8.cpp
	src/common/Text/GXmlTree.cpp
	src/common/Widgets/GBitmap.cpp
	src/common/Widgets/GBox.cpp
	src/common/Widgets/GItemContainer.cpp
	src/common/Widgets/LList.cpp
	src/common/Widgets/GPanel.cpp
	src/common/Widgets/GPopup.cpp
	src/common/Widgets/GProgressDlg.cpp
	src/common/Widgets/GSplitter.cpp
	src/common/Widgets/GStatusBar.cpp
	src/common/Widgets/GTableLayout.cpp
	src/common/Widgets/GTabView.cpp
	src/common/Widgets/GTextLabel.cpp
	src/common/Widgets/GToolBar.cpp	
	src/common/Widgets/GTree.cpp
	include/common/GMessage.h
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
	target_include_directories(Lgi PUBLIC
		"include/mac/carbon"
		"include/posix"
		"/opt/local/include"
		"${ROOT}/../../../CodeLib/libpng"
		"${ROOT}/../../../CodeLib/libpng/build"
		)
	target_link_libraries(Lgi ${EXTRA_LIBS})
	set(OBJCPP_SOURCE
		src/common/Lgi/LgiCommon.cpp
		src/mac/common/GMemDC.cpp
		src/mac/carbon/Lgi/GGeneral.cpp
		)
	set_source_files_properties(${OBJCPP_SOURCE} PROPERTIES 
                            COMPILE_FLAGS "-x objective-c++")
elseif (LINUX)
	target_include_directories(Lgi PUBLIC "include/linux/Gtk" "include/linux" ${GTK2_INCLUDE_DIRS} ${GST_INCLUDE_DIRS})
	target_link_libraries(Lgi ${GTK2_LIBRARIES} ${GST_LIBRARIES} magic)
elseif (MSVC)
	target_include_directories(Lgi PUBLIC
							"${ROOT}/include/linux"
							"${ROOT}/include/linux/gtk"
							"${VCPKG}/installed/x64-windows/include")
	target_link_libraries(	Lgi							
							gdk-3.0.lib
							pango-1.0.lib
							glib-2.0.lib
							gtk-3.0.lib
							cairo.lib
							gobject-2.0.lib
							pangocairo-1.0.lib
							gdk_pixbuf-2.0.lib
							Ws2_32.lib)
	target_link_directories(Lgi PUBLIC ${VCPKG}/installed/x64-windows/lib)
endif ()

target_include_directories (Lgi PUBLIC
							"${ROOT}/include/common"
							${PNGLIB_INCLUDE_DIR}
							"${CODELIB}/libiconv-1.14/include")



set_target_properties(Lgi PROPERTIES LINKER_LANGUAGE CXX)
