cmake_minimum_required(VERSION 3.28)
project(tungsten_language VERSION 0.1.4)

add_subdirectory(tungsten)
#add_subdirectory(tpkg) # will continue working on after shipping the language

# === CPack ===
install(DIRECTORY "${CMAKE_BINARY_DIR}/tungsten/tungsten/bin" DESTINATION ".")
install(DIRECTORY "${CMAKE_BINARY_DIR}/tungsten/tungsten/lib" DESTINATION ".")
install(DIRECTORY "${CMAKE_BINARY_DIR}/tungsten/tungsten/include" DESTINATION ".")
#install(DIRECTORY "build/${CMAKE_BUILD_TYPE}/tpkg/tpkg" DESTINATION ".")

if (${CMAKE_SYSTEM_NAME} MATCHES "Windows")
    set(CPACK_GENERATOR "ZIP;7Z;WIX")
    set(CPACK_WIX_UPGRADE_GUID "79E19F45-55A1-4F2F-9258-5E98CF6B5694")
else ()
    set(CPACK_GENERATOR "TGZ;STGZ;7Z;DEB")
    set(CPACK_DEBIAN_PACKAGE_MAINTAINER "Riccardo Tedeschi")
    set(CPACK_DEBIAN_PACKAGE_ARCHITECTURE "amd64")
    set(CPACK_DEBIAN_PACKAGE_DESCRIPTION "Tungsten compiler and package manager")
    set(CPACK_DEBIAN_PACKAGE_DEPENDS "libcurl4, libarchive13, llvm")
endif ()

set(CPACK_RESOURCE_FILE_LICENSE "${CMAKE_SOURCE_DIR}/LICENSE.txt")
set(CPACK_PACKAGE_README "${CMAKE_SOURCE_DIR}/README.md")
set(CPACK_PACKAGE_VENDOR "Riccardo Tedeschi")
set(CPACK_PACKAGE_DESCRIPTION_SUMMARY "Tungsten compiler and package manager")
set(CPACK_PACKAGE_NAME "Tungsten")
set(CPACK_PACKAGE_VERSION ${PROJECT_VERSION})

include(CPack)