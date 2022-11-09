
find_package(PkgConfig REQUIRED)
pkg_check_modules(GTK3 REQUIRED gtk+-3.0)

set(CMAKE_CXX_STANDARD 14)

add_executable(crash-handler
    CrashHandler.cpp)

target_include_directories(crash-handler
    PRIVATE
        ../../../include)

if (LINUX)
    target_include_directories(crash-handler
        PRIVATE
            ../../../include/lgi/linux/Gtk
            ${GTK3_INCLUDE_DIRS})
endif()