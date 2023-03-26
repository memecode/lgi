
set(APP_MANIFEST)
if (WIN32)
	set(APP_TYPE WIN32)
	set(APP_MANIFEST ${LGI_DIR}/src/win/General/Lgi.manifest)
elseif (APPLE)
	set(APP_TYPE MACOSX_BUNDLE)
elseif (LINUX)
endif()

add_executable(SlogViewer ${APP_TYPE}
    ${LGI_DIR}/src/common/Lgi/DocApp.cpp
    ${LGI_DIR}/src/common/Lgi/LgiMain.cpp
    src/SlogViewerMain.cpp
	${APP_MANIFEST}
	)

target_link_libraries(SlogViewer lgi)

target_include_directories(SlogViewer
	PRIVATE
		resources)
		
if (APPLE)
	macBundlePostBuild(SlogViewer)
endif()

set_target_properties(SlogViewer PROPERTIES EXCLUDE_FROM_ALL TRUE)