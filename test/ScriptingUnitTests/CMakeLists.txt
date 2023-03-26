
set(APP_RESOURCE LgiScript.lr8)
set(APP_MANIFEST)
if (WIN32)
	set(APP_TYPE WIN32)
	set(APP_MANIFEST ${LGI_DIR}/src/win/General/Lgi.manifest)
	enable_language(ASM_MASM)
elseif (APPLE)
	set(APP_TYPE MACOSX_BUNDLE)
	set_source_files_properties(${APP_RESOURCE}
		PROPERTIES
			MACOSX_PACKAGE_LOCATION "Resources")
elseif (LINUX)
endif()

add_executable(LgiScript ${APP_TYPE}
    ${LGI_DIR}/src/common/Coding/LexCpp.cpp
    ${LGI_DIR}/src/common/Coding/ScriptCompiler.cpp
    ${LGI_DIR}/src/common/Coding/ScriptLibrary.cpp
    ${LGI_DIR}/src/common/Coding/ScriptVM.cpp
    ${LGI_DIR}/src/common/Lgi/LgiMain.cpp
    Main.cpp
	${APP_MANIFEST}
	)

target_link_libraries(LgiScript lgi)

if (APPLE)
	macBundlePostBuild(LgiScript)
elseif (WIN32)
	target_sources(LgiScript PRIVATE ${LGI_DIR}/src/common/Coding/ExternCall.asm)
endif()

set_target_properties(LgiScript PROPERTIES EXCLUDE_FROM_ALL TRUE)