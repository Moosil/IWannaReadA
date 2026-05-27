cmake_minimum_required(VERSION 4.0)
project(OCRTest VERSION 1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED TRUE)

find_package(fmt CONFIG REQUIRED)
find_package(yaml-cpp CONFIG REQUIRED)
find_package(ncnn CONFIG REQUIRED)
find_package(OpenCV CONFIG REQUIRED)
find_package(Clipper2 CONFIG REQUIRED)
find_package(spdlog CONFIG REQUIRED)
find_package(clip CONFIG REQUIRED)
find_package(nlohmann_json CONFIG REQUIRED)
find_package(httplib CONFIG REQUIRED)
find_package(ZLIB REQUIRED)
find_package(Qt6 CONFIG REQUIRED COMPONENTS Widgets Concurrent)

set(CMAKE_AUTOMOC TRUE)

if(MSVC)
    set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} /MT")
    set(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} /MTd")
    add_compile_options(/utf-8)
endif()

set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS} /fp:contract /GA /Qpar")

add_compile_definitions(WIN32_LEAN_AND_MEAN)

add_subdirectory(third_party/utfcpp)
add_subdirectory(third_party/QHotkey)

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
        src/cccedict.h
        src/application.cpp
        src/application.h
)

set_property(TARGET ${PROJECT_NAME} PROPERTY INTERPROCEDURAL_OPTIMIZATION TRUE)

target_link_libraries(${PROJECT_NAME} PRIVATE
        yaml-cpp::yaml-cpp
        Clipper2::Clipper2
        spdlog::spdlog
        ncnn
        ${OpenCV_LIBS}
        fmt::fmt
        utf8cpp
        clip::clip
        nlohmann_json::nlohmann_json
        httplib::httplib
        Qt6::Widgets
        Qt6::Concurrent
        qhotkey
)

add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E make_directory
        $<TARGET_FILE_DIR:${PROJECT_NAME}>/platforms

        COMMAND ${CMAKE_COMMAND} -E copy_if_different
        ${CMAKE_CURRENT_BINARY_DIR}/vcpkg_installed/x64-windows/Qt6/plugins/platforms/qwindows.dll
        $<TARGET_FILE_DIR:${PROJECT_NAME}>/platforms/qwindows.dll
)