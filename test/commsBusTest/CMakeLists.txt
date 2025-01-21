cmake_minimum_required (VERSION 3.18)

project(commsBusTest)
set(LGI_LIB_ONLY True)
add_subdirectory(../.. lgi)


# test the comms bus class
add_executable(commsBusTest
    src/main.cpp)
target_link_libraries(commsBusTest
    PRIVATE lgi)


# test the udp transport class
add_executable(udpTransportTest
    src/udp.cpp)
target_link_libraries(udpTransportTest
    PRIVATE lgi)