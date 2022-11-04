
set(APP_MANIFEST)
if (WIN32)
	set(APP_TYPE WIN32)
	message("FIXME: Updater.manifest causes error.")
	# set(APP_MANIFEST Updater.manifest)
elseif (APPLE)
	set(APP_TYPE MACOSX_BUNDLE)
elseif (LINUX)
endif()

add_executable(Updater ${APP_TYPE}
    Main.cpp
	${APP_MANIFEST}
	)

target_compile_definitions(Updater PRIVATE LGI_STATIC)
target_link_libraries(Updater lgi)

