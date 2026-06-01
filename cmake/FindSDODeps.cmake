# cmake/FindSDODeps.cmake
#
# Find all required dependencies for Smart Downloads Organizer.
# Sets SDO_DEPS_FOUND and provides imported targets.
#
# Usage in CMakeLists.txt:
#   list(APPEND CMAKE_MODULE_PATH "${CMAKE_SOURCE_DIR}/cmake")
#   include(FindSDODeps)
#   target_link_libraries(sdo PRIVATE ${SDO_ALL_LIBS})

cmake_minimum_required(VERSION 3.16)
include(FindPackageHandleStandardArgs)

set(SDO_DEPS_FOUND TRUE)
set(SDO_ALL_LIBS "")
set(SDO_ALL_INCLUDES "")

# ─── GTK3 ─────────────────────────────────────────────────────────────────────
find_package(PkgConfig REQUIRED)
pkg_check_modules(GTK3 REQUIRED gtk+-3.0>=3.24)

if(NOT GTK3_FOUND)
    message(FATAL_ERROR
        "GTK3 >= 3.24 not found.\n"
        "Install with:\n"
        "  Ubuntu/Debian: sudo apt install libgtk-3-dev\n"
        "  Fedora:        sudo dnf install gtk3-devel\n"
        "  Arch:          sudo pacman -S gtk3")
    set(SDO_DEPS_FOUND FALSE)
else()
    message(STATUS "  GTK3:    ${GTK3_VERSION}")
    list(APPEND SDO_ALL_LIBS     ${GTK3_LIBRARIES})
    list(APPEND SDO_ALL_INCLUDES ${GTK3_INCLUDE_DIRS})
endif()

# ─── SQLite3 ──────────────────────────────────────────────────────────────────
pkg_check_modules(SQLITE3 REQUIRED sqlite3>=3.31)

if(NOT SQLITE3_FOUND)
    message(FATAL_ERROR
        "SQLite3 >= 3.31 not found.\n"
        "Install with:\n"
        "  Ubuntu/Debian: sudo apt install libsqlite3-dev\n"
        "  Fedora:        sudo dnf install sqlite-devel\n"
        "  Arch:          sudo pacman -S sqlite")
    set(SDO_DEPS_FOUND FALSE)
else()
    message(STATUS "  SQLite3: ${SQLITE3_VERSION}")
    list(APPEND SDO_ALL_LIBS     ${SQLITE3_LIBRARIES})
    list(APPEND SDO_ALL_INCLUDES ${SQLITE3_INCLUDE_DIRS})
endif()

# ─── OpenSSL ──────────────────────────────────────────────────────────────────
pkg_check_modules(OPENSSL REQUIRED openssl>=1.1)

if(NOT OPENSSL_FOUND)
    # Fallback to CMake's FindOpenSSL
    find_package(OpenSSL 1.1 REQUIRED)
    if(OpenSSL_FOUND)
        list(APPEND SDO_ALL_LIBS OpenSSL::SSL OpenSSL::Crypto)
        message(STATUS "  OpenSSL: ${OPENSSL_VERSION} (via FindOpenSSL)")
    else()
        message(FATAL_ERROR
            "OpenSSL >= 1.1 not found.\n"
            "Install with:\n"
            "  Ubuntu/Debian: sudo apt install libssl-dev\n"
            "  Fedora:        sudo dnf install openssl-devel\n"
            "  Arch:          sudo pacman -S openssl")
        set(SDO_DEPS_FOUND FALSE)
    endif()
else()
    message(STATUS "  OpenSSL: ${OPENSSL_VERSION}")
    list(APPEND SDO_ALL_LIBS     ${OPENSSL_LIBRARIES})
    list(APPEND SDO_ALL_INCLUDES ${OPENSSL_INCLUDE_DIRS})
endif()

# ─── libuuid ──────────────────────────────────────────────────────────────────
pkg_check_modules(UUID uuid)

if(NOT UUID_FOUND)
    # Manual fallback
    find_library(UUID_LIB NAMES uuid)
    find_path(UUID_INC NAMES uuid/uuid.h)
    if(UUID_LIB AND UUID_INC)
        set(UUID_FOUND TRUE)
        set(UUID_LIBRARIES ${UUID_LIB})
        set(UUID_INCLUDE_DIRS ${UUID_INC})
        message(STATUS "  libuuid: found (manual)")
    else()
        message(FATAL_ERROR
            "libuuid not found.\n"
            "Install with:\n"
            "  Ubuntu/Debian: sudo apt install uuid-dev\n"
            "  Fedora:        sudo dnf install libuuid-devel\n"
            "  Arch:          sudo pacman -S util-linux-libs")
        set(SDO_DEPS_FOUND FALSE)
    endif()
else()
    message(STATUS "  libuuid: ${UUID_VERSION}")
endif()

list(APPEND SDO_ALL_LIBS     ${UUID_LIBRARIES})
list(APPEND SDO_ALL_INCLUDES ${UUID_INCLUDE_DIRS})

# ─── Threading ────────────────────────────────────────────────────────────────
find_package(Threads REQUIRED)
list(APPEND SDO_ALL_LIBS Threads::Threads)

# ─── std::filesystem ──────────────────────────────────────────────────────────
# GCC < 9 needs -lstdc++fs; later versions include it in libstdc++ automatically
if(CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
    if(CMAKE_CXX_COMPILER_VERSION VERSION_LESS "9.0")
        list(APPEND SDO_ALL_LIBS stdc++fs)
        message(STATUS "  stdc++fs: linking explicitly (GCC < 9)")
    endif()
endif()

# ─── Summary ──────────────────────────────────────────────────────────────────
if(NOT SDO_DEPS_FOUND)
    message(FATAL_ERROR "One or more required dependencies are missing. See messages above.")
endif()

# Remove duplicate entries
list(REMOVE_DUPLICATES SDO_ALL_LIBS)
list(REMOVE_DUPLICATES SDO_ALL_INCLUDES)

mark_as_advanced(SDO_ALL_LIBS SDO_ALL_INCLUDES SDO_DEPS_FOUND)
