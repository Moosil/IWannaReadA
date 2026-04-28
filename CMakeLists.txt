cmake_minimum_required(VERSION 4.0)
project(OCRTest VERSION 1.0)

set(CMAKE_CXX_STANDARD 23)

find_package(fmt CONFIG REQUIRED)
find_package(yaml-cpp CONFIG REQUIRED)
find_package(ncnn CONFIG REQUIRED)
find_package(OpenCV CONFIG REQUIRED)
find_package(Clipper2 CONFIG REQUIRED)
find_package(spdlog CONFIG REQUIRED)
find_package(turbobase64 CONFIG REQUIRED)
find_package(unofficial-webview2 CONFIG REQUIRED)
find_package(clip CONFIG REQUIRED)
find_package(nlohmann_json CONFIG REQUIRED)
find_package(httplib CONFIG REQUIRED)
find_package(ZLIB REQUIRED)

# Check if found
if(NOT LEXBOR_INCLUDE_DIR OR NOT LEXBOR_LIBRARY)
    message(FATAL_ERROR "lexbor not found")
endif()

if(MSVC)
    set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} /MT")
    set(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} /MTd")
    add_compile_options(/utf-8)
endif()

set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS} /fp:contract /GA /Qpar")
set(TARGET lto PROPERTY INTERPROCEDURAL_OPTIMIZATION TRUE)

add_compile_definitions(WIN32_LEAN_AND_MEAN)

add_subdirectory(third_party/utfcpp)

add_executable(${PROJECT_NAME}
        ${CMAKE_CURRENT_SOURCE_DIR}/src/ocr_engine.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/src/ocr_engine.h
        ${CMAKE_CURRENT_SOURCE_DIR}/src/det.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/src/det.h
        ${CMAKE_CURRENT_SOURCE_DIR}/src/util.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/src/util.h
        ${CMAKE_CURRENT_SOURCE_DIR}/src/rec.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/src/rec.h
        ${CMAKE_CURRENT_SOURCE_DIR}/src/main.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/src/config.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/src/config.h
        ${CMAKE_CURRENT_SOURCE_DIR}/src/screenshot.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/src/screenshot.h
        ${CMAKE_CURRENT_SOURCE_DIR}/src/tooltip.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/src/tooltip.h
        ${CMAKE_CURRENT_SOURCE_DIR}/src/implwebview2.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/src/implwebview2.h
        ${CMAKE_CURRENT_SOURCE_DIR}/src/log.h
        ${CMAKE_CURRENT_SOURCE_DIR}/src/anki_connect.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/src/anki_connect.h
        ${CMAKE_CURRENT_SOURCE_DIR}/src/ccedict.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/src/dict_parser.h
        ${CMAKE_CURRENT_SOURCE_DIR}/src/util_utf8.h
        ${CMAKE_CURRENT_SOURCE_DIR}/src/util_pinyin.h
        ${CMAKE_CURRENT_SOURCE_DIR}/src/util_ocr.h
        ${CMAKE_CURRENT_SOURCE_DIR}/src/util_ocr.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/src/util_text.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/src/util_text.h
        ${CMAKE_CURRENT_SOURCE_DIR}/src/util_pinyin.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/src/util_utf8.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/src/log.cpp
)

target_include_directories(${PROJECT_NAME} PUBLIC src ${LEXBOR_INCLUDE_DIR})

target_link_libraries(${PROJECT_NAME} PRIVATE
        yaml-cpp::yaml-cpp
        Clipper2::Clipper2
        spdlog::spdlog
        ncnn
        ${OpenCV_LIBS}
        fmt::fmt
        msimg32 # AlphaBlend
        utf8cpp
        unofficial::webview2::webview2
        clip::clip
        nlohmann_json::nlohmann_json
        httplib::httplib
)