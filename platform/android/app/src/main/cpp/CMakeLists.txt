cmake_minimum_required(VERSION 3.22.1)

project(mueang_android C)

set(NATIVE_APP_GLUE
    ${CMAKE_ANDROID_NDK}/sources/android/native_app_glue/android_native_app_glue.c
)

add_library(mueang_android SHARED
    native_main.c
    ../../../../../../shared/game.c
    ${NATIVE_APP_GLUE}
)

target_include_directories(mueang_android PRIVATE
    ${CMAKE_ANDROID_NDK}/sources/android/native_app_glue
    ../../../../../../shared
)

target_compile_features(mueang_android PRIVATE c_std_11)

target_link_libraries(mueang_android
    android
    log
    EGL
    GLESv2
    m
)