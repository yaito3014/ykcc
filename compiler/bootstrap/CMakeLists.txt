cmake_minimum_required(VERSION 3.23)

project(ykcc LANGUAGES CXX)

include(FetchContent)
FetchContent_Declare(
  polyfill
  GIT_REPOSITORY https://github.com/yaito3014/polyfill.git
  GIT_TAG main
)
FetchContent_MakeAvailable(polyfill)

add_executable(ykcc)

target_compile_features(ykcc PRIVATE cxx_std_23)

target_sources(ykcc PRIVATE src/main.cpp)

add_subdirectory(asteroid)
target_link_libraries(ykcc PRIVATE yk::asteroid)

