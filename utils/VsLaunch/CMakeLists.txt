

if(WIN32)
	set(APP_TYPE WIN32)
elseif(APPLE)
	set(APP_TYPE MACOSX_BUNDLE)
endif()

add_executable(VsLaunch ${APP_TYPE}
	Main.cpp
	${LGI_DIR}/src/common/Lgi/LgiMain.cpp)

target_link_libraries(VsLaunch lgi)