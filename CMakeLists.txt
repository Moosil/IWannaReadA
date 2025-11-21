cmake_minimum_required(VERSION 4.0)
project(OCRTest VERSION 1.0)

set(CMAKE_CXX_STANDARD 23)

find_package(yaml-cpp CONFIG REQUIRED)
find_package(ncnn CONFIG REQUIRED)
find_package(unofficial-usockets CONFIG REQUIRED)
find_package(unofficial-uwebsockets CONFIG REQUIRED)
find_package(OpenCV CONFIG REQUIRED)
find_package(Clipper2 CONFIG REQUIRED)
find_package(spdlog CONFIG REQUIRED)
find_package(fmt CONFIG REQUIRED)

if(MSVC)
    set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} /MT")
    set(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} /MTd")
endif()

add_executable(${PROJECT_NAME}
        ${CMAKE_CURRENT_SOURCE_DIR}/src/ocr_engine.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/src/det.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/src/util.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/src/rec.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/src/main.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/src/config.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/src/screenshot.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/src/tooltip.cpp
)

target_include_directories(${PROJECT_NAME} PUBLIC src)

target_link_libraries(${PROJECT_NAME} PRIVATE
        yaml-cpp::yaml-cpp
        Clipper2::Clipper2
        spdlog::spdlog
        ncnn
        unofficial::usockets::usockets
        unofficial::uwebsockets::uwebsockets
        ${OpenCV_LIBS}
        fmt::fmt
        msimg32 # AlphaBlend
)