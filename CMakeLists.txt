cmake_minimum_required(VERSION 3.5)

project(jtPlayer LANGUAGES CXX)

set(CMAKE_INCLUDE_CURRENT_DIR ON)

set(CMAKE_AUTOUIC ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)

set(CMAKE_CXX_STANDARD 11)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# QtCreator supports the following variables for Android, which are identical to qmake Android variables.
# Check http://doc.qt.io/qt-5/deployment-android.html for more information.
# They need to be set before the find_package(Qt5 ...) call.

#if(ANDROID)
#    set(ANDROID_PACKAGE_SOURCE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/android")
#    if (ANDROID_ABI STREQUAL "armeabi-v7a")
#        set(ANDROID_EXTRA_LIBS
#            ${CMAKE_CURRENT_SOURCE_DIR}/path/to/libcrypto.so
#            ${CMAKE_CURRENT_SOURCE_DIR}/path/to/libssl.so)
#    endif()
#endif()

find_package(Qt5 COMPONENTS Widgets REQUIRED)

if(ANDROID)
  add_library(jtPlayer SHARED
    main.cpp
    widget.cpp
    widget.h
    widget.ui
  )
else()
  add_executable(jtPlayer
    main.cpp
    widget.cpp
    widget.h
    widget.ui
    src/threadpool.cpp
  )
endif()

set(INCLUDEPATH ${CMAKE_CURRENT_SOURCE_DIR}/thirdParty/ffmpeg/include
                ${CMAKE_CURRENT_SOURCE_DIR}/thirdParty/sdl/include
                ${CMAKE_CURRENT_SOURCE_DIR}/include)
set(LINKPATH ${CMAKE_CURRENT_SOURCE_DIR}/thirdParty/ffmpeg/lib
             ${CMAKE_CURRENT_SOURCE_DIR}/thirdParty/sdl/lib/x64)
set(FFMPEGDLL avcodec-58.dll
              avdevice-58.dll
              avfilter-7.dll
              avformat-58.dll
              avutil-56.dll
              postproc-55.dll
              swresample-3.dll
              swscale-5.dll)
set(SDLDLL SDL2.dll)

target_include_directories(jtPlayer PRIVATE ${INCLUDEPATH})
target_link_directories(jtPlayer PRIVATE ${LINKPATH})
target_link_libraries(jtPlayer PRIVATE Qt5::Widgets)
target_link_libraries(jtPlayer PRIVATE ${FFMPEGDLL})
target_link_libraries(jtPlayer PRIVATE ${SDLDLL})
#target_compile_definitions(jtPlayer PRIVATE QT_NO_DEBUG_OUTPUT)
