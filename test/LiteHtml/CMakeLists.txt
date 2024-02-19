cmake_minimum_required(VERSION 3.18)

project(LgiLiteHtml)

set(LGI_LIB_ONLY TRUE)
add_subdirectory(../.. lgi)
add_subdirectory(../../../../../codelib/litehtml litehtml)

set(SRC
    src/main.cpp
    ../../src/common/Lgi/LgiMain.cpp)

if(WIN32)
    set(APP_TYPE WIN32)
else()
    set(APP_TYPE)
endif()

add_executable(LgiLiteHtml ${APP_TYPE} ${SRC})
target_link_libraries(LgiLiteHtml PUBLIC lgi litehtml)