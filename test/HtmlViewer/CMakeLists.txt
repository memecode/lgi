
set(APP_MANIFEST)
set(APP_RESOURCE HtmlTestSuite.lr8)
if (WIN32)
	set(APP_TYPE WIN32)
	set(APP_ICON)
	set(APP_MANIFEST ${LGI_DIR}/src/win/General/Lgi.manifest)
	enable_language(ASM_MASM)
elseif (APPLE)
	set(APP_TYPE MACOSX_BUNDLE)
	set(APP_ICON)
	set_source_files_properties(${APP_ICON}
		PROPERTIES
			MACOSX_PACKAGE_LOCATION "Resources")
elseif (LINUX)
	set(APP_ICON)
endif()

set(LGI_COMMON "${LGI_DIR}/src/common")

add_executable(HtmlTest ${APP_TYPE}

    src/TestSuite.cpp
    ${LGI_COMMON}/Coding/LexCpp.cpp
    ${LGI_COMMON}/Coding/ScriptCompiler.cpp
    ${LGI_COMMON}/Coding/ScriptLibrary.cpp
    ${LGI_COMMON}/Coding/ScriptVM.cpp
    ${LGI_COMMON}/Coding/ScriptingPriv.h

    ${LGI_COMMON}/Gdc2/Font/EmojiFont.cpp
    ${LGI_COMMON}/Gdc2/Tools/GdcTools.cpp

    ${LGI_COMMON}/Images/Compare/ImageComparison.cpp

    ${LGI_COMMON}/Net/Http.cpp
    ${LGI_COMMON}/Net/OpenSSLSocket.cpp

    ${LGI_COMMON}/Lgi/LgiMain.cpp

    ${LGI_COMMON}/Text/Emoji/EmojiMap.cpp
    ${LGI_COMMON}/Text/Homoglyphs/Homoglyphs.cpp
    ${LGI_COMMON}/Text/Homoglyphs/HomoglyphsTable.cpp
    
	# Html1
	${LGI_COMMON}/Text/Html.cpp
	${LGI_DIR}/include/lgi/common/Html.h
	${LGI_COMMON}/Text/Html2.cpp
	${LGI_DIR}/include/lgi/common/Html2.h

	# Html common:
    ${LGI_COMMON}/Text/HtmlCommon.cpp
    ${LGI_COMMON}/Text/HtmlParser.cpp

    ${LGI_COMMON}/Widgets/ZoomView.cpp

	${APP_MANIFEST})

if (WIN32)
	target_sources(HtmlTest PRIVATE ${LGI_DIR}/src/common/Coding/ExternCall.asm)
endif()

target_include_directories(HtmlTest
	PRIVATE
		src
		resources
		${OPENSSL_INCLUDE_DIR}
		../../private/common)

target_link_libraries(HtmlTest
	PRIVATE
		lgi)

if (APPLE)
    macBundlePostBuild(HtmlTest)
endif()

set_target_properties(HtmlTest PROPERTIES EXCLUDE_FROM_ALL TRUE)