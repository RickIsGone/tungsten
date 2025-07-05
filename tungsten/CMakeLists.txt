cmake_minimum_required(VERSION 3.28)
project(tungsten_compiler VERSION 0.1.0)

# === Compiler Settings ===
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# === Required Packages ===
find_package(LLVM REQUIRED CONFIG)
list(APPEND CMAKE_MODULE_PATH "${LLVM_CMAKE_DIR}")
find_file(AddLLVM_CMAKE_FILE AddLLVM.cmake)
include(HandleLLVMOptions)
include(AddLLVM)

# === CMake Options ===
if (CMAKE_BUILD_TYPE MATCHES Debug OR CMAKE_BUILD_TYPE MATCHES RelWithDebInfo)
    add_definitions(-DTUNGSTEN_DEBUG)
endif ()

# === Source Files ===
file(GLOB_RECURSE SRC "src/*.cpp")
add_llvm_executable(tungsten ${SRC})

# === LLVM ===
target_include_directories(tungsten PRIVATE ${LLVM_INCLUDE_DIRS})
llvm_map_components_to_libnames(LLVM_LIBS
        support
        core
        irreader
        native
        nativecodegen
        orcjit
)
message("found LLVM at: ${LLVM_DIR}")
message("found LLVM libs: ${LLVM_LIBS}")
target_link_libraries(tungsten PRIVATE ${LLVM_LIBS})
add_definitions(${LLVM_DEFINITIONS})

# === Emscripten ===
if (EMSCRIPTEN)
    find_package(ZLIB REQUIRED)
    target_link_libraries(tungsten PRIVATE ZLIB::ZLIB)
    set_target_properties(tungsten PROPERTIES
            LINK_FLAGS "--bind -sEXPORTED_RUNTIME_METHODS=['FS','FS_writeFile','FS_readFile','callMain'] -sEXCEPTION_CATCHING_ALLOWED='1'"
            OUTPUT_NAME "tungsten"
    )
endif ()

# === Modules ===
file(GLOB_RECURSE MODULES "src/*.cppm")
target_sources(tungsten PUBLIC FILE_SET tungsten_modules TYPE CXX_MODULES FILES
        ${MODULES}
)

# === Output Structure ===
set(TUNGSTEN_OUTPUT_DIR ${CMAKE_BINARY_DIR}/tungsten/tungsten)
file(MAKE_DIRECTORY ${TUNGSTEN_OUTPUT_DIR})
file(MAKE_DIRECTORY ${TUNGSTEN_OUTPUT_DIR}/bin)
file(MAKE_DIRECTORY ${TUNGSTEN_OUTPUT_DIR}/include)
set_target_properties(tungsten PROPERTIES
        RUNTIME_OUTPUT_DIRECTORY ${TUNGSTEN_OUTPUT_DIR}/bin
)

# === Copy stdlib modules ===
file(GLOB STDLIB "src/stdlib/*.tgs")
foreach (LIB IN LISTS STDLIB)
    get_filename_component(LIBNAME ${LIB} NAME)
    message("copying lib: ${LIB} as ${LIBNAME}")
    file(COPY_FILE ${LIB} ${TUNGSTEN_OUTPUT_DIR}/include/${LIBNAME})
endforeach ()