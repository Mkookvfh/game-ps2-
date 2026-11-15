
cmake_minimum_required(VERSION 3.22.1)

project(mueang_android C)

set(NATIVE_APP_GLUE_DIR
    ${CMAKE_ANDROID_NDK}/sources/android/native_app_glue
)

add_library(app_glue STATIC
    ${NATIVE_APP_GLUE_DIR}/android_native_app_glue.c
)

target_include_directories(app_glue PUBLIC
    ${NATIVE_APP_GLUE_DIR}
)

add_library(mueang_android SHARED
    native_main.c
    renderer3d.c
    ../../../../../../shared/game.c
)

target_include_directories(mueang_android PRIVATE
    ../../../../../../shared
)

target_compile_features(mueang_android PRIVATE c_std_11)

target_link_libraries(mueang_android
    app_glue
    android
    log
    EGL
    GLESv2
    m
)
