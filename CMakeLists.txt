cmake_minimum_required(VERSION 3.28)
project(tungsten VERSION 0.1.0)

set(CMAKE_CXX_STANDARD 20)
file(GLOB_RECURSE SRC "src/*.cpp")
add_executable(tungsten ${SRC})

if (CMAKE_BUILD_TYPE MATCHES Debug)
    add_definitions(-DTUNGSTEN_DEBUG)
endif ()

if (EMSCRIPTEN)
    set_target_properties(tungsten PROPERTIES
            LINK_FLAGS "--bind -sEXPORTED_RUNTIME_METHODS=['FS','FS_writeFile','FS_readFile','callMain'] -sEXCEPTION_CATCHING_ALLOWED='1'"
            OUTPUT_NAME "tungsten"
    )
endif ()

find_package(LLVM REQUIRED CONFIG)
message("Found LLVM at: ${LLVM_DIR}")
include_directories(${LLVM_INCLUDE_DIRS})
message("llvm include dirs: ${LLVM_INCLUDE_DIRS}")
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