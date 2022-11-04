
set(APP_RESOURCE LgiScript.lr8)
set(APP_MANIFEST)
if (WIN32)
	set(APP_TYPE WIN32)
	set(APP_MANIFEST ../src/win/General/Lgi.manifest)
	enable_language(ASM_MASM)
elseif (APPLE)
	set(APP_TYPE MACOSX_BUNDLE)
	set_source_files_properties(${APP_RESOURCE}
		PROPERTIES
			MACOSX_PACKAGE_LOCATION "Resources")
elseif (LINUX)
endif()

add_executable(LgiScript ${APP_TYPE}
    ../src/common/Coding/LexCpp.cpp
    ../src/common/Coding/ScriptCompiler.cpp
    ../src/common/Coding/ScriptLibrary.cpp
    ../src/common/Coding/ScriptVM.cpp
    ../src/common/Lgi/LgiMain.cpp
    Main.cpp
	${APP_MANIFEST}
	)

target_link_libraries(LgiScript lgi)

if (WIN32)
	target_sources(LgiScript PRIVATE ../src/common/Coding/ExternCall.asm)
endif()