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
find_path(RAPIDXML_INCLUDE_DIRS "rapidxml/rapidxml.hpp")
find_package(ZLIB REQUIRED)

if(MSVC)
    set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} /MT")
    set(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} /MTd")
endif()


add_subdirectory(third_party/mdict)
add_subdirectory(third_party/utfcpp)

add_executable(${PROJECT_NAME}
        ${CMAKE_CURRENT_SOURCE_DIR}/src/ocr_engine.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/src/det.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/src/util.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/src/rec.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/src/main.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/src/config.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/src/screenshot.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/src/tooltip.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/src/implwebview2.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/src/log.h
        ${CMAKE_CURRENT_SOURCE_DIR}/src/task.h
)

target_include_directories(${PROJECT_NAME} PUBLIC src
        ${RAPIDXML_INCLUDE_DIRS}
)

target_link_libraries(${PROJECT_NAME} PRIVATE
        yaml-cpp::yaml-cpp
        Clipper2::Clipper2
        spdlog::spdlog
        ncnn
        ${OpenCV_LIBS}
        fmt::fmt
        msimg32 # AlphaBlend
        utf8cpp
        mdict
        unofficial::webview2::webview2
        clip::clip
        nlohmann_json::nlohmann_json
)