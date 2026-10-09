set(CMAKE_SYSTEM_NAME Haiku)
set(CMAKE_SYSTEM_PROCESSOR x86_64)

# Force CMake to skip the test compilation binary check
set(CMAKE_TRY_COMPILE_TARGET_TYPE "STATIC_LIBRARY")

# Point to your official Haiku cross-compiler paths
get_filename_component(
	TOOLCHAIN_BIN
	"${CMAKE_CURRENT_LIST_DIR}/../../../../../source/generated.x86_64/cross-tools-x86_64/bin"
	ABSOLUTE)
if (NOT EXISTS "${TOOLCHAIN_BIN}")
    message(FATAL_ERROR "TOOLCHAIN_BIN='${TOOLCHAIN_BIN}' doesn't exist")
endif()
set(CMAKE_C_COMPILER "${TOOLCHAIN_BIN}/x86_64-unknown-haiku-gcc")
set(CMAKE_CXX_COMPILER "${TOOLCHAIN_BIN}/x86_64-unknown-haiku-g++")

# Prevent CMake from searching your host Linux folders for headers/libs
set(CMAKE_FIND_ROOT_PATH_MODE_PROGRAM NEVER)
set(CMAKE_FIND_ROOT_PATH_MODE_LIBRARY ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_INCLUDE ONLY)
