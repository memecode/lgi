

set(APP_MANIFEST)
if (WIN32)
	set(APP_TYPE WIN32)
	set(APP_ICON Rich.rc)
	set(APP_MANIFEST ../src/win/General/Lgi.manifest)
elseif (APPLE)
	set(APP_TYPE MACOSX_BUNDLE)
	set(APP_ICON Resources/mac-icon.icns)
	set_source_files_properties(${APP_ICON}
		PROPERTIES
			MACOSX_PACKAGE_LOCATION "Resources")
elseif (LINUX)
	set(APP_ICON Resources/icon128.png)
endif()

if (NOT TARGET libjpeg9a)
	add_subdirectory(${CODELIB}/libjpeg-9a libjpeg)
endif()

add_executable(HtmlEdit ${APP_TYPE}

    Rich.cpp
    ${LGI_DIR}/src/common/Gdc2/Filters/Jpeg.cpp
    ${LGI_DIR}/src/common/Gdc2/Filters/Png.cpp
    ${LGI_DIR}/src/common/Gdc2/Tools/GdcTools.cpp
    ${LGI_DIR}/src/common/Net/Http.cpp
    ${LGI_DIR}/src/common/Net/OpenSSLSocket.cpp
    ${LGI_DIR}/src/common/Lgi/LgiMain.cpp
    ${LGI_DIR}/src/common/Text/Emoji/EmojiMap.cpp
    ${LGI_DIR}/src/common/Text/Emoji/EmojiTools.cpp
    ${LGI_DIR}/src/common/Text/Homoglyphs/Homoglyphs.cpp
    ${LGI_DIR}/src/common/Text/Homoglyphs/HomoglyphsTable.cpp
    ${LGI_DIR}/src/common/Text/Html.cpp
    ${LGI_DIR}/src/common/Text/HtmlCommon.cpp
    ${LGI_DIR}/src/common/Text/HtmlParser.cpp
    ${LGI_DIR}/src/common/Widgets/Editor/BlockCursor.cpp
    ${LGI_DIR}/src/common/Widgets/Editor/RichTextEdit.cpp
    ${LGI_DIR}/src/common/Widgets/Editor/RichTextEditPriv.cpp
    ${LGI_DIR}/src/common/Widgets/Editor/HorzRuleBlock.cpp
    ${LGI_DIR}/src/common/Widgets/Editor/ImageBlock.cpp
    ${LGI_DIR}/src/common/Widgets/Editor/TextBlock.cpp
    ${LGI_DIR}/src/win/Lgi/SpellCheckWindows.cpp
    resource.h

	${APP_ICON}
	${APP_MANIFEST})

target_include_directories(HtmlEdit
	PRIVATE
		Code
		Resources
		${OPENSSL_INCLUDE_DIR}
		../private/common)

target_link_libraries(HtmlEdit
	PRIVATE
		lgi
		libpng15
		libjpeg9a)
