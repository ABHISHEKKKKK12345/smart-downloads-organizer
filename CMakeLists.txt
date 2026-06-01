cmake_minimum_required(VERSION 3.16)

project(SmartDownloadsOrganizer
    VERSION 2.0.0
    DESCRIPTION "Enterprise-grade automatic downloads folder organizer"
    LANGUAGES CXX
)

# ─── Language standard ────────────────────────────────────────────────────────
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# ─── Build type default ───────────────────────────────────────────────────────
if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Release CACHE STRING "Build type" FORCE)
endif()
message(STATUS "Build type: ${CMAKE_BUILD_TYPE}")

# ─── Output directories ───────────────────────────────────────────────────────
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)

# ─── Compiler warnings ────────────────────────────────────────────────────────
if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
    add_compile_options(
        -Wall -Wextra -Wpedantic
        -Wno-unused-parameter
        -Wno-deprecated-declarations
        $<$<CONFIG:Debug>:-g3>
        $<$<CONFIG:Debug>:-O0>
        $<$<CONFIG:Debug>:-fsanitize=address,undefined>
        $<$<CONFIG:Release>:-O3>
        $<$<CONFIG:Release>:-DNDEBUG>
        $<$<CONFIG:RelWithDebInfo>:-O2>
        $<$<CONFIG:RelWithDebInfo>:-g>
    )
    if(CMAKE_BUILD_TYPE STREQUAL "Debug")
        add_link_options(-fsanitize=address,undefined)
    endif()
endif()

# ─── Find packages ────────────────────────────────────────────────────────────
find_package(PkgConfig REQUIRED)

pkg_check_modules(GTK3    REQUIRED gtk+-3.0)
pkg_check_modules(SQLITE3 REQUIRED sqlite3)
pkg_check_modules(OPENSSL REQUIRED openssl)

# uuid — prefer pkg-config, fall back to find_library
pkg_check_modules(UUID uuid)
if(NOT UUID_FOUND)
    find_library(UUID_LIBRARIES uuid REQUIRED)
    find_path(UUID_INCLUDE_DIRS uuid/uuid.h REQUIRED)
    set(UUID_FOUND TRUE)
endif()

# ─── Source files ─────────────────────────────────────────────────────────────
set(SDO_SOURCES
    src/main.cpp
    src/logger.cpp
    src/config_manager.cpp
    src/file_analyzer.cpp
    src/file_watcher.cpp
    src/database.cpp
    src/organizer_engine.cpp
    src/app_window.cpp
)

set(SDO_HEADERS
    include/types.hpp
    include/logger.hpp
    include/config_manager.hpp
    include/file_analyzer.hpp
    include/file_watcher.hpp
    include/database.hpp
    include/organizer_engine.hpp
    include/app_window.hpp
)

# ─── Main executable ──────────────────────────────────────────────────────────
add_executable(sdo ${SDO_SOURCES} ${SDO_HEADERS})

target_include_directories(sdo PRIVATE
    ${CMAKE_SOURCE_DIR}/include
    ${GTK3_INCLUDE_DIRS}
    ${SQLITE3_INCLUDE_DIRS}
    ${OPENSSL_INCLUDE_DIRS}
    ${UUID_INCLUDE_DIRS}
)

target_compile_definitions(sdo PRIVATE
    SDO_VERSION="${PROJECT_VERSION}"
    $<$<CONFIG:Debug>:SDO_DEBUG=1>
)

target_compile_options(sdo PRIVATE
    ${GTK3_CFLAGS_OTHER}
)

target_link_libraries(sdo PRIVATE
    ${GTK3_LIBRARIES}
    ${SQLITE3_LIBRARIES}
    ${OPENSSL_LIBRARIES}
    ${UUID_LIBRARIES}
    stdc++fs
    pthread
)

target_link_directories(sdo PRIVATE
    ${GTK3_LIBRARY_DIRS}
    ${SQLITE3_LIBRARY_DIRS}
    ${OPENSSL_LIBRARY_DIRS}
)

# ─── Tests ────────────────────────────────────────────────────────────────────
option(SDO_BUILD_TESTS "Build unit tests" ON)

