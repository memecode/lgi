
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

add_executable(HtmlTest ${APP_TYPE}

    src/TestSuite.cpp
    ${LGI_DIR}/src/common/Coding/LexCpp.cpp
    ${LGI_DIR}/src/common/Coding/ScriptCompiler.cpp
    ${LGI_DIR}/src/common/Coding/ScriptLibrary.cpp
    ${LGI_DIR}/src/common/Coding/ScriptVM.cpp
    ${LGI_DIR}/src/common/Coding/ScriptingPriv.h

    ${LGI_DIR}/src/common/Gdc2/Filters/Gif.cpp
    ${LGI_DIR}/src/common/Gdc2/Filters/Jpeg.cpp
    ${LGI_DIR}/src/common/Gdc2/Filters/Lzw.cpp
    ${LGI_DIR}/src/common/Gdc2/Filters/Png.cpp
    ${LGI_DIR}/src/common/Gdc2/Font/EmojiFont.cpp
    ${LGI_DIR}/src/common/Gdc2/Tools/GdcTools.cpp

    ${LGI_DIR}/src/common/Images/Compare/ImageComparison.cpp

    ${LGI_DIR}/src/common/Net/Http.cpp
    ${LGI_DIR}/src/common/Net/OpenSSLSocket.cpp

    ${LGI_DIR}/src/common/Lgi/LgiMain.cpp

    ${LGI_DIR}/src/common/Text/Emoji/EmojiMap.cpp
    ${LGI_DIR}/src/common/Text/Homoglyphs/Homoglyphs.cpp
    ${LGI_DIR}/src/common/Text/Homoglyphs/HomoglyphsTable.cpp
    ${LGI_DIR}/src/common/Text/Html.cpp
    ${LGI_DIR}/src/common/Text/HtmlCommon.cpp
    ${LGI_DIR}/src/common/Text/HtmlParser.cpp

    ${LGI_DIR}/src/common/Widgets/ZoomView.cpp

	${APP_MANIFEST})

if (WIN32)
	target_sources(HtmlTest PRIVATE ${LGI_DIR}/src/common/Coding/ExternCall.asm)
endif()

target_include_directories(HtmlTest
	PRIVATE
		Code
		Resources
		${OPENSSL_INCLUDE_DIR}
		../../private/common)

target_link_libraries(HtmlTest
	PRIVATE
		lgi
		libpng16
		libjpeg9a)

if (APPLE)
    macBundlePostBuild(HtmlTest)
endif()

set_target_properties(HtmlTest PROPERTIES EXCLUDE_FROM_ALL TRUE)