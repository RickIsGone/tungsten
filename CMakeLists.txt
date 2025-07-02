cmake_minimum_required(VERSION 3.28)
project(tungsten VERSION 0.1.0)

set(CMAKE_CXX_STANDARD 20)
list(APPEND CMAKE_MODULE_PATH "${LLVM_DIR}") # for find_package(LLVM)
message("CMAKE_MODULE_PATH: ${CMAKE_MODULE_PATH}")
include(AddLLVM)
file(GLOB_RECURSE SRC "src/*.cpp")
add_llvm_executable(tungsten ${SRC})

if (CMAKE_BUILD_TYPE MATCHES Debug OR CMAKE_BUILD_TYPE MATCHES RelWithDebInfo)
    add_definitions(-DTUNGSTEN_DEBUG)
endif ()

if (EMSCRIPTEN)
    set(ZLIB_LIBRARY "C:/vcpkg/vcpkg/installed/x64-windows/lib") # for vcpkg
    set(ZLIB_INCLUDE_DIR "C:/vcpkg/vcpkg/installed/x64-windows/include") # for vcpkg
    find_package(ZLIB REQUIRED)
    target_link_libraries(tungsten PRIVATE ZLIB::ZLIB)
    set_target_properties(tungsten PROPERTIES
            LINK_FLAGS "--bind -sEXPORTED_RUNTIME_METHODS=['FS','FS_writeFile','FS_readFile','callMain'] -sEXCEPTION_CATCHING_ALLOWED='1'"
            OUTPUT_NAME "tungsten"
    )
endif ()

find_package(LLVM REQUIRED CONFIG)
include_directories(${LLVM_INCLUDE_DIRS})
llvm_map_components_to_libnames(LLVM_LIBS
        core
        support
        irreader
        native
        nativecodegen
        orcjit
)
message("found LLVM libs: ${LLVM_LIBS}")
target_link_libraries(tungsten PRIVATE ${LLVM_LIBS})
add_definitions(${LLVM_DEFINITIONS})

file(GLOB_RECURSE MODULES "src/*.cppm")
target_sources(tungsten PUBLIC FILE_SET tungsten_modules TYPE CXX_MODULES FILES
        ${MODULES}
)