if(SDO_BUILD_TESTS)
    enable_testing()

    # Test helper library (all sources except main.cpp)
    set(SDO_LIB_SOURCES ${SDO_SOURCES})
    list(REMOVE_ITEM SDO_LIB_SOURCES src/main.cpp)

    add_library(sdo_lib STATIC ${SDO_LIB_SOURCES})
    target_include_directories(sdo_lib PUBLIC
        ${CMAKE_SOURCE_DIR}/include
        ${GTK3_INCLUDE_DIRS}
        ${SQLITE3_INCLUDE_DIRS}
        ${OPENSSL_INCLUDE_DIRS}
        ${UUID_INCLUDE_DIRS}
    )
    target_compile_options(sdo_lib PUBLIC ${GTK3_CFLAGS_OTHER})
    target_link_libraries(sdo_lib PUBLIC
        ${GTK3_LIBRARIES}
        ${SQLITE3_LIBRARIES}
        ${OPENSSL_LIBRARIES}
        ${UUID_LIBRARIES}
        stdc++fs pthread
    )
    target_link_directories(sdo_lib PUBLIC
        ${GTK3_LIBRARY_DIRS}
        ${SQLITE3_LIBRARY_DIRS}
        ${OPENSSL_LIBRARY_DIRS}
    )

    # Individual test executables
    foreach(TEST_NAME
        test_file_analyzer
        test_database
        test_organizer_engine
        test_config_manager
        test_file_watcher
    )
        add_executable(${TEST_NAME} tests/${TEST_NAME}.cpp)
        target_link_libraries(${TEST_NAME} PRIVATE sdo_lib)
        add_test(NAME ${TEST_NAME} COMMAND ${TEST_NAME})
        set_tests_properties(${TEST_NAME} PROPERTIES TIMEOUT 30)
    endforeach()
endif()

# ─── Install rules ────────────────────────────────────────────────────────────
include(GNUInstallDirs)

install(TARGETS sdo
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
)

install(FILES resources/com.sdo.organizer.desktop
    DESTINATION ${CMAKE_INSTALL_DATADIR}/applications
)

install(FILES resources/icons/sdo_256.png
    DESTINATION ${CMAKE_INSTALL_DATADIR}/icons/hicolor/256x256/apps
    RENAME com.sdo.organizer.png
)

install(FILES resources/icons/sdo_128.png
    DESTINATION ${CMAKE_INSTALL_DATADIR}/icons/hicolor/128x128/apps
    RENAME com.sdo.organizer.png
)

install(FILES resources/icons/sdo_48.png
    DESTINATION ${CMAKE_INSTALL_DATADIR}/icons/hicolor/48x48/apps
    RENAME com.sdo.organizer.png
)

install(FILES resources/com.sdo.organizer.appdata.xml
    DESTINATION ${CMAKE_INSTALL_DATADIR}/metainfo
)

install(FILES README.md
    DESTINATION ${CMAKE_INSTALL_DOCDIR}
)

# ─── CPack packaging ──────────────────────────────────────────────────────────
set(CPACK_PACKAGE_NAME              "smart-downloads-organizer")
set(CPACK_PACKAGE_VENDOR            "SDO Project")
set(CPACK_PACKAGE_DESCRIPTION_SHORT "Enterprise automatic downloads folder organizer")
set(CPACK_PACKAGE_VERSION           "${PROJECT_VERSION}")
set(CPACK_PACKAGE_CONTACT           "sdo@example.com")
set(CPACK_RESOURCE_FILE_LICENSE     "${CMAKE_SOURCE_DIR}/LICENSE")
set(CPACK_RESOURCE_FILE_README      "${CMAKE_SOURCE_DIR}/README.md")

# DEB
set(CPACK_DEBIAN_PACKAGE_DEPENDS
    "libgtk-3-0 (>= 3.24), libsqlite3-0 (>= 3.31), libssl3 (>= 3.0), libuuid1 (>= 2.34)")
set(CPACK_DEBIAN_PACKAGE_SECTION    "utils")
set(CPACK_DEBIAN_PACKAGE_PRIORITY   "optional")
set(CPACK_DEBIAN_PACKAGE_HOMEPAGE   "https://github.com/sdo/organizer")

# RPM
set(CPACK_RPM_PACKAGE_REQUIRES
    "gtk3 >= 3.24, sqlite >= 3.31, openssl-libs >= 3.0, libuuid >= 2.34")
set(CPACK_RPM_PACKAGE_GROUP         "Applications/System")
set(CPACK_RPM_PACKAGE_LICENSE       "MIT")

include(CPack)

# ─── Status summary ───────────────────────────────────────────────────────────
message(STATUS "")
message(STATUS "─────────────────────────────────────────")
message(STATUS " Smart Downloads Organizer ${PROJECT_VERSION}")
message(STATUS "─────────────────────────────────────────")
message(STATUS " Build type : ${CMAKE_BUILD_TYPE}")
message(STATUS " Install to : ${CMAKE_INSTALL_PREFIX}")
message(STATUS " Tests      : ${SDO_BUILD_TESTS}")
message(STATUS " GTK3       : ${GTK3_VERSION}")
message(STATUS " SQLite     : ${SQLITE3_VERSION}")
message(STATUS " OpenSSL    : ${OPENSSL_VERSION}")
message(STATUS "─────────────────────────────────────────")
message(STATUS "")
