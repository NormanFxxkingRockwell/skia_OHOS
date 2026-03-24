# the minimum version of CMake.
cmake_minimum_required(VERSION 3.5.0)
project(skia_OHOS)

set(NATIVERENDER_ROOT_PATH ${CMAKE_CURRENT_SOURCE_DIR})
set(SKIA_ROOT_DIR ${NATIVERENDER_ROOT_PATH}/../../../../skia)
set(SKIA_INCLUDE_DIR ${SKIA_ROOT_DIR})
set(SKIA_LIB_DIR ${SKIA_ROOT_DIR}/prebuilt/${CMAKE_OHOS_ARCH_ABI})

if(DEFINED PACKAGE_FIND_FILE)
    include(${PACKAGE_FIND_FILE})
endif()

include_directories(${NATIVERENDER_ROOT_PATH}
                    ${NATIVERENDER_ROOT_PATH}/include
                    ${SKIA_INCLUDE_DIR})

add_library(skia SHARED IMPORTED)
set_target_properties(skia PROPERTIES
                      IMPORTED_LOCATION ${SKIA_LIB_DIR}/libskia.so)

add_library(entry SHARED napi_init.cpp)
target_compile_features(entry PRIVATE cxx_std_17)
target_link_libraries(entry PUBLIC
                      libace_napi.z.so
                      libace_ndk.z.so
                      libnative_window.so
                      libEGL.so
                      libGLESv3.so
                      libhilog_ndk.z.so
                      skia)
