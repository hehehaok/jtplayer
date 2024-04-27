cmake_minimum_required(VERSION 3.5)

project(jtPlayer LANGUAGES CXX)

set(CMAKE_INCLUDE_CURRENT_DIR ON)

set(CMAKE_AUTOUIC ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)

set(CMAKE_CXX_STANDARD 11)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(CMAKE_RUNTIME_OUTPUT_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/../bin")

find_package(Qt5 COMPONENTS Widgets REQUIRED)

FILE(GLOB_RECURSE FORMS "*.ui")
FILE(GLOB_RECURSE RESOURCES "*.qrc")
FILE(GLOB_RECURSE HEADERS "*.h")
FILE(GLOB_RECURSE SOURCES "*cpp")

SOURCE_GROUP("Form Files" FILES ${FORMS})
SOURCE_GROUP("ReSource Files" FILES ${RESOURCES})
SOURCE_GROUP("Head Files" FILES ${HEADERS})
SOURCE_GROUP("Source Files" FILES ${SOURCES})

if(ANDROID)
  add_library(jtPlayer SHARED
      ${FORMS}
      ${RESOURCES}
      ${HEADERS}
      ${SOURCES}
  )
else()
  add_executable(jtPlayer
      ${FORMS}
      ${RESOURCES}
      ${HEADERS}
      ${SOURCES}
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
