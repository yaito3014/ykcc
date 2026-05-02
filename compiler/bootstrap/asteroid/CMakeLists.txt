cmake_minimum_required(VERSION 3.23)

project(yk-asteroid LANGUAGES CXX)

add_library(yk_asteroid INTERFACE)

target_compile_features(yk_asteroid INTERFACE cxx_std_26)

target_sources(
    yk_asteroid
    INTERFACE FILE_SET yk_asteroid_headers TYPE HEADERS BASE_DIRS include FILES
)

add_library(yk::asteroid ALIAS yk_asteroid)

option(YK_ASTEROID_BUILD_TESTING "build testing" ${PROJECT_IS_TOP_LEVEL})

if(YK_ASTEROID_BUILD_TESTING)
    include(CTest)
    add_subdirectory(test)
endif()
