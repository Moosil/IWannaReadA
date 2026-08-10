cmake_minimum_required(VERSION 4.0)
project(OCRTest VERSION 1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED TRUE)

find_package(Qt6 CONFIG REQUIRED COMPONENTS Widgets Concurrent)
find_package(OpenCV CONFIG REQUIRED)

set(CMAKE_AUTOMOC TRUE)

if(MSVC)
    add_compile_options(/utf-8)
endif()

set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS} /fp:contract /GA /Qpar")

add_subdirectory(third_party/clip)
add_subdirectory(third_party/clipper2/CPP)
add_subdirectory(third_party/fmt)
add_subdirectory(third_party/httplib)
add_subdirectory(third_party/indicators)
add_subdirectory(third_party/json)
add_subdirectory(third_party/mio)
add_subdirectory(third_party/ncnn)
add_subdirectory(third_party/QHotkey)
add_subdirectory(third_party/spdlog)
add_subdirectory(third_party/utfcpp)
add_subdirectory(third_party/yaml-cpp)

add_executable(${PROJECT_NAME}
        ${CMAKE_CURRENT_SOURCE_DIR}/src/ocr_engine.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/src/ocr_engine.h
        ${CMAKE_CURRENT_SOURCE_DIR}/src/det.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/src/det.h
        ${CMAKE_CURRENT_SOURCE_DIR}/src/rec.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/src/rec.h
        ${CMAKE_CURRENT_SOURCE_DIR}/src/main.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/src/config.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/src/config.h
        ${CMAKE_CURRENT_SOURCE_DIR}/src/screenshot.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/src/screenshot.h
        ${CMAKE_CURRENT_SOURCE_DIR}/src/tooltip.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/src/tooltip.h
        ${CMAKE_CURRENT_SOURCE_DIR}/src/log.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/src/log.h
        ${CMAKE_CURRENT_SOURCE_DIR}/src/anki_connect.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/src/anki_connect.h
        ${CMAKE_CURRENT_SOURCE_DIR}/src/ccedict.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/src/dict_parser.h
        ${CMAKE_CURRENT_SOURCE_DIR}/src/util.h
        ${CMAKE_CURRENT_SOURCE_DIR}/src/util_ocr.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/src/util_ocr.h
        ${CMAKE_CURRENT_SOURCE_DIR}/src/util_text.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/src/util_text.h
        ${CMAKE_CURRENT_SOURCE_DIR}/src/util_utf8.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/src/util_utf8.h
        ${CMAKE_CURRENT_SOURCE_DIR}/src/screenshot_viewer.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/src/screenshot_viewer.h
        ${CMAKE_CURRENT_SOURCE_DIR}/src/tooltip_entry.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/src/tooltip_entry.h
        ${CMAKE_CURRENT_SOURCE_DIR}/src/util_qt.h
        ${CMAKE_CURRENT_SOURCE_DIR}/src/cccedict.h
        ${CMAKE_CURRENT_SOURCE_DIR}/src/application.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/src/application.h
)

if (WIN32)
    target_compile_definitions(${PROJECT_NAME} PRIVATE WIN32_LEAN_AND_MEAN)
endif()

if(OPENMP_FOUND)
    target_compile_definitions(${PROJECT_NAME} PRIVATE OPENMP_FOUND=1)
else()
    target_compile_definitions(${PROJECT_NAME} PRIVATE OPENMP_FOUND=0)
endif()

set_property(TARGET ${PROJECT_NAME} PROPERTY INTERPROCEDURAL_OPTIMIZATION TRUE)

target_include_directories(${PROJECT_NAME} PRIVATE ${OpenCV_INCLUDE_DIRS})

target_link_libraries(${PROJECT_NAME} PRIVATE
        yaml-cpp
        Clipper2
        spdlog
        ncnn
        ${OpenCV_LIBS}
        fmt
        utf8cpp
        clip
        nlohmann_json
        httplib
        Qt6::Widgets
        Qt6::Concurrent
        qhotkey
        indicators
        mio
)

add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E make_directory
        $<TARGET_FILE_DIR:${PROJECT_NAME}>/platforms

        COMMAND ${CMAKE_COMMAND} -E copy_if_different
        ${CMAKE_CURRENT_BINARY_DIR}/vcpkg_installed/x64-windows/Qt6/plugins/platforms/qwindows.dll
        $<TARGET_FILE_DIR:${PROJECT_NAME}>/platforms/qwindows.dll
)