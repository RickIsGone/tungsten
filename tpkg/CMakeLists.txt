cmake_minimum_required(VERSION 3.28)
project(tungsten_package_manager VERSION 0.1.0)
# === Compiler Settings ===
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# === Source Files ===
file(GLOB_RECURSE SRC "src/*.cpp")
add_executable(tpkg ${SRC})

# === Modules ===
file(GLOB_RECURSE MODULES "src/*.cppm")
target_sources(tpkg PUBLIC FILE_SET tpkg_modules TYPE CXX_MODULES FILES
        ${MODULES}
)

# === Dependencies ===
if (WIN32)
    set(VCPKG_TARGET_TRIPLET "x64-windows-static")
    add_definitions(-DCURL_STATICLIB)
endif ()
find_package(CURL REQUIRED)
find_package(LibArchive REQUIRED)
target_include_directories(tpkg PRIVATE ${CURL_INCLUDE_DIRS})
target_link_libraries(tpkg PRIVATE CURL::libcurl)
target_link_libraries(tpkg PRIVATE LibArchive::LibArchive)

# === Output Structure ===
set(TPKG_OUTPUT_DIR ${CMAKE_BINARY_DIR}/tpkg/tpkg)
file(MAKE_DIRECTORY ${TPKG_OUTPUT_DIR}/bin)
file(MAKE_DIRECTORY ${TPKG_OUTPUT_DIR}/downloads)
file(MAKE_DIRECTORY ${TPKG_OUTPUT_DIR}/installed)

# Imposta dove mettere l'eseguibile
set_target_properties(tpkg PROPERTIES
        RUNTIME_OUTPUT_DIRECTORY ${TPKG_OUTPUT_DIR}
)
