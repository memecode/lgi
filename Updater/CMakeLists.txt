
set(APP_MANIFEST)
if (WIN32)
	set(APP_TYPE WIN32)
elseif (APPLE)
	set(APP_TYPE MACOSX_BUNDLE)
elseif (LINUX)
endif()

add_executable(Updater ${APP_TYPE}
    Main.cpp
	)

target_compile_definitions(Updater PRIVATE LGI_STATIC)
target_link_libraries(Updater lgi)

if (APPLE)
	macBundlePostBuild(Updater)
elseif (WIN32)
	set_target_properties(Updater
		PROPERTIES
			LINK_FLAGS "/MANIFESTUAC:\"level='requireAdministrator' uiAccess='false'\" /SUBSYSTEM:WINDOWS")
endif()

set_target_properties(Updater PROPERTIES EXCLUDE_FROM_ALL TRUE)