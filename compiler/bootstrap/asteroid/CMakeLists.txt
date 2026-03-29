cmake_minimum_required(VERSION 3.23)

project(yk-asteroid LANGUAGES CXX)

add_library(asteroid INTERFACE)

target_compile_features(asteroid INTERFACE cxx_std_23)

target_sources(
    asteroid
    INTERFACE
        FILE_SET yk_asteroid_headers
            TYPE HEADERS
            BASE_DIRS include
            FILES
                include/yk/asteroid/preprocess/lexer.hpp
)

add_library(yk::asteroid ALIAS asteroid)

option(YK_ASTERDOID_BUILD_TESTING "build testing" ${PROJECT_IS_TOP_LEVEL})

if(YK_ASTERDOID_BUILD_TESTING)
    include(CTest)
    add_subdirectory(test)
endif()
