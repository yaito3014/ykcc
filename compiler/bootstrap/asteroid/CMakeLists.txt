cmake_minimum_required(VERSION 3.23)

project(yk-asteroid LANGUAGES CXX)

add_library(asteroid STATIC)

target_compile_features(asteroid PUBLIC cxx_std_23)

target_sources(asteroid PUBLIC FILE_SET yk_asteroid_headers TYPE HEADERS BASE_DIRS include FILES
  include/yk/asteroid/core.hpp
  include/yk/asteroid/nullable_char.hpp
  include/yk/asteroid/preprocess/token.hpp
  include/yk/asteroid/preprocess/lexer.hpp
  include/yk/asteroid/preprocess/lexer.ipp
)

target_sources(asteroid PRIVATE
  src/core.cpp
)

target_link_libraries(asteroid PUBLIC yk::polyfill::common)

add_library(yk::asteroid ALIAS asteroid)

enable_testing()
add_subdirectory(test)
