cmake_minimum_required (VERSION 3.18)

project(commsBusTest)
set(LGI_LIB_ONLY True)
add_subdirectory(../.. lgi)

add_executable(commsBusTest
    src/main.cpp)

target_link_libraries(commsBusTest
    PRIVATE lgi)