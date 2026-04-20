cmake_minimum_required(VERSION 3.23)

project(yk-asteroid LANGUAGES CXX)

add_library(asteroid INTERFACE)

target_compile_features(asteroid INTERFACE cxx_std_26)

target_sources(
    asteroid
    INTERFACE
        FILE_SET yk_asteroid_headers
            TYPE HEADERS
            BASE_DIRS include
            FILES
                include/yk/asteroid/diagnostics/diagnostic.hpp
                include/yk/asteroid/diagnostics/source_location.hpp
                include/yk/asteroid/preprocess/directive.hpp
                include/yk/asteroid/preprocess/lexer.hpp
                include/yk/asteroid/preprocess/line_splicer.hpp
                include/yk/asteroid/preprocess/macro.hpp
                include/yk/asteroid/preprocess/pp_token.hpp
                include/yk/asteroid/preprocess/preprocessor.hpp
)

add_library(yk::asteroid ALIAS asteroid)

option(YK_ASTEROID_BUILD_TESTING "build testing" ${PROJECT_IS_TOP_LEVEL})

if(YK_ASTEROID_BUILD_TESTING)
    include(CTest)
    add_subdirectory(test)
endif()
