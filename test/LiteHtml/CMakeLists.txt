cmake_minimum_required(VERSION 3.18)

project(LgiLiteHtml)

set(LGI_LIB_ONLY TRUE)
add_subdirectory(../.. lgi)
add_subdirectory(../../../../../codelib/litehtml_theirs litehtml)

set(SRC
    src/main.cpp
    ../../src/common/LiteHtml/LiteHtmlView.cpp
    ../../src/common/Net/Http.cpp
    ../../src/common/Net/OpenSSLSocket.cpp
    ../../src/common/Lgi/LgiMain.cpp
    ../../src/common/Gdc2/Filters/Png.cpp
    ../../src/common/Gdc2/Filters/Jpeg.cpp)

if(WIN32)
    set(APP_TYPE WIN32)
else()
    set(APP_TYPE)
endif()

add_executable(LgiLiteHtml ${APP_TYPE} ${SRC})
target_link_libraries(LgiLiteHtml PUBLIC lgi litehtml)
if(WIN32)
    target_include_directories(LgiLiteHtml
        PRIVATE
            "c:/Program Files/OpenSSL-Win64/include"
            ${CODELIB}/zlib
            ${CODELIB}/zlib/build64
            ${CODELIB}/libpng
            ${CODELIB}/libpng/build64
            ${CODELIB}/libjpeg-9a)
endif()